#pragma once

#include <Windows.h>
#include <atlstr.h>
#include <map>
#include <queue>
#include <unordered_map>
#include <vector>
#include <array>

// For debugging allocations. This affects the performance significantly.
#define ENABLE_TRACE_ALLOC	false

#if ENABLE_TRACE_ALLOC
#pragma message("***** WARNING : Allocation traces enabled *****")
#endif

#define MAX_ALLOCS			80000
#define ALLOC_BUF_SIZE		MAX_ALLOCS * (sizeof(AllocInfo) + sizeof(Address))
#define DEF_MEMFILE_SIZE	2048
#define DEF_STR_SIZE		1024
#define BACKTRACE_SIZE		64
#define TIMEOUT				20000
#define THREAD_SLEEP		100
#define NUM_SAMPLES			200
#define DEFAULT_FONT_SIZE   13

#if defined(_WIN64)
#define FREE_MASK			0x8000000000000000
#else
#define FREE_MASK			0x80000000
#endif
#define IS_ALLOC(x)	(!(x & FREE_MASK))

#define MEMFILE_ALLOC		"MemFileAllocs"
#define MEMFILE_ADDR		"MemFileAddr"
#define MEMFILE_MSG			"MemFileMsg"

#define WM_BEGIN_PROGRESS	(WM_APP + 1)
#define WM_STEP_PROGRESS	(WM_APP + 2)
#define WM_END_PROGRESS		(WM_APP + 3)
#define WM_TARGET_FINISHED	(WM_APP + 4)


using Address = UINT_PTR;
using CallStack = std::array<Address, BACKTRACE_SIZE>;

struct RangeU64
{
	RangeU64() : from(ULLONG_MAX), to(ULLONG_MAX) {};
	RangeU64(UINT64 from, UINT64 to) : from(from), to(to) {};
	UINT64 from;
	UINT64 to;
};

struct AllocBuffer
{
	long length;
	BYTE* pBuffer;
};

#pragma pack(push, 1)
struct Alloc
{
	Address addr{};
	size_t numBytes{};
	int numFrames{};
	Address framesAddr[BACKTRACE_SIZE]{};
};
#pragma pack(pop)

struct CallStackInfo
{
	int numAllocs;
	size_t numBytes;
	size_t numBytesInUse;
	int numFrames;
	CString* frames[BACKTRACE_SIZE];
};

// Represents an allocation that has not been freed yet.
struct ActiveAllocation
{
	size_t numAllocBytes; // Number of bytes reserved by this allocation.
	CallStackInfo* pCallStackInfo; // Call stack from where this allocation was performed.
};

struct CallStackHasher
{
	std::size_t operator()(const CallStack& stack) const
	{
		std::size_t h = 0;
		for (auto frame : stack)
			h ^= std::hash<Address>{}(frame)+0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

struct AllocInfo
{
	AllocInfo(size_t numBytes, size_t numTotalAllocs, size_t numTotalBytes, CallStack callStack, CallStackInfo* pCallStackInfo)
		: numBytes(numBytes), numTotalAllocs(numTotalAllocs), numTotalBytes(numTotalBytes), callStack(callStack), pCallStackInfo(pCallStackInfo) {
	};
	size_t numBytes;
	size_t numTotalAllocs;
	size_t numTotalBytes;
	CallStack callStack;
	CallStackInfo* pCallStackInfo;
};

using AddressQueue = std::queue<Address>;
using AddressMap = std::unordered_map<Address, CString*>; // Retrieves a string associated to an address. The string can be the hexadecimal value or the corresponding symbol.
using ActiveAllocationsMap = std::unordered_map<Address, ActiveAllocation>; // Maps an allocated address to its call stack info (only if the allocation has not been freed).
using AllocBufferQueue = std::queue<AllocBuffer>;
using AllocInfoQueue = std::queue<AllocInfo>;
using CallStackMap = std::unordered_map<CallStack, CallStackInfo, CallStackHasher>;
using StringArray = std::vector<CString>;


inline void Trace(LPCSTR fmt, ...)
{
#ifdef _DEBUG
	constexpr int max = 2048;
	va_list args;
	char buffer[max + 2];
	va_start(args, fmt);
	vsprintf_s(buffer, max, fmt, args);
	va_end(args);
	size_t l = strlen(buffer);
	buffer[l] = '\n';
	buffer[l + 1] = 0;
	OutputDebugString(buffer);
#else
	UNREFERENCED_PARAMETER(fmt);
#endif
}

inline void TraceRel(LPCSTR fmt, ...)
{
	constexpr int max = 2048;
	va_list args;
	char buffer[max + 2];
	va_start(args, fmt);
	vsprintf_s(buffer, max, fmt, args);
	va_end(args);
	size_t l = strlen(buffer);
	buffer[l] = '\n';
	buffer[l + 1] = 0;
	OutputDebugString(buffer);
}

