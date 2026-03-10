#include "stdafx.h"
#include "Monitor.h"
#include "Utils.h"
#include <psapi.h> // EnumProcessModules 

static constexpr bool g_bEnablePerfLog = false; // Enables the performance log, stored in "C:\Users\...\AppData\Local\Temp\AlmonLogs"
static constexpr bool g_bEnableAllocTraces = ENABLE_TRACE_ALLOC;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ModuleRanges

bool ModuleRanges::FindModuleRetry(UINT_PTR address, CString& sModuleName)
{
	if (FindModule(address, sModuleName))
		return true;
	if (!UpdateModuleList(m_dwProcessId, m_modules))
		return false;
	return FindModule(address, sModuleName);
}

bool ModuleRanges::FindModule(UINT_PTR address, CString& sModuleName)
{
	for (auto moduleInfo : m_modules)
	{
		if (address >= moduleInfo.startAddress && address <= moduleInfo.endAddress)
		{
			sModuleName = moduleInfo.sName;
			return true;
		}
	}
	return false;
}

bool ModuleRanges::UpdateModuleList(DWORD processID, std::vector<MODULEINFOEX>& modules)
{
	modules.clear();
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
	if (nullptr == hProcess)
		return false;

	// Get a list of all the modules in this process.
	MODULEINFO moduleInfo;
	TCHAR szModName[MAX_PATH];
	HMODULE hMods[1024];
	DWORD cbNeeded;
	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			if (GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)) &&
				GetModuleInformation(hProcess, hMods[i], &moduleInfo, sizeof(MODULEINFO)))
			{
				TCHAR* pModuleName, * pModuleNameTemp = &(szModName[0]);
				do
				{
					pModuleName = pModuleNameTemp;
					pModuleNameTemp = _tcsstr(pModuleNameTemp + 1, _T("\\"));
				} while (pModuleNameTemp);
				if (pModuleName[0] == _T('\\')) pModuleName++;

				DWORD64 startAddress = reinterpret_cast<DWORD64>(moduleInfo.lpBaseOfDll);
				modules.emplace_back(startAddress, moduleInfo.SizeOfImage, pModuleName);
			}
		}
	}
	std::sort(modules.begin(), modules.end(), [](MODULEINFOEX a, MODULEINFOEX b) { return a.startAddress > b.startAddress; });
	CloseHandle(hProcess);
	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// TextMessages

TextMessages::TextMessages() : m_idxWrite(0), m_numMessages(0)
{
	InitializeSRWLock(&m_lckTextMessages);
}

void TextMessages::AddTextMessage(const CString& message)
{
	AcquireSRWLockExclusive(&m_lckTextMessages);
	m_textMessages[m_idxWrite++] = message;
	if (m_idxWrite >= m_maxSize)
		m_idxWrite = 0;
	++m_numMessages;
	ReleaseSRWLockExclusive(&m_lckTextMessages);
}

bool TextMessages::GetTextMessages(CString& messages, DWORD& numMessages)
{
	messages.Empty();
	if (numMessages != m_numMessages)
	{
		AcquireSRWLockExclusive(&m_lckTextMessages);
		numMessages = m_numMessages;
		if (m_numMessages >= m_maxSize)
		{
			for (DWORD i = m_idxWrite; i < m_maxSize; i++)
			{
				messages.Append(m_textMessages[i]);
				messages.Append("\r\n");
			}
		}
		for (DWORD i = 0; i < m_idxWrite; i++)
		{
			messages.Append(m_textMessages[i]);
			messages.Append("\r\n");
		}
		ReleaseSRWLockExclusive(&m_lckTextMessages);
		return true;
	}
	return false;
}

void TextMessages::Clear()
{
	m_textMessages->Empty();
	m_idxWrite = 0;
	m_numMessages = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Monitor

Monitor::Monitor()
{
	ZeroMemory(&m_targetProcInfo, sizeof(m_targetProcInfo));

	InitializeSRWLock(&m_lckBufferQueue);
	InitializeSRWLock(&m_lckGraphQueue);
	InitializeSRWLock(&m_lckAddrQueue);
	InitializeSRWLock(&m_lckAddrMap);

	m_hExitThreads = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

Monitor::~Monitor()
{
	ClearTranslatedAddresses();
	CloseHandle(m_hExitThreads);
}

void Monitor::ClearTranslatedAddresses()
{
	for (CString* pStr : m_listTranslatedAddr)
		delete pStr;
	m_listTranslatedAddr.clear();
}

void Monitor::ClearAllocs()
{
	if (m_threadAllocReader.IsStarted())
	{
		// Clear the allocations from the working thread.
		m_bClearAllocs = true;
		while (m_bClearAllocs) // Wait to be false (should be quick).
			Sleep(20);
	}
	else
		ClearAllocsInternal(); // Clear the allocations here.
}

void Monitor::ClearAllocsInternal()
{
	AcquireSRWLockExclusive(&m_lckBufferQueue);
	ClearQueue(&m_bufferQueue);
	ReleaseSRWLockExclusive(&m_lckBufferQueue);

	AcquireSRWLockExclusive(&m_lckGraphQueue);
	ClearContainer(m_graphQueue);
	ReleaseSRWLockExclusive(&m_lckGraphQueue);

	m_activeAllocationsMap.clear();
	m_callStackMap.clear();

	m_totalAllocs = 0;
	m_totalFrees = 0;
	m_totalBytes = 0;
}

DWORD Monitor::LoadLibraryInjection(HANDLE hProcess, const char* dllName)
{
	LPVOID remoteString = (LPVOID)VirtualAllocEx(hProcess, nullptr, strlen(dllName), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (remoteString == nullptr)
	{
		CloseHandle(hProcess); // Close the process handle.
		throw std::runtime_error("LoadLibraryInjection: Error on VirtualAllocEx.");
	}

	if (WriteProcessMemory(hProcess, (LPVOID)remoteString, dllName, strlen(dllName), nullptr) == 0)
	{
		VirtualFreeEx(hProcess, remoteString, 0, MEM_RELEASE); // Free the memory we were going to use.
		CloseHandle(hProcess); // Close the process handle.
		throw std::runtime_error("LoadLibraryInjection: Error on WriteProcessMemeory.");
	}

	LPVOID LoadLibAddy = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

	HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibAddy, (LPVOID)remoteString, 0, nullptr);
	if (hThread == nullptr)
	{
		VirtualFreeEx(hProcess, remoteString, 0, MEM_RELEASE); // Free the memory we were going to use.
		CloseHandle(hProcess); // Close the process handle.
		throw std::runtime_error("LoadLibraryInjection: Error on CreateRemoteThread.");
	}

	// Wait for the thread to finish.
	WaitForSingleObject(hThread, INFINITE);

	// Lets see what it says...
	DWORD threadExitCode = 0;
	GetExitCodeThread(hThread, &threadExitCode);

	// No need for this handle anymore, lets get rid of it.
	CloseHandle(hThread);

	// Lets clear up that memory we allocated earlier.
	VirtualFreeEx(hProcess, remoteString, 0, MEM_RELEASE);

	return threadExitCode;
}

void Monitor::AllocReaderThread()
{
	try
	{
		while (!m_bExitThreads || m_bufferQueue.size() > 0)
		{
			while (m_bufferQueue.size() > 0 || m_bClearAllocs)
			{
				if (m_bClearAllocs)
				{
					ClearAllocsInternal();
					m_bClearAllocs = false;
					break;
				}

				AcquireSRWLockExclusive(&m_lckBufferQueue);
				BufferInfo bufferInfo = m_bufferQueue.front();
				m_bufferQueue.pop();
				ReleaseSRWLockExclusive(&m_lckBufferQueue);
				UpdateCallStackMap(bufferInfo);

				if constexpr (g_bEnablePerfLog)
					m_perfLogger.AddBufferSize(m_bufferQueue.size());
			}
			if constexpr (g_bEnablePerfLog)
				m_perfLogger.AddBufferSize(m_bufferQueue.size());

			WaitForSingleObject(m_hExitThreads, 20);
		}
	}
	catch (CException* pEx)
	{
		TraceRel("Monitor::AllocReaderThread-> Exception: %s", GetExceptionMessage(pEx));
	}
}

void Monitor::UpdateCallStackMap(BufferInfo bufferInfo)
{
	long len = 0;
	AllocInfo info; // The call stack field will not be filled here.
	CallStack callStack; // The call stack will be copied here instead.
	CallStackInfo* pCallStackInfo = nullptr;
	CallStackInfo newCallStackInfo;

	BYTE* pBuffer = bufferInfo.pBuffer;

	while (len < bufferInfo.length)
	{
		CopyMemory((PVOID) & (info.addr), (PVOID)(pBuffer + len), sizeof(info.addr));
		len += sizeof(info.addr);

		if (IS_ALLOC(info.addr))
		{
			CopyMemory((PVOID) & (info.numBytes), (PVOID)(pBuffer + len), sizeof(info.numBytes));
			len += sizeof(info.numBytes);

			if constexpr (g_bEnableAllocTraces)
			{
				Trace("[ALLOC] addr: 0x%p - size: %zd", info.addr, info.numBytes);
				if (info.numBytes == 0)  Trace("[ALLOC] alloc size is ZERO");
				if (info.addr == 0)  	 Trace("[ALLOC] alloc addr is ZERO");
			}

			CopyMemory((PVOID) & (info.numFrames), (PVOID)(pBuffer + len), sizeof(info.numFrames));
			len += sizeof(info.numFrames);

			ZeroMemory((PVOID)(callStack.data()), BACKTRACE_SIZE * sizeof(UINT_PTR));
			CopyMemory((PVOID)(callStack.data()), (PVOID)(pBuffer + len), info.numFrames * sizeof(UINT_PTR));
			len += BACKTRACE_SIZE * sizeof(UINT_PTR);

			if (info.numFrames > 0)
			{
				m_totalAllocs++;
				m_totalBytes += info.numBytes;

				auto iter = m_callStackMap.find(callStack);

				if (iter == m_callStackMap.end())	// Call stack not found. Add it.
				{
					newCallStackInfo.numAllocs = 1;
					newCallStackInfo.numBytes = info.numBytes;
					newCallStackInfo.numBytesInUse = info.numBytes;
					newCallStackInfo.numFrames = info.numFrames;

					// Translate call stack
					UINT_PTR addr;
					for (int i = 0; i < info.numFrames; i++)
					{
						addr = callStack[i];
						AcquireSRWLockExclusive(&m_lckAddrMap);
						if (m_addrMap.find(addr) == m_addrMap.end())
						{
							// Add a new string
							CString* pAddrStr = new CString;
							pAddrStr->Format("0x%Ix", addr);
							m_listTranslatedAddr.push_back(pAddrStr);
							m_addrMap[addr] = pAddrStr;
							AcquireSRWLockExclusive(&m_lckAddrQueue);
							m_addrQueue.push(addr); // pending to translate
							ReleaseSRWLockExclusive(&m_lckAddrQueue);
						}
						newCallStackInfo.frames[i] = m_addrMap[addr];
						ReleaseSRWLockExclusive(&m_lckAddrMap);
					}

					const auto& [iter, inserted] = m_callStackMap.emplace(callStack, newCallStackInfo);
					pCallStackInfo = &(iter->second);
				}
				else
				{
					// Call stack found. Update counters.
					pCallStackInfo = &(iter->second);
					pCallStackInfo->numAllocs++;
					pCallStackInfo->numBytes += info.numBytes;
					pCallStackInfo->numBytesInUse += info.numBytes;
				}

				if constexpr (g_bEnableAllocTraces)
				{
					if (m_activeAllocationsMap.find(info.addr) != m_activeAllocationsMap.end())
						Trace("[ALLOC] addr: 0x%p already in map -> info will be lost", info.addr);
				}

				m_activeAllocationsMap.emplace(info.addr, ActiveAllocation{ info.numBytes , pCallStackInfo });

				AcquireSRWLockExclusive(&m_lckGraphQueue);
				m_graphQueue.emplace(info.numBytes, m_totalAllocs, m_totalBytes, callStack, pCallStackInfo);
				ReleaseSRWLockExclusive(&m_lckGraphQueue);
			}
			else ASSERT(0);

		}
		else // free
		{
			info.addr &= ~FREE_MASK; // Remove free mask.

			m_totalFrees++;

			if (info.addr != 0)
			{
				if constexpr (g_bEnableAllocTraces)
					Trace("[FREE] addr: 0x%Ix", info.addr);

				if (auto iter = m_activeAllocationsMap.find(info.addr); iter != m_activeAllocationsMap.end())
				{
					ActiveAllocation& activeAllocation = iter->second;
					activeAllocation.pCallStackInfo->numBytesInUse -= activeAllocation.numAllocBytes;
					m_activeAllocationsMap.erase(iter);
				}
			}
			else if constexpr (g_bEnableAllocTraces)
			{
				Trace("[FREE] addr is ZERO");
				ASSERT(0);
			}
		}
	}
	delete[] pBuffer;
}

void Monitor::BufferReaderThread()
{
	HANDLE hEvents[2];
	hEvents[0] = m_memFileAlloc.GetEventBufferReady();
	hEvents[1] = m_hExitThreads;

	ResetEvent(m_memFileAlloc.GetEventBufferReady());

	BufferInfo bufferInfo;

	DWORD waitResult;
	DWORD lastBufferCounter = (DWORD)-1;
	DWORD bufferCounter = 0;
	DWORD offset = 0;

	while (!m_bExitThreads)
	{
		waitResult = WaitForMultipleObjects(2, hEvents, FALSE, TIMEOUT);

		switch (waitResult)
		{
		case WAIT_OBJECT_0:
		{
			CopyMemory((PVOID) & (bufferCounter), (PVOID)(m_memFileAlloc.GetBuffer()), sizeof(DWORD));
			offset = sizeof(DWORD);

			if (bufferCounter != (lastBufferCounter + 1))
			{
				m_textMessages.AddTextMessage("Wrong buffer order.");
				TraceRel("Monitor::BufferReaderThread-> ERROR: wrong order: current=%d - last=%d", bufferCounter, lastBufferCounter);
			}
			lastBufferCounter = bufferCounter;

			CopyMemory((PVOID) & (bufferInfo.length), (PVOID)(m_memFileAlloc.GetBuffer() + offset), sizeof(long));
			offset += sizeof(long);

			bufferInfo.pBuffer = new BYTE[bufferInfo.length];
			CopyMemory((PVOID)(bufferInfo.pBuffer), (PVOID)((m_memFileAlloc.GetBuffer()) + offset), bufferInfo.length);

			AcquireSRWLockExclusive(&m_lckBufferQueue);
			m_bufferQueue.push(bufferInfo);
			ReleaseSRWLockExclusive(&m_lckBufferQueue);

			SetEvent(m_memFileAlloc.GetEventBufferProcessed());
		}
		break;
		case WAIT_TIMEOUT:
			break;
		case WAIT_FAILED:
			TraceRel("Monitor::BufferReaderThread-> wait failed with code %d\n", GetLastError());
			WaitForSingleObject(m_hExitThreads, THREAD_SLEEP);
			break;
		}
	}
}

void Monitor::AddressTranslatorThread()
{
	ResetEvent(m_memFileAddr.GetEventBufferReady());
	ResetEvent(m_memFileAddr.GetEventBufferProcessed());

	// Auxiliary buffer to hold the frame string
	long frameStrLen = 1;
	char* pFrameStr = new char[frameStrLen + 1]; // Make room for the '\0'	

	HANDLE hEvents[2];
	hEvents[0] = m_memFileAddr.GetEventBufferProcessed();
	hEvents[1] = m_hExitThreads;

	while (!m_bExitThreads)
	{
		while (!m_bExitThreads && m_addrQueue.size() > 0)
		{
			AcquireSRWLockExclusive(&m_lckAddrQueue);
			INT_PTR addr = m_addrQueue.front();
			m_addrQueue.pop();
			ReleaseSRWLockExclusive(&m_lckAddrQueue);

			// Copy address
			CopyMemory((PVOID)(m_memFileAddr.GetBuffer()), (PVOID)&addr, sizeof(UINT_PTR));

			// Signal that the address is ready			
			SetEvent(m_memFileAddr.GetEventBufferReady());

			// Wait for the response
			DWORD dwWaitResult = 0;
			do
			{
				dwWaitResult = WaitForMultipleObjects(2, hEvents, FALSE, TIMEOUT);
			} while (dwWaitResult != WAIT_OBJECT_0 && !m_bExitThreads);

			if (dwWaitResult == WAIT_OBJECT_0)
			{
				// Get buffer length.
				long len = 0;
				CopyMemory((PVOID)&len, (PVOID)(m_memFileAddr.GetBuffer()), sizeof(long));

				if (len > 1)
				{
					// The string is stored in the buffer.
					char* pBuffer = (char*)(m_memFileAddr.GetBuffer() + sizeof(long));

					if (len > frameStrLen) // Make the auxiliar buffer bigger.
					{
						delete[] pFrameStr;
						frameStrLen = len;
						pFrameStr = new char[frameStrLen + 1];
					}

					CopyMemory((PVOID)pFrameStr, pBuffer, len);
					pFrameStr[len] = 0;

					// Update string in map
					AcquireSRWLockExclusive(&m_lckAddrMap);
					CString* pAddressStr = m_addrMap[addr];
					ReleaseSRWLockExclusive(&m_lckAddrMap);

					if (*pFrameStr == '0')
					{
						// The address could not be translated on the target process.
						// Try to replace it with the name of the module that contains that adress.
						if (!m_modules.FindModuleRetry(addr, *pAddressStr))
							pAddressStr->SetString(pFrameStr);
					}
					else
						pAddressStr->SetString(pFrameStr);
				}
				else
					Trace("Monitor::AddressTranslatorThread-> ERROR: bad string");
			}
		}
		WaitForSingleObject(m_hExitThreads, THREAD_SLEEP);
	}
	delete[] pFrameStr;
}

void Monitor::MessageListenerThread()
{
	USES_CONVERSION;
	DWORD waitResult = 0;
	HANDLE hEvents[2];
	hEvents[0] = m_memFileMsg.GetEventBufferReady();
	hEvents[1] = m_hExitThreads;
	ResetEvent(m_memFileMsg.GetEventBufferReady());
	char* msg = nullptr;

	while (!m_bExitThreads)
	{
		waitResult = WaitForMultipleObjects(2, hEvents, FALSE, TIMEOUT);

		switch (waitResult)
		{
		case WAIT_OBJECT_0:
		{
			msg = (char*)m_memFileMsg.GetBuffer();
			if (strstr(msg, "Critical Error:"))
				m_bHasError = true;
			AddTextMessage(msg);
			SetEvent(m_memFileMsg.GetEventBufferProcessed());
		}
		}
	}
}

void StopThread(std::thread& thread, const CString& threadName, std::function<void(void)> callback = nullptr)
{
	CString message = "StopThread " + threadName + "\n";
	DWORD reason = WAIT_FAILED;
	auto handle = thread.native_handle();
	if (handle != INVALID_HANDLE_VALUE)
	{
		while ((reason = WaitForSingleObject(handle, 1000)) == WAIT_TIMEOUT)
		{
			Trace(message.GetString());
			if (callback)
				callback();
		}
	}
	if (thread.joinable())
		thread.join();
}

void Monitor::TargetMonitorThread()
{
	bool bProcessFinished = FALSE;

	while (!m_bExitThreads && m_targetProcInfo.dwProcessId == 0)
		Sleep(100); // Wait for the process to be created.	

	while (!m_bExitThreads && !bProcessFinished)
		bProcessFinished = (WaitForSingleObject(m_targetProcInfo.hProcess, THREAD_SLEEP) == WAIT_OBJECT_0);

	m_pDialog->PostMessage(WM_TARGET_FINISHED, 0, 0); // Notify that the target process finished.	
}

bool Monitor::Launch(CString& exe, const CString& arguments)
{
	if (!PathFileExists(exe))
		return ShowError("Target executable not found:\n%s", exe);

	m_bHasError = false;

	constexpr bool bIsApp64 = IsApp64Bit();

	// Select correct dll name depending on whether x64 or win32 version launched.
	char* heapyInjectDllName = bIsApp64 ? "HeapyInject_x64.dll" : "HeapyInject_Win32.dll";

	// Assume that the injection payload dll is in the same directory.
	CString dllPath = StringFmt("%s\\%s", GetCurrentProcessPath(), heapyInjectDllName);
	if (!PathFileExists(dllPath))
		return ShowError("Hook not found:\n%s", dllPath);

	CString commandLine = StringFmt("%s %s", exe, arguments.IsEmpty() ? "" : arguments);

	DWORD flags = CREATE_SUSPENDED;
	STARTUPINFO si;
	GetStartupInfo(&si);

	if (CreateProcess(nullptr, commandLine.GetBuffer(), nullptr, nullptr, 0, flags, nullptr, (LPSTR)".", &si, &m_targetProcInfo) == 0)
		return ShowError("Cannot create target process:\n\n'%s'\n\nReason: %s", commandLine, GetWinErrorDescription(GetLastError()));

	Trace("Monitor::Launch-> CreateProcess:  ProcessId = %d - hProcess = %d", m_targetProcInfo.dwProcessId, m_targetProcInfo.hProcess);

	// Check that the target process architecture is 64 bits.
	if (IsTargetProcess32Bit(m_targetProcInfo.hProcess))
		return ShowError("The target must be a 64 bit process");

	// Inject our dll.
	// This method returns only when injection thread returns.
	try
	{
		Trace("Monitor::Launch-> before LoadLibraryInjection");
		if (!LoadLibraryInjection(m_targetProcInfo.hProcess, dllPath.GetString()))
			return ShowError("LoadLibrary failed.");
	}
	catch (const std::exception& e)
	{
		TerminateProcess(m_targetProcInfo.hProcess, 0);
		return ShowError("Error while injecting process:\n%s", e.what());
	}

	// Once the injection thread has returned it is safe to resume the main thread.
	ResumeThread(m_targetProcInfo.hThread);

	m_modules.SetProcessId(m_targetProcInfo.dwProcessId);
	return true;
}

void Monitor::GetAllocData(size_t& totalAllocs, size_t& totalBytes)
{
	totalAllocs = m_totalAllocs;
	totalBytes = m_totalBytes;
}

void Monitor::GetData(size_t& totalAllocs, size_t& totalFrees, size_t& totalBytes, size_t& allocQueue, size_t& allocMap, size_t& addrQueue, size_t& addrMap, size_t& callstackMap)
{
	totalAllocs = m_totalAllocs;
	totalFrees = m_totalFrees;
	allocQueue = m_bufferQueue.size();
	allocMap = m_activeAllocationsMap.size();
	addrQueue = m_addrQueue.size();
	addrMap = m_addrMap.size();
	callstackMap = m_callStackMap.size();
	totalBytes = m_totalBytes;
}

bool Monitor::Start(const CString& exe, const CString& arguments)
{
	m_targetExe = exe;
	m_bIsStarted = false;

	if (!CreateMemFiles())
		return false;

	ClearAllocs();

	m_bExitThreads = false;
	ResetEvent(m_hExitThreads);
	StartThreads();

	if (!Launch(m_targetExe, arguments))
		return false;

	m_bIsStarted = true;
	return true;
}

void Monitor::Stop()
{
	m_pDialog->SendMessage(WM_BEGIN_PROGRESS, 0, 6);

	NotifyThreadExit();
	StopThreads();

	m_pDialog->SendMessage(WM_STEP_PROGRESS, 0, (LPARAM)new CString("Closing memory files"));
	CloseMemFiles(); // Close memfiles after the threads are closed

	if constexpr (g_bEnablePerfLog)
		m_perfLogger.Write(StringFmt("perf_%s.csv", GetTimestamp()));

	m_pDialog->PostMessage(WM_END_PROGRESS, 0, 0);

	m_bIsStarted = false;
}

void Monitor::NotifyThreadExit()
{
	m_bExitThreads = true;
	SetEvent(m_hExitThreads);
}

void Monitor::StartThreads()
{
	m_threadAllocReader.Start(&Monitor::AllocReaderThread, this, "Allocation Reader");
	m_threadBufferReader.Start(&Monitor::BufferReaderThread, this, "Buffer Reader");
	m_threadAddressTranslator.Start(&Monitor::AddressTranslatorThread, this, "Address Translator");
	m_threadMessageListener.Start(&Monitor::MessageListenerThread, this, "Message Listener");
	m_threadTargetMonitor.Start(&Monitor::TargetMonitorThread, this, "Target Monitor");
}

void Monitor::StopThread(Thread<Monitor>& thread, Thread<Monitor>::Callback callback)
{
	m_pDialog->SendMessage(WM_STEP_PROGRESS, 0, (LPARAM)new CString(StringFmt("Stopping %s", thread.GetName().GetString())));
	thread.Stop(callback);
}

void Monitor::StopThreads()
{
	StopThread(m_threadAllocReader);

	if (m_bufferQueue.size() != 0)
		::MessageBox(NULL, "Buffer is not empty", "Warning", MB_OK | MB_ICONWARNING);

	StopThread(m_threadBufferReader);
	StopThread(m_threadAddressTranslator);
	StopThread(m_threadMessageListener);
	StopThread(m_threadTargetMonitor);
}

bool Monitor::CreateMemFiles()
{
	if (!m_memFileAlloc.CreateMemFile(ALLOC_BUF_SIZE, MEMFILE_ALLOC))
	{
		::MessageBox(NULL, m_memFileAlloc.GetErrorDescription().GetString(), "Cannot create Alloc memory file", MB_OK | MB_ICONERROR);
		return false;
	}
	if (!m_memFileAddr.CreateMemFile(DEF_MEMFILE_SIZE, MEMFILE_ADDR))
	{
		::MessageBox(NULL, m_memFileAddr.GetErrorDescription().GetString(), "Cannot create Address memory file", MB_OK | MB_ICONERROR);
		return false;
	}
	if (!m_memFileMsg.CreateMemFile(DEF_MEMFILE_SIZE, MEMFILE_MSG))
	{
		::MessageBox(NULL, m_memFileAddr.GetErrorDescription().GetString(), "Cannot create Message memory file", MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}

void Monitor::CloseMemFiles()
{
	m_memFileAlloc.CloseMemFile();
	m_memFileAddr.CloseMemFile();
	m_memFileMsg.CloseMemFile();
}

void Monitor::Clear()
{
	m_activeAllocationsMap.clear();
	m_callStackMap.clear();
	m_addrMap.clear();

	m_totalAllocs = 0;
	m_totalFrees = 0;
	m_totalBytes = 0;

	ClearQueue(&m_bufferQueue);
	ClearContainer(m_graphQueue);
	ClearContainer(m_addrQueue);

	CloseMemFiles();

	ZeroMemory(&m_targetProcInfo, sizeof(PROCESS_INFORMATION));

	m_targetExe.Empty();
	m_bExitThreads = false;
	m_bClearAllocs = false;
	m_textMessages.Clear();
}



