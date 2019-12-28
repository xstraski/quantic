// cvar.h -- configuration variables

#ifndef CVAR_H
#define CVAR_H

#include "common.h"

//
// cvar flags
//
#define CVAR_READONLY (1 << 0)             // a variable that can only be read, write is forbidden

//
// cvar interface
//
void Cvar_Init(void);
void Cvar_Shutdown(void);

void Cvar_Check(void);

void Cvar_DefineVariable(const char *name, const char *value, unsigned flags);
void Cvar_ForgetVariable(const char *name);
void Cvar_ForgetAllVariables(void);

void Cvar_PrintVariable(printf_t printcall, const char *name);

void Cvar_WriteFile(filehandle_t filehandle);

qboolean_t Cvar_VariableString(const char *name, char *out, unsigned outsize, const char *def);
qboolean_t Cvar_VariableInt(const char *name, int *out, int def);
qboolean_t Cvar_VariableFloat(const char *name, float *out, float def);
qboolean_t Cvar_VariableBoolean(const char *name, qboolean_t *out, qboolean_t def);

qboolean_t Cvar_SetVariableString(const char *name, const char *value);
qboolean_t Cvar_SetVariableInt(const char *name, int value);
qboolean_t Cvar_SetVariableFloat(const char *name, float value);
qboolean_t Cvar_SetVariableBoolean(const char *name, qboolean_t value);

#endif // #ifndef CVAR_H
