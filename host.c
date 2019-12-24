// host.c

#include "host.h"
#include "common.c"
#include "mathlib.c"
#include "hunk.c"
#include "cmd.c"
#include "cvar.c"
#ifdef WINDOWS
#include "sys_windows.c"
#include "vid_windows.c"
#endif
#include "screen.c"

qboolean_t host_dedicated;

// timings
unsigned   host_frames = 0;
qw_t       host_frameclocks = 0;
float      host_framesecs = 0.0f;
float      host_framerate = 0.0f;

/*
=================
Host_LoadConfiguration
=================
*/
static void Host_LoadConfiguration(void)
{
	Cmd_ExecuteScript("default.cfg");
	Cmd_ExecuteScript("user.cfg");

	if (com_devmode) Cmd_ExecuteScript("devmode.cfg");
	if (com_safe)    Cmd_ExecuteScript("safe.cfg");
}

/*
=================
Host_SaveConfiguration
=================
*/
static void Host_SaveConfiguration(void)
{}

/*
=================
Host_Init
=================
*/
void Host_Init(hostparams_t *params)
{
	benchmark_t initbenchmark;
	
#ifdef PARANOID
	if (!params)
		Sys_Error("Host_Init: null params");
#endif

	//
	// prepare timings
	//
	Sys_BeginBenchmark(&initbenchmark);
	
	//
	// initialize all systems
	//
	COM_Init(params->rootpath, H_BASEDIR, H_USERDIR);    // common utilities init (memory, filesystem, etc)

	Host_LoadConfiguration();                            // configuration reading

	SCR_Init();                                          // gfx screen init

	//
	// finalize
	//
	Sys_Benchmark(&initbenchmark, false);	
	COM_DevPrintf("Host_Init: initbenchmark.secs_elapsed %f\n", initbenchmark.secs_elapsed);
	COM_DevPrintf("Host_Init: initbenchmark.clks_elapsed %d\n", initbenchmark.clks_elapsed);
	
	COM_Printf("========== %s Initialized Ok ==========\n", H_NAME);
}

/*
=================
Host_Shutdown
=================
*/
void Host_Shutdown(qboolean_t aftererror)
{
	SCR_Shutdown();

	if (!aftererror)
		Host_SaveConfiguration();
	
	COM_Shutdown();
}

/*
=================
Host_Frame
=================
*/
void Host_FrameSynced(void);
void Host_Frame(void)
{
	static qboolean_t pioneerframe = true;    // is this frame first at all?
	static benchmark_t framebenchmark;
	int currentfreq;
	float targetrate;

	//
	// do the frame
	//
	if (pioneerframe)
		Sys_BeginBenchmark(&framebenchmark);
	Host_FrameSynced();
	Sys_Benchmark(&framebenchmark, false);
	
	//
	// sync the frame
	//
	VID_CurrentModeProps(0, 0, 0, &currentfreq);
	targetrate = 1.0f / (float)currentfreq;
	if (framebenchmark.secs_elapsed < targetrate) {
		while (framebenchmark.secs_elapsed < targetrate) {
			Sys_Sleep((unsigned)((targetrate - framebenchmark.secs_elapsed) * 1000));
			Sys_Benchmark(&framebenchmark, false);
		}
	}

	Sys_Benchmark(&framebenchmark, true);
	
	host_frames++;
	host_framesecs   = framebenchmark.secs_elapsed;
	host_frameclocks = framebenchmark.clks_elapsed;
	host_framerate   = framebenchmark.exec_per_sec;

	//
	// end of frame
	//
	pioneerframe = false;
}

/*
=================
Host_FrameSynced
=================
*/
static void Host_FrameSynced(void)
{
	//
	// update screen
	//
	SCR_Frame();

	//
	// flush all commands
	//
	Cbuf_Execute();
}
