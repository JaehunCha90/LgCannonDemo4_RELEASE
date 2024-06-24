#pragma once

#include "resource.h"

#define WM_CLIENT_LOST         WM_USER+1
#define WM_REMOTE_CONNECT      WM_USER+2
#define WM_REMOTE_LOST         WM_USER+3
#define WM_SYSTEM_STATE        WM_USER+4

#define WM_CAM_STATE           WM_USER+5
#define WM_MT_IMAGE            WM_USER+6


#define CAM_DEFAULT_STATE  (CAM_ON)


#define VIDEO_PORT       5000

//#define NOT_USE_SERVER      1

extern HWND hWndMain;

extern int PRINT(const TCHAR* fmt, ...);
extern void DisplayMessageOkBox(const char* Msg);
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------
