// vid_windows.c -- Windows video driver

#include "vid.h"
#include "sys.h"
#include "sys_windows.h"

#define DM_COMMON_FIELDS DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPELPELS | DM_DISPLAYFREQUENCY

#define ORIGINAL_MODE_ID ((int)-1)

static qboolean_t vid_initialized;
qboolean_t ActiveApp = true;

int desktop_width, dekstop_height;
int desktop_center_x, desktop_center_y;

// window
static WNDCLASS wc;
HWND global_hWnd;
HDC  global_hDC;
static DWORD dwWindowStyle;
static DWORD dwWindowExStyle;
int window_width, window_height;
int window_x, window_y;
int window_center_x, window_center_y;
qboolean_t window_visible, window_minimized;
static qboolean_t virginwindow = true;           // true if the window has never been created
static criticalcode_t windowcriticalcode;

// video mode
typedef struct {
	int id;
	int width;
	int height;
	int depth;
	int frequency;
} vidmode_t;
static vidmode_t origmode;
static vidmode_t *vidmodes, *currentmode;
static unsigned vidmodesnum;
static qboolean_t inwindowed = true, inmodechange;
static criticalcode_t modecriticalcode;

// cursors
static qboolean_t cursor_enabled = true;
static HCURSOR hCursorArrow, hCursorWait, hCursorCross;
static HCURSOR hCurrentCursor, hPreBusyCursor;
static int busycursors;

void VID_FreeWindow(void);
void VID_FreeModes(void);

/*
=================
VID_EnableCursor
=================
*/
void VID_EnableCursor(void)
{
	SetCursor(hCurrentCursor);
	cursor_enabled = true;
}

/*
=================
VID_DisableCursor
=================
*/
void VID_DisableCursor(void)
{
	SetCursor(0);
	cursor_enabled = false;
}

/*
=================
VID_BeginBusyCursor
=================
*/
void VID_BeginBusyCursor(void)
{
#ifdef PARANOID	
	if (busycursors == INT_MAX)
		Sys_Error("VID_BeginBusyCursor: limit error");
#endif	

	if (!busycursors) {
		hPreBusyCursor = hCurrentCursor;
		hCurrentCursor = hCursorWait;

		if (cursor_enabled)
			SetCursor(hCurrentCursor);
	}
	busycursors++;
}

/*
=================
VID_EndBusyCursor
=================
*/
void VID_EndBusyCursor(void)
{
#ifdef PARANOID
	if (busycursors == 0)
		Sys_Error("VID_EndBusyCursor: limit error");
#endif

	if (busycursor == 1) {
		hCurrentCursor = hPreBusyCursor;

		if (cursor_enabled)
			SetCursor(hCurrentCursor);
	}
	busycursors--;
}

/*
=================
VID_NewDesktopMetrics
=================
*/
static void VID_NewDesktopMetrics(void)
{
	desktop_width = GetSystemMetrics(SM_CXSCREEN);
	desktop_height = GetSystemMetrics(SM_CYSCREEN);

	desktop_center_x = desktop_width / 2;
	desktop_center_y = desktop_height / 2;
}

/*
=================
WindowProc
=================
*/
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_APPACTIVE:
		if (wParam == TRUE) ActiveApp = true;
		else                ActiveApp = false;
		break;
		
	case WM_DESTROY:
		if (!inmodechange)
			PostQuitMessage(0);
		break;

	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)                                 window_minimized = true;
		else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) window_minimized = false;
		
		if (wParam == SIZE_MAXHIDE)      window_visible = false;
		else if (wParam == SIZE_MAXSHOW) window_visible = true;
		else                             window_visible = !window_minimized;

		window_width = (int)LOWORD(lParam);
		window_height = (int)HIWORD(lParam);
		window_center_x = window_width / 2;
		window_center_y = window_height / 2;
		break;

	case WM_SHOW:
		if (wParam) window_visible = true;
		else        window_visible = false;
		break;

	case WM_MOVE:
		window_x = (int)LOWORD(lParam);
		window_y = (int)HIWORD(lParam);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*
=================
VID_NewWindow
=================
*/
static void VID_NewWindow(int width, int height, qboolean_t windowed)
{
	VID_FreeWindow();

	EnterCriticalCode(&windowcriticalcode);

	//
	// setup window settings
	//
	if (windowed) {
		RECT rc = {0, 0, width, height};
		
		dwWindowStyle = WS_OVERLAPPEDWINDOW;
		dwWindowExStyle = WS_EX_APPWINDOW;

		AdjustWindowRect(&rc, dwWindowStyle, FALSE);
		window_width = rc.right - rc.left;
		window_height = rc.bottom - rc.top;
		window_center_x = window_width / 2;
		window_center_y = window_height / 2;

		if (COM_CheckArg("-centerwindow")) {
			window_x = desktop_center_x - window_center_x;
			window_y = desktop_center_y - window_center_y;
		} else {
			window_x = CW_USEDEFAULT;
			window_y = CW_USEDEFAULT;
		}
	} else {
		dwWindowStyle = WS_SYSMENU | WS_CAPTION;
		dwWindowExStyle = WS_EX_APPWINDOW;

		window_width = width;
		window_height = height;
		window_center_x = window_width / 2;
		window_center_y = window_height / 2;

		window_x = CW_USEDEFAULT;
		window_y = CW_USEDEFAULT;
	}

	//
	// spawn window
	//
	global_hWnd = CreateWindowEx(dwWindowExStyle, wc.lpszClassName, H_NAME_LONG, dwWindowStyle,
								 window_x, window_y, window_width, window_height, 0, 0, global_hInstance, 0);
	if (!global_hWnd) {
		COM_DevPrintf("VID_NewWindow: CreateWindowEx failed (code 0x%x)\n", GetLastError());
		Sys_Error("Cannot create window");
	}

	global_hDC = GetDC(global_hWnd);
	if (!global_hDC) {
		COM_DevPrintf("VID_NewWindow: GetDC failed (code 0x%x)\n", GetLastError());
		Sys_Error("Cannot get device context");
	}

	COM_Printf("Window (%dx%d %s) created\n", window_width, window_height, windowed ? "normal" : "fullscreen");

	//
	// show window
	//
	ShowWindow(global_hWnd, virginwindow ? global_nCmdShow : SW_SHOW);
	virginwindow = false;

	LeaveCriticalCode(&windowcriticalcode);
}

/*
=================
VID_FreeWindow
=================
*/
static void VID_FreeWindow(void)
{
	EnterCriticalCode(&windowcriticalcode);
	
	if (global_hDC)
		ReleaseDC(global_hWnd, global_hDC);
	if (global_hWnd)
		DestroyWindow(global_hWnd);

	LeaveCriticalCode(&windowcriticalcode);
}

/*
=================
VID_Init
=================
*/
void VID_Init(void)
{
	DEVMODE dm;

	hCursorArrow = LoadCursor(0, MAKEINTRESOURCE(IDC_ARROW));
	hCursorWait = LoadCursor(0, MAKEINTRESOURCE(IDC_WAIT));
	hCursorCross = LoadCursor(0, MAKEINTRESOURCE(IDC_CROSS));
	hCurrentCursor = GetCursor();
	
	//
	// query current settings to save them for future restore
	//
	dm.dmSize = sizeof(dm);
	dm.dmFields = DM_COMMON_FIELDS;
	if (!EnumDisplaySettings(0, 0, &dm)) {
		COM_DevPrintf("VID_Init: EnumDisplaySettings failed (code 0x%x)\n", GetLastError());
		Sys_Error("Current video mode unavailable");
	}

	origmode.id = ORIGINAL_MODE_ID;
	origmode.width = dm.dmPelsWidth;
	origmode.height = dm.dmPelsHeight;
	origmode.depth = dm.dmBitsPerPel;
	origmode.frequency = dm.dmDisplayFrequency;
	currentmode = &origmode;

	VID_NewDesktopMetrics();

	//
	// discover available video modes
	//
	VID_RefreshModes();
	
	VID_PrintModes();

	//
	// register window class
	//
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpszClassName = (H_NAME "WindowClass");
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = global_hInstance;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

	if (!RegisterClass(&wc)) {
		COM_DevPrintf("VID_Init: RegisterClass failed (code 0x%x)\n", GetLastError());
		Sys_Error("Cannot register window class");
	}

	COM_Printf("Video interface initialized\n");
	vid_initialized = true;
}	
 
/*
=================
VID_Shutdown
=================
*/
void VID_Shutdown(void)
{
	VID_RestoreMode();
	
	VID_FreeWindow();

	VID_FreeModes();
	
	UnregisterClass(wc.lpszClassName, global_hInstance);

	vid_initialized = false;
}

/*
=================
VID_Restart
=================
*/
void VID_Restart(void)
{
	COM_Printf("Restarting video mode...\n");
	
	VID_SetMode(currentmode->id, windowedmode);
}

/*
=================
VID_ModePtr
=================
*/
inline vidmode_t * VID_ModePtr(int modeid)
{
	if (modeid < 0 || modeid >= vidmodesnum)
		return 0;

	return &vidmodes[modeid];
}

/*
=================
VID_ModeProps
=================
*/
qboolean_t VID_ModeProps(int modeid, int *o_width, int *o_height, int *o_depth, int *o_frequency)
{
	vidmode_t *mode;

	EnterCriticalCode(&modecriticalcode);

	mode = VID_ModePtr(modeid);
	if (!mode) {
		COM_DevPrintf("VID_ModeProps: modeid range error\n");
		return false;
	}

	if (o_width) *o_width = mode->width;
	if (o_height) *o_height = mode->height;
	if (o_depth) *o_depth = mode->depth;
	if (o_frequency) *o_frequency = mode->frequency;

	LeaveCriticalCode(&modecriticalcode);
	return true;
}

/*
=================
VID_CurrentModeProps
=================
*/
void VID_CurrentModeProps(int *o_width, int *o_height, int *o_depth, int *o_frequency)
{
	VID_ModeProps(currentmode->id, o_width, o_height, o_depth, o_frequency);
}

/*
=================
VID_CurrentModeFullScreen
=================
*/
qboolean_t VID_CurrentModeFullScreen(void)
{
	return !windowedmode;
}

/*
=================
VID_SetMode
=================
*/
qboolean_t VID_SetMode(int modeid, qboolean_t fullscreen)
{
	DEVMODE dm;
	vidmode_t *mode;

	COM_Printf("Setting video mode %d...\n", modeid);

	inmodechange = true;
	EnterCriticalCode(&modecriticalcode);

	mode = VID_ModePtr(modeid);
	if (!mode)
		Sys_Error("VID_SetMode: modeid range error");
	
	dm.dmSize = sizeof(dm);
	dm.dmFields = DM_COMMON_FIELDS;
	dm.dmPelsWidth = mode->width;
	dm.dmPelsHeight = mode->height;
	dm.dmBitsPerPel = mode->depth;
	dm.dmDisplayFrequency = mode->frequency;
	if (!ChangeDisplaySettings(&dm, 0)) {
		LeaveCriticalCode(&modecriticalcode);
		inmodechange = false;
		return false;
	}
	currentmode = mode;
	inwindowed = !fullscreen;

	VID_NewDesktopMetrics();
	
	VID_NewWindow(mode->width, mode->height, inwindowed);

	LeaveCriticalCode(&modecriticalcode);
	inmodechange = false;

	return true;
}

/*
=================
VID_RestoreMode
=================
*/
void VID_RestoreMode(void)
{
	vidmode_t *mode;
	DEVMODE dm;

	if (!vid_initialized)
		return;

	inmodechange = true;
	EnterCriticalCode(&modecriticalcode);

	mode = &origmode;

	dm.dmSize = sizeof(dm);
	dm.dmFields = DM_COMMON_FIELDS;
	dm.dmPelsWidth = mode->width;
	dm.dmPelsHeight = mode->height;
	dm.dmBitsPerPel = mode->depth;
	dm.dmDisplayFrequency = mode->frequency;

	ChangeDisplaySettings(0, 0, &dm);
	currentmode = mode;
	inwindowed = true;

	LeaveCriticalCode(&modecriticalcode);
	inmodechange = false;
}

/*
=================
VID_RefreshModes
=================
*/
void VID_RefreshModes(void)
{
	DEVMODE dm;
	int i = 0;

	EnterCriticalCode(&modecriticalcode);
	
	VID_FreeModes();

	dm.dmSize = sizeof(dm);
	dm.dmFields 
	while (EnumDisplaySettings(0, i, &dm))
		i++;
	vidmodesnum = i;
	vidmodes = Zone_Alloc(sizeof(vidmode_t) * i);

	dm.dmFields = DM_COMMOM_FIELDS;
	for (i = 0; i < vidmodesnum; i++) {
		if (!EnumDisplaySettings(0, i, &dm)) {
			COM_DevPrintf("VID_RefreshModes: EnumDisplaySettings failed (code 0x%x)\n", GetLastError());
			COM_Printf("Enum on video mode %d failed, breaking...\n", i);
			
			vidmodesnum = i;
			break;
		}

		vidmodes[i].id = i;
		vidmodes[i].width = dm.dmPelsWidth;
		vidmodes[i].height = dm.dmPelsHeight;
		vidmodes[i].depth = dm.dmBitsPerPel;
		vidmodes[i].frequency = dm.dmDisplayFrequency;
	}

	LeaveCriticalCode(&modecriticalcode);
}

/*
=================
VID_FreeModes
=================
*/
static void VID_FreeModes(void)
{
	if (vidmodes)
		Zone_Free(vidmodes);
	vidmodesnum = 0;
}
