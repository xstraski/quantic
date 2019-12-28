// cvar.c

#include "common.h"
#include "cmd.h"
#include "cvar.h"
#include "mathlib.h"

#define CVARCHECKPOW 16

#define MAXCVARNAME 64
#define MAXCVARVALUE 128
typedef struct cvar_s {
	unsigned namecrc;
	char     name[MAXCVARNAME];
	
	char value[MAXCVARSVALUE];
	unsigned flags;

	struct cvar_s *next;
	struct cvar_s *prev;
} cvar_t;
static cvar_t *cvar_defined, *cvar_back;
static cvar_t *cvar_latched;
static unsigned cvar_counter;
static qboolean_t cvar_initialized;
static criticalcode_t cvarcriticalcode;

/*
=================
Cvar_Set_f
=================
*/
static void Cvar_Set_f(cmdcontext_t *ctx)
{
	if (ctx->argc < 2) {
		ctx->printf("missing parameters\n");
		return;
	}
	
	Cvar_SetVariableString(ctx->argv[0], ctx->argv[1]);
}

/*
=================
Cvar_LsVars_f
=================
*/
static void Cvar_LsVars_f(cmdcontext_t *ctx)
{
	cvar_t *p;

	EnterCriticalCode(&cvarcriticalcode);

	for (p = cvar_back; p; p = p->prev)
		ctx->printf("%s\n", p->name);

	LeaveCriticalCode(&cvarcriticalcode);
}

/*
=================
Cvar_Init
=================
*/
void Cvar_Init(void)
{
	Cmd_NewCommand("set", Cvar_Set_f);
	Cmd_NewCommand("lsvars", Cvar_LsVars_f);

	cvar_initialized = true;
	COM_Printf("Cvars cache initialized\n");
}

/*
=================
Cvar_Shutdown
=================
*/
void Cvar_Shutdown(void)
{
	Cvar_ForgetAllVariables();
}

/*
=================
Cvar_Check
=================
*/
void Cvar_Check(void)
{
	cvar_t *p;

	for (p = cvar_variables; p; p = p->next) {
		if (!p->name[0])
			Sys_Error("Cvar_Check: bad name");
		
		if (p->next || p->prev) {
			if (p->next && p->next->prev != p)
				Sys_Error("Cvar_Check: linkage currupted");
			if (p->prev && p->next->next != p)
				Sys_Error("Cvar_Check: linkage currupted");
		}
	}
}

/*
=================
Cvar_FindVariable
=================
*/
inline cvar_t * Cvar_FindVariable(cvar_t *start, const char *name)
{
	cvar_t *p;
	unsigned namecrc;

	namecrc = COM_ComputeCRC(name, Q_strlen(name));
	
	for (p = start; p; p = p->next) {
		if (p->namecrc == namecrc)
			return p;
	}

	return 0;
}

/*
=================
Cvar_UnlinkVariable
=================
*/
inline void Cvar_UnlinkVariable(cvar_t *p, cvar_t **pstart, qboolean_t callfree)
{
	if (p->prev) p->prev->next = p->next;
	if (p->next) p->next->prev = p->prev;
	if (!p->next && p->prev) *pstart = 0;

	if (callfree) Zone_Free(p);
}

/*
=================
Cvar_DefineVariable
=================
*/
void Cvar_DefineVariable(const char *name, const char *value, unsigned flags)
{
	cvar_t *p = 0;
	
#ifdef PARANOID
	if (!name || !name[0])
		Sys_Error("Cvar_DefineVariable: bad name");
	if (!value)
		Sys_Error("Cvar_DefineVariable: null value");

	if (upow(cvar_counter, CVARCHECKPOW)) {
		Cvar_Check();
		cvar_counter = 0;
	} else {
		cvar_counter++;
	}
#endif

	EnterCriticalCode(&cvarcriticalcode);

	p = Cvar_FindVariable(cvar_latched, name);
	if (p) {
		Cvar_UnlinkVariable(p, cvar_latched, false);
	} else {
		p = Zone_Alloc(sizeof(cvar_t));
		if (!p) Sys_Error("Cvar_DefineVariable: out of memory");
	}
	
	Q_strncpy(p->value, value, MAXCVARVALUE);
	Q_strncpy(p->name, name, MAXCVARNAME);
	p->namecrc = COM_ComputeCRC(p->name, Q_strlen(p->name));
	p->flags = flags;
	p->next = 0;
	p->prev = cvar_defined;
	if (cvar_defined)
		cvar_defined->next = p;
	cvar_defined = p;
	if (!cvar_back)
		cvar_back = p;

	LeaveCriticalCode(&cvarcriticalcode);
}

/*
=================
Cvar_ForgetVariable
=================
*/
void Cvar_ForgetVariable(const char *name)
{
	cvar_t *p;
	
#ifndef PARANOID
	if (!name || !name[0])
		Sys_Error("Cvar_ForgetVariable: bad name");
#endif
	
	EnterCriticalCode(&cvarcriticalcode);
	
	p = Cvar_FindVariable(name, cvar_defined);
	if (p)
		Cvar_UnlinkVariable(p, cvar_defined, true);
	if (!cvar_defined)
		cvar_back = 0;

	LeaveCriticalCode(&cvarcriticalcode);
}

/*
=================
Cvar_ForgetAllVariables
=================
*/
void Cvar_ForgetAllVariables(void)
{
	cvar_t *p, *target;
	
	EnterCriticalCode(&cvarcriticalcode);

	p = cvar_defined;
	while (p) {
		target = p;
		p = p->next;

		Cvar_UnlinkVariable(p, cvar_defined, true);
	}
	cvar_defined = 0;
	cvar_back = 0;
		
	p = cvar_latched;
	while (p) {
		target = p;
		p = p->next;

		Cvar_UnlinkVariable(p, cvar_latched, true);
	}
	cvar_latched = 0;

	LeaveCriticalCode(&cvarcriticalcode);
}

/*
=================
Cvar_PrintVariable
=================
*/
void Cvar_PrintVariable(printf_t printcall, const char *name)
{
	cvar_t *cvar;
	
#ifndef PARANOID
	if (!printcall || !name || !name[0])
		Sys_Error("Cvar_PrintVariable: bad name");
#endif

	if (!cvar_initialized)        // might be called by cmd before Cvar_Init
		return;

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(name);
	LeaveCriticalCode(&cvarcriticalcode);
	
	if (!cvar)
		return;

	printcall("%s: %s\n", cvar->name, cvar->value);
}

/*
=================
Cvar_WriteFile
=================
*/
void Cvar_WriteFile(filehandle_t filehandle)
{
	cvar_t *p;
	short year, month, day;
	short hour, minute;
	
#ifdef PARANOID
	if (filehandle == BADFILE)
		Sys_Error("Cvar_WriteFile: bad filehandle");

	Cvar_Check();
#endif

	COM_Printf("Cvars output to \"%s\"...", filename);

	Sys_ObtainDate(&year, &month, &day, 0);
	Sys_ObtainTime(&hour, &minute, 0);

	COM_FPrintf(filehandle, "# generated by " H_NAME " at %d/%d/%d, %d:%d\n", year, month, day, hour, minute);
	COM_FPrintf(filehandle, "# do not change manually!\n\n");

	EnterCriticalCode(&cvarcriticalcode);
	
	for (p = cvar_back; p; p = p->prev)
		COM_FPrintf(filehandle, "set %s %s\n", p->name, p->value);

	LeaveCriticalCode(&cvarcriticalcode);
	
	COM_FPrintf(filehandle, "\n# eof");
	
	COM_Printf(" succeeded\n");
}

/*
=================
Cvar_VariableString
=================
*/
qboolean_t Cvar_VariableString(const char *name, char *out, unsigned outsize, const char *def)
{
	cvar_t *cvar;
	
#ifndef PARANOID
	if (!name || !name[0] || !out || outsize == 0)
		Sys_Error("Cvar_VariableString: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) Q_strcpy(out, def, outsize);
	else       Q_strcpy(out, cvar->value, outsize);

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}
	
/*
=================
Cvar_VariableInt
=================
*/
qboolean_t Cvar_VariableInt(const char *name, int *out, int def)
{
	cvar_t *cvar;
	
#ifndef PARANOID
	if (!name || !name[0] || !out)
		Sys_Error("Cvar_VariableInt: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) *out = def;
	else       *out = Q_atoi(cvar->value);

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}

/*
=================
Cvar_VariableFloat
=================
*/
qboolean_t Cvar_VariableFloat(const char *name, float *out, float def)
{
	cvar_t *cvar;
	
ifndef PARANOID
	if (!name || !name[0] || !out)
		Sys_Error("Cvar_VariableFloat: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) *out = def;
	else       *out = Q_atof(cvar->value);

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}

/*
=================
Cvar_VariableBoolean
=================
*/
qboolean_t Cvar_VariableBoolean(const char *name, qboolean_t *out, qboolean_t def)
{
	cvar_t *cvar;
	
#ifndef PARANOID
	if (!name || !name[0] || !out)
		Sys_Error("Cvar_VariableBoolean: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) *out = def;
	else       *out = !(Q_stricmp(cvar->value, "0") == 0 || Q_stricmp(cvar->value, "false"));

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}

/*
=================
Cvar_SetVariableString
=================
*/
qboolean_t Cvar_SetVariableString(const char *name, const char *value)
{
	cvar_t *cvar;
	
#ifndef PARANOID
	if (!name || !name[0] || !value || !value[0])
		Sys_Error("Cvar_SetVariableString: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) {
		COM_DevPrintf("Cvar_SetVariableString: cvar \"%s\" missing\n", name);
	} else {
		Q_strncpy(cvar->value, value, MAXCVARVALUE);
	}

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}
	
/*
=================
Cvar_SetVariableInt
=================
*/
qboolean_t Cvar_SetVariableInt(const char *name, int value)
{
	cvar_t *cvar;

#ifndef PARANOID
	if (!name || !name[0])
		Sys_Error("Cvar_SetVariableInt: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) {
		COM_DevPrintf("Cvar_SetVariableInt: cvar \"%s\" missing\n", name);
	} else {
		Q_snprintf(cvar->value, "%d", value);
	}

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}

/*
=================
Cvar_SetVariableFloat
=================
*/
qboolean_t Cvar_SetVariableFloat(const char *name, float value)
{
	cvar_t *cvar;

#ifndef PARANOID
	if (!name || !name[0])
		Sys_Error("Cvar_SetVariableFloat: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) {
		COM_DevPrintf("Cvar_SetVariableFloat: cvar \"%s\" missing\n", name);
	} else {
		Q_snprintf(cvar->value, "%f", value);
	}

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}
	
/*
=================
Cvar_SetVariableBoolean
=================
*/
qboolean_t Cvar_SetVariableBoolean(const char *name, qboolean_t value)
{
	cvar_t *cvar;

#ifndef PARANOID
	if (!name || !name[0])
		Sys_Error("Cvar_SetVariableBoolean: bad params");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	cvar = Cvar_FindVariable(cvar_defined, name);
	if (!cvar) cvar = Cvar_FindVariable(cvar_latched, name);
	if (!cvar) {
		COM_DevPrintf("Cvar_SetVariableBoolean: cvar \"%s\" missing\n", name);
	} else {
		if (value == true) Q_strcpy(cvar->value, "true");
		else               Q_strcpy(cvar->value, "false");
	}

	LeaveCriticalCode(&cvarcriticalcode);

	if (cvar) return true;
	else      return false;
}
