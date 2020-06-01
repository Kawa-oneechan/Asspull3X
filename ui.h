#include "asspull.h"
#include "SDL_syswm.h"
#include "resource.h"
#include <Windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <Uxtheme.h>
#include <vsstyle.h>

#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#define _getcwd getcwd // the POSIX version doesn't have the underscore
#endif

extern int pauseState;
extern bool stretch200, fpsCap, reloadROM, reloadIMG;

extern HWND hWndAbout, hWndMemViewer, hWndOptions, hWndDevices;
extern HFONT headerFont, monoFont;

extern char startingPath[FILENAME_MAX];
extern char lastPath[FILENAME_MAX];

extern HWND hWnd;
extern HINSTANCE hInstance;
extern HWND hWndStatusBar;
extern int statusBarHeight;
extern int idStatus;

extern int uiCommand, uiData, uiKey;
extern char uiString[512];

extern int statusTimer;
extern std::string uiStatus;
extern bool fpsVisible;
extern bool wasPaused;
extern bool autoUpdate;
extern bool fpsCap, stretch200;


extern bool ShowFileDlg(bool toSave, char* target, size_t max, const char* filter);

extern void InsertDisk(int devId);
extern void EjectDisk();

extern HBRUSH hbrBack, hbrStripe, hbrList;
extern COLORREF rgbBack, rgbStripe, rgbText, rgbHeader, rgbList, rgbListBk;
extern void DrawWindowBk(HWND hwndDlg, bool stripe);
extern void DrawCheckbox(HWND hwndDlg, LPNMCUSTOMDRAW dis);
extern void SetThemeColors();
extern bool IsWin10();

extern void ShowAbout();
extern void ShowMemViewer();
extern void ShowOptions();
extern void ShowDevices();

extern void UpdateDevicePage(HWND hwndDlg);
