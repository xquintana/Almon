#pragma once

#include <mutex>

#include "..\Almon\CommonDefs.h"
#include "..\Almon\MemoryFile.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Notifier
// Sends messages to the monitor application through a memory file.

class Notifier
{
public:
	bool Init();
	void Destroy();
	void SendTxtMessage(char* text);
	void NotifyBufferSmall(int errorCode);

protected:
	bool m_bNotifiedBufferSmall{}; // So the message 'buffer too small' is notified only once.
	MemoryFile m_memFileMsg; // Used to send text messages.
	SRWLOCK m_lock; // Messages can be sent from different threads.
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CircularBuffer
// Implements a circular buffer that stores the malloc/free operations.

struct CircularBuffer
{
	CircularBuffer()
	{
		InitializeSRWLock(&m_lock);
		ZeroMemory(m_buffer, ALLOC_BUF_SIZE);
		m_pBufferFirst = &m_buffer[0];
		m_pBufferLast = &(m_buffer[sizeof(m_buffer) - 1]);
		m_pWrite = m_pRead = m_pBufferFirst;
	}

	inline void WriteAlloc(void* ptr, size_t size) noexcept;
	inline void WriteFree(void* ptr) noexcept;
	int Read(BYTE* pBuffer, size_t maxLen, size_t& bufferLen) noexcept;
	void SetNotifier(Notifier* pNotifier) { m_pNotifier = pNotifier; }

	BYTE *m_pRead{}, *m_pWrite{}, *m_pBufferFirst{}, *m_pBufferLast{}, *m_pLapLast{};
	UINT64 m_readLap{}, m_writeLap{}; // A lap is incremented when the pointer reaches the end of the buffer and starts from the beginning.
	Notifier* m_pNotifier{};
	SRWLOCK m_lock;
	BYTE m_buffer[ALLOC_BUF_SIZE];
	bool m_bBufferTooSmall{}, m_bNotifiedBufferTooSmall{};
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HeapProfiler
// Handles the calls to malloc and free and stores the information in a circular buffer.

class HeapProfiler {

public:
	HeapProfiler();
	~HeapProfiler();

	void malloc(void* ptr, size_t size);
	void free(void* ptr);

	bool Init();
	void Destroy();

	void BufferWriterThread();
	void AddressTranslatorThread();

private:
	void WriteBuffer();

private:
	std::atomic<bool> m_bExitThreads = false;	

	CircularBuffer m_CircBuffer;
	BYTE m_buffer[ALLOC_BUF_SIZE];
	size_t m_bufferLen{};

	HANDLE m_hExitThreads{}; // Signaled when all threads must exit	
	HANDLE m_hBufferWriterThread{};
	HANDLE m_hAddrDecoderThread{};

	MemoryFile m_memFileAlloc; // Used to send alloc info
	MemoryFile m_memFileAddr; // Used to translate memory addresses (Instruction Pointers) to symbols

	Notifier m_notifier;

	// The translation of function address generate allocations.
	// These members prevent these allocations from being processed.
	bool m_bSkipAllocs{};
	DWORD m_skipThreadId{};
};