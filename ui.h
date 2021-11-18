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

extern WCHAR startingPath[FILENAME_MAX];
extern WCHAR lastPath[FILENAME_MAX];

extern HWND hWndMain;
extern HINSTANCE hInstance;
extern HWND hWndStatusBar;
extern int statusBarHeight;

extern int statusTimer;
extern WCHAR uiStatus[512];
extern bool fpsVisible;
extern bool wasPaused;
extern bool fpsCap, stretch200;

extern bool ShowFileDlg(bool toSave, WCHAR* target, size_t max, const WCHAR* filter);

extern void InsertDisk(int devId);
extern void EjectDisk(int devId);

namespace Presentation
{
	extern HBRUSH hbrBack, hbrStripe, hbrList;
	extern COLORREF rgbBack, rgbStripe, rgbText, rgbHeader, rgbList, rgbListBk;
	extern HFONT headerFont, monoFont;

	extern void DrawWindowBk(HWND hwndDlg, bool stripe);
	extern void DrawWindowBk(HWND hwndDlg, bool stripe, PAINTSTRUCT* ps, HDC hdc);
	extern void DrawCheckbox(HWND hwndDlg, LPNMCUSTOMDRAW dis);
	extern bool DrawDarkButton(HWND hwndDlg, LPNMCUSTOMDRAW dis);
	extern bool DrawComboBox(LPDRAWITEMSTRUCT dis);
	extern void GetIconPos(HWND hwndDlg, int ctlID, RECT* iconRect, int leftOffset, int topOffset);
	namespace Windows10
	{
		extern bool IsWin10();
	}
	extern void SetThemeColors();
}

namespace Images
{
	extern HIMAGELIST hIml;
	extern HBITMAP GetImageListImage(int);
	extern HBITMAP LoadPNGResource(int);
}

namespace About
{
	extern void Show();
	extern HWND hWnd;
}
namespace MemoryViewer
{
	extern void Show();
	extern HWND hWnd;
}
namespace Options
{
	extern void Show();
	extern HWND hWnd;
}
namespace DeviceManager
{
	extern void UpdatePage(HWND hwndDlg);
	extern void Show();
	extern HWND hWnd;
}
namespace PalViewer
{
	extern void Show();
	extern HWND hWnd;
}
namespace Shaders
{
	extern void Show();
	extern HWND hWnd;
}
