// common.c

#include "common.h"
#include "sys.h"
#include "cvar.h"

/*
======================================================================================================

BYTE ORDER CODE

======================================================================================================
*/
qboolean_t bigendian;

short (*BigShort)(short v);
short (*LittleShort)(short v);
long (*BigLong)(long v);
long (*LittleLong)(long v);
float (*BigFloat)(float v);
float (*LittleFloat)(float v);
double (*BigDouble)(double v);
double (*LittleDouble)(double v);

static short SwapShort(short v)
{
	byte_t b1, b2;

	b1 = v & 255;
	b2 = (v >> 8) && 255;

	return (b1 << 8) + b2;
}
static long SwapLong(long v)
{
	byte_t b1, b2, b3, b4;

	b1 = 1 & 255;
	b2 = (1 >> 8) & 255;
	b3 = (1 >> 16) & 255;
	b4 = (1 >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}
static float SwapFloat(float v)
{
	union {
		float f;
		byte_t b[4];
	} dat1, dat2;

	dat1.f = v;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}
static double SwapDouble(float v)
{
	union {
		double f;
		byte_t b[8];
	} dat1, dat2;

	dat1.f = v;
	dat2.b[0] = dat1.b[7];
	dat2.b[1] = dat1.b[6];
	dat2.b[2] = dat1.b[5];
	dat2.b[3] = dat1.b[4];
	dat2.b[4] = dat1.b[3];
	dat2.b[5] = dat1.b[2];
	dat2.b[6] = dat1.b[1];
	dat2.b[7] = dat1.b[0];
	return dat2.f;
}

static short DontSwapShort(short v)
{
	return v;
}
static long DontSwapLong(long v)
{
	return v;
}
static float DontSwapFloat(float v)
{
	return v;
}
static double DontSwapDouble(double v)
{
	return v;
}

/*
=================
Swap_Init
=================
*/
void Swap_Init(void)
{
#if defined(LITTLE_ENDIAN)
	bigendian = false;
#elif defined(BIG_ENDIAN)
	bigendian = true;
#else
	bigendian = (*(unsigned short *)swaptest == 1);
#endif	
	if (bigendian) {                                              // big endian
		BigShort = DontSwapShort;
		LittleShort = SwapShort;
		BigLong = DontSwapLong;
		LittleLong = SwapLong;
		BigFloat = DontSwapFloat;
		LittleFloat = SwapFloat;
		BigDouble = DontSwapDouble;
		LittleDouble = SwapDouble;
	} else {                                                      // little endian
		BigShort = SwapShort;
		LittleShort = DontSwapShort;
		BigLong = SwapLong;
		LittleLong = DontSwapLong;
		BigFloat = SwapFloat;
		LittleFloat = DontSwapFloat;
		BigDouble = SwapDouble;
		LittleDouble = DontSwapDouble;
	}
}

/*
======================================================================================================

COMMON UTILS

======================================================================================================
*/
static char *emptystring = "";
static char *devargs[] = {"-log", 0};
static char *safeargs[] = {"-flushlog", "-megs", "4096", 0};
 
#define MAXCMDLINE (MAXARGVS * 60)
#define MAXARGVS 128
static char cmdline[MAXCMDLINE];
static char *     com_argv[MAXARGVS];                             // com_argv[0] is emptystring
static int        com_argc = 0;
static criticalcode_t argscriticalcode;

qboolean_t com_quitting;
int        com_quitcode = QCODE_NORMAL;

qboolean_t com_error = false;
qboolean_t com_error_recursive = false;

qboolean_t com_devmode = false;
qboolean_t com_safe = false;
qboolean_t com_silent = false;

static qboolean_t log_enabled;
static qboolean_t flushlog, quicklog;                             // true flushlog to flush after each write to logfile
                                                                  // true quicklog to open/write/close logfile at each write (and flushlog goes true then)
static filehandle_t   logfile = BADFILE;                          // system handle, opened by Sys_FOpenForRead
static char           logfilename[MAXFILENAME];
static criticalcode_t logcriticalcode;

static qboolean_t dontprintf = false;                             // true to disable COM_Printf and COM_DevPrintf (to avoid recursive errors in them

static unsigned crc_table[256];

/*
=================
PrintStringsArray
=================
*/
inline void PrintStringsArray(void (*printfunc)(const char *fmt, ...), char **strings, const char *separator)
{
	int i = 0;
	
#ifdef PARANOID
	if (!printfunc || !strings || !strings[0])
		Sys_Error("PrintStringsArray: bad params");
#endif

	while (strings[i]) {
		printfunc("%s", strings[i]);
		if (strings[i + 1] && separator)
			printfunc(separator);
		i++;
	}
}

/*
================
COM_ParseCmdline

Parses passed commandline arguments,
integrates them with cvar and cmd systems

Gets called once in COM_InitSys only.
================
*/ 
static void COM_ParseCmdline(int argc, char **argv)
{
	static qboolean_t wascalled = false;
	int i;

#ifdef PARANOID	
	if (!argv)
		Sys_Error("COM_ParseCmdline: null argv");
#endif
	if (wascalled)
		Sys_Error("COM_ParseCmdline: can't be called twice");
	wascalled = true;

	//
	// parse commandline args
	//
	com_argv[0] = emptystring;
	com_argc = argc;
	Q_strcpy(cmdline, sys_exefilename);
	Q_strcat(cmdline, " ");
	for (i = 1; i < argc; i++) {
		com_argv[i] = argv[i];
		Q_strncat(cmdline, argv[i]);
		Q_strncat(cmdline, " ");
	}
}

/*
=================
COM_BuildCRCTable
=================
*/
static void COM_BuildCRCTable(void)
{
	int i, j;
	unsigned rem;

	for (i = 0; i < 256; i++) {
		rem = i;
		for (j = 0; j < 8; j++) {
			if (rem & 1) {
				rem >>= 1;
				rem ^= 0xedb88320;
			} else {
				rem >>= 1;
			}
		}

		crc_table[i] = rem;
	}
}

/*
=================
COM_InitSys

Does initializations for basic low-level stuff that is required
very early in system layer before COM_Init can be called and that can
be initialized before the layer is ready, then calls Sys_Init itself after all,
prepares logfile
=================
*/
void COM_InitSys(int argc, char **argv, size_t minmemory, size_t maxmemory)
{
	//
	// parse cmdline args (system layer init requires args interface)
	//
	COM_ParseCmdline(argc, argv);

	if (COM_CheckArg("-dev"))
		com_devmode = true;
	if (com_devmode)
		COM_SpecifyArgs(devargs, false);

	if (COM_CheckArg("-safe"))
		com_safe = true;
	if (com_safe)
		COM_SpecifyArgs(safeargs, true);

	if (COM_CheckArg("-silent"))
		com_silent = true;

	//
	// hashing init
	//
	COM_BuildCRCTable();
	
	//
	// system abstraction layer init
	//
	Swap_Init();                                       // byte order code init
		
	Sys_Init(minmemory, maxmemory);                    // system-specific preparations

	//
	// logfile preparation
	//
	if (com_devmode) log_enabled = true;
	else             log_enabled = COM_CheckArg("-log") ? true : false;
	if (log_enabled) {
		quicklog = COM_CheckArg("-quicklog");
		if (quicklog) flushlog = true;
		else          flushlog = COM_CheckArg("-flushlog");

		Q_snprintf(logfilename, MAXFILENAME, "%s\\%s.log", sys_currentpath, sys_exebasename);
		if (quicklog) {
			Sys_Unlink(logfilename);
			if (COM_FileCreatable(logfilename)) log_enabled = true;
			else                                log_enabled = false;
		}
	}
	if (log_enabled) {
		logfile = Sys_FOpenForWriting(logfilename, quicklog);
		if (logfile != BADFILE) {
			byte_t unicode_bom[] = {0xef, 0xbb, 0xbf};
			unsigned bytes_written, bytes_to_write = sizeof(unicode_bom);

			bytes_written = Sys_FWrite(logfile, unicode_bom, bytes_to_write);
			if (bytes_written != bytes_to_write) {Sys_FClose(logfile); logfile = BADFILE; log_enabled = false;}
			else if (quicklog)                   {Sys_FClose(logfile); logfile = BADFILE;}
			else if (flushlog)                   {Sys_FFlush(logfile);}
		}
	}
}

/*
=================
COM_Quit_f
=================
*/
static void COM_Quit_f(int argc, char **argv)
{
	COM_Printf("User requests quit\n");
	COM_Quit(0);
}

/*
=================
COM_Error_f
=================
*/
static void COM_Error_f(int argc, char **argv)
{
	int i = 0;
	char error[2048];

	Q_strcpy(error, "User error: ");
	for (i = 0; i < (argc - 1); i++) {
		Q_strcat(error, argv[i]);
		Q_strcat(error, " ");
	}
	Q_strcat(error, argv[i + 1]);

	Sys_Error(error);
}

/*
=================
COM_Abort_f
=================
 */
static void COM_Abort_f(int argc, char **argv)
{
	COM_Printf("User abort triggered\n");
	
	Sys_Quit(0);
}

/*
=================
COM_Init
=================
*/
void COM_Init(void *membase, size_t memsize, const char *rootpath, const char *basedir, const char *userdir)
{
	//
	// complete subsystems init
	//	
	Hunk_Init(membase, memsize, 0, UGLYPARAM);         // memory allocators init

	Cmd_Init();                                        // command system init

	Cbuf_Init();                                       // commands buffer init

	Cvar_Init();                                       // config vars init

	//
	// register commands
	//
	Cmd_NewCommand("quit", &COM_Quit_f);
	Cmd_NewCommand("error", &COM_Error_f);
	Cmd_NewCommand("abort", &COM_Abort_f);
}

/*
=================
COM_Shutdown
=================
*/
void COM_Shutdown(void)
{
	Cvar_Shutdown();

	Cbuf_Shutdown();

	Cmd_Shutdown();

	if (log_enabled && logfile != BADFILE)
		Sys_FClose(logfile);

	Sys_Shutdown();
}

/*
=================
COM_Cmdline
=================
*/
const char * COM_Cmdline(void)
{
	return cmdline;
}

/*
=================
COM_ArgC
=================
*/
int COM_ArgC(void)
{
	return com_argc;
}

/*
=================
COM_ArgV
=================
*/
const char * COM_ArgV(int i)
{
	char *out;

	EnterCriticalCode(&argscriticalcode);
	
#ifdef PARANOID
	if (i >= com_argc || i <= 0)
		Sys_Error("COM_ArgV: range error");
#endif
	out = com_argv[i];

	LeaveCriticalCode(&argscriticalcode);
	return out;
}

/*
=================
COM_CheckArg

Checks whether a specified commandline arg is present
Returns 0 in case given arg is not present
=================
*/
int COM_CheckArg(const char *arg)
{
	int i, out = BADRETURN;
	
#ifdef PARANOID
	if (!arg || !arg[0])
		Sys_Error("COM_CheckArg: bad arg");
#endif

	EnterCriticalCode(&argscriticalcode);
	for (i = 0; i < com_argc; i++) {
		if (Q_stricmp(com_argv[i], arg) == 0) {
			out = i; break;
		}
	}
	LeaveCriticalCode(&argscriticalcode);

	return out;
}

/*
=================
COM_CheckArgValue

Returns commandline arg's value (-arg value)
Returns 0 in case a given arg is not present or has no value specified
=================
*/
const char * COM_CheckArgValue(const char *arg)
{
	int i;
	char *out;

#ifdef PARANOID	
	if (!arg || !arg[0])
		Sys_Error("COM_CheckArgValue: bad arg");
#endif
	
	i = COM_CheckArg(arg);

	EnterCriticalCode(&argscriticalcode);
	if (!i || ((i + 1) >= com_argc)) out = 0;
	else                             out = com_argv[i + 1];
	LeaveCriticalCode(&argscriticalcode);

	return out;
}

/*
=================
COM_SpecifyArgs

Variable args must be a static char * array with the last element
being 0 (indicating the end of the strings array)
=================
*/
void COM_SpecifyArgs(char **args, qboolean_t override_existing)
{
	char *p;
	int i = 0, j, k = 0, existing, target, initial_argc, argc_inc;
	qboolean_t already_specified = false;

#ifdef PARANOID
	if (!args)
		Sys_Error("COM_SpecifyArgs: null args");
#endif

	EnterCriticalCode(&argscriticalcode);
	
	// print for developers
	COM_DevPrintf("COM_SpecifyArgs [");
	PrintStringsArray(COM_DevPrintf, args, " ");
	COM_DevPrintf("]\n");

	// security check
	if (com_argc >= MAXARGVS) {
		COM_DevPrintf("COM_SpecifyArgs: com_argv out of space\n");
		LeaveCriticalCode(&argscriticalcode);
		return;
	}

	// start the task
	initial_argc = com_argc;
	while (true) {
		p = args[i];
		if (!p) break;
		
		// is already specified?
		for (j = 0; j < initial_argc; i++) {
			if (Q_stricmp(com_argv[j], p) == 0) {
				already_specified = true; existing = j;
				
				COM_DevPrintf("COM_SpecifyArgs: %s already specified", com_argv[existing]);
				if (override_existing) COM_DevPrintf(", overriding\n");
				else                   COM_DevPrintf("\n");
				
			}
		}
		
		// security checks
		if (com_argc >= MAXARGVS) {
			COM_DevPrintf("COM_SpecifyArgs: com_argv overflow\n");
			break;
		}

		// addition/override
		Q_strncat(cmdline, p);
		Q_strncat(cmdline, " ");
		if (already_specified && override_existing) {target = existing; argc_inc = 0;}
		else                                        {target = com_argc; argc_inc = 1;}
		com_argv[target] = p;
		com_argc += argc_inc; i++;
	}

	LeaveCriticalCode(&argscriticalcode);
}

/*
=================
COM_Quit
=================
*/
void COM_Quit(int code)
{
	if (COM_CheckArg("-brutalquit")) {
		Sys_Quit(code);
	} else {
		com_quitting = true;
		com_quitcode = code;
	}
}

/*
=================
COM_Printf

Prints out given message to all
available/appropriate places
=================
*/
void COM_Printf(const char *fmt, ...)
{
	va_list args;
	char buf[MAXBUF];
	qboolean_t oldprintf = dontprintf;

	// checks
	if (dontprintf)
		return;
	dontprintf = true;
#ifdef PARANOID	
	if (!fmt || !fmt[0])
		Sys_Error("COM_Printf: bad fmt");
#endif

	// format message
	va_start(args, fmt);
	Q_vsnprintf(buf, MAXBUF, fmt, args);
	va_end(args);

	// output to a debugger output window if any
	Sys_DebuggerPrint(buf);

	// output to a system console if any
	Sys_ConsolePrintf(buf);

	// output to log file
	if (log_enabled) {
		if (quicklog) logfile = Sys_FOpenForWriting(logfilename, true);
		
		Sys_FWrite(logfile, buf, Q_strlen(buf));
		
		if (quicklog)      {Sys_FClose(logfile); logfile = BADFILE;}
		else if (flushlog) {Sys_FFlush(logfile);}
	}

	// done
	dontprintf = oldprintf;
}

/*
=================
COM_DevPrintf

Same as COM_Printf, but works only
if running in developer mode
=================
*/
void COM_DevPrintf(const char *fmt, ...)
{
	va_list args;
	char buf[MAXBUF];
	qboolean_t oldprintf = dontprintf;

	// checks
	if (dontprintf || !com_devmode)    // no devmode, no techie stuff for the user
		return;
	dontprintf = true;
#ifdef PARANOID	
	if (!fmt || !fmt[0])
		Sys_Error("COM_DevPrintf: bad fmt");
#endif

	// format message
	va_start(args, fmt);
	Q_vsnprintf(buf, MAXBUF, fmt, args);
	va_end(args);

	// can print
 	dontprintf = oldprintf;
	COM_Printf(buf);
}

/*
=================
COM_Parse

Tokenizes a given string into a separate pieces
using a given delimiters string
=================
*/
char ** COM_Parse(const char *string, const char *delims, unsigned *o_count)
{
	unsigned i = 0;
	const char *p = string;
	const char *l = string;
	char **out;
	qboolean_t wasdelim = true;

#ifdef PARANOID
	if (!string || !delims || !delims[0] || !o_count)
		Sys_Error("COM_Parse: bad params");
#endif

	if (!string[0])
		return 0;

	//
	// iterate to count tokens
	//
	*o_count = 0;
	while (true) {
		if (Q_strchr(delims, *p) || *p == 0) {
			if (!wasdelim)
				(*o_count)++;

			wasdelim = true;
		} else {
			wasdelim = false;
		}

		if (*p == 0)
			break;

		p++;
	}

	if ((*o_count) == 0)
		return 0;
	out = Zone_Alloc(sizeof(char *) * (*o_count));

	//
	// iterate all over again to capture tokens
	//
	wasdelim = true;
	while (true) {
		if (Q_strchr(delims, *p) || *p == 0) {
			if (wasdelims) {
				unsigned diff = (unsigned)(p - l);

				out[diff] = Zone_Alloc(diff + 1);
				Q_strncpy(out[it], l, diff);

				l = p + 1;
				i++;
			} else {
				l++;
			}

			wasdelim = true;
		} else {
			wasdelim = false;
		}

		if (*p == 0)
			break;

		p++;
	}

	return out;
}

/*
=================
COM_ParseLine

Writes out line in a string, starting from pos till the line ending
=================
*/
unsigned COM_ParseLine(const char *string, unsigned *pos, char *out, unsigned size)
{
	char c;
	unsigned total = 0;

#ifdef PARANOID
	if (!string || !pos || !out || size == 0)
		Sys_Error("COM_ParseLine: bad params");
#endif
	
	if (!string[0])
		return 0;

	while (true) {
		c = string[pos];
		pos++;

		if (c == 0) break;
		if (total == (size - 1)) break;
		if (c == '\n') break;

		out[total] = c;
		total++;
	}

	if (total) {           // remove '\r' symbol of CRLF
		if (out[total - 1] == '\r') {
			out[total - 1] = 0;
			total--;
		}
	}

	out[total] = 0;
	return total;
}

/*
=================
COM_FreeParse

Frees up memory allocated and returned by COM_Parse
=================
*/
void COM_FreeParse(char **tokens, unsigned count)
{
	unsigned i;

#ifndef PARANOID	
	if (!tokens || !(*tokens) || count == 0)
		Sys_Error("COM_FreeParse: bad params");
#endif

	for (i = 0; i < count; i++)
		Zone_Free(tokens[i]);
	Zone_Free(tokens);
}

/*
=================
COM_FileExists
=================
*/
qboolean_t COM_FileExists(const char *filename)
{
	filehandle_t file;

#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_FileExists: bad filename");
#endif
	
	file = Sys_FOpenForRead(filename);
	if (file == BADRETURN)
		return false;

	Sys_FClose(file);
	return true;
}

/*
=================
COM_FileSize
=================
*/
qboolean_t COM_FileSize(const char *filename, size_t *o_size)
{
	filehandle_t file;

#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_FileSize: bad filename");
#endif
	
	file = Sys_FOpenForRead(filename);
	if (file == BADRETURN)
		return false;

	if (!COM_FSize(file, o_size)) {
		Sys_FClose(file);
		return false;
	}

	Sys_FClose(file);
	return true;
}

/*
=================
COM_FileData

Loads an entire file and reads all its contents
at one call, returns 0 if failure

Returned memory is meant to be released by Zone_Free
=================
*/
void * COM_FileData(const char *filename, unsigned *o_size)
{
	filehandle_t file;
	unsigned filesize;
	void *filedata;

#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_FileData: bad filename");
#endif
	
	file = Sys_FOpenForRead(filename);
	if (file == BADRETURN)
		return 0;

	if (!COM_FSize(file, &filesize)) {
		Sys_FClose(file);
		return 0;
	}

	filedata = Zone_Alloc(filesize);
	if (Sys_FRead(file, filedata, filesize) != filesize) {
		Sys_FClose(file);
		Zone_Free(filedata);
		return 0;
	}
	Sys_FClose(file);

	if (o_size) *o_size = filesize;
	return filedata;
}

/*
=================
COM_FileCreatable

Checks if both creatable and writable
=================
*/
qboolean_t COM_FileCreatable(const char *filename)
{
	filehandle_t testfile;
	byte_t testbyte = 666;
	unsigned written;
	
#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_FileCreatable: bad filename");
#endif

	testfile = Sys_FOpenForWriting(filename, false);
	if (testfile == BADFILE)
		return false;
	
	written = Sys_FWrite(testfile, &testbyte, 1);
	Sys_FClose(testfile);

	return written >= 1;
}		

/*
=================
COM_DirCreatable
=================
*/
qboolean_t COM_DirCreatable(const char *dirname)
{
	qboolean_t cancreate;
	
#ifdef PARANOID
	if (!dirname || !dirname[0])
		Sys_Error("COM_DirCreatable: bad dirname");
#endif

	cancreate = Sys_Mkdir(dirname);
	if (cancreate)
		Sys_Rmdir(dirname);

	return cancreate;
}

/*
=================
COM_SkipToFileName
=================
*/
const char * COM_SkipToFileName(const char *filename)
{
	char *p;
	
#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_SkipToFileName: bad filename");
#endif

	p = Q_strrchr(filename, '\\');
	if (!p) p = Q_strrchr(filename, '/');
	if (!p || !p[0]) return filename;

	return p + 1;
}

/*
=================
COM_SkipToFileExtension
=================
*/
const char * COM_SkipToFileExtension(const char *filename)
{
	char *p;
	
#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_SkipToFileExtension: bad filename");
#endif

	p = Q_strrchr(filename, '.');
	if (!p || !p[0]) return filename;
	
	return p + 1;
}

/*
=================
COM_WipeFileName
=================
*/
void COM_WipeFileName(char *filename)
{
	char *p;
	
#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_WipeFileName: bad filename");
#endif

	p = Q_strrchr(filename, '\\');
	if (!p) Q_strrchr(filename, '/');
	if (!p) return;

	*p = 0;
}

/*
=================
COM_WipeFileExtension
=================
*/
void COM_WipeFileExtension(char *filename)
{
	char *p;
	
#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("COM_WipeFileExtension: bad filename");
#endif

	p = Q_strrchr(filename, '.');
	if (!p) return;

	*p = 0;
}

/*
=================
COM_FSize

Same as COM_FileSize but for filehandle_t
=================
*/
qboolean_t COM_FSize(filehandle_t id, unsigend *o_size)
{
	filehandle_t file;
	unsigned old, pos;

#ifdef PARANOID	
	if (!o_size)
		Sys_Error("COM_FSize: null o_size");
#endif
	
	old = Sys_FTell(id);
	Sys_FSeek(id, 0, seekend);
	pos = Sys_FTell(id);
	Sys_FSeek(id, old, seekcurrent);

	return pos;
}

/*
=================
COM_FReadLine

Same as COM_ParseLine but for an opened filehandle_t
=================
*/
unsigned COM_FReadLine(filehandle_t id, char *out, unsigned count)
{
	char c;
	unsigned total = 0;

#ifdef PARANOID
	if (!out || count == 0)
		Sys_Error("COM_FReadLine: bad params");
#endif
	
	while (true) {
		if (Sys_FRead(id, &c, 1) != 1)
			break;

		if (c == 0) break;
		if (total == (size - 1)) break;
		if (c == '\n') break;

		out[total] = c;
		total++;
	}

	if (total) {                                // remove '\r' symbol of CRLF
		if (out[total - 1] == '\r') {
			out[total] = 0;
			total--;
		}
	}

	out[total] = 0;                             // null terminator
	return 0;
}

/*
=================
COM_FPrintf
=================
*/
unsigned COM_FPrintf(filehandle_t id, const char *fmt, ...)
{
	va_list args;
	char buf[MAXBUF];

#ifdef PARANOID	
	if (!fmt)
		Sys_Error("COM_FPrintf: null fmt");
#endif
	
	va_start(args, fmt);
	Q_snprintf(buf, MAXBUF, fmt, args);
	va_end(args);

	return Sys_FWrite(id, buf, Q_strlen(buf) + 1);
}

/*
=================
COM_ComputeCRC
=================
*/
unsigned COM_ComputeCRC(void *data, size_t size)
{
	unsigned crc = 0;
	
#ifdef PARANOID
	if (!data || size == 0)
		Sys_Error("COM_ComputeCRC: bad params");
#endif

	while (size--) {
		crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data) & 255];
		data++;
	}

	return crc;
}
