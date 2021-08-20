#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "ui.h"

static char startingPath[FILENAME_MAX];
static char lastPath[FILENAME_MAX];

HWND hWnd;
HINSTANCE hInstance;
HWND hWndStatusBar;
int statusBarHeight = 0;

int uiCommand;
char uiString[512];

int mouseTimer = -1;
int statusTimer = 0;
std::string uiStatus;
bool fpsVisible = false;
bool wasPaused = false;
bool autoUpdateMemViewer = false;
bool autoUpdatePalViewer = false;
int theme = 0;

HWND hWndAbout = NULL, hWndMemViewer = NULL, hWndOptions = NULL, hWndDevices = NULL, hWndPalViewer = NULL, hWndShaders = NULL;
HFONT headerFont = NULL, monoFont = NULL, statusFont = NULL;
HBRUSH hbrBack = NULL, hbrStripe = NULL, hbrList = NULL;
HPEN hpnStripe = NULL;
COLORREF rgbBack = NULL, rgbStripe = NULL, rgbText = NULL, rgbHeader = NULL, rgbList = NULL, rgbListBk = NULL;
HIMAGELIST hIml = NULL;

bool ShowFileDlg(bool toSave, char* target, size_t max, const char* filter);

void DrawWindowBk(HWND hwndDlg, bool stripe)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwndDlg, &ps);
	DrawWindowBk(hwndDlg, stripe, &ps, hdc);
	EndPaint(hwndDlg, &ps);
}

void DrawWindowBk(HWND hwndDlg, bool stripe, PAINTSTRUCT* ps, HDC hdc)
{
	if (stripe)
	{
		RECT rect = { 0 };
		rect.bottom = 7 + 14 + 7; //margin, button, padding
		MapDialogRect(hwndDlg, &rect);
		auto h = rect.bottom;
		GetClientRect(hwndDlg, &rect);
		rect.bottom -= h;
		FillRect(hdc, &ps->rcPaint, hbrStripe);
		FillRect(hdc, &rect, hbrBack);
	}
	else
	{
		FillRect(hdc, &ps->rcPaint, hbrBack);
	}
	return;
}

void DrawCheckbox(HWND hwndDlg, LPNMCUSTOMDRAW nmc)
{
	int idFrom = nmc->hdr.idFrom;
	switch(nmc->dwDrawStage)
	{
		case CDDS_PREERASE:
		{
			SetBkColor(nmc->hdc, rgbBack);
			SetTextColor(nmc->hdc, rgbText);

			HTHEME hTheme = OpenThemeData(nmc->hdr.hwndFrom, L"BUTTON");
			if(!hTheme)
			{
				CloseThemeData(hTheme);
				SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LONG_PTR)CDRF_DODEFAULT);
				return;
			}

			LRESULT state = SendMessage(nmc->hdr.hwndFrom, BM_GETSTATE, 0, 0 );

			auto checked = IsDlgButtonChecked(hwndDlg, idFrom);
			int stateID = checked ? CBS_CHECKEDNORMAL : CBS_UNCHECKEDNORMAL;
			if (nmc->uItemState & CDIS_SELECTED)
				stateID = checked ? CBS_CHECKEDPRESSED : CBS_UNCHECKEDPRESSED;
			else if (nmc->uItemState & CDIS_HOT)
				stateID = checked ? CBS_CHECKEDHOT : CBS_UNCHECKEDHOT;
                           
			RECT r;
			SIZE s;

			GetThemePartSize(hTheme, nmc->hdc, BP_CHECKBOX, stateID, NULL, TS_TRUE, &s);

			r.left = nmc->rc.left;
			r.top = nmc->rc.top + 3;
			r.right = r.left + s.cx;
			r.bottom = r.top + s.cy;

			DrawThemeBackground(hTheme, nmc->hdc, BP_CHECKBOX, stateID, &r, NULL);

			nmc->rc.left +=  3 + s.cx;

			char text[256];
			GetDlgItemText(hwndDlg, idFrom, text, 255);
			DrawText(nmc->hdc, text, -1, &nmc->rc, DT_SINGLELINE | DT_VCENTER);

			CloseThemeData(hTheme);
			SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LONG_PTR)CDRF_SKIPDEFAULT);
			return;
		}
	}
}

bool DrawDarkButton(HWND hwndDlg, LPNMCUSTOMDRAW nmc)
{
	if (theme == 0)
		return false; //don't draw jack
	int idFrom = nmc->hdr.idFrom;
	switch(nmc->dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			SetBkColor(nmc->hdc, rgbBack);
			SetTextColor(nmc->hdc, rgbText);

			if (nmc->uItemState & CDIS_DISABLED)
				SetTextColor(nmc->hdc, rgbListBk);

			HBRUSH border = CreateSolidBrush(rgbText);

			FillRect(nmc->hdc, &nmc->rc, border);
			InflateRect(&nmc->rc, -1, -1);
			if (nmc->uItemState & CDIS_DEFAULT)
				InflateRect(&nmc->rc, -1, -1);

			FillRect(nmc->hdc, &nmc->rc, (nmc->uItemState & CDIS_SELECTED) ? hbrList : hbrBack);

			if (nmc->uItemState & CDIS_FOCUS)
			{
				InflateRect(&nmc->rc, -1, -1);
				DrawFocusRect(nmc->hdc, &nmc->rc);
			}
		
			DeleteObject(border);

			SetBkColor(nmc->hdc, (nmc->uItemState & CDIS_SELECTED) ? rgbListBk : rgbBack);
			char text[256];
			GetDlgItemText(hwndDlg, idFrom, text, 255);
			DrawText(nmc->hdc, text, -1, &nmc->rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LONG_PTR)CDRF_SKIPDEFAULT);
		}
	}
	return true;
}

bool DrawComboBox(HWND hwndDlg, LPDRAWITEMSTRUCT dis)
{
	if (dis->itemID == -1)
		return false;

	SetTextColor(dis->hDC, dis->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHTTEXT) : rgbList);
	SetBkColor(dis->hDC, dis->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHT) : rgbListBk);

	TEXTMETRIC tm;
	GetTextMetrics(dis->hDC, &tm);
	int y = (dis->rcItem.bottom + dis->rcItem.top - tm.tmHeight) / 2;
	int x = LOWORD(GetDialogBaseUnits()) / 4;
	if (dis->itemState & ODS_COMBOBOXEDIT)
		x += 2;

	char text[256];
	SendMessage(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)text);
	ExtTextOut(dis->hDC, x, y, ETO_CLIPPED | ETO_OPAQUE, &dis->rcItem, text, (UINT)strlen(text), NULL);
	return true;
}

typedef void (WINAPI * fnRtlGetNtVersionNumbers) (LPDWORD major, LPDWORD minor, LPDWORD build);
bool IsWin10()
{
	auto RtlGetNtVersionNumbers = (fnRtlGetNtVersionNumbers)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetNtVersionNumbers");
	if (RtlGetNtVersionNumbers)
	{
		DWORD major, minor, build;
		RtlGetNtVersionNumbers(&major, &minor, &build);
		build &= ~0xF0000000;
		if (major == 10 && minor == 0 && build > 18360)
			return true;
	}
	return false;
}

int MatchTheme()
{
	if (!IsWin10()) return 0;
	DWORD nResult;
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &key)) return 0;
	DWORD dwBufferSize = sizeof(DWORD);
	RegQueryValueEx(key, "AppsUseLightTheme", 0, NULL, (LPBYTE)&nResult, &dwBufferSize);
	RegCloseKey(key);
	//AppsUseLightTheme == 0 means light, and that's the inverse of our list.
	return !nResult;
}

void SetThemeColors()
{
	theme = ini.GetLongValue("media", "theme", 0);

	if (theme == 2) //match
		theme = MatchTheme();

	switch (theme)
	{
		case 0: //light
		{
			rgbBack = RGB(255, 255, 255);
			rgbStripe = RGB(240, 240, 240);
			rgbText = RGB(0, 0, 0);
			rgbList = RGB(0, 0, 0);
			rgbHeader = RGB(0x00, 0x33, 0x99);
			rgbListBk = RGB(255, 255, 255);
			break;
		}
		case 1: //dark
		{
			rgbBack = RGB(42, 42, 42);
			rgbStripe = RGB(76, 74, 72);
			rgbText = RGB(255, 255, 255);
			rgbList = RGB(255, 255, 255);
			rgbHeader = RGB(0x8E, 0xCA, 0xF8);
			rgbListBk = RGB(76, 74, 72);
		}
		//this space for rent
	}
	if (hbrBack != NULL) DeleteObject(hbrBack);
	if (hbrStripe != NULL) DeleteObject(hbrStripe);
	if (hbrList != NULL) DeleteObject(hbrList);
	if (hpnStripe != NULL) DeleteObject(hpnStripe);
	hbrBack = CreateSolidBrush(rgbBack);
	hbrStripe = CreateSolidBrush(rgbStripe);
	hbrList = CreateSolidBrush(rgbListBk);
	hpnStripe = CreatePen(PS_SOLID, 1, rgbStripe);

	if (hWndAbout != NULL) RedrawWindow(hWndAbout, NULL, NULL, RDW_INVALIDATE);
	if (hWndDevices != NULL) RedrawWindow(hWndDevices, NULL, NULL, RDW_INVALIDATE);
	if (hWndMemViewer != NULL) RedrawWindow(hWndMemViewer, NULL, NULL, RDW_INVALIDATE);
	//Being called right before the options window is closed, we don't need to bother with it.
	//if (hWndOptions != NULL) RedrawWindow(hWndOptions, NULL, NULL, RDW_INVALIDATE);
}

HBITMAP GetImageListImage(int index)
{
	//Step one, prepare a canvas.
	auto scrDC = GetDC(0);
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biWidth = bmi.bmiHeader.biHeight = 16;
	auto hbm = CreateDIBSection(scrDC, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
	ReleaseDC(0, scrDC);
	//Step two, draw on it.
	auto dc = CreateCompatibleDC(0);
	auto oldBm = SelectObject(dc, hbm);
	ImageList_Draw(hIml, index, dc, 0, 0, ILD_NORMAL);
	SelectObject(dc, oldBm);
	ReleaseDC(0, dc);

	return hbm;
}

WNDPROC SDLWinProc = NULL;
LRESULT WndProc(HWND hWnd, unsigned int message, WPARAM wParam, LPARAM lParam)
{
	//We get to do a lotta fancy fuckery now that this is a Win32 wndproc and not
	//SDL's watered-down one.
	switch(message)
	{
		case WM_COMMAND:
		{
			if (wParam > 1000 && wParam < 2000)
			{
				uiCommand = (int)(wParam - 1000);
				if (uiCommand >= cmdMemViewer && uiCommand <= cmdDevices)
				{
					void(*commands[])(void) = {
						ShowMemViewer, ShowPalViewer, ShowAbout,
						ShowOptions, ShowShaders, ShowDevices
					};
					commands[uiCommand - cmdMemViewer]();
					uiCommand = 0;
				}
				return 0;
			}
		}
		default:
			return CallWindowProc(SDLWinProc, hWnd, message, wParam, lParam);
	}
}

std::string statusFPS, statusText;

HDC statusRealDC = NULL;
HDC statusDC = NULL;
HBITMAP statusBmp = NULL;
void ResizeStatusBar()
{
	RECT sbRect;
	GetClientRect(hWnd, &sbRect);
	SetWindowPos(hWndStatusBar, HWND_NOTOPMOST, 0, sbRect.bottom - statusBarHeight, sbRect.right, statusBarHeight, SWP_NOZORDER);

	if (statusRealDC) ReleaseDC(hWnd, statusRealDC);
	if (statusDC) DeleteDC(statusDC);
	if (statusBmp) DeleteObject(statusBmp);
	statusRealDC = GetDC(hWndStatusBar);
	statusDC = CreateCompatibleDC(statusRealDC);
	statusBmp = CreateCompatibleBitmap(statusRealDC, sbRect.right, sbRect.bottom);

	SelectObject(statusDC, GetStockObject(DC_PEN));
	SelectObject(statusDC, statusFont);
	SelectObject(statusDC, statusBmp);
}

void DrawStatusBar()
{
	RECT sbRect;
	GetClientRect(hWnd, &sbRect);
	sbRect.bottom = statusBarHeight;

	auto hdc = statusDC;

	FillRect(hdc, &sbRect, hbrBack);

	SetDCPenColor(hdc, rgbStripe);
	MoveToEx(hdc, 0, 0, NULL);
	LineTo(hdc, sbRect.right, 0);
	MoveToEx(hdc, 40, 3, NULL);
	LineTo(hdc, 40, sbRect.bottom - 3);

	SetBkColor(hdc, rgbBack);
	SetTextColor(hdc, rgbText);
	RECT sbText = { 4, 4, 40, sbRect.bottom - 4 };
	DrawText(hdc, statusFPS.c_str(), statusFPS.length(), &sbText, DT_RIGHT);
	sbText.left = 48;
	sbText.right = sbRect.right - 48;
	DrawText(hdc, statusText.c_str(), statusText.length(), &sbText, DT_END_ELLIPSIS); 

	BitBlt(statusRealDC, 0, 0, sbRect.right, sbRect.bottom, hdc, 0, 0, SRCCOPY);
}

extern void AnimateAbout();

void HandleUI()
{
	if (statusTimer)
		statusTimer--;
	else
		statusText = "";

	if (mouseTimer > 0)
	{
		mouseTimer--;
		if (mouseTimer == 0)
			SDL_ShowCursor(false);
	}
	if (mouseTimer == 0 && (SDL_GetModState() & KMOD_RCTRL))
	{
		SDL_ShowCursor(true);
		mouseTimer = 6 * 60;
	}

	DrawStatusBar();

	if (autoUpdateMemViewer && hWndMemViewer != NULL && IsWindowVisible(hWndMemViewer))
		InvalidateRect(GetDlgItem(hWndMemViewer, IDC_MEMVIEWERGRID), NULL, true);
	if (autoUpdatePalViewer && hWndPalViewer != NULL && IsWindowVisible(hWndPalViewer))
		InvalidateRect(GetDlgItem(hWndPalViewer, IDC_MEMVIEWERGRID), NULL, true);

	if (hWndAbout != NULL)
		AnimateAbout();
}

void InitializeUI()
{
	//SetProcessDPIAware();

	GetCurrentDirectory(FILENAME_MAX, startingPath);

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(sdlWindow, &info))
	{
		if (info.subsystem != SDL_SYSWM_WINDOWS)
			return;

		hWnd = info.info.win.window;
		hInstance = info.info.win.hinstance;

		SDLWinProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG)WndProc);

		InitCommonControls();

		hIml = ImageList_Create(16, 16, ILC_COLOR32, 32, 0);
		ImageList_Add(hIml, (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(IDB_ICONS), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION), NULL);

		HMENU menuBar = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAINMENU));

		MENUINFO mainInfo =
		{
			sizeof(MENUINFO),
			MIM_APPLYTOSUBMENUS | MIM_STYLE,
			/* MNS_MODELESS | */ MNS_AUTODISMISS,
			0,
			NULL,
			NULL,
			0
		};
		SetMenuInfo(menuBar, &mainInfo);

		MENUITEMINFO miInfo = { sizeof(MENUITEMINFO), MIIM_BITMAP };
		for (int i = 0; i < 20; i++)
		{
			//auto hBmp = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(1000 + i), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION);
			auto hBmp = GetImageListImage(20 + i);
			if (hBmp == NULL)
				continue;
			miInfo.hbmpItem = hBmp;
			SetMenuItemInfo(menuBar, 1000 + i, FALSE, &miInfo);
		}
		SetMenu(hWnd, menuBar);

		hWndStatusBar = CreateWindowEx(0, "STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 0, 0, 0, hWnd, 0, hInstance, NULL);
		statusBarHeight = 23;
		statusFPS = "     ";

		auto scrDC = GetDC(0);
		auto dpiY = GetDeviceCaps(scrDC, LOGPIXELSY);
		auto headerSize = MulDiv(13, dpiY, 72);
		ReleaseDC(0, scrDC);

		headerFont = CreateFont(headerSize, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Segoe UI");
		monoFont = CreateFont(16, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Courier New");
		statusFont = CreateFont(14, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Segoe UI");

		SetThemeColors();
	}
	return;
}

bool ShowFileDlg(bool toSave, char* target, size_t max, const char* filter)
{
	OPENFILENAME ofn;
	char sFilter[512];
	strcpy_s(sFilter, 512, filter);
	char* f = sFilter;
	while (*f)
	{
		if (*f == '|')
			*f = 0;
		f++;
	}
	*f++ = 0;
	*f++ = 0;

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = target;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = max;
	ofn.lpstrFilter = sFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	char cwd[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, cwd);

	if ((!toSave && (GetOpenFileName(&ofn) == TRUE)) || (toSave && (GetSaveFileName(&ofn) == TRUE)))
	{
		SetCurrentDirectory(cwd);
		strcpy_s(target, max, ofn.lpstrFile);
		return true;
	}

	SetCurrentDirectory(cwd);
	return false;
}

void SetStatus(std::string text)
{
	statusText = text;
	statusTimer = 4 * text.length();
}

void SetStatus(int stab)
{
	char b[256];
	LoadString(hInstance, stab, b, 256);
	SetStatus(b);
}

void SetFPS(int fps)
{
	if (fpsVisible)
		itoa(fps, &statusFPS[0], 10); //I too like to live dangerously.
	else
		statusFPS = "     ";
}

static char getStringBuffer[512];
char* GetString(int stab)
{
	LoadString(hInstance, stab, getStringBuffer, 512);
	return getStringBuffer;
}

extern unsigned char* pixels;

#define MAXSNOW 256
int snowData[MAXSNOW * 2];
int snowTimer = -1;

void LetItSnow()
{
	if (snowTimer == -1)
	{
		//Prepare
		srand(0xC001FACE);
		for (int i = 0; i < MAXSNOW; i++)
		{
			snowData[(i * 2) + 0] = rand() % 640;
			snowData[(i * 2) + 1] = rand() % 480;
		}
		snowTimer = 1;
	}

	snowTimer--;
	if (snowTimer == 0)
	{
		snowTimer = 4;
		for (int i = 0; i < MAXSNOW; i++)
		{
			snowData[(i * 2) + 0] += -1 + (rand() % 3);
			snowData[(i * 2) + 1] += rand() % 2;
			if (snowData[(i * 2) + 1] >= 480)
			{
				snowData[(i * 2) + 0] = rand() % 640;
				snowData[(i * 2) + 1] = -10;
			}
		}
	}

	for (int i = 0; i < MAXSNOW; i++)
	{
		int x = snowData[(i * 2) + 0];
		int y = snowData[(i * 2) + 1];
		if (x < 0 || y < 0 || x >= 640 || y >= 480)
			continue;
		auto target = ((y * 640) + x) * 4;
		pixels[target + 0] = pixels[target + 1] = pixels[target + 2] = 255;
	}
}

void ResetPath()
{
	SetCurrentDirectory(startingPath);
}

void ShowOpenFileDialog(int command, int data, std::string pattern)
{
	char* thing = (char*)ini.GetValue("media", "lastRom", "");
	if (thing[0] == 0)
	{
		thing = lastPath;
		if (thing[0] == 0)
			thing = startingPath;
		if (thing[strlen(thing)] != '\\')
			strcat(thing, "\\");
	}
	if (thing[0] != 0)
	{
		auto lastSlash = (char*)strrchr(thing, '\\');
		if (lastSlash)
			*(lastSlash + 1) = 0;
	}

	uiCommand = 0;

	char thePath[FILENAME_MAX] = { 0 };
	strcpy_s(thePath, FILENAME_MAX, thing);
	if (ShowFileDlg(false, thePath, 256, pattern.c_str()))
	{
		strcpy_s(uiString, 512, thePath);
		uiCommand = command;
	}

	//if (pauseState == 0)
	//	pauseState = 1;
}

void InsertDisk(int devId)
{
	ShowOpenFileDialog(cmdInsertDisk, devId, (((DiskDrive*)devices[devId])->GetType() == ddDiskette ? GetString(IDS_DDFILTER) : GetString(IDS_HDFILTER)));
	if (uiCommand == 0)
		return;
	char key[16];
	sprintf_s(key, 16, "%d", devId);
	auto ret = ((DiskDrive*)devices[devId])->Mount(uiString);
	if (ret == -1)
		SetStatus(IDS_EJECTFIRST); //"Eject the diskette first, with Ctrl-Shift-U."
	else if (ret != 0)
		SDL_Log("Error %d trying to open disk image.", ret);
	else
	{
		if (((DiskDrive*)devices[devId])->GetType() == ddDiskette)
			ini.SetValue("devices/diskDrive", key, uiString);
		else
			ini.SetValue("devices/hardDrive", key, uiString);
		ResetPath();
		ini.SaveFile("settings.ini");
		if (hWndDevices != NULL) UpdateDevicePage(hWndDevices);
	}
}

void EjectDisk(int devId)
{
	((DiskDrive*)devices[devId])->Unmount();
	char key[16];
	sprintf_s(key, 16, "%d", devId);
	if (((DiskDrive*)devices[devId])->GetType() == ddDiskette)
		ini.SetValue("devices/diskDrive", key, "");
	else
		ini.SetValue("devices/hardDrive", key, "");
	ResetPath();
	ini.SaveFile("settings.ini");
	SetStatus(IDS_DISKEJECTED); //"Disk ejected."
	if (hWndDevices != NULL) UpdateDevicePage(hWndDevices);
}
