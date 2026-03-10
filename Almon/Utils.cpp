#include "stdafx.h"
#include "Utils.h"

#define MAX_TEXT_LEN 2048



bool ShowError(LPCSTR fmt, ...)
{
	va_list args;
	char buffer[MAX_TEXT_LEN];
	va_start(args, fmt);
	vsprintf_s(buffer, MAX_TEXT_LEN, fmt, args);
	va_end(args);
	OutputDebugString(buffer);
	::MessageBox(nullptr, buffer, "Error", MB_OK | MB_ICONERROR);
	return false;
}

void ShowInfo(LPCSTR fmt, ...)
{
	va_list args;
	char buffer[MAX_TEXT_LEN];
	va_start(args, fmt);
	vsprintf_s(buffer, MAX_TEXT_LEN, fmt, args);
	va_end(args);
	OutputDebugString(buffer);
	::MessageBox(nullptr, buffer, "Error", MB_OK | MB_ICONINFORMATION);
}

CString StringFmt(LPCSTR fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = _vsctprintf(fmt, args) + 1; // Add '\0'
	TCHAR* pBuffer = new TCHAR[len];
	if (pBuffer)
		_vstprintf_s(pBuffer, len, fmt, args);
	va_end(args);
	if (pBuffer)
	{
		CString sOutput(pBuffer);
		delete[] pBuffer;
		return sOutput;
	}
	return "";
}

bool IsTargetProcess32Bit(HANDLE hProcess)
{
	typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS2)(HANDLE, USHORT*, USHORT*);

	HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
	if (hKernel32)
	{
		LPFN_ISWOW64PROCESS2 fnIsWow64Process2 = (LPFN_ISWOW64PROCESS2)GetProcAddress(hKernel32, "IsWow64Process2");
		if (fnIsWow64Process2)
		{
			USHORT pm = 0, nm = 0;
			if (fnIsWow64Process2(hProcess, &pm, &nm))
				return pm != IMAGE_FILE_MACHINE_UNKNOWN;  // If processMachine is IMAGE_FILE_MACHINE_UNKNOWN, then it's a native process.
		}
		else // Fall back to IsWow64Process for older OS
		{
			BOOL bIsWow64 = FALSE;
			if (IsWow64Process(hProcess, &bIsWow64))
				return bIsWow64;
		}
	}
	return false; // If all checks fail, assume process is native bitness (probably 32-bit on 32-bit OS)
}

void ProcessWinMessages()
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

CString GetWinErrorDescription(DWORD errorCode)
{
	CString description;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	description = (LPCTSTR)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return description;
}

CString GetExceptionMessage(CException* pEx)
{
	constexpr int nMaxSize = 2048;
	char sMessage[nMaxSize];
	ZeroMemory(sMessage, nMaxSize);
	if (pEx) pEx->GetErrorMessage(sMessage, nMaxSize);
	return CString(sMessage);
}

CString GetTimestamp()
{
	SYSTEMTIME t;
	GetLocalTime(&t);	
	return StringFmt("%02d%02d%02d-%02d%02d%02d", t.wYear - 2000, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
}

DWORD GetTicks() { return (DWORD)GetTickCount64(); }

void GetUnits(size_t numBytes, CString& units)
{
	if (numBytes >= 1073741824)
		units.Format("%.01f %s", (double)numBytes / 1073741824.0, "GB");
	else if (numBytes >= 1048576)
		units.Format("%.01f %s", (double)numBytes / 1048576.0, "MB");
	else if (numBytes >= 1024)
		units.Format("%.01f %s", (double)numBytes / 1024.0, "KB");
	else
		units.Format("%zd %s", numBytes, "Bytes");
}

void ClearQueue(BufferQueue* pQueue, bool bDelete)
{
	while (pQueue->size() > 0)
	{
		BufferInfo bufferInfo = pQueue->front();
		pQueue->pop();
		delete[] bufferInfo.pBuffer;
	}
	if (bDelete)
		delete pQueue;
}

StringArray Split(const CString& str, char sep)
{
	StringArray out;
	if (!str.IsEmpty())
	{
		int end{}, begin{};
		while (end >= 0)
		{
			if (end = str.Find(sep, begin); end >= begin)
			{
				if (end > begin)
					out.push_back(str.Mid(begin, end - begin));
				begin = end + 1;
			}
		}
		if (begin < str.GetLength())
			out.push_back(str.Mid(begin));
	}
	return out;
}

CString GetCurDirectory()
{
	char dir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, dir);
	return CString(dir) + CString("\\");
}

CString GetCurrentProcessPath()
{
	char exePath[MAX_PATH], drive[_MAX_DRIVE], path[_MAX_PATH];

	GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	CString sPath;
	if (_splitpath_s(exePath, drive, _MAX_DRIVE, path, _MAX_PATH, nullptr, 0, nullptr, 0) == 0)
		sPath.Format("%s%s", drive, path);
	return sPath;
}

CString GetFileName(const CString& fullPath) 
{
	int pos = fullPath.ReverseFind(_T('\\'));
	if (pos != -1)
		return fullPath.Right(fullPath.GetLength() - pos - 1);	
	return fullPath;
}

CString GetFileExtension(const CString& fullPath) 
{
	int pos = fullPath.ReverseFind(_T('.'));

	// Ensure the dot is after the last backslash to avoid finding dots in folder names
	int lastSlash = fullPath.ReverseFind(_T('\\'));

	if (pos != -1 && pos > lastSlash) 
		return fullPath.Mid(pos);	
	return ""; // No extension found
}

bool EnsureDirectoryExists(const char* path)
{
	DWORD attrs = GetFileAttributes(path);

	// Path exists
	if (attrs != INVALID_FILE_ATTRIBUTES)
		return (attrs & FILE_ATTRIBUTE_DIRECTORY);

	return CreateDirectory(path, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool ContainsFrame(const CallStackInfo& stack, std::vector<CString>& filter)
{
	for (int i = 0; i < stack.numFrames; i++)
	{
		CString frame(*stack.frames[i]);
		frame.MakeUpper();
		for (const auto& f : filter)
			if (frame.Find(f) >= 0)
				return true;
	}
	return false;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Logger

Logger::~Logger()
{
	if (m_file != nullptr)
		Close();
}

bool Logger::Open(const CString& fileFullath)
{
	DeleteFileA(fileFullath);
	return (fopen_s(&m_file, fileFullath, "wt") == 0 && m_file != nullptr);
}

void Logger::Close()
{
	if (m_file != nullptr)
	{
		fclose(m_file);
		m_file = nullptr;
	}
}

void Logger::Log(const char* psText, ...)
{
	if (m_file != nullptr)
	{
		va_list args;
		char sBuffer[2048];
		va_start(args, psText);
		vsprintf_s(sBuffer, 2048, psText, args);
		va_end(args);
		strcat_s(sBuffer, 2048, "\n");
		fprintf(m_file, sBuffer);
	}
}

CString Logger::GetLogDirectory()
{
	TCHAR szTempPath[MAX_PATH];
	if (GetTempPath(MAX_PATH, szTempPath) == 0)
		return "";

	CString logPath(szTempPath);

	if (logPath.Right(1) != _T("\\"))
		logPath += _T("\\");

	logPath += _T("AlmonLogs\\");
	return logPath;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PerfLogger

void PerfLogger::AddBufferSize(size_t bufferSize)
{
	if (GetTicks() - m_lastTime > 1000) // update every 1 second
	{
		m_lastTime = GetTicks();

		if (m_bFirstTime)
		{
			m_startTime = m_lastTime;
			m_bFirstTime = false;
		}
		m_sizeList.emplace_back(m_lastTime - m_startTime, bufferSize);
	}
}

void PerfLogger::Write(const CString& logName)
{
	CString logDirectory = GetLogDirectory();
	if (EnsureDirectoryExists(logDirectory))
		if (Open(GetLogDirectory() + logName))
		{
			for (auto& [time, size] : m_sizeList)
				Log("%u, %u", time, size);
			Close();
		}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HighResCounter

HighResCounter::HighResCounter()
{
	if (!QueryPerformanceFrequency(&m_frequency))
	{		
		ASSERT(0);
	}
}

void HighResCounter::StartCounter()
{
	QueryPerformanceCounter(&m_startingTime);
}

DWORD HighResCounter::GetElapsedMicrosec()
{
	QueryPerformanceCounter(&m_endingTime);
	m_elapsed.QuadPart = m_endingTime.QuadPart - m_startingTime.QuadPart;
	m_elapsed.QuadPart *= 1000000; //microseconds		
	m_elapsed.QuadPart /= m_frequency.QuadPart;
	return (DWORD)m_elapsed.QuadPart;
}

DWORD HighResCounter::GetElapsedMillisec()
{
	QueryPerformanceCounter(&m_endingTime);
	m_elapsed.QuadPart = m_endingTime.QuadPart - m_startingTime.QuadPart;
	m_elapsed.QuadPart *= 1000; //milliseconds
	m_elapsed.QuadPart /= m_frequency.QuadPart;
	return (DWORD)m_elapsed.QuadPart;
}
