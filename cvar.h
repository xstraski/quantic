// cvar.h

#ifndef CVAR_H
#define CVAR_H

#include "common.h"

/*
============================================================================================

CONFIGURATION VARIABLES (CVARS) SYSTEM

============================================================================================
*/

// config variable information
#define MAXCVARNAME 64
#define MAXCVARSVAL 128
typedef struct cvar_s {
	char name[MAXCVARNAME];
	char sval[MAXCVARSVAL];
} cvar_t;

void Cvar_Init(void);
void Cvar_Shutdown(void);

void Cvar_Check(void);

void Cvar_NewCvar(cvar_t *var);
void Cvar_ForgetCvar(const char *name);
void Cvar_ForgetAllCvars(void);

qboolean_t Cvar_AsString(const char *name, char *out, unsigned outsize);
qboolean_t Cvar_AsInt(const char *name, int *out);
qboolean_t Cvar_AsFloat(const char *name, float *out);

qboolean_t Cvar_SetString(const char *name, const char *value);
qboolean_t Cvar_SetInt(const char *name, int value);
qboolean_t Cvar_SetFloat(const char *name, float value);

#endif // #ifndef CVAR_H
