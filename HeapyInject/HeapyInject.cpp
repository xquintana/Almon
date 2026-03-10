#include "HeapProfiler.h"
#include "MinHook.h"
#include <dbghelp.h>


using PtrMalloc = void * (__cdecl *)(size_t);
using PtrFree = void(__cdecl *)(void *);
using PtrFreeDbg = void(__cdecl *)(void *, int);

// Hook tables. (Lots of static data, but it's the only way to do this.)
const int numHooks = 128;
std::mutex hookTableMutex;
int nUsedMallocHooks = 0;
int nUsedFreeHooks = 0;
int nUsedFreeDbgHooks = 0;
PtrMalloc mallocHooks[numHooks];
PtrFree freeHooks[numHooks];
PtrFreeDbg freeDbgHooks[numHooks];
PtrMalloc originalMallocs[numHooks];
PtrFree originalFrees[numHooks];
PtrFreeDbg originalFreeDbgs[numHooks];


HeapProfiler *heapProfiler;


// Mechanism to stop us profiling ourself.
static __declspec(thread) int _depthCount = 0; // use thread local count
struct PreventSelfProfile
{
	PreventSelfProfile() { _depthCount++; }
	~PreventSelfProfile() { _depthCount--; }
	inline bool shouldProfile() { return _depthCount <= 1; }
private:
	PreventSelfProfile(const PreventSelfProfile&) {}
	PreventSelfProfile& operator=(const PreventSelfProfile&) {}
};
void PreventEverProfilingThisThread() { _depthCount++; }

// Malloc hook function. Templated so we can hook many mallocs.
template <int N>
void* __cdecl mallocHook(size_t size)
{
	PreventSelfProfile preventSelfProfile;
	void* p = originalMallocs[N](size);
	if (preventSelfProfile.shouldProfile())
		heapProfiler->malloc(p, size);	
	return p;
}

// Free hook function.
template <int N>
void  __cdecl freeHook(void * p)
{
	PreventSelfProfile preventSelfProfile;
	originalFrees[N](p);
	if (preventSelfProfile.shouldProfile())	
		heapProfiler->free(p);	
}

// Debug Free hook function.
template <int N>
void  __cdecl freeDbgHook(void * p, int block_use)
{
	PreventSelfProfile preventSelfProfile;
	originalFreeDbgs[N](p, block_use);
	if (preventSelfProfile.shouldProfile())		
		heapProfiler->free(p);
}

// Template recursion to init a hook table.
template<int N>
struct InitNHooks
{
	static void initHook()
	{
		InitNHooks<N - 1>::initHook();  // Compile time recursion. 
		mallocHooks[N - 1] = &mallocHook<N - 1>;
		freeHooks[N - 1] = &freeHook<N - 1>;
		freeDbgHooks[N - 1] = &freeDbgHook<N - 1>;
	}
};

template<>
struct InitNHooks<0>
{
	static void initHook() {}; // stop the recursion	
};


// Callback which recieves addresses for mallocs/frees which we hook.
BOOL CALLBACK EnumSymbolsCallback(PSYMBOL_INFO symbolInfo, ULONG symbolSize, PVOID userContext)
{
	std::lock_guard<std::mutex> lk(hookTableMutex);
	PreventSelfProfile preventSelfProfile;

	PCSTR moduleName = (PCSTR)userContext;	

	// Hook mallocs.
	if (strcmp(symbolInfo->Name, "malloc") == 0)
	{
		if (nUsedMallocHooks >= numHooks)
		{
			Trace("HeapyInject::EnumSymbolsCallback-> All malloc hooks used up!");
			return true;
		}
		Trace("HeapyInject::EnumSymbolsCallback-> Hooking malloc from module '%s' into malloc hook num %d.", moduleName, nUsedMallocHooks);
		if (MH_CreateHook((void*)symbolInfo->Address, mallocHooks[nUsedMallocHooks], (void **)&originalMallocs[nUsedMallocHooks]) == MH_OK)
		{
			if (MH_EnableHook((void*)symbolInfo->Address) == MH_OK)
				nUsedMallocHooks++;
			else
				Trace("HeapyInject::EnumSymbolsCallback-> Enable malloc hook failed!");
		}
		else
			Trace("HeapyInject::EnumSymbolsCallback-> Create hook malloc failed!");
	}

	// Hook frees.
	if (strcmp(symbolInfo->Name, "free") == 0)
	{
		if (nUsedFreeHooks >= numHooks)
		{
			Trace("HeapyInject::EnumSymbolsCallback-> All free hooks used up!\n");
			return true;
		}
		Trace("HeapyInject::EnumSymbolsCallback-> Hooking free from module '%s' into free hook num %d.", moduleName, nUsedFreeHooks);
		if (MH_CreateHook((void*)symbolInfo->Address, freeHooks[nUsedFreeHooks], (void **)&originalFrees[nUsedFreeHooks]) == MH_OK)
		{
			if (MH_EnableHook((void*)symbolInfo->Address) == MH_OK)
				nUsedFreeHooks++;
			else
				Trace("HeapyInject::EnumSymbolsCallback-> Enable free failed!");
		}
		else
			Trace("HeapyInject::EnumSymbolsCallback-> Create hook free failed!");
	}

	// Hook debug frees.
	if (strcmp(symbolInfo->Name, "_free_dbg") == 0)
	{
		if (nUsedFreeDbgHooks >= numHooks)
		{
			Trace("HeapyInject::EnumSymbolsCallback-> All debug free hooks used up!");
			return true;
		}
		Trace("HeapyInject::EnumSymbolsCallback-> Hooking debug free from module '%s' into debug free hook num %d.", moduleName, nUsedFreeDbgHooks);
		if (MH_CreateHook((void*)symbolInfo->Address, freeDbgHooks[nUsedFreeDbgHooks], (void **)&originalFreeDbgs[nUsedFreeDbgHooks]) == MH_OK)
		{
			if (MH_EnableHook((void*)symbolInfo->Address) == MH_OK)
				nUsedFreeDbgHooks++;
			else
				Trace("HeapyInject::EnumSymbolsCallback-> Enable debug free failed!");
		}
		else
			Trace("HeapyInject::EnumSymbolsCallback-> Create hook debug free failed!");
	}
	return true;
}

// Callback which recieves loaded module names which we search for malloc/frees to hook.
BOOL CALLBACK EnumModulesCallback(PCSTR ModuleName, DWORD_PTR BaseOfDll, PVOID UserContext)
{
	// TODO: Hooking msvcrt causes problems with cleaning up stdio - avoid for now.
	if (strcmp(ModuleName, "msvcrt") == 0)
		return true;

	SymEnumSymbols(GetCurrentProcess(), BaseOfDll, "malloc", EnumSymbolsCallback, (void*)ModuleName);
	SymEnumSymbols(GetCurrentProcess(), BaseOfDll, "free", EnumSymbolsCallback, (void*)ModuleName);
	SymEnumSymbols(GetCurrentProcess(), BaseOfDll, "_free_dbg", EnumSymbolsCallback, (void*)ModuleName);
	return true;
}

void ChangeCurrentDirectory()
{
	CHAR dirProc[MAX_PATH];
	if (GetModuleFileName(NULL, dirProc, MAX_PATH) > 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		CHAR drive[_MAX_DRIVE];
		CHAR dir[_MAX_DIR];
		CHAR fname[_MAX_FNAME];
		CHAR ext[_MAX_EXT];
		_splitpath_s(dirProc, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);
		_makepath_s(dirProc, MAX_PATH, drive, dir, nullptr, nullptr);
		SetCurrentDirectory(dirProc);
	}	
}

bool SetupHeapProfiling()
{
	Trace("HeapyInject::SetupHeapProfiling-> Injecting library...\n");
	
	// The current directory of the target process is set to the profiler directory by default.
	// Change it to the directory of the target executable.
	ChangeCurrentDirectory();	

	nUsedMallocHooks = 0;
	nUsedFreeHooks = 0;

	PreventEverProfilingThisThread();

	// Create our hook pointer tables using template meta programming fu.
	InitNHooks<numHooks>::initHook();

	// Init min hook framework.
	MH_Initialize();

	// Init dbghelp framework.
	SymSetOptions(SYMOPT_LOAD_LINES);
	if (!SymInitialize(GetCurrentProcess(), NULL, true))
		Trace("HeapyInject::SetupHeapProfiling-> SymInitialize failed");

	// Yes this leaks - cleaning it up at application exit has zero real benefit.	
	heapProfiler = new HeapProfiler();

	// Trawl through loaded modules and hook any mallocs and frees we find.
	SymEnumerateModules(GetCurrentProcess(), EnumModulesCallback, NULL);

	return heapProfiler->Init();
}

int LockLibraryIntoProcessMem(HMODULE DllHandle, HMODULE *LocalDllHandle)
{
	if (NULL == LocalDllHandle)
		return ERROR_INVALID_PARAMETER;

	*LocalDllHandle = NULL;

	TCHAR moduleName[1024];

	if (0 == GetModuleFileName(
		DllHandle,
		moduleName,
		sizeof(moduleName) / sizeof(TCHAR)))
		return GetLastError();

	*LocalDllHandle = LoadLibrary(moduleName);

	if (NULL == *LocalDllHandle)
		return GetLastError();

	return NO_ERROR;
}

extern "C" {

	BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID lpReserved)
	{
		HMODULE hProfiler{};

		switch (reasonForCall)
		{
		case DLL_PROCESS_ATTACH:
			Trace("HeapyInject-> Attaching profiler...");
			if (SetupHeapProfiling())
			{
				int code = LockLibraryIntoProcessMem((HMODULE)hModule, &hProfiler);
				if (code == NO_ERROR)
				{
					if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN, "HeapyInject_x64.dll", &hProfiler) && hProfiler != NULL)
						Trace("HeapyInject-> Profiler successfully attached to process.");
					else
						Trace("HeapyInject-> The profiler could not be attached to process. Last error: %d", GetLastError());
				}
				else
					Trace("HeapyInject-> LockLibraryIntoProcessMem failed. Last error: %d", code);
			}		
			else
				Trace("HeapyInject-> Failed to setup the profiler");
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			Trace("HeapyInject-> Destroying profiler...");
			heapProfiler->Destroy();
			//delete heapProfiler; // Yes this leaks - cleaning it up at application exit has zero real benefit.	
			Trace("HeapyInject-> Profiler detached from process.");
			break;
		}
		return TRUE;
	}
}
