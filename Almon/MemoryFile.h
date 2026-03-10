#pragma once

#include "CommonDefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MemoryFile
// Encapsulates a shared memory space used for data transfer between the monitor and the target application.

class MemoryFile
{
public:
	~MemoryFile();

	bool CreateMemFile(DWORD bufferSize, LPCSTR name);
	bool OpenMemFile(DWORD bufferSize, LPCSTR name);
	void CloseMemFile();

	HANDLE GetMapFile() { return m_hMapFile; }
	BYTE* GetBuffer() { return m_pBuffer; }
	DWORD  GetBufferSize() { return m_bufferSize; }
	HANDLE GetEventBufferReady() { return m_hBufferReady; }
	HANDLE GetEventBufferProcessed() { return m_hBufferProcessed; }

	void WriteBufferWithCounter(void* pBuffer, size_t bufferLen); // Used to send the alloc/free data.
	void SignalBufferReady();

	CString GetErrorDescription() { return m_errorDescription; }

private:
	void BuildNames(LPCSTR baseName);
	void SetErrorDescription(const CString& msg);

private:
	HANDLE m_hMapFile{ INVALID_HANDLE_VALUE };
	BYTE* m_pBuffer{ nullptr }; // Access to this buffer is synchronized by event, so no mutex is required.
	HANDLE m_hBufferReady{ INVALID_HANDLE_VALUE };
	HANDLE m_hBufferProcessed{ INVALID_HANDLE_VALUE };

	DWORD m_bufferCounter{};
	DWORD m_bufferSize{};
	char m_mapFileName[MAX_PATH];
	char m_bufferReadyName[MAX_PATH];
	char m_bufferProcessedName[MAX_PATH];
	char m_muxName[MAX_PATH];

	CString m_errorDescription;
};

