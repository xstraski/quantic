// cvar.h

#ifndef CVAR_H
#define CVAR_H

#include "common.h"

/*
============================================================================================

CONFIGURATION VARIABLES (CVARS) SYSTEM

============================================================================================
*/

// config variable flags
#define CVAR_READONLY (1 << 0)             // can be read only once loaded from a file

// config variable information
#define MAXCVARNAME 64
#define MAXCVARSVAL 128
typedef struct cvar_s {
	char name[MAXCVARNAME];
	char sval[MAXCVARSVAL];
	int  ival;
	unsigned flags;

	struct cvar_s *next;
	struct cvar_s *prev;
} cvar_t;

void Cvar_Init(void);
void Cvar_Shutdown(void);

void Cvar_Check(void);

cvar_t * Cvar_NewVariable(const char *name, const char *value, unsigned flags);
void Cvar_ForgetVariable(const char *name);
void Cvar_ForgetAllVariables(void);

qboolean_t Cvar_VariableString(const char *name, char *out, unsigned outsize);
qboolean_t Cvar_VariableInt(const char *name, int *out);
qboolean_t Cvar_VariableFloat(const char *name, float *out);
qboolean_t Cvar_VariableBoolean(const char *name, qboolean_t *out);

qboolean_t Cvar_SetVariableString(const char *name, const char *value);
qboolean_t Cvar_SetVariableInt(const char *name, int value);
qboolean_t Cvar_SetVariableFloat(const char *name, float value);
qboolean_t Cvar_SetVariableBoolean(const char *name, qboolean_t value);

#endif // #ifndef CVAR_H
