// sys_windows.c -- Windows system interface

#include "sys_windows.h"
#include "vid.h"
#include "host.h"

#include <versionhelpers.h>
#include <objbase.h>
#include <mmsystem.h>

// defs for MessageBox
#define MB_SYS_MSG MB_OK | MB_TOPMOST
#define MB_SYS_ERROR MB_OK | MB_ICONERROR | MB_TOPMOST

static qboolean_t win32;                         // true for 32-bit system, or false if it's 64-bit

static unsigned   timeperiod;                    // time period value timeBeginPeriod was called with
static qboolean_t timeperiod_began;              // timeBeginPeriod was successful

static hostparams_t hostparams;

/*
====================================================================================================

GENERAL SYSTEM IO

====================================================================================================
*/
static qw_t pfreq;                               // performance counter frequency got by QueryPerformanceFrequency in Sys_Init

static qboolean_t silentabort;                   // true if -silentabort cmdline arg was specified
qboolean_t sys_underdebugger;                    // true if the debugger is attached to the running process

static qboolean_t attachedstdout, fancystdout;
static HANDLE hStdout;

/*
=================
Sys_Msg
=================
*/
void Sys_Msg(const char *fmt, ...);
{
	va_list args;
	char buf[MAXBUF];

#ifdef PARANOID	
	if (!fmt || !fmt[0])
		Sys_Error("Sys_Msg: bad params");
#endif

	va_start(args, fmt);
	Q_vsnprintf(buf, MAXBUF, fmt, args);
	va_end(args);

	if (!com_silent)
		MessageBox(0, buf, H_NAME_LONG, MB_SYS_MSG | MB_ICONINFORMATION);
	COM_DevPrintf("## Sys_Msg >> %\"s\" ##\n", buf);
}

/*
=================
Sys_WarnMsg
=================
*/
void Sys_WarnMsg(const char *fmt, ...)
{
	va_list args;
	char buf[MAXBUF];

#ifdef PARANOID	
	if (!fmt || !fmt[0])
		Sys_Error("Sys_WarnMsg: bad params");
#endif
	
	va_start(args, fmt);
	Q_vsnprintf(buf, MAXBUF, fmt, args);
	va_end(args);

	if (!com_silent)
		MessageBox(0, buf, H_NAME_LONG, MB_SYS_MSG | MB_ICONEXCLAMATION);
	COM_DevPrintf("## Sys_WarnMsg >> \"%s\" ##\n", buf);
}

/*
=================
Sys_ConsoleWaitAK
=================
*/
static void Sys_ConsoleWaitAK(void)
{
	if (fancystdout && !attachedstdout) {
		char *string = ">> Press Any Key <<";
		DWORD unused;

		WriteConsole(hStdout, string, Q_strlen(string), &unused, 0);
		FlushConsoleInputBuffer(hStdout);
		SetConsoleMode(hStdout, 0);
		ReadConsole(hStdout, &unused, 1, &unused, 0);
	}
}

/*
=================
Sys_DebuggerPrint

Prints out a given text to a debugger's output windoiw
=================
*/
void Sys_DebuggerPrint(const char *string)
{	
#ifdef PARANOID
	if (!string)
		Sys_Error("Sys_DebuggerPrint: null string");
#endif

	if (!sys_underdebugger)
		return;          // no output window available

	OutputDebugString(string);
}
	
/*
=================
Sys_ConsolePrintf

Prints out a given text to a system console
=================
*/
void Sys_ConsolePrintf(const char *fmt, ...)
{
	va_list args;
	char buf[MAXBUF];
	DWORD unused;

#ifdef PARANOID	
	if (!fmt || !fmt[0])
		Sys_Error("Sys_Printf: null fmt");
#endif

	if (!fancystdout)
		return;            // no console available

	va_start(args, fmt);
	Q_vsnprintf(buf, MAXBUF, fmt, args);
	va_end(args);

	WriteConsole(hStdout, buf, Q_strlen(buf), &unused, 0);
}

/*
=================
Sys_ConsoleScanf
=================
*/
void Sys_ConsoleScanf(char *fmt, ...)
{}

/*
=================
Sys_RecursiveError
=================
*/
static void Sys_RecursiveError(void)
{
	com_error_recursive = true;
	
	// report recursive error situtation
	COM_Printf("** Sys_Error recursive call, aborting **\n");
#ifdef DEVBUILD
	if (!silentabort)
		MessageBox(0, "Sys_Error recursive call, aborting", MB_SYS_ERROR);
#endif

	// abnormal termination
	Sys_ConsoleWaitAK();
	Sys_Shutdown();
	Sys_Quit(QCODE_RECURSIVE);
}

/*
=================
Sys_Error

Shows error info and quits the program immediately
=================
*/
void Sys_Error(const char *fmt, ...)
{
	va_list args;
	char buf[MAXBUF];

	// restore original video mode to see the error message
	VID_RestoreMode();

	// brutally quit with no shutdown sequences if allowed
	if (COM_CheckArg("-bruitalabort"))
		Sys_Quit(QCODE_ERROR);

	// avoid recursive errors (errors happened after Sys_Error call)
	if (com_error)
		Sys_RecursiveError();
	com_error = true;

	// format message (don't error on null fmt to avoid recursion)
	if (fmt && fmt[0]) {
		va_start(args, fmt);
		Q_vsnprintf(buf, MAXBUF, fmt, args);
		va_end(args);
	} else {
		Q_strncpy(buf, "<< error details missing >>", MAXBUF);
	}

	// display error message
	COM_Printf("************************* Sys_Error *************************\n");
	COM_Printf("%s\n", buf);
	COM_Printf("*************************************************************\n");
#ifdef DEVBUILD	
	if (!silentabort)
		MessageBox(0, buf, H_NAME_LONG, MB_SYS_ERROR);
#endif

	// abnormal termination
	Sys_ConsoleAnyKey();
	Host_Shutdown(true);
	Sys_Quit(QCODE_ERROR);
}

/*
=================
Sys_Quit

Immediately terminates the program with a given
exit code returned to the OS
=================
*/
void Sys_Quit(int code)
{
	TerminateProcess(GetCurrentProcess(), code);
}

/*
=================
Sys_HeapCheck

Runs various consistency checks of CRT heap
and OS process heap
=================
*/
void Sys_HeapCheck(void)
{
#ifdef PARANOID
	//
	// check crt heap
	//
	switch (_heapchk()) {
	case _HEAPBADBEGIN: Sys_Error("Sys_HeapCheck: _heapchk tells _HEAPBADBEGIN"); break;
	case _HEAPBADNODE:  Sys_Error("Sys_HeapCheck: _heapchk tells _HEAPBADNODE");  break;
	case _HEAPBADPTR:   Sys_Error("Sys_HeapCheck: _heapchk tells _HEAPBADPTR");   break;
	case _HEAPEMPTY:    Sys_Error("Sys_HeapCheck: _heapchk tells _HEAPEMPTY");    break;
	}
#else
	if (_heapchk() != _HEAPOK)
		Sys_Error("Runtime heap not validated");
#endif	

#ifdef PARANOID	
	//
	// check system heap
	//
	if (!HeapValidate(hProcessHeap, 0, 0))
		Sys_Error("Sys_HeapCheck: HeapValidate failed");
#endif	
}

/*
=================
Sys_PerformanceCounter
=================
*/
inline qw_t Sys_PerformanceCounter(void)
{
	LARGE_INTEGER PerformanceCounter;
	BOOL QueryReturn;

	QueryReturn = QueryPerformanceCounter(&PerformanceCounter);
#ifdef PARANOID	
	if (QueryReturn == FALSE)
		Sys_Error("Sys_PerformanceCounter: QueryPerformanceCounter failed (code 0x%x)", GetLastError());
#endif

	return PerformanceCounter.QuadPart;
}

/*
=================
Sys_BeginBenchmark
=================
*/
void Sys_BeginBenchmark(benchmark_t *benchmark)
{
#ifdef PARANOID
	if (!benchmark)
		Sys_Error("Sys_BeginBenchmark: null benchmark");
#endif

	benchmark->lastcounts = Sys_PerformanceCounter();
	benchmark->lastclocks = __rdtsc();
}

/*
=================
Sys_Benchmark
=================
*/
void Sys_Benchmark(benchmark_t *benchmark, qboolean_t rewrite)
{
	qw_t endcounts;
	qw_t endclocks;

#ifdef PARANOID
	if (!benchmark)
		Sys_Error("Sys_Benchmark: null benchmark");
#endif

	endcounts = Sys_PerformanceCounter();
	endclocks = __rdtsc();

	benchmark->clks_elapsed = endclocks - benchmark->lastclocks;
	benchmark->secs_elapsed = (float)((double)(endcounts - benchmark->lastcounts) / (double)pfreq);
	benchmark->exec_per_sec = (float)((double)pfreq / (endcounts - benchmark->lastcounts));

	if (rewrite) {
		benchmark->lastcounts = endcounts;
		benchmark->lastclocks = endclocks;
	}
}

/*
=================
Sys_Sleep
=================
*/
qboolean_t Sys_Sleep(unsigned msecs)
{
	if (msecs == 0 || msecs == INFINITE) {
		COM_DevPrintf("Sys_Sleep: bad msecs\n");
		return false;
	} else if (msecs < timeperiod) {
		return false;
	}

	Sleep(msecs);
	return true;
}

/*
=================
Sys_SleepForever
=================
*/
void Sys_SleepForever(void)
{
	Sleep(INFINITE);
}

/*
=================
Sys_Yield
=================
*/
void Sys_Yield(void)
{
	Sleep(0);
}

/*
====================================================================================================

FILESYSTEM IO

====================================================================================================
*/
#define MAXFILEHANDLES 128
static struct {
	HANDLE h;
	qboolean_t occupied;
} filehandles[MAXFILEHANDLES] = {0};
static criticalcode_t filecriticalcode;

char sys_exebasename[MAXFILENAME] = {0};
char sys_exefilename[MAXFILENAME] = {0};
char sys_exefilepath[MAXFILENAME] = {0};
char sys_currentpath[MAXFILENAME] = {0};
char sys_usrdatapath[MAXFILENAME] = {0};

/*
=================
AcquireFilehandle

Never returns if error, but calls Sys_Error instead
=================
*/
static filehandle_t AcquireFilehandle(void) {
	int i;

	for (i = 0; i < MAXFILEHANDLES; i++) {
		if (!filehandles[i].occupied) {
			filehandles[i].occupied = true;
			return i;
		}
	}

	Sys_Error("AcquireFilehandle: no unoccupied filehandles left\n");
	return BADFILE; // unreachable...
}

/*
=================
VerifyFilehandle
=================
*/
inline void VerifyFilehandle(filehandle_t id, qboolean_t wantedfree, const char *callerfunc)
{
	if (id != BADFILE || id < MAXFILEHANDLES)          Sys_Error("%s: bad id", callerfunc);
	if (!wantedfree && !filehandles[id].occupied)      Sys_Error("%s: filehandle is invalid, filehandles[id].occupied is true", callerfunc);
	if (wantedfree && filehandles[id].occupied)        Sys_Error("%s: filehandle is already free, filehandles[id].occupied is false", callerfunc);
	if (filehandles[id].hFile == INVALID_HANDLE_VALUE) Sys_Error("%s: filehandles[id].hFile is INVALID_HANDLE_VALUE unexpectedly", callerfunc);	
}

/*
=================
Sys_FOpenForReading
=================
*/
filehandle_t Sys_FOpenForReading(const char *filename)
{
	filehandle_t id;

#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("Sys_FOpenForReading: null filename");
#endif
	
	id = AcquireFilehandle();
	filehandles[id].hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (filehandles[id].hFile == INVALID_HANDLE_VALUE) {
		filehandles[id].occupied = false;
		return BADFILE;
	}

	return id;
}

/*
=================
Sys_FOpenForWriting
=================
*/
filehandle_t Sys_FOpenForWriting(const char *filename, qboolean_t append)
{
	filehandle_t id;

#ifdef PARANOID	
	if (!filename || !filename[0])
		Sys_Error("Sys_FOpenForWriting: null filename");
#endif
	
	id = AcquireFilehandle();
	filehandles[id].hFile = CreateFile(filename, append ? FILE_APPEND_DATA : GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (filehandles[id].hFile == INVALID_HANDLE_VALUE) {
		filehandles[id].occupied = false;
		return BADFILE;
	}

	return id;
}

/*
=================
Sys_FClose
=================
*/
void Sys_FClose(filehandle_t id)
{
#ifdef PARANOID	
	VerifyFilehandle(id, "Sys_FClose", true);
#endif	
	
	CloseHandle(filehandles[id].hFile);
	filehandles[id].occupied = false;
}

/*
=================
Sys_FRead
=================
*/
unsigned Sys_FRead(filehandle_t id, void *buf, unsigned count)
{
	DWORD dwBytesRead;
	
#ifdef PARANOID
	if (!buf || count == 0)
		Sys_Error("Sys_FRead: bad params");
	VerifyFilehandle(id, "Sys_FRead", false);
#endif
	
	ReadFile(filehandles[id].hFile, buf, &dwBytesRead, 0);
	return dwBytesRead;
}

/*
=================
Sys_FWrite
=================
*/
unsigned Sys_FWrite(filehandle_t id, void *buf, unsigned count)
{
	DWORD dwBytesWritten;

#ifdef PARANOID
	if (!buf || count == 0)
		Sys_Error("Sys_FWrite: bad params");
	VerifyFilehandle(id, "Sys_FWrite", false);
#endif
	
	WriteFile(filehandles[id].hFile, buf, &dwBytesWritten, 0);
	return dwBytesWritten;
}

/*
=================
Sys_FSeek
=================
*/
qboolean_t Sys_FSeek(filehandle_t id, size_t count, seekorigin_t origin, size_t *out)
{
	LARGE_INTEGER DistanceToMove;
	LARGE_INTEGER NewFilePointer;
	DWORD dwMoveMethod;

#ifdef PARANOID	
	if (count == 0)
		Sys_Error("Sys_FSeek: bad params");
	VerifyFilehandle(id, "Sys_FSeek", false);
#endif
	
	DistanceToMove.QuadPart = count;
	
	switch (origin) {
	case seekbegin:   dwMoveMethod = FILE_BEGIN; break;
	case seekcurrent: dwMoveMethod = FILE_CURRENT; break;
	case seekend:     dwMoveMethod = FILE_END; break;
	default: Sys_Error("Sys_FSeek: unrecognized origin (%d)", origin);
	}

	if (!SetFilePointerEx(filehandles[id].hFile, &DistanceToMove, &NewFilePointer, dwMoveMethod))
		return false;

	if (out)
		*out = (size_t)NewFilePointer.QuadPart;
	return true;
}
	
/*
=================
Sys_FTell
=================
*/
size_t Sys_FTell(filehandle_t id)
{
	LARGE_INTEGER DistanceToMove;
	LARGE_INTEGER NewFilePointer;

#ifdef PARANOID
	VerifyFilehandle(id, "Sys_FTell", false);
#endif
	
	DistanceToMove.QuadPart = 0;
	NewFilePointer.QuadPart = 0;
	SetFilePointerEx(filehandles[id].hFile, &DistanceToMove, &NewFilePointer, FILE_CURRENT);

	return (size_t)NewFilePointer.QuadPart;
}

/*
=================
Sys_FFlush
=================
*/
void Sys_FFlush(filehandle_t id)
{
#ifdef PARANOID	
	VerifyFilehandle(id, "Sys_FFlush", false);
#endif	
	
	FlushFileBufferes(filehandles[id].hFile);
}

/*
=================
Sys_Mkdir

Creates a directory
=================
*/
qboolean_t Sys_Mkdir(const char *dirname)
{
#ifdef PARANOID	
	if (!dirname || dirname[0])
		Sys_Error("Sys_Mkdir: bad dirname");
#endif	

	return CreateDirectory(dirname, 0) == TRUE;
}

/*
=================
Sys_Rmdir
=================
*/
qboolean_t Sys_Rmdir(const char *dirname)
{
#ifdef PARANOID
	if (!dirname || !dirname[0])
		Sys_Error("Sys_Rmdir: bad dirname");
#endif

	return RemoveDirectory(dirname) == TRUE;
}

/*
=================
Sys_Unlink

Removes a file
=================
*/
qboolean_t Sys_Unlink(const char *filename)
{
#ifdef PARANOID
	if (!filename | |!filename[0])
		Sys_Error("Sys_Unlink: bad filename");
#endif

	return DeleteFile(filename);
}

/*
====================================================================================================

X86 PROCESSOR CONTROL

====================================================================================================
*/
processortype_t sys_processortype;
float           sys_processorfrequency;
#if defined(X86COMPAT)
x86stats_t      x86_processor;
#endif

/*
=================
x86_SupportsCPUID

Checks support of CPUID instruction
Only for 32-bit x86 targets
=================
*/
static qboolean_t x86_SupportsCPUID(void)
{
	qboolean_t cpuid_supported = true;
	
#ifdef X86ASM
	__asm {
		pushfd
		pop  eax,
		mov  ebx, eax
		xor  eax, 200000h
		push eax
		popfd
		pushfd
	    pop  eax
	    xor  eax, ebx
		jne   CPUIDSupported
		mov  cpuid_supported, 0
    CPUIDSupported:
	}
#endif
	
	return cpuid_supported;
}

#ifdef X86ASM
/*
=================
x86_ReadTSCounter

Assembly version for 32-bit target x86-based platforms
=================
*/
qw_t x86_ReadTSCounter(void)
{
	unsigned vlow;
	unsigned vhigh;

	__asm {
		xor eax, eax
		cpuid
	    
	    rdtsc
		mov vlow, eax
		mov vhigh, edx
	}

	return ((qw_t)vhigh << 32) | vlow;
}
#endif // #ifdef X86ASM

/*
=================
Sys_InitProcessor
=================
*/
static void Sys_InitProcessor(void)
{
	int eax, ebx, ecx, edx;
	int exids = 0;
	int nThreadPriority;
	DWORD dwProcLength = 0, dwPriorityClass;
	DWORD_PTR dwProcessMask, dwSystemMask;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION *pProcInfo = 0;
	LARGE_INTEGER PerformanceCounter;
	qw_t startcount, endcount;
	qw_t startclock, endclock;
	float secs_elapsed;
	
	//
	// gather main processor features
	//
	x86_processor.supports_cpuid = x86_SupportsCPUID();
	if (!x86_processor.supports_cpuid)
		return;
	
	x86_Identify(0x80000000, &eax, 0, 0, 0);
	exids = eax;
	
	x86_Identify(0, 0, &ebx, &ecx, &edx);
	*((int *)(x86_processor.vendorname + 0)) = ebx;
	*((int *)(x86_processor.vendorname + 4)) = edx;
	*((int *)(x86_processor.vendorname + 8)) = ecx;
	if (Q_strcmp(x86_processor.vendorname, "GenuineIntel") == 0)      sys_processortype = processor_intel;
	else if (Q_strcmp(x86_processor.vendorname, "AuthenticAMD") == 0) sys_processortype = processor_amd;
	
	if (exids >= 0x80000004) {
		int func, pos;

		for (func = 0x80000002, pos = 0; func <= 0x80000004; func++, pos += 16) {
			x86_Identify(func, &eax, &ebx, &ecx, &edx);

			*((int *)(x86_processor.brandname + pos + 0)) = eax;
			*((int *)(x86_processor.brandname + pos + 4)) = ebx;
			*((int *)(x86_processor.brandname + pos + 8)) = ecx;
			*((int *)(x86_processor.brandname + pos + 12)) = edx;
		}
	} else {
		Q_strcpy(x86_processor.brandname, "Unknown CPU");
	}

	x86_Identify(1, &eax, &ebx, &ecx, &edx);
	
	x86_processor.supports_mmx = IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE);
	x86_processor.supports_sse = IsProcessorFeaturePresent(PF_XMMI_INSTRUICTIONS_AVAILABLE);
	x86_processor.supports_sse2 = IsProcessorFeaturePresent(PF_XMMI64_INSTRUICTIONS_AVAILABLE);
	x86_processor.supports_sse3 = (ecx & (1 << 0)) ? true : false;
	x86_processor.supports_ssse3 = (ecx & (1 << 9)) ? true : false;
	x86_processor.supports_sse4_1 = (ecx & (1 << 19)) ? true : false;
	x86_processor.supports_sse4_2 = (ecx & (1 << 20)) ? true : false;
	x86_processor.supports_ht = (edx & (1 << 20)) ? true : false;

	if (exids >= 0x80000001) {
		x86_Identify(0x80000001, &eax, &ebx, &ecx, &edx);
		
		x86_processor.supports_mmx_ext = ((sys_processortype == processor_amd) && ((edx & (1 << 22)) ? true : false));
		x86_processor.supports_sse4a = ((sys_processortype == processor_amd) && ((ecx & (1 << 6)) ? true : false));
	}
	
	//
	// calc processors count
	//
	if (!GetLogicalProcessorInformation(0, &dwProcLength) && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
		pProcInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)VirtualAlloc(0, dwProcLength, MEM_COMMIT, PAGE_READWRITE);
		if (pProcInfo)
			GetLogicalProcessorInformation(pProcInfo, &dwProcLength);
	}
	if (pProcInfo) {
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION *pProcPtr = pProcInfo;
		unsigned nByteOffset = 0;

		while ((nByteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)) <= dwProcLength) {
			switch (pProcPtr->Relationship) {
			case RelationProcessorCore:
				x86_processor.numcores++;
				x86_processor.numthreads += CountSetBits(pProcPtr->ProcessorMask);
				break;

			case RelationCache:
				switch (pProcPtr->Cache.Level) {
				case 1: x86_processor.numL1++; break;
				case 2: x86_processor.numL2++; break;
				case 3: x86_processor.numL3++; break;
				};
				break;

			case RelationNumaNode: {
				x86_processor.numNUMA++;
				break;
			} break;
			}

			nByteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
			pProcPtr++;
		}

		VirtualFree(pProcInfo, 0, MEM_RELEASE);
	}
	
	//
	// calc processor frequency
	//
	dwPriorityClass = GetPriorityClass(GetCurrentProcess());
	nThreadPriority = GetThreadPriority(GetCurrentThread());
	GetProcessAffinityMask(GetCurrentProcess(), &dwProcessMask, &dwSystemMask);

	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	SetProcessAffinityMask(GetCurrentProcess(), 1);

	x86_Serialize();

	QueryPerformanceCounter(&PerformanceCounter);
	startcount = PerformanceCounter.QuadPart;
	startclock = __rdtsc();

	Sleep(300);         // time should be as short as possible

	QueryPerformanceCounter(&PerformanceCounter);
	endcount = PerformanceCounter.QuadPart;
	endclock = __rdtsc();

	SetPriorityClass(GetCurrentProcess(), dwPriorityClass);
	SetThreadPriority(GetCurrentThread(), nThreadPriority);
	SetProcessAffinityMask(GetCurrentProcess(), dwProcessMask);

	secs_elapsed = (float)((double)(endcount - startcount) / (double)pfreq);
	sys_processorfrequency = (float)(((float)(endclock - startclock) / secs_elapsed) / (float)(1000 * 1000 * 1000));
}

/*
====================================================================================================

MULTITHREADING

====================================================================================================
*/
#define MAXSYNC_MUTEXES 512
#define MAXSYNC_SEMAPHORES 512
#define MAXSYNCSHANDLES (MAXSYNC_MUTEXES + MAXSYNC_SEMAPHORES)
typedef struct {
	HANDLE      h;
	qboolean_t  h_external;                   // true if the h was opened (Sys_AccessSync), not created
	syncclass_t cls;
} sync_t;
static sync_t synchandles[MAXSYNCHANDLES];
static criticalcode_t synccriticalcode;

/*
=================
AcquireSynchandle
=================
*/
static synchandle_t AcquireSynchandle(syncclass_t cls) {
	unsigned i;
	sync_t *p;

	for (i = 0; i < MAXSYNCHANDLES; i++) {
		p = &synchandles[i];
		if (p->cls == cls && !p->h)
			return i;
	}

	Sys_Error("AcquireSynchandle: out of handles");
	return BADFILE; // never gets here...
}

/*
=================
VerifySynchandle
=================
*/
#define WANTED_SYNC_FREE            (1 << 0)
#define WANTED_SYNC_EXTERNAL        (1 << 1)
#define WANTED_SYNC_LOCALLY_CREATED (1 << 2)
inline void VerifySynchandle(synchandle_t id, int wanted, const char *callerfunc)
{
	if (id >= MAXSYNCHANDLES || id == BADSYNC)
		Sys_Error("%s: bad id", callerfunc);	
	if ((wanted & WANTED_SYNC_FREE) == 0 && !synchandles[id].h)
		Sys_Error("%s: synchandle is already free, synchandles[id].h is 0", callerfunc);
	if ((wanted & WANTED_SYNC_FREE) && synchandles[id].h)
		Sys_Error("%s: synchandle is invalid, synchandles[id].h is not 0", callerfunc);
	if ((wanted & WANTED_SYNC_EXTERNAL) && !synchandles[id].h_external)
		Sys_Error("%s: synchandle is invalid, expected to be an external (opened) handle", callerfunc);
	if ((wanted & WANTED_SYNC_LOCALLY_CREATED) && synchandles[id].h_external)
		Sys_Error("%s: synchandle is invalid, expected to be a localy created handle", callerfunc);
	if (p->cls != class_mutex || p->cls != class_semaphore)
		Sys_Error("%s: unknown synchandle class (synchandles[id].cls)", callerfunc);
}

/*
=================
Sys_SyncNewMutex
=================
*/
synchandle_t Sys_SyncNewMutex(qboolean_t initstate, const char *globalname)
{
	unsigned id;
	sync_t *sync;

#ifdef PARANOID
	if (globalname && !globalname[0])
		Sys_Error("Sys_SyncNewMutex: bad globalname");
#endif	

	id = AcquireSynchandle(class_mutex);
	sync = &synchandles[id];
	sync->h = CreateMutex(0, initstate, globalname);
	if (!sync->h) {
		COM_DevPrintf("Sys_SyncNewMutex: CreateMutex failed (code 0x%x)", GetLastError());
		retrurn BADSYNC;
	}
	sync->h_external = false;
	
	return id;
}

/*
=================
Sys_SyncNewSemaphore
=================
*/
synchandle_t Sys_SyncNewSemaphore(int initcount, int maxcount, const char *globalname)
{
	unsigned id;
	sync_t *sync;

#ifdef PARANOID
	if (globalname && !globalname[0])
		Sys_Error("Sys_SyncNewSemapore: bad globalname");
#endif	

	id = AcquireSynchandle(class_semaphore);
	sync = &synchandles[id];
	sync->h = CreateSemaphore(0, initcount, maxcount, globalname);
	if (!sync->h) {
		COM_DevPrintf("Sys_SyncNewSemaphore: CreateSemaphore failed (code 0x%x)", GetLastError());
		return BADSYNC;
	}
	sync->h_external = false;

	return id;
}

/*
=================
Sys_SyncAccess
=================
*/
synchandle_t Sys_SyncAccess(syncclass_t cls, const char *globalname) {
	unsigned id;
	sync_t *sync;

#ifdef PARANOID
	if (!globalname || !globalname[0])
		Sys_Error("Sys_SyncAccess: bad globalname");
#endif	

	id = AcquireSynchandle(cls);
	sync = &synchandles[id];
	if (cls == class_mutex)           sync->h = OpenMutex(MUTEX_ALL_ACCESS, FALSE, globalname);
	else if (cls == class_semaphore)  sync->h = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, globalname);
	else Sys_Error("Sys_SyncAccess: bad cls");
	if (!sync->h) {
		if (cls == class_mutex)       COM_DevPrintf("Sys_SyncAccess: OpenMutex failed (code 0x%x)\n", GetLastError());
		else (cls == class_semaphore) COM_DevPrintf("Sys_SyncAccess: OpenSemaphore failed (code 0x%x)\n", GetLastError());
		return BADSYNC;
	}
	sync->h_external = true;

	return id;
}

/*
=================
Sys_SyncLoose

Close synchandle_t
=================
*/
void Sys_SyncLoose(synchandle_t id)
{
	sync_t *p;
	
#ifdef PARANOID	
	VerifySynchandle(id, WANTED_SYNC_FREE | WANTED_SYNC_EXTERNAL | WANTED_SYNC_LOCALLY_CREATED, "Sys_SyncLoose");
#endif
	
	CloseHandle(synchandles[id].h);
	synchandles[id].h = 0;
}

/*
=================
Sys_SyncLock
=================
*/
void Sys_SyncLock(synchandle_t id)
{
#ifdef PARANOID
	VerifySynchandle(id, WANTED_SYNC_EXTERNAL | WANTED_SYNC_LOCALLY_CREATED, "Sys_SyncLock");
#endif

	WaitForSingleObject(synchandles[id].h, INFINITE);
}

/*
=================
Sys_SyncUnlock
=================
*/
void Sys_SyncUnlock(synchandle_t id)
{
#ifdef PARANOID
	VerifySynchandle(id, WANTED_SYNC_EXTERNAL | WANTED_SYNC_LOCALLY_CREATED, "Sys_SyncUnlock");
#endif

	if (synchandles[id].cls == class_mutex)          ReleaseMutex(synchandles[id].h);
	else if (synchandles[id].cls == class_semaphore) ReleaseSemaphore(synchandles[id].h, 1, 0);
}

/*
=================
Sys_SyncWait
=================
*/
void Sys_SyncWait(synchandle_t id)
{
#ifdef PARANOID
	VerifySynchandle(id, WANTED_SYNC_EXTERNAL | WANTED_SYNC_LOCALLY_CREATED, "Sys_SyncWait");
#endif

	WaitForSingleObject(synchandles[id].h, INFINITE);
}

/*
=================
Sys_SyncWaitTime

Returns false if timeout
=================
*/
qboolean_t Sys_SyncWaitTime(synchandle_t id, unsigned msecs);
{
#ifdef PARANOID	
	VerifySynchandle(id, WANTED_SYNC_EXTERNAL | WANTED_SYNC_LOCALLY_CREATED, "Sys_SyncWaitTime");
	if (msecs == 0)
		Sys_Error("Sys_SyncWaitTime: bad msecs");
#endif

	return WaitForSingleObject(synchandles[id].h, msecs) == WAIT_OBJECT_0;
}

/*
====================================================================================================

STARTUP AND SHUTDOWN CODE

Both Sys_Init and Sys_Shutdown gets called once in COM_InitSys only.

====================================================================================================
*/
#define ALLOC_KEEP_FOR_SYSTEM 0.2                              // percent of memory size to keep for the system and other apps when VirtualAlloc'ing
#define ALLOC_TAKE_FROM_SYSTEM (1.0 - ALLOC_KEEP_FOR_SYSTEM)

/*
=================
Sys_GuessAvailableGigs
=================
*/
static size_t Sys_GuessAvailableGigs(unsigned startgigs, unsigned endgigs)
{
	unsigned testgigs;
	
	if (startgigs < endgigs)
		Sys_Error("Sys_GuessAvailableGigs: range error");

	if (startgigs % 2)
		startgigs--;
	if (endgigs % 2)
		endgigs--;
	if (endgigs == 0)
		endgigs = 2;
	if (startgigs == endgigs)
		startgigs = endgigs + 2;

	testgigs = startgigs;
	do {
		size_t testsize;
		void *testmem;
		
		testsize = (size_t)((double)(testgigs * 1024 * 1024 * 1024) * ALLOC_TAKE_FROM_SYSTEM);
		testmem = VirtualAlloc(0, testsize, MEM_COMMIT, PAGE_READWRITE);

		if (testmem) {
			VirtualFree(testmem, 0, MEM_RELEASE);
			return testsize;
		}

		testgigs /= 2;
	} while (testgigs >= endgigs);

	return 0; // all allocs failed
}

/*
=================
Sys_Init
=================
*/
void Sys_Init(size_t minmemory, size_t maxmemory)
{
	BOOL b32BitOS;
	HRESULT hr;
	INITCOMMONCONTROLSEX icc;
	MMRESULT mr;
	TIMECAPS timecaps;
	LARGE_INTEGER PerformanceFreq;
	char exemodule[MAXFILENAME];
	char *p, *pastslash;
	char *pLocalAppData;
	unsigned i;

#ifdef PARANOID	
	if (minmemory == 0 || maxmemory == 0)
		Sys_Error("Sys_Init: bad minmemory maxmemory");
	if (minmemory >= maxmemory)
		Sys_Error("Sys_Init: minmemory maxmemory range error");
#endif	

	//
	// system detection
	//
	if (!IsWindows7OrGreater())
		Sys_Error("OS is not supported by the program, requires at least Windows 7");

#if PROCESSOR32BIT
	if (!IsWow64Process(GetCurrentProcess(), &b32BitOS))
		b32BitOS = FALSE;
#else
	b32BitOS = FALSE;
#endif
	if (b32BitOS) {
		win32 = true;
		win64 = false;
	} else {
		win32 = false;
		win64 = true;
	}

	sys_underdebugger = IsDebuggerPresent();

	silentabort = COM_CheckArg("-silentabort");

	//
	// internal handles init
	//
	for (i = 0; i < MAXFILEHANDLES; i++) {
		filehandles[i].hFile = INVALID_HANDLE_VALUE;
		filehandles[i].occupied = false;
	}
	for (i = 0; i < MAXSYNC_MUTEXES; i++) {
		synchandles[i].cls = sync_mutex;
		synchandles[i].h = 0;
		synchandles[i].h_external = false;
	}
	for (; i < MAXSYNC_SEMAPHORES; i++) {
		synchandles[i].cls = sync_semaphore;
		synchandles[i].h = 0;
		synchandles[i].h_external = false;
	}

	//
	// startup console if requested
	//
	if (COM_CheckArg("-stdout")) {
		hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!hStdout) {
			BY_HANDLE_FILE_INFORMATION fileinfo;

			if (!GetFileInformationByHandle(hStdOut, &fileinfo))
				hStdout = 0;
		}
		if (!hStdout) {
			CONSOLE_FONT_INFOEX ConsoleFontInfo = {0};
			
			if (AttachConsole(ATTACH_PARENT_PROCESS)) {
				DWORD unused;
				
				hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
				WriteConsole(hStdout, "\n", 1, &unused, 0);
				attachedstdout = true;
			}
			if (!hStdout && AllocConsole())
				hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			fancystdout = true;

			ConsoleFontInfo.cbSize = sizeof(ConsoleFontInfo);
			if (GetCurrentConsoleFontEx(hStdout, FALSE, &ConsoleFontInfo)) {
				if (!ConsoleFontInfo.FaceName[0]) {
					Q_strcpy(ConsoleFontInfo.FaceName, "Lucida Console");
					ConsoleFontInfo.FaceFamily = FF_DONTCARE;
					SetCurrentConsoleFontEx(hStdout, FALSE, &ConsoleFontInfo);
				}
			}
		}
	}

	//
	// COM (common object model) initialization, some APIs (like DX) require this crap
	//
	hr = CoInitializeEx(0, COINIT_MULTITHREADED);
#ifdef DEVBUILD
	if (FAILED(hr)) Sys_ConsolePrintf("Sys_Init: CoInitializeEx failed (code 0x%x)\n", GetLastError());
#endif
	if (FAILED(hr)) Sys_Error("Failed initializing COM");
	
	hr = CoInitializeSecurity(0, -1, 0, 0, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE, 0);
#ifdef DEVBUILD	
	if (FAILED(hr)) Sys_ConsolePrintf("Sys_Init: CoInitializeSecurity failed (code 0x%x)\n", GetLastError());
#endif
	if (FAILED(hr)) Sys_Error("Failed initializing COM security");

	//
	// common controls initialization, visual styles setup for modern look of program controls
	//
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_STANDARD_CLASSES;
	if (!InitCommonControlsEx(&icc)) {
#ifdef DEVBUILD		
		Sys_ConsolePrintf("Sys_Init: InitCommonControlsEx failed (code 0x%x)\n", GetLastError());
#endif
		Sys_Error("Failed initializing Common Controls");
	}

	//
	// timing stuff initialization
	//
	// it seems that the only way to make Sleep as accurate as we need is by changing OS scheduler granularity
	// by using an old mmsystem.h's timer API, which is odd, but whatever...
	//
	mr = timeGetDevCaps(&timecaps, sizeof(timecaps));
	if (mr != MMSYSERR_NOERROR) timeperiod = timecaps.wPeriodMin;
	else                        timeperiod = 1;   // assume minimum resolution of 1 ms
	mr = timeBeginPeriod(timeperiod);
	if (mr != MMSYSERR_NOCANDO) timeperiod_began = true;
	else                        timeperiod_began = false;
#ifdef DEVBUILD
	if (timeperiod_began) Sys_ConsolePrintf("Sys_Init: timeBeginPeriod(%d)\n", timeperiod);
	else                  Sys_ConsolePrintf("Sys_Init: timeBeginPeriod failed (code 0x%x)\n", mr);
#endif
	if (!QueryPerformanceFrequency(&PerformanceFreq)) Sys_Error("No hardware timer available");
	pfreq = PerformanceFreq.QuadPart;

	//
	// parsing executable name
	//
	if (!GetModuleFileName(global_hInstance, exemodule, MAXFILENAME)) {
#ifdef DEVBUILD
		Sys_ConsolePrintf("Sys_Init: GetModuleFileName failed (code 0x%x)\n", GetLastError());
#endif
		Sys_Error("Executable name unavailable");
	}
	
	Q_strcpy(sys_exefilename, COM_SkipToFileName(exemodule));
	Q_strcpy(sys_exebasename, exemodule);
	Q_strcpy(sys_exefilepath, exemodule);
	
	COM_WipeFileName(sys_exefilepath);
	COM_WipeFilePath(sys_exebasename);
	COM_WipeFileExtension(sys_exebasename);
	
	//
	// parsing system paths
	//
	hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, KF_FLAG_DEFAULT, 0, &pLocalAppData);
	if (SUCCEEDED(hr)) {
		Q_strncpy(sys_usrdatapath, pLocalAppData, MAXFILENAME);
		CoTaskMemFree(pLocalAppData);
	} else {
		char fallback[MAXFILENAME];
		
		Q_snprintf(fallback, MAXFILENAME, "%s//appdata", sys_exefilepath);
		Sys_Mkdir(fallback);

		Q_strcpy(sys_usrdatapath, fallback);
#ifdef DEVBUILD		
		Sys_ConsolePrintf("Sys_Init: SHGetKnownFolderPath failed (code 0x%x)\n", GetLastError());
#endif		
		Sys_ConsolePrintf("AppData directory location unavailable, using fallback \"%s\"", sys_usrdatapath);
		if (!COM_CheckArg("-silent"))
			Sys_WarnMsg("AppData directory not found, using fallback path");
	}

	//
	// parsing cwd
	//
	p = COM_CheckArgValue("-workpath");             // change/query program cwd
	if (p) {
		if (!SetCurrentDirectory(p))
			Sys_Error("Current directory change failed");
	}
	if (!GetCurrentDirectory(MAXFILENAME, sys_currentpath))
		Sys_Error("Sys_Init: current directory unavailable");
	hostparams.rootpath = sys_currentpath;

	//
	// preallocation of main dynamic memory
	//
	p = COM_CheckArgValue("-megs");
	if (p) {
		hostparams.memsize = Q_strtoull(p, 0, 10);
		hostparams.memsize *= (1024 * 1024);        // megs to bytes
	} else {
		MEMORYSTATUSEX memstat;
		size_t freesize = 0;

		memstat.dwLength = sizeof(memstat);
		if (GlobalMemoryStatusEx(&memstat)) {
			freesize = (size_t)((double)memstat.ullAvailPhys * ALLOC_TAKE_FROM_SYSTEM);
		} else {
#ifdef DEVBUILD			
			Sys_ConsolePrintf("Sys_Init: GlobalMemoryStatusEx failed (code 0x%x)\n", GetLastError());
#endif
			Sys_ConsolePrintf("Guessing available free memory...\n");
			
#if defined(PROCESSOR32BIT)
			if (win32) {
				size_t largesize, smallsize;
				void *largemem;

				smallsize = 2 * 1024 * 1024 * 1024;
				largesize = 3 * 1024 * 1024 * 1024; // requires /largeaddressaware linker option
				largemem = VirtualAlloc(0, (size_t)((double)largesize * ALLOC_TAKE_FROM_SYSTEM), MEM_COMMIT, PAGE_READWRITE);
				if (largemem) {freesize = largesize; VirtualFree(largemem, 0, MEM_RELEASE);}
				else          {freesize = smallsize;}
			} else {
				freesize = Sys_GuessAvailableGigs(16, 2);
			}
#elif defined(PROCESSOR64BIT)
			freesize = Sys_GuessAvailableGigs(16, 2);
#endif			
		}

		if (freesize > maxmemory)
			freesize = maxmemory;

		hostparams.memsize = freesize * ALLOC_TAKE_FROM_SYSTEM;
	}
	if (hostparams.memsize < minmemory)
		hostparams.memsize = minmemory;
	hostparams.membase = VirtualAlloc(0, hostparams.memsize, MEM_COMMIT, PAGE_READWRITE);
	if (!hostparams.membase) {
#ifdef DEVBUILD
		Sys_ConsolePrintf("Sys_Init: VirtualAlloc failed (code 0x%x)\n", GetLastError());
#endif		
		Sys_Error("Not enough memory, at least %d Mb of free RAM required", minmemory / 1024 / 1024);
	}
	Sys_ConsolePrintf("VirtualAlloc'd: %d Mb", hostparams.memsize / 1024 / 1024);
}

/*
=================
Sys_Shutdown
=================
*/
void Sys_Shutdown(void)
{
	if (hostparams.membase)
		VirtualFree(hostparams.membase, 0, MEM_RELEASE);

	if (timeperiod_began)
		timeEndPeriod(timeperiod);

	CoUninitialize();

	if (fancystdout && !attachedstdout)
		FreeConsole();
}

/*
====================================================================================================

ENTRY POINT

====================================================================================================
*/
HINSTANCE global_hInstance;
int       global_nCmdShow;

/*
=================
WinMain
=================
*/
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	if (hPrevInstance)
		Sys_Error("The program cannot run on 16-bit Windows");          // wtf?
	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;
	COM_InitSys(__argc, __argv, H_MIN_MEMORY, H_MAX_MEMORY);
	
	Host_Init(&hostparams);
	
	while (!com_quitting) {
		static MSG msg;
		
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				COM_Quit((int)msg.wParam);

			TranslateMessage(&msg);
			DispatchMessage(&msg;)
		}
		
		if (ActiveApp) {
			Host_Frame();
		} else {
			Sys_Yield();       // don't waste cpu time
		}
	}

	Host_Shutdown(false);
	
	return com_quitcode;
}
