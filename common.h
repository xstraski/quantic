// common.h -- common defs shared everywhere

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef DEVBUILD
#ifndef PARANOID
#define PARANOID
#endif
#endif

#if defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64)
#define X86COMPAT
#if defined(_M_X64) || defined(_M_AMD64)
#define AMD64
#define PROCESSOR64BIT
#else
#define PROCESSOR32BIT
#endif
#endif

#if defined(X86COMPAT) && defined(PROCESSOR32BIT)
#define X86ASM
#endif

#if defined(DEVBUILD)
#define BUILDSTRING "DevBuild"
#elif defined(PARANOID)
#define BUILDSTRING "Paranoid"
#else
#define BUILDSTRING "Retail"
#endif

#if defined(WINDOWS)
#define SYSTEMSTRING "Windows"
#endif

#if defined(X86COMPAT)
#if defined(PROCESSOR32BIT)
#define PROCESSORSTRING "x86"
#elif defined(PROCESSOR64BIT)
#define PROCESSORSTRING "x86_64"
#endif
#endif

#if defined(LITTLE_ENDIAN)
#define ENDIANSTRING "LE"
#elif defined(BIG_ENDIAN)
#define ENDIANSTRING "BE"
#else
#define ENDIANSTRING ""
#endif

typedef unsigned char byte_t;
#if defined(_MSC_VER)
typedef unsigned __int64 qw_t;
typedef signed __int64 qwsigned_t;
#else
typedef unsigned long long qw_t;
typedef signed long long qwsigned_t;
#endif
typedef enum {false = 0, true} qboolean_t;

#define BADRETURN ((int)-1)                              // errcode for funcs returning signed int
#define BADPARAM BADRETURN
#define BADHANDLE BADRETURN

#define UGLYRETURN ((unsigned)0xffffffff)                // same as above but unsigned
#define UGLYPARAM UGLYRETURN
#define UGLYHANDLE UGLYRETURN

#define MAXBUF 2048                                      // size of generic string buffer

#define XSTRING2(x) #x
#define XSTRING(str) XSTRING2(str)

/*
============================================================================================

CRT VARIOUS REPLACEMENTS AND ADDITIONS

Provided for portability reasons, might be better suited and/or better optimized
for target platforms.

May be just dummy definitions redirecting to standard CRT functions,
because some CRT implementations execute unexpectedly pretty fast, or because they may
be implemented in a future.

============================================================================================
*/
#define Q_memcpy memcpy
#define Q_memset memset

#define Q_toupper toupper
#define Q_tolower tolower

#define Q_strcpy strcpy
#define Q_strncpy strncpy
#define Q_strcat strcat
#define Q_strncat strncat
#define Q_strlen strlen
#define Q_strcmp strcmp
#define Q_strncmp strncmp
#ifdef _MSC_VER
#define Q_stricmp stricmp
#define Q_strnicmp strnicmp
#endif
#define Q_strcasecmp Q_stricmp
#define Q_strncasecmp Q_strnicmp
#define Q_strstr strstr
#define Q_strchr strchr
#define Q_strrchr strrchr
#define Q_strupr strupr
#define Q_strlwr strlwr
#define Q_strtol strtol
#define Q_strtoll strtoll
#define Q_strtoul strtoul
#define Q_strtoull strtoull
#define Q_strtof Q_atof
#define Q_atof atof
#define Q_sprintf sprintf
#define Q_snprintf snprintf
#define Q_vsprintf vsprintf
#define Q_vsnprintf vsnprintf

/*
============================================================================================

COMMON UTILITIES PRIMARY INTERFACE

============================================================================================
*/
void COM_InitSys(int argc, char **argv, size_t minmemory, size_t maxmemory); // system layer early required common subsystems init
void COM_Init(void *membase, size_t memsize, const char *rootpath, const char *basedir, const char *userdir);
void COM_Shutdown(void);

const char * COM_Cmdline(void);
int          COM_ArgC(void);
const char * COM_ArgV(int i);                                                // arg 0 is an empty string
int          COM_CheckArg(const char *arg);
const char * COM_CheckArgValue(const char *arg);
void         COM_SpecifyArgs(char **args, qboolean_t override_existing);     // args is a char ptrs array, must be static and the last element must be 0

#define QCODE_NORMAL    0
#define QCODE_ERROR     666
#define QCODE_RECURSIVE 667                                                  // an error that causes itself recursively when processing it (f.e. by Sys_Error)
void COM_Quit(int code);
extern qboolean_t com_quitting;
extern int        com_quitcode;

void COM_Printf(const char *fmt, ...);
void COM_DevPrintf(const char *fmt, ...);                                    // prints out only if com_devmode is true

// these should be read only after common initialization
extern qboolean_t com_devmode;                                               // true if running in developer mode: features for developers-only are enabled
extern qboolean_t com_safe;                                                  // true if running in safe mode: safe config loaded to avoid maximum potential errors
extern qboolean_t com_silent;                                                // true if running in silent mode: no messages interrupting a user

// these are for Sys_Error (and such reporters)
extern qboolean_t com_error;                                                 // true if an error reporter being called
extern qboolean_t com_error_recursive;                                       // true with com_error if an error is recursive

char **  COM_Parse(const char *string, const char *delims, unsigned *o_count);
unsigned COM_ParseLine(const char *string, unsigned *pos, char *out, unsigned size);
void     COM_FreeParse(char **tokens, unsigned count);                       // frees tokens got by COM_Parse

// file utils
qboolean_t COM_FileExists(const char *filename);
qboolean_t COM_FileSize(const char *filename, size_t *o_size);
void *     COM_FileData(const char *filename, unsigned *o_size);             // returned memory can be released by Zone_Free

qboolean_t COM_FileCreatable(const char *filename);                          // tests whether a specified file can be created
qboolean_t COM_DirCreatable(const char *dirname);                            // same as above but for a directory

const char * COM_SkipToFileName(const char *filename);                       // returns pointing past last slash (dos or unix style) in filename
const char * COM_SkipToFileExtension(const char *filename);                  // returns pointing past last period in filename
void COM_WipeFileName(char *filename);
void COM_WipeFileExtension(char *filename);

// filehandle_t based utils
qboolean_t COM_FSize(filehandle_t id, unsigend *o_size);
unsigned   COM_FReadLine(filehandle_t id, char *out, unsigned count);
unsigned   COM_FPrintf(filehandle_t id, const char *fmt, ...);

/*
============================================================================================

BYTE ORDER AND BIT FIELDS

============================================================================================
*/
void Swap_Init(void);                                                // funcptrs below are null before Swap_Init call
extern short (*BigShort)(short v);
extern short (*LittleShort)(short v);
extern long (*BigLong)(long v);
extern long (*LittleLong)(long v);
extern float (*BigFloat)(float v);
extern float (*LittleFloat)(float v);
extern double (*BigDouble)(double v);
extern double (*LittleDouble)(double v);

extern qboolean_t bigendian;                                         // false if little endian

// counts set bits in a bit mask
inline unsigned CountSetBits(unsigned mask)
{
	unsigned leftshift = sizeof(unsigned) * 8 - 1;
	unsigned i, count = 0, test = ((unsigned)1 << leftshift);

	for (i = 0; i <= leftshift; i++) {
		count += ((mask & test) ? 1 : 0);
		test /= 2;
	}

	return count;
}

inline int BitscanForward(unsigned value)      // returns BADRETURN if not found
{
#ifdef _MSC_VER
	int out;
	
	if (!_BitScanForward((unsigned long *)&out, value))
		out = BADRETURN;
	return out;
#else
	int out = BADRETURN, test;
	
	for (test = 0; test < 32; test++) {
		if (value & (1 << test)) {
			out = test; break;
		}
	}

	return out;
#endif
}
inline int BitscanBackward(size_t value)       // returns BADRETURN if not found
{
#ifdef _MSC_VER
	int out;
	
	if (!_BitScanReverse((unsigned long *)&out, value))
		out = BADRETURN;
	return out;
#else
	int out = BADRETURN, test;
	
	for (test = 31; test >= 0; test--) {
		if (value & (1 << test)) {
			out = test; break;
		}
	}
	
	return out;
#endif
}

/*
============================================================================================

THREAD SYNCHRONIZATION

============================================================================================
*/
void Sys_Error(const char *fmt, ...);          // from sys.h, needed by some code below

//
// Interlocked (thread-safe) atomic operations
// 
#ifdef _MSC_VER
inline int AtomicIncrement32(volatile int *v)
{return _InterlockedIncrement((volatile long *)v);}
inline qwsigned_t AtomicIncrement64(volatile qwsigned_t *v)
{return _InterlockedIncrement64((volatile __int64 *)v);}
inline int AtomicDecrement32(volatile int *v)
{return _InterlockedDecrement((volatile long *)v);}
inline qwsigned_t AtomicDecrement64(volatile qwsigned_t *v)
{return _InterlockedDecrement64((volatile __int64 *)v);}
inline int AtomicExchange32(volatile int *v, int val)
{return _InterlockedExchange((volatile long *)v, val);}
inline qwsigned_t AtomicExchange64(volatile qwsigned_t *v, qwsigned_t val)
{return _InterlockedExchange64((volatile __int64 *)v, val);}
inline int AtomicCompareExchange32(volatile int *v, int val, int exp)
{return _InterlockedCompareExchange((volatile long *)v, val, exp);}
inline qwsigned_t AtomicCompareExchange64(volatile qwsigned_t *v, qwsigned_t val, qwsigned_t exp)
{return _InterlockedCompareExchange((volatile __int64 *)v, val, exp);}
#endif

//
// Memory barriers: completes all writes/reads before future writes/reads for thread-safety
//
#ifdef _MSC_VER
inline void MemoryBarrierForRead(void) {_WriteBarrier(); _mm_sfence();}
inline void MemoryBarrierForWrite(void) {_ReadBarrier(); _mm_lfence();}
#endif
inline void MemoryBarrier(void)
{
	MemoryBarrierForRead();
	MemoryBarrierForWrite();
}

//
// Processor execution control
//
#ifdef _MSC_VER
inline void YieldProcessor(void) {_mm_pause();}
#endif		

//
// Critical Code: basic thread sync mechanism - ticket mutex.
//
// Beware that the structure must be zero-initialized (by hand or using ZeroCriticalSection) before calling
// locking/unlocking functions for proper execution.
//
typedef struct {
	volatile qw_t tickets;
	volatile qw_t serving;
} criticalcode_t;
inline void ZeroCriticalCode(criticalcode_t *criticalcode)
{
#ifdef PARANOID	
	if (!criticalcode)
		Sys_Error("ZeroCriticalCode: null criticalcode");
#endif	
	criticalcode->tickets = 0;
	criticalcode->serving = 0;
}
inline void EnterCriticalCode(criticalcode_t *criticalcode)
{
#ifdef PARANOID	
	if (!criticalcode)
		Sys_Error("EnterCriticalCode: null criticalcode");
#endif	
	AtomicIncrement64(&criticalcode->tickets);
	while (criticalcode->tickets - 1 != criticalcode->serving)
		YieldProcessor();
}
inline qboolean_t TryEnterCriticalCode(criticalcode_t *criticalcode)
{
#ifdef PARANOID
	if (!criticalcode)
		Sys_Error("TryEnterCriticalCode: null criticalcode");
#endif	
	if (criticalcode->tickets != criticalcode->serving)
		return false;
	AtomicIncrement64(&criticalcode->tickets);
	return true;
}
inline void LeaveCriticalCode(criticalcode_t *criticalcode)
{
#ifdef PARANOID	
	if (!criticalcode)
		Sys_Error("LeaveCriticalCode: null criticalcode");
#endif	
	AtomicIncrement64(&criticalcode->serving);
}

#endif // #ifndef COMMON_H
