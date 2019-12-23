// cvar.c

#include "common.h"

typedef struct cvar_reg_s {
	cvar_t *var;
	
	cvar_reg_s *next;
	cvar_reg_s *prev;
} cvar_reg_t;
static cvar_reg_t *cvar_regs;
static criticalcode_t cvarcriticalcode;

/*
=================
Cvar_Init
=================
*/
void Cvar_Init(void)
{
	COM_Printf("Cvar system initialized\n");
}

/*
=================
Cvar_Shutdown
=================
*/
void Cvar_Shutdown(void)
{
	Cvar_ForgetAllCvars();
}

/*
=================
Cvar_CheckReg
=================
*/
static void Cvar_CheckReg(cvar_reg_t *reg)
{
#ifdef PARANOID
	if (!reg)
		Sys_Error("Cvar_CheckReg: null reg");
#endif

	if (!reg->cvar) Sys_Error("Cvar_CheckReg: null cvar");
	if (reg->next || reg->prev) {
		if (reg->next) {
			if (reg->next->prev != reg)
				Sys_Error("Cvar_CheckReg: linkage corruption");
		}
		if (reg->prev) {
			if (reg->prev->next != reg)
				Sys_Error("Cvar_CheckReg: linkage corruption");
		}
	}
}

/*
=================
Cvar_Check
=================
*/
void Cvar_Check(void)
{
	cvar_reg_t *p;

	EnterCriticalCode(&cvarcriticalcode);
	
	for (p = cvar_regs; p; p = p->next)
		Cvar_CheckReg(p);

	LeaveCriticalCode(&cvarcriticalcode);
}

/*
=================
Cvar_NewCvar
=================
*/
void Cvar_NewCvar(cvar_t *cvar)
{
	cvar_reg_t **new;
	
#ifdef PARANOID
	if (!cvar)
		Sys_Error("Cvar_NewCvar: null cvar");

	Cvar_Check();
#endif

	EnterCriticalCode(&cvarcriticalcode);

	new = Zone_Alloc(sizeof(cvar_reg_t));
	if (!new)
		Sys_Error("Cvar_NewCvar: out of memory");

	new->var = cvar;
	new->prev = 0;
	new->next = cvar_regs;
	if (cvar_regs)
		cvar_regs->prev = new;
	cvar_regs = new;

	LeaveCriticalCode(&cvarcriticalcode);
}
	
/*
=================
Cvar_FindReg
=================
*/
static cvar_reg_t * Cvar_FindReg(const char *name)
{
	cvar_reg_t *p;
	
	if (!name || !name[0])
		Sys_Error("Cvar_FindReg: bad name");

	for (p = cvar_regs; p; p = p->next) {
#ifdef PARANOID		
		Cvar_CheckReg(p);
#endif	
		if (Q_strcmp(p->cvar->name, name) == 0)
			return p;
	}

	return 0;
}

/*
=================
Cvar_ForgetCvar
=================
*/
void Cvar_ForgetCvar(const char *name)
{
	cvar_reg_t *p;
	
#ifdef PARANOID
	if (!name || !name[0])
		Sys_Error("Cvar_ForgetCvar: bad name");
#endif

	EnterCriticalCode(&cvarcriticalcode);

	p = Cvar_FindReg(name);
	if (!p) {
		COM_DevPrintf("Cvar_ForgetCvar: cvar missing");
		return 0;
	}

	if (p->next) p->next->prev = p->prev;
	if (p->prev) p->prev->next = p->next;
	if (!p->next && !p->prev) cvar_regs = 0;

	Zone_Free(p);

	LeaveCriticalCode(&cvarcriticalcode);
}

/*
=================
Cvar_ForgetAllCvars
=================
*/
void Cvar_ForgetAllCvars(void)
{
	cvar_reg_t *p, *target;

	EnterCriticalCode(&cvarcriticalcode);
	
	p = cvar_regs;
	while (p) {
		target = p;
		p = p->next;

		Zone_Free(target);
	}

	LeaveCriticalCode(&cvarcriticalcode);
}
		
