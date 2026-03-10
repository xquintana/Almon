#pragma once
#include "CommonDefs.h"
#include <type_traits>

bool ShowError(LPCSTR fmt, ...);
void ShowInfo(LPCSTR fmt, ...);
CString StringFmt(LPCSTR sFmt, ...);

static constexpr bool IsApp64Bit() noexcept
{
#if defined(_WIN64)
	return true;
#else
	return false;
#endif    
}

bool IsTargetProcess32Bit(HANDLE hProcess);
void ProcessWinMessages();

CString GetWinErrorDescription(DWORD errorCode);
CString GetExceptionMessage(CException* pEx);

// Returns a string with the current local time.
CString GetTimestamp();

// Avoids the 'GetTickCount()' warning
DWORD GetTicks();

void GetUnits(size_t numBytes, CString& units);

// Used to clear containers that do not provide a method to do it, such as std::queue.
// The container must be swappable.
template<typename T>
void ClearContainer(T& container)
{
	T emptyContainer;
	std::swap(container, emptyContainer);
}

void ClearQueue(BufferQueue* pQueue, bool bDelete = false);

StringArray Split(const CString& str, char sep = ';');

CString GetCurDirectory();
CString GetCurrentProcessPath();
CString GetFileName(const CString& fullPath);
CString GetFileExtension(const CString& fullPath);
bool EnsureDirectoryExists(const char* dir);

// Returns true if a call stack contains one or more frames
bool ContainsFrame(const CallStackInfo& stack, std::vector<CString>& filter);

// Returns a string showing the input numeric value.
template<typename T>
CString NumberToText(T value) 
{
	static_assert(std::is_arithmetic_v<T>, "NumberToText requires a numeric type.");

	CString result;

	if constexpr (std::is_integral_v<T>) {
		if constexpr (std::is_unsigned_v<T>)
			result.Format(_T("%llu"), static_cast<unsigned long long>(value));		
		else
			result.Format(_T("%lld"), static_cast<long long>(value));		
	}
	else if constexpr (std::is_floating_point_v<T>)
		result.Format(_T("%f"), static_cast<double>(value));	
	return result;
}


// Encapsulates a thread that can be waited on a timeout.
// This thread can only launch methods that do not receive any arguments and return void.
template<typename T>
class Thread
{
public:
	using ThreadStub = void(T::*)();
	using Callback = std::function<void(void)>;

	Thread() : m_timeout{ m_defaultTimeout } {};
	~Thread() { Stop(); }

	void Start(ThreadStub stub, T* pObj, CString threadName = "")
	{
		Trace("Thread::Start-> thread: %s", threadName.GetString());
		m_thread = std::thread(stub, pObj);
		m_threadName = threadName;
	}

	bool IsStarted() { return m_thread.joinable(); }

	void SetTimeout(DWORD timeout) { m_timeout = timeout; }

	void Stop()
	{		
		if (m_thread.joinable())
			m_thread.join();
		Trace("Thread::Stop-> thread: %s", m_threadName.GetString());
		m_thread = std::thread();		
	}

	// Responsive stop.
	DWORD Stop(Callback callback)
	{
		DWORD reason{ WAIT_FAILED };
		if (m_thread.joinable())
		{
			HANDLE hThread{ GetHandle() };
			if (hThread != INVALID_HANDLE_VALUE)
			{
				while ((reason = WaitForSingleObject(hThread, m_timeout)) == WAIT_TIMEOUT) { callback ? callback() : ProcessWinMessages(); };
				if (m_thread.joinable()) m_thread.join();
			}
			m_thread = std::thread();
		}
		return reason;
	}	

	HANDLE GetHandle() { return static_cast<HANDLE>(m_thread.native_handle()); }
	DWORD  GetThreadId() { GetThreadId(GetHandle()); }
	const CString& GetName() { return m_threadName; }

protected:
	std::thread m_thread;
	CString m_threadName;
	const DWORD m_defaultTimeout{ 1000 };
	DWORD m_timeout{};
};


class Logger
{
public:
	Logger() { m_file = nullptr; };	
	~Logger();

	bool Open(const CString& fileFullath);
	void Close();
	void Log(const char* pwsText, ...);	

protected:
	CString GetLogDirectory();

protected:		
	FILE* m_file{};
};


class PerfLogger : public Logger
{
public:
	void AddBufferSize(size_t bufferSize);
	void Write(const CString& logName);

protected:
	std::vector<std::pair<DWORD, size_t>> m_sizeList;
	DWORD m_lastTime{ 0 }, m_startTime{ 0 };
	bool m_bFirstTime{ true };	
};


class HighResCounter
{
public:
	HighResCounter();
	void  StartCounter();
	DWORD GetElapsedMicrosec();
	DWORD GetElapsedMillisec();
protected:

	LARGE_INTEGER m_startingTime{}, m_endingTime{}, m_elapsed{};
	LARGE_INTEGER m_frequency{};
};
