// sys.h -- various system unportables

#ifndef SYS_H
#define SYS_H

#include "common.h"

void Sys_Init(size_t minmemory, size_t maxmemory);
void Sys_Shutdown(void);

extern qboolean_t sys_underdebugger;                                           // true if the debugger is attached to the running process
void Sys_DebuggerPrint(const char *string);

void Sys_ConsolePrintf(const char *fmt, ...);
void Sys_ConsoleScanf(char *fmt, ...);

void Sys_Msg(const char *fmt, ...);
void Sys_WarnMsg(const char *fmt, ...);

void Sys_Error(const char *fmt, ...);
void Sys_Quit(int code);                                                       // end program execution immeditaly

void Sys_HeapCheck(void);                                                      // pretty slow sometimes, not to be used in a final build

// timing
typedef struct {
	qw_t lastcounts;
	qw_t lastclocks;
	
	qw_t  clks_elapsed;
	float secs_elapsed;
	float exec_per_sec;
} benchmark_t;
void Sys_BeginBenchmark(benchmark_t *benchmark);
void Sys_Benchmark(benchmark_t *benchmark, qboolean_t rewrite);

qboolean_t Sys_Sleep(unsigned msecs);
void       Sys_SleepForever(void);
void Sys_Yield(void);

void Sys_ObtainDate(short *year, short *month, short *day, short *dayofweek);
void Sys_ObtainTime(short *hour, short *minute, short *second);

// file IO
#define BADFILE BADHANDLE
typedef int filehandle_t;
typedef enum {
    seekbegin,
	seekcurrent,
	seekend
} seekorigin_t;
filehandle_t Sys_FOpenForReading(const char *filename);                        // returns BADFILE in case of error
filehandle_t Sys_FOpenForWriting(const char *filename, qboolean_t append);     // returns BADFILE in case of error
void       Sys_FClose(filehandle_t id);

qboolean_t Sys_FLock(filehandle_t id, size_t offset, size_t count);
qboolean_t Sys_FUnlock(filehandle_t id, size_t offset, size_t count);
unsigned   Sys_FRead(filehandle_t id, void *buf, unsigned count);
unsigned   Sys_FWrite(filehandle_t id, void *buf, unsigned count);
qboolean_t Sys_FSeek(filehandle_t id, size_t count, seekorigin_t origin, size_t *out);
size_t     Sys_FTell(filehandle_t id);
void       Sys_FFlush(filehandle_t id);

// filesystem
qboolean_t Sys_Mkdir(const char *dirname);
qboolean_t Sys_Rmdir(const char *dirname);

qboolean_t Sys_Unlink(const char *filename);

#define MAXFILENAME 1024
extern char sys_exebasename[MAXFILENAME];
extern char sys_exefilename[MAXFILENAME];
extern char sys_exefilepath[MAXFILENAME];
extern char sys_currentpath[MAXFILENAME];
extern char sys_usrdatapath[MAXFILENAME];         // where all the user-specific data is placed by the system

// generic processor interface
typedef enum {
	processor_unknown = 0,
    processor_intel,
	processor_amd
} processortype_t;
extern processortype_t sys_processortype;
extern float           sys_processorfrequency;

// x86/x87 processor control
#ifdef X86COMPAT
void        x86_Identify(int func, int *o_eax, int *o_ebx, int *o_ecx, int *o_edx);   // "cpuid" instruction wrapper
#ifdef X86ASM
qw_t        x86_ReadTSCounter(void);                                                  // "rdtsc" instruction wrapper (asm version)
#else
inline qw_t x86_ReadTSCounter(void) {return __rdtsc();}                               // "rdtsc" instruction wrapper (inline intrinsic)
#endif
inline void x86_Serialize(void) {x86_Identify(0, 0, 0, 0, 0);}

void x87_InterruptExceptions(void);                                                   // interrupt fp exceptions control
void x87_IgnoreExceptions(void);
void x87_LowPrecision(void);                                                          // fp precision control: low is 24-bit, high is 53-bit, 64-bit one is useless
void x87_HighPrecision(void);

void x87_PushCW_LowPrecision(void);                                                   // fp control word stack
void x87_PushCW_HighPrecision(void);
void x87_PopCW(void);

typedef struct {
	char vendor[13];                          // vendor name string (f.e. "GenuineIntel" or "AuthenticAMD")
	char brand[49];                           // brand name string
	qboolean_t supports_cpuid;                // if false then any other data in the structure is not valid / present
	qboolean_t supports_rdtsc;
	qboolean_t supports_mmx;
	qboolean_t supports_mmx_ext;              // amd-specific feature
	qboolean_t supports_sse;
	qboolean_t supports_sse2;
	qboolean_t supports_sse3;
	qboolean_t supports_ssse3;
	qboolean_t supports_sse4_1;
	qboolean_t supports_sse4_2;
	qboolean_t supports_sse4a;                // amd-specific feature
	qboolean_t supports_ht;                   // processor has hyper-threading technology

	unsigned numcores, numthreads;
	unsigned numL1, numL2, numL3;             // L caches count
	unsigned numNUMA;                         // count of NUMA nodes
} x86stats_t;
extern x86stats_t x86_processor;
#endif // #ifdef X86COMPAT

// thread sync
#define BADSYNC BADHANDLE
typedef int synchandle_t;
typedef enum {
    sync_mutex,
	sync_semaphore
} syncclass_t;
synchandle_t Sys_SyncNewMutex(qboolean_t initstate, const char *globalname);             // returns BADSYNC in case of error
synchandle_t Sys_SyncNewSemaphore(int initcount, int maxcount, const char *globalname);  // returns BADSYNC in case of error
synchandle_t Sys_SyncAccess(syncclass_t cls, const char *globalname);                    // returns BADSYNC in case of error
void       Sys_SyncLoose(synchandle_t id);

void       Sys_SyncLock(synchandle_t id);
void       Sys_SyncUnlock(synchandle_t id);
void       Sys_SyncWait(synchandle_t id);
qboolean_t Sys_SyncWaitTime(synchandle_t id, unsigned msecs);                            // returns false if timeout

#endif // #ifndef SYS_H

