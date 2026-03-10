#pragma once

#include "CommonDefs.h"
#include "Utils.h"
#include "MemoryFile.h"
#include "Canvas.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ModuleRanges
// Used to retrieve the name of the module corresponding to a memory address.

class ModuleRanges
{
	struct MODULEINFOEX
	{
		MODULEINFOEX(DWORD64 startAddress, DWORD size, TCHAR* pName)
		{
			this->startAddress = startAddress;
			this->endAddress = startAddress + size;
			this->sName = pName;
		}
		DWORD64 startAddress;
		DWORD64 endAddress;
		CString sName;
	};

public:	
	void SetProcessId(DWORD processId) { m_dwProcessId = processId; }
	bool FindModule(UINT_PTR address, CString& moduleName);
	bool FindModuleRetry(UINT_PTR address, CString& moduleName);
	bool UpdateModuleList(DWORD processID, std::vector<MODULEINFOEX>& modules);

protected:
	DWORD m_dwProcessId{};
	std::vector<MODULEINFOEX> m_modules;	
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TextMessages
// Stores the messages received from the remote Heap profiler.

class TextMessages
{
public:
	TextMessages();
	void AddTextMessage(const CString& message);
	bool GetTextMessages(CString& messages, DWORD& numMessages); // Returns true if there are new messages.
	void Clear();

protected:
	static const int m_maxSize{ 100 }; // Maximum number of text messages to keep.
	SRWLOCK m_lckTextMessages;
	CString m_textMessages[m_maxSize];
	DWORD m_idxWrite;
	DWORD m_numMessages; // Total number of text messages received.
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Monitor
// Injects the DLL with the heap profiler in the target process and receives the allocation information from the profiler.

class Monitor
{
public:
	Monitor();
	~Monitor();

	void SetParent(CWnd* pParent) { m_pDialog = pParent; }
	bool Start(const CString& exe, const CString& arguments);
	void Stop();	
	bool IsStarted() { return m_bIsStarted; }
	bool CreateMemFiles();
	void CloseMemFiles();	
	void ClearAllocs();

	void GetAllocData(size_t& totalAllocs, size_t& totalBytes);
	void GetData(size_t& totalAllocs, size_t& totalFrees, size_t& totalBytes, size_t& allocQueue, size_t& allocMap, size_t& addrQueue, size_t& addrMap, size_t& callstackMap);
	
	std::pair<GraphQueue&, SRWLOCK&> GetGraphQueue() { return { m_graphQueue, m_lckGraphQueue }; }
	CallStackMap* GetCallStackMap() { return &m_callStackMap; }	
	size_t GetBufferSize() { return  m_bufferQueue.size(); }
	bool GetTextMessages(CString& sMessages, DWORD& numMessages) { return m_textMessages.GetTextMessages(sMessages, numMessages); }

	bool HasError() { return m_bHasError; } // Returns true if the monitor has detected an error.

	void Clear();
	void NotifyThreadExit(); // Notifies the threads to finish.

protected:
	// Inject a DLL into the target process by creating a new thread at LoadLibrary
	// Waits for injected thread to finish and returns its exit code.
	// 
	// Originally from :
	// http://www.codeproject.com/Articles/2082/API-hooking-revealed 
	DWORD LoadLibraryInjection(HANDLE hProcess, const char *dllName);

	bool Launch(CString& exe, const CString& arguments);
	
	void ClearTranslatedAddresses();	
	void ClearAllocsInternal();

	void AddTextMessage(const CString& message) { m_textMessages.AddTextMessage(message); }

	// Threads
	void MessageListenerThread();
	void BufferReaderThread();
	void AllocReaderThread();	
	void AddressTranslatorThread();
	void TargetMonitorThread();
	void StartThreads();		
	void StopThreads(); // Waits for the threads to finish.
	void StopThread(Thread<Monitor>& thread, Thread<Monitor>::Callback callback = nullptr);	

	void UpdateCallStackMap(BufferInfo bufferInfo);	

protected:
	bool m_bIsStarted{};
	std::atomic<bool> m_bHasError{};  // Set to true when the monitor detects an error
	std::atomic<bool> m_bClearAllocs{}; // Set to true when the allocs must be cleared

	CWnd* m_pDialog{}; // The parent dialog

	// Counters
	UINT64 m_totalAllocs{};
	UINT64 m_totalFrees{};
	UINT64 m_totalBytes{};

	// Threads
	HANDLE m_hExitThreads{};
	std::atomic<bool> m_bExitThreads{};
	Thread<Monitor> m_threadBufferReader;
	Thread<Monitor> m_threadAllocReader;
	Thread<Monitor> m_threadAddressTranslator;
	Thread<Monitor> m_threadTargetMonitor;
	Thread<Monitor> m_threadMessageListener;		
	
	// Locks
	SRWLOCK m_lckBufferQueue;
	SRWLOCK m_lckGraphQueue;
	SRWLOCK m_lckAddrQueue;
	SRWLOCK m_lckAddrMap;		

	// Memory files
	MemoryFile m_memFileAlloc;   // Used to receive allocs from the target exe (1 direction)
	MemoryFile m_memFileAddr; // Used to send memory addresses (instruction pointers) and receive the corresponding symbols (bidirectional)
	MemoryFile m_memFileMsg;

	ActiveAllocationsMap m_activeAllocationsMap;
	CallStackMap m_callStackMap;
	BufferQueue m_bufferQueue;
	AddressQueue m_addrQueue; // Memory addresses (Instruction Pointers) pending to translate
	AddressMap m_addrMap; // Returns the string associated to an address (instruction pointer)
	GraphQueue m_graphQueue; // Contains the data to update the graphs

	PROCESS_INFORMATION m_targetProcInfo;
	CString m_targetExe; // Full path of the executable being monitorized

	std::vector<CString*> m_listTranslatedAddr;	

	ModuleRanges m_modules;
	TextMessages m_textMessages;
	PerfLogger m_perfLogger;
};

