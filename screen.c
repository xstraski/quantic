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
	VID_Init();                                 // video low-level interface init
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
