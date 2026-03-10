#include "MemoryFile.h"


MemoryFile::~MemoryFile()
{
	CloseMemFile();
}

void MemoryFile::CloseMemFile()
{
	if (m_pBuffer != nullptr)
		UnmapViewOfFile(m_pBuffer);
	if (m_hMapFile != INVALID_HANDLE_VALUE)
		CloseHandle(m_hMapFile);
	if (m_hBufferReady != INVALID_HANDLE_VALUE)
		CloseHandle(m_hBufferReady);
	if (m_hBufferProcessed != INVALID_HANDLE_VALUE)
		CloseHandle(m_hBufferProcessed);

	m_pBuffer = nullptr;
	m_hMapFile = INVALID_HANDLE_VALUE;
	m_hBufferReady = INVALID_HANDLE_VALUE;
	m_hBufferProcessed = INVALID_HANDLE_VALUE;
}

void MemoryFile::BuildNames(LPCSTR baseName)
{
	// Cannot use "Global\\" because don't have SeCreateGlobalPrivilege. May not work in Remote Desktop. 
	// See https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-createfilemappinga
	sprintf_s(m_mapFileName, MAX_PATH, "%s%s%s", "Local\\", baseName, "_Map");
	sprintf_s(m_bufferReadyName, MAX_PATH, "%s%s%s", "Local\\", baseName, "_EventBufferReady");
	sprintf_s(m_bufferProcessedName, MAX_PATH, "%s%s%s", "Local\\", baseName, "_EventBufferProcessed");
}

void MemoryFile::SetErrorDescription(const CString& msg)
{
	m_errorDescription.Format("%s. Error code: %d", msg.GetString(), GetLastError());
}

bool MemoryFile::CreateMemFile(DWORD bufferSize, LPCSTR name)
{
	m_bufferSize = bufferSize;

	BuildNames(name);

	m_hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		nullptr,                 // default security
		PAGE_READWRITE,          // read/write access
		0,                       // maximum object size (high-order DWORD)
		m_bufferSize,            // maximum object size (low-order DWORD)
		m_mapFileName);          // name of mapping object

	if (m_hMapFile == nullptr)
	{
		SetErrorDescription("Could not create file mapping object");
		return false;
	}

	m_pBuffer = (BYTE*)MapViewOfFile(m_hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		m_bufferSize);

	if (m_pBuffer == nullptr)
	{
		SetErrorDescription("Could not map view of file");
		CloseHandle(m_hMapFile);
		return false;
	}

	m_hBufferReady = CreateEvent(nullptr, false, false, m_bufferReadyName);
	if (m_hBufferReady == nullptr)
	{
		SetErrorDescription("Cannot create Buffer Ready event");
		return false;
	}

	m_hBufferProcessed = CreateEvent(nullptr, false, false, m_bufferProcessedName);
	if (m_hBufferProcessed == nullptr)
	{
		SetErrorDescription("Cannot create Buffer Processed event");
		return false;
	}
	return true;
}

bool MemoryFile::OpenMemFile(DWORD bufferSize, LPCSTR name)
{	
	m_bufferSize = bufferSize;

	BuildNames(name);

	m_hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,   // read/write access
		false,                 // do not inherit the name
		m_mapFileName);        // name of mapping object

	if (m_hMapFile == nullptr)
	{
		SetErrorDescription("Could not open file mapping object");
		return false;
	}

	m_pBuffer = (BYTE*)MapViewOfFile(m_hMapFile, // handle to map object
		FILE_MAP_ALL_ACCESS,  // read/write permission
		0,
		0,
		m_bufferSize);

	if (m_pBuffer == nullptr)
	{
		SetErrorDescription("Could not map view of file");
		CloseHandle(m_hMapFile);
		return false;
	}

	m_hBufferReady = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, false, m_bufferReadyName);
	if (m_hBufferReady == nullptr)
	{
		SetErrorDescription("Cannot open Buffer Ready event");
		CloseHandle(m_hMapFile);
		return false;
	}

	m_hBufferProcessed = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, false, m_bufferProcessedName);
	if (m_hBufferProcessed == nullptr)
	{
		SetErrorDescription("Cannot open Buffer Processed event");
		CloseHandle(m_hBufferReady);
		CloseHandle(m_hMapFile);
		return false;
	}
	return true;
}

void MemoryFile::WriteBufferWithCounter(void* pBuffer, size_t bufferLen) // Especial para enviar alloc/free
{
	CopyMemory(m_pBuffer, &m_bufferCounter, sizeof(DWORD));
	DWORD offset = sizeof(DWORD);
	CopyMemory(m_pBuffer + offset, &bufferLen, sizeof(size_t));
	offset += sizeof(long);
	CopyMemory(m_pBuffer + offset, pBuffer, bufferLen);
	m_bufferCounter++;
}

void MemoryFile::SignalBufferReady()
{
	SetEvent(m_hBufferReady);
}
