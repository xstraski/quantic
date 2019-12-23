// vid.h

#ifndef VID_H
#define VID_H

#include "common.h"

/*
=====================================================================================================

VIDEO INTERFACE

Does no graphics, only manages video modes, system cursors, gives low-level video memory access
if the system allows and/or doing non-accelerated (sofware) drawing. If the system is not designed
with direct hardware access in mind, the video memory interface is uninitialized and is meant to be
unused.

=====================================================================================================
*/
void VID_Init(void);
void VID_Shutdown(void);

void VID_Restart(void);

void VID_EnableCursor(void);
void VID_DisableCursor(void);

void VID_BeginBusyCursor(void);
void VID_EndBusyCursor(void);

qboolean_t VID_ModeProps(int modeid, int *o_width, int *o_height, int *o_depth, int *o_frequency);
void       VID_CurrentModeProps(int *o_width, int *o_height, int *o_depth, int *o_frequency);
qboolean_t VID_CurrentModeFullScreen(void);

qboolean_t VID_SetMode(int modeid, qboolean_t fullscreen);
void       VID_RestoreMode(void);

void       VID_RefreshModes(void);
void       VID_PrintModes(void);

#endif // #ifndef VID_H
