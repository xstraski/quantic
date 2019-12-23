// sys_windows.h -- Windows-specific shared code

#ifndef SYS_WINDOWS_H
#define SYS_WINDOWS_H

#include <windows.h>

extern HINSTANCE     global_hInstance;            // original hInstance from WinMain
extern int           global_nCmdShow;             // original nCmdShow from WinMain

extern HWND          global_hWnd;
extern HDC           global_hDC;

extern qboolean_t    ActiveApp;
extern int           desktop_width, dekstop_height;
extern int           desktop_center_x, desktop_center_y;

extern int           window_x, window_y;
extern int           window_center_x, window_center_y;
extern qboolean_t    window_visible, window_minimized;

#endif // #ifndef SYS_WINDOWS_H
