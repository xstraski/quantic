// screen.c

#include "screen.h"
#include "vid.h"

/*
=================
SCR_Init
=================
*/
void SCR_Init(void)
{
	int vid_mode;
	qboolean_t vid_windowed;
	
	//
	// basic video initialization
	//
	VID_Init();
	
	Cvar_VariableInt("vid_mode", &vid_mode, 0);
	Cvar_VariableBoolean("vid_windowed", &vid_windowed, false);
	VID_SetMode(vid_mode, vid_windowed);
};

/*
=================
SCR_Shutdown
=================
*/
void SCR_Shutdown(void)
{
	VID_Shutdown();
}
