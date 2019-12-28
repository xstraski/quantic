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
    cmdsource_code = 0,                           // executed directly from native code
    cmdsource_console,                            // executed from developer console
    cmdsource_script                              // executed from a script file
} cmdsource_t;

// command execution context
typedef struct {
	unsigned argc;
	char **  argv;
	
	cmdsource_t source;                           // where has been called from
	printf_t printf;
	printf_t devprintf;
} cmdcontext_t;
typedef void (*cmdfunc_t)(cmdcontext_t *ctx);

void Cmd_Init(void);
void Cmd_Shutdown(void);

void Cmd_Check(void);

void Cmd_NewCommand(const char *name, cmdfunc_t func);
void Cmd_ForgetCommand(const char *name);
void Cmd_ForgetAllCommands(void);

void Cmd_ExecuteCommand(const char *command);
void Cmd_ExecuteScript(const char *filename);

/*
============================================================================================

COMMAND BUFFER

============================================================================================
*/
void Cbuf_Init(void);
void Cbuf_Shutdown(void);

void Cbuf_Clear(void);
void Cbuf_SetText(const char *string);
void Cbuf_AppendText(const char *string);

void Cbuf_Execute(void);

#endif // #ifdef CMD_H
