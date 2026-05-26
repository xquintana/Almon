#include "HeapProfiler.h"

#include <Windows.h>
#include <dbghelp.h>

#pragma comment( lib, "psapi.lib" )

#define OK							0
#define ERR_CIRCUL_BUFFER_TOO_SMALL	1
#define ERR_CLIENT_BUFFER_TOO_SMALL	2

#define FRAMES_SKIP 3 // hook + malloc + CCircularBuffer::Write


static size_t			g_numAllocs = 0;
static size_t			g_numBytes = 0;
static size_t			g_numFree = 0;
static HANDLE			g_hProcess = nullptr;
static DWORD64			g_displace = 0;
static DWORD			g_displaceLine = 0;
static char				g_symbolName[MAX_SYM_NAME] = "";
static char				g_symbolNameAux[MAX_SYM_NAME] = "";
static IMAGEHLP_MODULE	g_moduleInfo;
static IMAGEHLP_LINE	g_line;
static SYMBOL_INFO* g_symInfo = nullptr;

static constexpr bool g_bEnableAllocTraces = ENABLE_TRACE_ALLOC;


void __declspec(dllexport) GetSymbol(Address addr, char* symbol, long& symbolLen)
{
	Address symbolAddress = 0;

	g_displace = 0;
	g_displaceLine = 0;
	symbolAddress = 0;

	g_symInfo->Address = addr;

	g_symbolName[0] = 0;
	g_moduleInfo.ModuleName[0] = 0;

	g_moduleInfo.SizeOfStruct = sizeof(g_moduleInfo);

	if (SymGetModuleInfo(g_hProcess, addr, &g_moduleInfo))
	{
		if (SymFromAddr(g_hProcess, addr, &g_displace, g_symInfo) && g_symInfo->NameLen < g_symInfo->MaxNameLen)
		{
			strcpy_s(g_symbolName, MAX_SYM_NAME, g_symInfo->Name);

			if (SymGetLineFromAddr(g_hProcess, addr, &g_displaceLine, &g_line))
			{
				if (g_displaceLine > 0)
				{
					sprintf_s(g_symbolNameAux, MAX_SYM_NAME, "+0x%X", g_displaceLine);
					strcat_s(g_symbolName, MAX_SYM_NAME, g_symbolNameAux);
				}
				sprintf_s(symbol, MAX_SYM_NAME, "%s ! %s ---- %s(%d)", g_moduleInfo.ModuleName, g_symbolName, g_line.FileName, g_line.LineNumber);
			}
			else
				sprintf_s(symbol, MAX_SYM_NAME, "%s ! %s (0x%llx)", g_moduleInfo.ModuleName, g_symbolName, addr);
		}
		else
			sprintf_s(symbol, MAX_SYM_NAME, "%s ! 0x%llx", g_moduleInfo.ModuleName, addr);
	}
	else
		sprintf_s(symbol, MAX_SYM_NAME, "0x%llx", addr); // Symbol not found. Return the hexadecimal value of the address.

	symbolLen = (long)strlen(symbol);
}


#pragma region HeapProfiler
////////////////////////////////////////////////////////////////////////////////////////////////////////
// HeapProfiler

DWORD WINAPI BufferWriterThreadStub(LPVOID lpParam)
{
	HeapProfiler* pThis = (HeapProfiler*)lpParam;
	pThis->BufferWriterThread();
	return 0;
}

DWORD WINAPI AddressTranslatorThreadStub(LPVOID lpParam)
{
	HeapProfiler* pThis = (HeapProfiler*)lpParam;
	pThis->AddressTranslatorThread();
	return 0;
}

HeapProfiler::HeapProfiler()
{
	TraceRel("HeapProfiler::HeapProfiler-> enter");

	g_hProcess = GetCurrentProcess();

	// Init dbghelp framework.
	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
	if (!SymInitialize(g_hProcess, NULL, true))
		TraceRel("HeapProfiler::HeapProfiler-> SymInitialize failed");

	m_hExitThreads = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	memset(&g_line, 0, sizeof(g_line));
	g_line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

	g_symInfo = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + MAX_SYM_NAME, 1);
	if (g_symInfo)
	{
		g_symInfo->MaxNameLen = MAX_SYM_NAME;
		g_symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
	}

	memset(&g_moduleInfo, 0, sizeof(IMAGEHLP_MODULE));
	g_moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
}

HeapProfiler::~HeapProfiler()
{
	TraceRel("HeapProfiler::~HeapProfiler-> enter");
	WriteBuffer();
	SymCleanup(g_hProcess);
	TraceRel("HeapProfiler::~HeapProfiler-> exit");
}

void HeapProfiler::WriteBuffer()
{
	if (m_bufferLen > 0)
	{
		m_memFileAlloc.WriteBufferWithCounter(m_buffer, m_bufferLen);

		// Notify that a new buffer is available
		m_memFileAlloc.SignalBufferReady();

		//Wait for the buffer to be read
		WaitForSingleObject(m_memFileAlloc.GetEventBufferProcessed(), TIMEOUT);
	}
	else
		TraceRel("HeapProfiler::WriteBuffer-> nBufferLen is zero!!");
}

void HeapProfiler::BufferWriterThread()
{
	m_notifier.SendTxtMessage("Heap profiler started.");
	TraceRel("HeapProfiler::BufferWriterThread-> enter");
	int nRet{ OK };

	while (!m_bExitThreads)
	{
		nRet = m_CircBuffer.Read(m_buffer, ALLOC_BUF_SIZE, m_bufferLen);
		if (nRet != OK)
			m_notifier.NotifyBufferSmall(nRet);
		else if (m_bufferLen > 0)
			WriteBuffer();
		WaitForSingleObject(m_hExitThreads, 20);
	}
	TraceRel("HeapProfiler::BufferWriterThread-> exit");
	m_notifier.SendTxtMessage("HeapProfiler::BufferWriterThread-> exit");
}

void HeapProfiler::AddressTranslatorThread()
{
	char symbol[MAX_SYM_NAME];
	TraceRel("HeapProfiler::AddressTranslatorThread-> enter");

	HANDLE hEvents[2];
	hEvents[0] = m_memFileAddr.GetEventBufferReady();
	hEvents[1] = m_hExitThreads;

	m_skipThreadId = GetCurrentThreadId();

	while (!m_bExitThreads)
	{
		DWORD waitResult = WaitForMultipleObjects(2, hEvents, FALSE, TIMEOUT);
		if (waitResult == WAIT_OBJECT_0)
		{
			// Get the address
			Address address{};
			CopyMemory((PVOID)&address, (PVOID)(m_memFileAddr.GetBuffer()), sizeof(Address));

			long stringLen{};
			m_bSkipAllocs = true; // Do not process the allocs generated by GetSymbol.
			GetSymbol(address, symbol, stringLen);
			CopyMemory((PVOID)(m_memFileAddr.GetBuffer()), (PVOID)&stringLen, sizeof(long));
			CopyMemory((PVOID)(m_memFileAddr.GetBuffer() + sizeof(long)), (PVOID)symbol, stringLen * sizeof(CHAR));
			m_bSkipAllocs = false;

			SetEvent(m_memFileAddr.GetEventBufferProcessed());
		}
		else if (waitResult == (WAIT_OBJECT_0 + 1))
			break;
	}
	Trace("HeapProfiler::AddressTranslatorThread-> exit");
	m_notifier.SendTxtMessage("HeapProfiler::AddressTranslatorThread-> exit");
}

bool HeapProfiler::Init()
{
	TraceRel("HeapProfiler::Init-> enter");
	if (!m_notifier.Init())
		return false;

	m_CircBuffer.SetNotifier(&m_notifier);
	if (!m_memFileAlloc.OpenMemFile(ALLOC_BUF_SIZE, MEMFILE_ALLOC))
	{
		TraceRel("Failed to open memory file: %s", m_memFileAlloc.GetErrorDescription().GetString());
		return false;
	}
	if (!m_memFileAddr.OpenMemFile(DEF_MEMFILE_SIZE, MEMFILE_ADDR))
	{
		TraceRel("Failed to open memory file (ip): %s", m_memFileAddr.GetErrorDescription().GetString());
		return false;
	}

	m_hBufferWriterThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&BufferWriterThreadStub, this, 0, nullptr);
	m_hAddrDecoderThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&AddressTranslatorThreadStub, this, 0, nullptr);

	TraceRel("HeapProfiler::Init-> Succeded, Allocation buffer size: %d bytes", ALLOC_BUF_SIZE);
	return true;
}

void HeapProfiler::Destroy()
{
	TraceRel("HeapProfiler::Destroy-> enter");

	m_bExitThreads = true;
	SetEvent(m_hExitThreads);

	WaitForSingleObject(m_hBufferWriterThread, INFINITE);
	WaitForSingleObject(m_hAddrDecoderThread, INFINITE);
	CloseHandle(m_hBufferWriterThread);
	CloseHandle(m_hAddrDecoderThread);
	CloseHandle(m_hExitThreads);

	char sText[DEF_STR_SIZE];
	sprintf_s(sText, DEF_STR_SIZE, "Sent %zd allocs (%zd bytes) and %zd frees", g_numAllocs, g_numBytes, g_numFree);
	m_notifier.SendTxtMessage(sText);
	TraceRel(sText);

	m_memFileAlloc.CloseMemFile();
	m_memFileAddr.CloseMemFile();

	m_notifier.Destroy();

	WriteBuffer();

	TraceRel("HeapProfiler::Destroy-> exit");
}

void HeapProfiler::malloc(void* ptr, size_t size)
{
	// Do not put traces in this function (traces may alloc resources)

	if ((m_bSkipAllocs && GetCurrentThreadId() == m_skipThreadId) || m_bExitThreads)
		return;
	g_numAllocs++;
	g_numBytes += size;
	m_CircBuffer.WriteAlloc(ptr, size);
}

void HeapProfiler::free(void* ptr)
{
	if ((m_bSkipAllocs && GetCurrentThreadId() == m_skipThreadId) || m_bExitThreads || !ptr)
		return;
	g_numFree++;
	m_CircBuffer.WriteFree(ptr);
}
#pragma endregion HeapProfiler

#pragma region CircularBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////
// CircularBuffer

void CircularBuffer::WriteAlloc(void* ptr, size_t size) noexcept
{
	Alloc alloc;
	alloc.numBytes = size;
	alloc.addr = (Address)ptr;
	alloc.numFrames = CaptureStackBackTrace(FRAMES_SKIP, BACKTRACE_SIZE, (PVOID*)(alloc.framesAddr), 0);

	AcquireSRWLockExclusive(&m_lock);
	if (m_pWrite + sizeof(Alloc) > m_pBufferLast) // Overflow
	{
		m_pLapLast = m_pWrite; // Remember the last write position for this lap
		m_pWrite = m_pBufferFirst;
		m_writeLap++;
	}

	CopyMemory(m_pWrite, &alloc, sizeof(Alloc));
	m_pWrite += sizeof(Alloc);
	ReleaseSRWLockExclusive(&m_lock);
}

void CircularBuffer::WriteFree(void* ptr) noexcept
{
	Address addrFree = (Address)ptr | FREE_MASK;

	AcquireSRWLockExclusive(&m_lock);
	if (m_pWrite + sizeof(Address) > m_pBufferLast) // Overflow
	{
		m_pLapLast = m_pWrite; // Remember the last write position for this lap
		m_pWrite = m_pBufferFirst;
		m_writeLap++;
	}
	CopyMemory(m_pWrite, &addrFree, sizeof(Address));
	m_pWrite += sizeof(Address);
	ReleaseSRWLockExclusive(&m_lock);
}

int CircularBuffer::Read(BYTE* pBuffer, size_t maxLen, size_t& bufferLen) noexcept
{
	bufferLen = 0;

	AcquireSRWLockExclusive(&m_lock);
	if (m_pWrite > m_pRead)
	{
		if (m_writeLap != m_readLap)
		{
			TraceRel("CircularBuffer::Read-> The circular buffer is too small (1). WriteLap = %d - ReadLap = %d", m_writeLap, m_readLap);
			ReleaseSRWLockExclusive(&m_lock);
			return ERR_CIRCUL_BUFFER_TOO_SMALL;
		}

		size_t numBytes = m_pWrite - m_pRead;
		if (numBytes > maxLen)
		{
			TraceRel("CircularBuffer::Read-> The client buffer is too small (1).");
			ReleaseSRWLockExclusive(&m_lock);
			return ERR_CLIENT_BUFFER_TOO_SMALL;
		}

		CopyMemory(pBuffer, m_pRead, numBytes);
		m_pRead = m_pWrite;
		m_readLap = m_writeLap;
		bufferLen = numBytes;
	}
	else if (m_pWrite < m_pRead)
	{
		if (m_readLap != (m_writeLap - 1))
		{
			TraceRel("CircularBuffer::Read-> The circular buffer is too small (1). WriteLap = %d - ReadLap = %d", m_writeLap, m_readLap);
			ReleaseSRWLockExclusive(&m_lock);
			return ERR_CIRCUL_BUFFER_TOO_SMALL;
		}

		// Copy up to the end of the buffer.
		size_t numBytes = m_pLapLast - m_pRead;
		if (numBytes > maxLen)
		{
			TraceRel("CircularBuffer::Read-> The client buffer is too small (2).");
			ReleaseSRWLockExclusive(&m_lock);
			return ERR_CLIENT_BUFFER_TOO_SMALL;
		}
		CopyMemory(pBuffer, m_pRead, numBytes);
		bufferLen = numBytes;

		// Copy the beginning of buffer.
		numBytes = m_pWrite - m_pBufferFirst;
		if (numBytes > (maxLen - bufferLen))
		{
			TraceRel("CircularBuffer::Read-> The buffer is too small (5).");
			ReleaseSRWLockExclusive(&m_lock);
			return ERR_CLIENT_BUFFER_TOO_SMALL;
		}
		CopyMemory(pBuffer + bufferLen, m_pBufferFirst, numBytes);
		bufferLen += numBytes;

		m_pRead = m_pWrite;
		m_readLap = m_writeLap;
	}
	ReleaseSRWLockExclusive(&m_lock);
	return OK;
}
#pragma endregion CircularBuffer

#pragma region Notifier
////////////////////////////////////////////////////////////////////////////////////////////////////////
// Notifier

bool Notifier::Init()
{
	InitializeSRWLock(&m_lock);
	if (!m_memFileMsg.OpenMemFile(DEF_MEMFILE_SIZE, MEMFILE_MSG))
	{
		TraceRel("Failed to open memory file (messages): %s", m_memFileMsg.GetErrorDescription().GetString());
		return false;
	}
	return true;
}

void Notifier::Destroy()
{
	m_memFileMsg.CloseMemFile();
}

void Notifier::SendTxtMessage(char* text)
{
	if (text == nullptr)
		return;

	size_t len = strlen(text);
	if (len > 0)
	{
		AcquireSRWLockExclusive(&m_lock);
		CopyMemory(m_memFileMsg.GetBuffer(), text, sizeof(char) * (len + 1)); //include '\0'

		// Notify that a new buffer is available
		m_memFileMsg.SignalBufferReady();

		//Wait for the buffer to be read
		WaitForSingleObject(m_memFileMsg.GetEventBufferProcessed(), TIMEOUT);
		ReleaseSRWLockExclusive(&m_lock);
	}
}

void Notifier::NotifyBufferSmall(int nErrorCode)
{
	if (!m_bNotifiedBufferSmall)
	{
		switch (nErrorCode)
		{
		case ERR_CIRCUL_BUFFER_TOO_SMALL: SendTxtMessage("Critical Error: The circular buffer is too small."); break;
		case ERR_CLIENT_BUFFER_TOO_SMALL: SendTxtMessage("Critical Error: The client buffer is too small."); break;
		}
		m_bNotifiedBufferSmall = true;
	}
}
#pragma endregion Notifier
