// cmd.h

#ifndef CMD_H
#define CMD_H

#include "common.h"

/*
============================================================================================

COMMANDS SYSTEM

============================================================================================
*/

// command execution call source
typedef enum {
  commandsource_code = 0,                           // executed directly from native code
  commandsource_console,                            // executed from developer console
  commandsource_script                              // executed from a script file
} commandsource_t;
typedef void (*commandsource_printf)(const char *fmt, ...);
typedef void (*commandsource_devprintf)(const char *fmt, ...);

// command execution context
typedef struct {
	int    argc;
	char **argv;
	
	commandsource_t source;                         // where has been called from
	commandsource_printf printf;
	commandsource_devprintf devprintf;
} commandcontext_t;
typedef void (*commandfunc_t)(commandcontext_t *ctx);

void Cmd_Init(void);
void Cmd_Shutdown(void);

void Cmd_Check(void);

void Cmd_NewCommand(const char *name, commandfunc_t func);
void Cmd_ForgetCommand(const char *name);
void Cmd_ForgetAllCommands(void);

void Cmd_Execute(const char *command);
void Cmd_ExecuteScript(const char *filename);

/*
============================================================================================

COMMAND BUFFER

gets flushed (executed) each frame
============================================================================================
*/
void Cbuf_Init(void);
void Cbuf_Shutdown(void);

void Cbuf_Clear(void);
void Cbuf_SetString(const char *string);
void Cbuf_AppendString(const char *string);

void Cbuf_Execute(void);

#endif // #ifdef CMD_H
