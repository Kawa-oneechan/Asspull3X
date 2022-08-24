#pragma once
#include <Windows.h>

namespace UI
{
	extern int uiCommand;

	extern WCHAR startingPath[FILENAME_MAX];
	extern WCHAR lastPath[FILENAME_MAX];

	extern HWND hWndMain;
	extern HINSTANCE hInstance;
	extern HWND hWndStatusBar;
	extern int statusBarHeight;
	extern int diskIconTimer, hddIconTimer;

	extern bool mouseLocked;

	extern int statusTimer;
	extern WCHAR uiStatus[512];
	extern WCHAR uiString[512];
	extern bool fpsVisible, fpsCap, reloadROM, reloadIMG;
	extern bool hideUI;

	extern bool wasPaused;

	extern bool ShowFileDlg(bool toSave, WCHAR* target, size_t max, const WCHAR* filter);
	extern bool ReportLoadingFail(int messageId, int err, int device, const WCHAR* fileName, bool offerToForget = false);

	extern void InsertDisk(int devId);
	extern void EjectDisk(int devId);

	extern WCHAR settingsFile[FILENAME_MAX];

	extern FILE* logFile;

	extern WCHAR* GetString(int stab);
	extern void SetStatus(const WCHAR*);
	extern void SetStatus(int);
	extern void ResizeStatusBar();
	extern void SetFPS(int fps);
	extern void ShowOpenFileDialog(int, const WCHAR*);
	extern void LetItSnow();
	extern void Initialize();
	extern void Update();
	extern void ResetPath();
	extern void SetTitle(const WCHAR*);
	extern void Complain(int message);
	extern void SaveINI();
	extern void HideUI(bool hide);

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

	namespace Tooltips
	{
		extern void Initialize();

		extern void CreateTooltips(HWND hWndClient, ...);

		extern void DestroyTooltips();
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
		extern void UpdatePage();
		extern void Show();
		extern HWND hWnd;
	}
	namespace PalViewer
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace TileViewer
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace Shaders
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace ButtonMaps
	{
		extern void Show();
		extern HWND hWnd;
	}
}
