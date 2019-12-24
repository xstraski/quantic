// cmd.c

#include "common.h"

/*
================================================================================================

COMMAND SYSTEM CODE

================================================================================================
*/
#define MAXCMDNAME 64
typedef struct cmd_s {
	char name[MAXCMDNAME];
	cmdfunc_t func;

	struct cmd_s *next;
	struct cmd_s *prev;
} cmd_t;
static cmd_t *cmd_commands;
static critialcode_t cmdcriticalcode;

/*
==================
Cmd_Exec_f
==================
*/
static void Cmd_Exec_f(cmdcontext_t *ctx)
{
	if (ctx->argc == 0 || !ctx->argv[0][0]) {
		ctx->printf("bad filename specified\n");
		return;
	}

	Cmd_ExecuteScript(ctx->argv[0]);
}

/*
==================
Cmd_LsCmds_f
==================
*/
static void Cmd_LsCmds_f(cmdcontext_t *ctx)
{
	cmd_t *p;
	
	EnterCriticalCode(&cmdcriticalcode);

	for (p = cmd_commands; p; p = p->next)
		ctx->printf("%s\n", p->name);

	LeaveCriticalCode(&cmdcriticalcode);
}

/*
==================
Cmd_Init
==================
*/
void Cmd_Init(void)
{
	Cmd_NewCommand("exec", Cmd_Exec_f);
	Cmd_NewCommand("lscmds", Cmd_LsCmds_f);
	
	COM_Printf("Cmd exec initialized\n");
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
Cmd_CheckCommand
==================
*/
static void Cmd_CheckCommand(command_t *p)
{
	if (!p->func) Sys_Error("Cmd_CheckCommand: null func");
	if (p->next || p->prev) {
		if (p->next && p->next->prev != p) Sys_Error("Cmd_CheckCommand: linked list corrupted");
		if (p->prev && p->prev->next != p) Sys_Error("Cmd_CheckCommand: linked list corrupted");
	}
}

/*
==================
Cmd_Check
==================
*/
void Cmd_Check(void)
{
	cmd_t *p;

	EnterCriticalCode(&cmdcriticalcode);
	
	for (p = cmd_commands; p; p = p->next)
		Cmd_CheckCommand(p);

	LeaveCriticalCode(&cmdcriticalcode);
}

/*
==================
Cmd_NewCommand

Registers new command
==================
*/
void Cmd_NewCommand(const char *name, commandfunc_t func)
{
	cmd_t *new;
	
#ifdef PARANOID
	if (!name || !name[0] || !func)
		Sys_Error("Cmd_NewCommand: bad params");

	Cmd_Check();
#endif

	EnterCriticalCode(&cmdcriticalcode);
	
	new = Zone_Alloc(sizeof(command_t));
	if (!new)
		Sys_Error("Cmd_NewCommand: out of memory");
	Q_strncpy(new->name, name, MAXCMDNAME);
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
inline cmd_t * Cmd_FindCommand(const char *name)
{
	cmd_t *it;

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
	cmd_t *cmd;

#ifdef PARANOID
	if (!name || !name[0])
		Sys_Error("Cmd_ForgetCommand: bad name");
#endif

	EnterCriticalCode(&cmdcriticalcode);

	cmd = Cmd_FindCommand(name);
	if (!it) {
		COM_DevPrintf("Cmd_ForgetCommand: \"%s\" missing", name);
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
	cmd_t *it, *target;

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
		cmd_t *cmd = Cmd_FindCommand(tokens[0]);
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
	
	COM_Printf("Cmd buffer initialized\n");
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
Cbuf_Grow
==================
*/
static void Cbuf_Grow(unsigned size)
{
	char *new;
	unsigned newsize;
	
	if (size == 0 || size <= cmd_bufsize)
		Sys_Error("Cbuf_Grow: bad size");

	newsize = (size_t)(((float)size * 1.5f) + 1);
	newsize += newsize % 8;
	new = Zone_Alloc(newsize);
	if (!new)
		Sys_Error("Cbuf_Grow: out of memory");

	if (cmd_buf) {
		Q_strcpy(new, cmd_buf);
		Zone_Free(cmd_buf);
	}
	cmd_buf = new;
	cmd_bufsize = newsize;
	
}

/*
==================
Cbuf_SetText
==================
*/
void Cbuf_SetText(const char *string)
{
	unsigned len;
	
#ifdef PARANOID
	if (!string || !string[0])
		Sys_Error("Cbuf_SetText: bad string");
#endif

	EnterCriticalCode(&cmdbufcriticalcode);
	
	len = Q_strlen(string);
	if (len > cmd_bufsize)
		Cbuf_Grow(len);

	Q_strcpy(cmd_buf, string);
	cmd_buf[len] = 0;
	cmd_bufpos += (len + 1);
	
	LeaveCriticalCode(&cmdbufcriticalcode);
}

/*
==================
Cbuf_AppendText
==================
*/
void Cbuf_AppendText(const char *string)
{
	unsigned len;
	char *p;
	
#ifdef PARANOID
	if (!string || !string[0])
		Sys_Error("Cbuf_AppendText: bad string");
#endif

	len = Q_strlen(string);
	if (cmd_bufpos + len >= cmd_bufsize)
		Cbuf_Grow(cmd_bufpos + len);
	
	p = cmd_buf + cmd_bufpos;
	Q_strncpy(p, string, len);
	cmd_buf[cmd_bufpos + len] = 0;
	cmd_bufpos += (len + 1);
}

/*
==================
Cbuf_Execute

executes command buffer
gets called at the end of Host_Frame
==================
*/
void Cbuf_Execute(void)
{
	char line[256];
	unsigned pos = 0;
	
	EnterCriticalCode(&cmdbufcriticalcode);

	while (COM_ParseLine(cmd_buf, &pos, line, sizeof(line)))
		Cmd_Execute(line);

	if (cmd_buf)
		cmd_buf[0] = 0;
	cmd_bufpos = 0;

	LeaveCriticalCode(&cmdbufcriticalcode);
}
