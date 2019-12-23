// cmd.c

#include "common.h"

/*
================================================================================================

COMMAND SYSTEM CODE

================================================================================================
*/
#define MAXCOMMANDNAME 64
typedef struct command_s {
	char name[MAXCOMMANDNAME];
	commandfunc_t func;

	struct command_s *next;
	struct command_s *prev;
} command_t;
static command_t *cmd_commands;
static critialcode_t cmdcriticalcode;

/*
==================
Cmd_Exec_f
==================
*/
static void Cmd_Exec_f(commandcontext_t *ctx)
{
	if (ctx->argc == 0) {
		ctx->printf("No filename specified\n");
		return;
	}

	Cmd_ExecuteScript(ctx->argv[0]);
}

/*
==================
Cmd_Init
==================
*/
void Cmd_Init(void)
{
	Cmd_NewCommand("exec", Cmd_Exec_f);
	
	COM_Printf("Command system initialized\n");
}

/*
==================
Cmd_Shutdown
==================
*/
void Cmd_Shutdown(void)
{
	Cmd_ForgetAllCommands();
}

/*
==================
Cmd_Check
==================
*/
void Cmd_Check(void)
{}

/*
==================
Cmd_NewCommand

Registers new command
==================
*/
void Cmd_NewCommand(const char *name, commandfunc_t func)
{
	command_t *new;
	
#ifdef PARANOID
	if (!name || !name[0] || !func)
		Sys_Error("Cmd_NewCommand: bad params");

	Cmd_Check();
#endif

	EnterCriticalCode(&cmdcriticalcode);
	
	new = Zone_Alloc(sizeof(command_t));
	if (!new)
		Sys_Error("Cmd_NewCommand: out of memory");
	Q_strncpy(new->name, name, MAXCOMMANDNAME);
	new->func = func;
	new->prev = 0;
	new->next = com_commands;
	if (cmd_commands)
		cmd_commands->prev = new;
	cmd_commands = new;

	LeaveCriticalCode(&cmdcriticalcode);
}

/*
==================
Cmd_FindCommand
==================
*/
inline command_t * Cmd_FindCommand(const char *name)
{
	command_t *it;

	for (it = cmd_commands; it; it = it->next) {
		if (Q_strcmp(it->name, name) == 0)
			return it;
	}

	return 0;
}

/*
==================
Cmd_ForgetCommand
==================
*/
void Cmd_ForgetCommand(const char *name)
{
	command_t *cmd;

#ifdef PARANOID
	if (!name || !name[0])
		Sys_Error("Cmd_ForgetCommand: bad name");
#endif

	EnterCriticalCode(&cmdcriticalcode);

	cmd = Cmd_FindCommand(name);
	if (!it) {
		COM_DevPrintf("Cmd_ForgetCommand: command \"%s\" not found", name);
		return;
	}
	
	if (cmd->prev) cmd->prev->next = cmd->next;
	if (cmd->next) cmd->next->prev = cmd->prev;
	if (!cmd->prev && !cmd->next) cmd_commands = 0;
			
	Zone_Free(cmd);

	LeaveCriticalCode(&cmdcriticalcode);
}

/*
==================
Cmd_ForgetAllCommands
==================
*/
void Cmd_ForgetAllCommands(void)
{
	command_t *it, *target;

	EnterCriticalCode(&cmdcriticalcode);

	it = cmd_commands;
	while (it) {
		target = it;
		it = it->next;

		Zone_Free(target);
	}

	LeaveCriticalCode(&cmdcriticalcode);
}

/*
==================
Cmd_Execute
==================
*/
void Cmd_Execute(const char *command)
{
	char **tokens;
	unsigned numtokens;
	
#ifdef PARANOID
	if (!command || !command[0])
		Sys_Error("Cmd_Execute: bad command");
#endif

	EnterCriticalCode(&cmdcriticalcode);

	tokens = COM_Parse(command, " \t\n", &numtokens);
	if (tokens) {
		command_t *cmd = Cmd_FindCommand(tokens[0]);
		if (cmd) cmd->func(numtokens > 2 ? &tokens[1] : 0);
		
		COM_FreeParse(tokens, numtokens);
	}

	LeaveCriticalCode(&cmdcriticalcode);
}

/*
==================
Cmd_ExecuteScript
==================
*/
void Cmd_ExecuteScript(const char *filename)
{
	filehandle_t file;
	
#ifdef PARANOID
	if (!filename || !filename[0])
		Sys_Error("Cmd_ExecuteScript: bad filename");
#endif

	file = Sys_FOpenForReading(filename);
	if (file != BADFILE) {
		char line[2048];
		
		while (COM_FReadLine(file, line, sizeof(line)))
			Cmd_Execute(line);

		Sys_FClose(file);
	}
}

/*
================================================================================================

COMMANDS BUFFER CODE

================================================================================================
*/
#define INITBUFSIZE 128

static char *cmd_buf;
static unsigned cmd_bufpos, cmd_bufsize;
static criticalcode_t cmdbufcriticalcode;

/*
==================
Cbuf_Init
==================
*/
void Cbuf_Init(void)
{
	cmd_buflen = 0;
	cmd_bufsize = INITBUFSIZE;
	cmd_buf = Zone_Alloc(cmd_bufsize);
	if (!cmd_buf)
		Sys_Error("Cbuf_Init: out of memory");
	
	COM_Printf("Commands buffer initialized\n");
}

/*
==================
Cbuf_Shutdown
==================
*/
void Cbuf_Shutdown(void)
{
	if (cmd_buf)
		Zone_Free(cmd_buf);
}

/*
==================
Cbuf_Clear
==================
*/
void Cbuf_Clear(void)
{
	EnterCriticalCode(&cmdbufcriticalcode);

	if (cmd_buf)
		cmd_buf[0] = 0;
	cmd_bufpos = 0;

	LeaveCriticalCode(&cmdbufcriticalcode);
}

/*
==================
Cbuf_Upsize
==================
*/
static void Cbuf_Upsize(unsigned size)
{
	char *new;
	unsigned newsize;
	
	if (size == 0 || size <= cmd_bufsize)
		Sys_Error("Cbuf_Upsize: bad size");

	newsize = (size_t)(((float)size * 1.5f) + 1);
	newsize += newsize % 8;
	new = Zone_Alloc(newsize);
	if (!new)
		Sys_Error("Cbuf_Upsize: out of memory");

	if (cmd_buf) {
		Q_strcpy(new, cmd_buf);
		Zone_Free(cmd_buf);
	}
	cmd_buf = new;
	cmd_bufsize = newsize;
	
}

/*
==================
Cbuf_SetString
==================
*/
void Cbuf_SetString(const char *string)
{
	unsigned len;
	
#ifdef PARANOID
	if (!string || !string[0])
		Sys_Error("Cbuf_SetString: bad string");
#endif

	EnterCriticalCode(&cmdbufcriticalcode);
	
	len = Q_strlen(string);
	if (len > cmd_bufsize)
		Cbuf_Upsize(len);

	Q_strcpy(cmd_buf, string);
	cmd_buf[len] = 0;
	cmd_bufpos += (len + 1);
	
	LeaveCriticalCode(&cmdbufcriticalcode);
}

/*
==================
Cbuf_AppendString
==================
*/
void Cbuf_AppendString(const char *string)
{
	unsigned len;
	char *p;
	
#ifdef PARANOID
	if (!string || !string[0])
		Sys_Error("Cbuf_AppendString: bad string");
#endif

	len = Q_strlen(string);
	if (cmd_bufpos + len >= cmd_bufsize)
		Cbuf_Upsize(cmd_bufpos + len);
	
	p = cmd_buf + cmd_bufpos;
	Q_strncpy(p, string, len);
	cmd_buf[cmd_bufpos + len] = 0;
	cmd_bufpos += (len + 1);
}

/*
==================
Cbuf_Execute
==================
*/
void Cbuf_Execute(void)
{
	char line[256];
	unsigned pos = 0;
	
	EnterCriticalCode(&cmdbufcriticalcode);

	while (COM_ParseLine(cmd_buf, &pos, line, sizeof(line)))
		Cmd_Execute(line);

	LeaveCriticalCode(&cmdbufcriticalcode);

	Cmd_Clear();
}
