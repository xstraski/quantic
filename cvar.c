// cvar.c

#include "common.h"
#include "cmd.h"
#include "mathlib.h"

#define CVARCHECKPOW 16

static cvar_t *cvar_defined;                    // variables defined by Cvar_NewVariable call, normal situation
static cvar_t *cvar_queued;                     // variables loaded before its define for some reason

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
