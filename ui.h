#include "asspull.h"
#include "SDL_syswm.h"
#include "resource.h"
#include <Windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <Uxtheme.h>
#include <vsstyle.h>

extern int pauseState;
extern bool stretch200, fpsCap, reloadROM, reloadIMG;

extern HWND hWndAbout, hWndMemViewer, hWndOptions, hWndDevices, hWndPalViewer, hWndShaders;
extern HFONT headerFont, monoFont;

extern WCHAR startingPath[FILENAME_MAX];
extern WCHAR lastPath[FILENAME_MAX];

extern HWND hWnd;
extern HINSTANCE hInstance;
extern HWND hWndStatusBar;
extern int statusBarHeight;

extern int statusTimer;
extern WCHAR uiStatus[512];
extern bool fpsVisible;
extern bool wasPaused;
extern bool autoUpdateMemViewer, autoUpdatePalViewer;
extern bool fpsCap, stretch200;

extern bool ShowFileDlg(bool toSave, WCHAR* target, size_t max, const WCHAR* filter);

extern void InsertDisk(int devId);
extern void EjectDisk(int devId);

extern HBRUSH hbrBack, hbrStripe, hbrList;
extern COLORREF rgbBack, rgbStripe, rgbText, rgbHeader, rgbList, rgbListBk;
extern void DrawWindowBk(HWND hwndDlg, bool stripe);
extern void DrawWindowBk(HWND hwndDlg, bool stripe, PAINTSTRUCT* ps, HDC hdc);
extern void DrawCheckbox(HWND hwndDlg, LPNMCUSTOMDRAW dis);
extern bool DrawDarkButton(HWND hwndDlg, LPNMCUSTOMDRAW dis);
extern bool DrawComboBox(HWND hwndDlg, LPDRAWITEMSTRUCT dis);
extern void SetThemeColors();
extern bool IsWin10();
extern HBITMAP LoadImageFromPNG(int);

extern void ShowAbout();
extern void ShowMemViewer();
extern void ShowOptions();
extern void ShowDevices();
extern void ShowPalViewer();
extern void ShowShaders();

extern void UpdateDevicePage(HWND hwndDlg);
