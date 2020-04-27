#if !WIN32
#error Sorry, Windows only.
#endif

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "../asspull.h"
#include "SDL_syswm.h"
#include "resource.h"
#include <Windows.h>
#include <commdlg.h>
#include <commctrl.h>

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

char startingPath[FILENAME_MAX];
char lastPath[FILENAME_MAX];

HWND hWnd;
HINSTANCE hInstance;
HWND hWndStatusBar;
int statusBarHeight = 0;
int idStatus;

int uiCommand, uiData, uiKey;
char uiString[512];

int statusTimer = 0;
std::string uiStatus;
bool fpsVisible = false;
bool wasPaused = false;
bool autoUpdate = false;

HWND hWndAbout = NULL, hWndMemViewer = NULL, hWndOptions = NULL, hWndDevices = NULL;

bool ShowFileDlg(bool toSave, char* target, size_t max, const char* filter);
void InsertDisk(int devId);
void EjectDisk();

BOOL CALLBACK AboutWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
	case WM_COMMAND:
		//EndDialog(hwndDlg, wParam);
		DestroyWindow(hwndDlg);
		hWndAbout = NULL;
		return true;
	}
	return false;
}

unsigned long memViewerOffset;
extern "C" unsigned int m68k_read_memory_8(unsigned int address);

void MemViewerDraw(DRAWITEMSTRUCT* dis)
{
	//auto hdc = dis->hDC;
	RECT rect;
	GetClientRect(dis->hwndItem, &rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	HDC hdc = CreateCompatibleDC(NULL);
	auto bmp = CreateCompatibleBitmap(hdc, w, h);
	auto oldBmp = SelectObject(hdc, bmp);

	FillRect(hdc, &dis->rcItem, (HBRUSH)GetStockObject(WHITE_BRUSH));
	auto oldFont = SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));
	SIZE fontSize;
	GetTextExtentPoint(hdc, "0", 1, &fontSize);
	SetTextColor(hdc, RGB(0,0,0));
	SetBkColor(hdc, RGB(255,255,255));
	RECT r;
	r.top = 3;
	r.left = 3;
	r.bottom = r.top + fontSize.cy;
	r.right = rect.right - 3;

	char buf[256] = { 0 };

	const char hex[] = "0123456789ABCDEF";
	unsigned int offset = memViewerOffset;
	for (int row = 0; row < 16; row++)
	{
		r.left = 3;
		sprintf(buf, "%08X", offset);
		DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);

		for (int col = 0; col < 8; col++)
		{
			auto here = m68k_read_memory_8(offset++);
			r.left = 3 + 80 + (col * 22);
			sprintf(buf, "%c%c", hex[(here & 0xF0) >> 4], hex[here & 0x0F]);
			DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
			r.left = 3 + 266 + (col * fontSize.cx);
			sprintf(buf, "%c", here);
			DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
		}

		r.top += fontSize.cy;
		r.bottom += fontSize.cy;
	}

	SelectObject(hdc, oldFont);

	BitBlt(dis->hDC, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
	SelectObject(hdc, oldBmp);
	DeleteDC(hdc);
	DeleteObject(bmp);
}

void SetMemViewer(HWND hwndDlg, int to)
{
	auto max = VRAM_ADDR + VRAM_SIZE - (16 * 8);
	if (to < 0) to = 0;
	if (to > max) to = max;
	memViewerOffset = to;
	char asText[64] = { 0 };
	sprintf_s(asText, 64, "%08X", memViewerOffset);
	SetDlgItemText(hwndDlg, IDC_MEMVIEWEROFFSET, asText);
	InvalidateRect(GetDlgItem(hwndDlg, IDC_MEMVIEWERGRID), NULL, true);
	SetScrollPos(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, to / 8, true);
}

void MemViewerScroll(HWND hwndDlg, int message, int position)
{
	switch (message)
	{
		case SB_BOTTOM:
			SetMemViewer(hwndDlg, ((VRAM_ADDR + VRAM_SIZE) / 8) - 16);
			return;
		case SB_TOP:
			SetMemViewer(hwndDlg, 0);
			return;
		case SB_LINEDOWN:
			SetMemViewer(hwndDlg, memViewerOffset + (16 * 8));
			return;
		case SB_LINEUP:
			SetMemViewer(hwndDlg, memViewerOffset - (16 * 8));
			return;
		case SB_PAGEDOWN:
			SetMemViewer(hwndDlg, memViewerOffset + (256 * 8));
			return;
		case SB_PAGEUP:
			SetMemViewer(hwndDlg, memViewerOffset - (256 * 8));
			return;
		case SB_THUMBPOSITION:
		{
			SCROLLINFO si;
			ZeroMemory(&si, sizeof(si));
			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, &si);
			auto newTo = si.nTrackPos;
			SetMemViewer(hwndDlg, newTo * 8);
		}
	}
}

void MemViewerComboProc(HWND hwndDlg)
{
	int index = SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_GETCURSEL, 0, 0);
	uint32_t areas[] = { BIOS_ADDR, CART_ADDR, WRAM_ADDR, DEVS_ADDR, REGS_ADDR, VRAM_ADDR };
	SetMemViewer(hwndDlg, areas[index]);
}

WNDPROC oldTextProc;
LRESULT CALLBACK MemViewerEditProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYDOWN:
			switch (wParam)
			{
				case VK_RETURN:
					char thing[16] = { 0 };
					GetWindowText(wnd, thing, 16);
					SetMemViewer(hWndMemViewer, strtol(thing, NULL, 16));
					return 0;
			}
		default:
			return CallWindowProc(oldTextProc, wnd, msg, wParam, lParam);
	}
	return 0;
}

BOOL CALLBACK MemViewerWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
		{
			DestroyWindow(hwndDlg);
			hWndMemViewer = NULL;
			return true;
		}
		case WM_INITDIALOG:
		{
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, WM_SETFONT, (WPARAM)GetStockObject(SYSTEM_FIXED_FONT), false);
			LPCSTR areas[] = { "BIOS", "Cart", "WRAM", "Devices", "Registers", "VRAM" };
			for (int i = 0; i < 6; i++)
				SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_ADDSTRING, 0, (LPARAM)areas[i]);
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_SETCURSEL, 1, 0);
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, EM_SETLIMITTEXT, 8, 0);
			oldTextProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_MEMVIEWEROFFSET), GWLP_WNDPROC, (LONG_PTR)MemViewerEditProc);
			SetScrollRange(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, 0, ((VRAM_ADDR + VRAM_SIZE) / 8) - 16, false);
			CheckDlgButton(hwndDlg, IDC_AUTOUPDATE, autoUpdate);
			MemViewerComboProc(hwndDlg); //force update
			return true;
		}
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDC_MEMVIEWERDROP)
			{
				if (HIWORD(wParam) == CBN_SELCHANGE)
				{
					MemViewerComboProc(hwndDlg);
					return true;
				}
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_AUTOUPDATE)
			{
				autoUpdate = (IsDlgButtonChecked(hwndDlg, IDC_AUTOUPDATE) == 1);
				return true;
			}
		}
		case WM_DRAWITEM:
			if (wParam == IDC_MEMVIEWERGRID)
			{
				MemViewerDraw((DRAWITEMSTRUCT*)lParam);
				return true;
			}
		case WM_VSCROLL:
			MemViewerScroll(hwndDlg, LOWORD(wParam), HIWORD(wParam));
			return true;
	}
	return false;
}

extern bool fpsCap, stretch200;

BOOL CALLBACK OptionsWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
	{
		DestroyWindow(hwndDlg);
		hWndOptions = NULL;
		return true;
	}
	case WM_INITDIALOG:
	{
		CheckDlgButton(hwndDlg, IDC_FPSCAP, fpsCap);
		CheckDlgButton(hwndDlg, IDC_ASPECT, stretch200);
		return true;
	}
	case WM_COMMAND:
	{
		ResetPath();
		if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SHOWFPS)
		{
			fpsVisible = (IsDlgButtonChecked(hwndDlg, IDC_SHOWFPS) == 1);
			ini.SetBoolValue("video", "showFps", fpsVisible);
			ini.SaveFile("settings.ini");
			return true;
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_FPSCAP)
		{
			fpsCap = (IsDlgButtonChecked(hwndDlg, IDC_FPSCAP) == 1);
			ini.SetBoolValue("video", "fpsCap", fpsCap);
			ini.SaveFile("settings.ini");
			return true;
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_ASPECT)
		{
			stretch200 = (IsDlgButtonChecked(hwndDlg, IDC_ASPECT) == 1);
			ini.SetBoolValue("video", "stretch200", stretch200);
			ini.SaveFile("settings.ini");
			return true;
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_RELOAD)
		{
			reloadROM = (IsDlgButtonChecked(hwndDlg, IDC_RELOAD) == 1);
			ini.SetBoolValue("media", "reloadRom", reloadROM);
			ini.SaveFile("settings.ini");
			return true;
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_REMOUNT)
		{
			reloadIMG = (IsDlgButtonChecked(hwndDlg, IDC_REMOUNT) == 1);
			ini.SetBoolValue("media", "reloadImg", reloadIMG);
			ini.SaveFile("settings.ini");
			return true;
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK)
		{
			DestroyWindow(hwndDlg);
			hWndOptions = NULL;
			return true;
		}
	}
	}
	return false;
}

void UpdateDevicePage(HWND hwndDlg)
{
	int devNum = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
	auto device = devices[devNum];

	//Don't allow changing device #0 from disk drive
	EnableWindow(GetDlgItem(hwndDlg, IDC_DEVTYPE), (devNum > 0));

	//Hide everything regardless at first.
	int everything[] = { IDC_DEVNONE, IDC_DDFILE, IDC_DDINSERT, IDC_DDEJECT };
	for (int i = 0; i < 4; i++)
		ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_HIDE);

	if (device == NULL)
	{
		ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
		SetDlgItemText(hwndDlg, IDC_HEADER, "No device");
		SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, LB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hwndDlg, IDC_DECO, STM_SETICON, (WPARAM)LoadImage(hInstance, MAKEINTRESOURCE(IDI_BLANK), IMAGE_ICON, 0, 0, 0), 0);
	}
	else
	{
		switch (device->GetID())
		{
		case 0x0144:
		{
			for (int i = 1; i < 4; i++)
				ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_SHOW);
			SetDlgItemText(hwndDlg, IDC_HEADER, (((DiskDrive*)device)->GetType() == ddHardDisk) ? "Hard drive" : "Diskette drive");
			SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, LB_SETCURSEL, (((DiskDrive*)device)->GetType() == ddHardDisk) ? 2 : 1, 0);
			char key[8] = { 0 };
			SDL_itoa(devNum, key, 10);
			auto val = ini.GetValue("devices/diskDrive", key, "");
			SetDlgItemText(hwndDlg, IDC_DDFILE, val);
			EnableWindow(GetDlgItem(hwndDlg, IDC_DDINSERT), val[0] == 0);
			EnableWindow(GetDlgItem(hwndDlg, IDC_DDEJECT), val[0] != 0);
			SendDlgItemMessage(hwndDlg, IDC_DECO, STM_SETICON, (WPARAM)LoadImage(hInstance, MAKEINTRESOURCE((((DiskDrive*)device)->GetType() == ddHardDisk) ? IDI_HARDDRIVE :IDI_DISKDRIVE), IMAGE_ICON, 0, 0, 0), 0);
			break;
		}
		case 0x4C50:
		{
			ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
			SetDlgItemText(hwndDlg, IDC_HEADER, "Line printer");
			SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, LB_SETCURSEL, 3, 0);
			SendDlgItemMessage(hwndDlg, IDC_DECO, STM_SETICON, (WPARAM)LoadImage(hInstance, MAKEINTRESOURCE(IDI_PRINTER), IMAGE_ICON, 0, 0, 0), 0);
			break;
		}
		}
	}
}

void UpdateDeviceList(HWND hwndDlg)
{
	int selection = 0;
	if (SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCOUNT, 0, 0))
		selection = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
	char item[64] = { 0 };
	SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_RESETCONTENT, 0, 0);
	for (int i = 0; i < MAXDEVS; i++)
	{
		if (devices[i] == NULL)
		{
			sprintf_s(item, 64, "%d. Nothing", i + 1);
		}
		else
		{
			switch (devices[i]->GetID())
			{
			case 0x0144:
				if (((DiskDrive*)devices[i])->GetType() == ddDiskette)
					sprintf_s(item, 64, "%d. Diskette drive", i + 1);
				else
					sprintf_s(item, 64, "%d. Hard drive", i + 1);
				break;
			case 0x4C50:
				sprintf_s(item, 64, "%d. Line printer", i + 1);
				break;
			}
		}
		SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_ADDSTRING, 0, (LPARAM)item);
	}

	SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_SETCURSEL, selection, 0);
	UpdateDevicePage(hwndDlg);
}

void SwitchDevice(HWND hwndDlg)
{
	int devNum = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
	int newType = SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, LB_GETCURSEL, 0, 0);

	int oldType = 0;
	if (devices[devNum] != NULL)
	{
		switch (devices[devNum]->GetID())
		{
		case 0x0144:
			oldType = 1;
			if (((DiskDrive*)devices[devNum])->GetType() == ddHardDisk)
				oldType = 2;
			break;
		case 0x4C50:
			oldType = 2;
			break;
		}
	}

	if (newType == oldType)
		return;

	char key[8] = { 0 };
	SDL_itoa(devNum, key, 10);

	if (devices[devNum] != NULL) delete devices[devNum];
	switch (newType)
	{
		case 0:
			devices[devNum] = NULL;
			//ini.SetValue("devices", key, "");
			ini.Delete("devices", key, true);
			break;
		case 1:
			devices[devNum] = (Device*)(new DiskDrive(0));
			ini.SetValue("devices", key, "diskDrive");
			break;
		case 2:
			devices[devNum] = (Device*)(new DiskDrive(1));
			ini.SetValue("devices", key, "hardDrive");
			break;
		case 3:
			devices[devNum] = (Device*)(new LinePrinter());
			ini.SetValue("devices", key, "linePrinter");
			break;
	}
	UpdateDeviceList(hwndDlg);
}

BOOL CALLBACK DevicesWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
	{
		DestroyWindow(hwndDlg);
		hWndDevices = NULL;
		if (!wasPaused) pauseState = 0;
		return true;
	}
	case WM_INITDIALOG:
	{
		LPCSTR devices[] = { "Nothing", "Diskette drive", "Hard drive", "Line printer" };
		for (int i = 0; i < 4; i++)
			SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, LB_ADDSTRING, 0, (LPARAM)devices[i]);
		UpdateDeviceList(hwndDlg);
		return true;
	}
	case WM_COMMAND:
	{
		if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == IDC_DEVLIST)
		{
			UpdateDevicePage(hwndDlg);
			return true;
		}
		if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == IDC_DEVTYPE)
		{
			SwitchDevice(hwndDlg);
			return true;
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_DDINSERT)
		{
			int devID = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
			InsertDisk(devID);
			return true;
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_DDEJECT)
		{
			EjectDisk();
		}
		else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK)
		{
			DestroyWindow(hwndDlg);
			hWndDevices = NULL;
			if (!wasPaused) pauseState = 0;
			return true;
		}
	}
	}
	return false;
}

void WndProc(void* userdata, void* hWnd, unsigned int message, Uint64 wParam, Sint64 lParam)
{
	if (message == WM_COMMAND)
	{
		if (wParam > 1000 && wParam < 2000)
		{
			uiCommand = (int)(wParam - 1000);
			if (uiCommand == cmdAbout)
			{
				uiCommand = cmdNone;
				if (!IsWindow(hWndAbout))
				{
					hWndAbout = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_ABOUT), (HWND)hWnd, (DLGPROC)AboutWndProc);
					ShowWindow(hWndAbout, SW_SHOW);
				}
			}
			else if (uiCommand == cmdMemViewer)
			{
				uiCommand = cmdNone;
				if (!IsWindow(hWndMemViewer))
				{
					hWndMemViewer = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MEMVIEWER), (HWND)hWnd, (DLGPROC)MemViewerWndProc);
					ShowWindow(hWndMemViewer, SW_SHOW);
				}
			}
			else if (uiCommand == cmdOptions)
			{
				uiCommand = cmdNone;
				if (!IsWindow(hWndOptions))
				{
					hWndOptions = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_OPTIONS), (HWND)hWnd, (DLGPROC)OptionsWndProc);
					ShowWindow(hWndOptions, SW_SHOW);
				}
			}
			else if (uiCommand == cmdDevices)
			{
				uiCommand = cmdNone;
				if (!IsWindow(hWndDevices))
				{
					wasPaused = pauseState > 0;
					if (!wasPaused) pauseState = 1;
					hWndDevices = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICES), (HWND)hWnd, (DLGPROC)DevicesWndProc);
					ShowWindow(hWndDevices, SW_SHOW);
				}
			}
		}
	}
}

void ResizeStatusBar()
{
	SendMessage(hWndStatusBar, WM_SIZE, SIZE_RESTORED, 0);
}

void HandleUI()
{
	if (statusTimer)
		statusTimer--;
	else
		SendMessage(hWndStatusBar, SB_SETTEXT, 1 | (SBT_NOBORDERS << 8), (LPARAM)"");

	if (autoUpdate && hWndMemViewer != NULL)
		InvalidateRect(GetDlgItem(hWndMemViewer, IDC_MEMVIEWERGRID), NULL, true);
}

void InitializeUI()
{
	_getcwd(startingPath, FILENAME_MAX);

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(SDL_GetWindowWMInfo(sdlWindow, &info))
	{
		if (info.subsystem != SDL_SYSWM_WINDOWS)
			return;

		hWnd = info.info.win.window;
		hInstance = info.info.win.hinstance;

		SDL_SetWindowsMessageHook(WndProc, 0);

		HMENU menuBar = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAINMENU));

		MENUINFO mainInfo =
		{
			sizeof(MENUINFO),
			MIM_APPLYTOSUBMENUS | MIM_STYLE,
			MNS_MODELESS | MNS_AUTODISMISS,
			0,
			NULL,
			NULL,
			0
		};
		SetMenuInfo(menuBar, &mainInfo);

		SetMenu(hWnd, menuBar);

		hWndStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL, SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)idStatus, hInstance, NULL);
		int parts[] = { 48, -1 };
		SendMessage(hWndStatusBar, SB_SETPARTS, (WPARAM)2, (LPARAM)parts);
		LPRECT lpRect;
		GetWindowRect(hWndStatusBar, lpRect);
		statusBarHeight = lpRect->bottom - lpRect->top;
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
	SendMessage(hWndStatusBar, SB_SETTEXT, 1 | (SBT_NOBORDERS << 8), (LPARAM)text.c_str());
	statusTimer = 100;
}

void SetFPS(int fps)
{
	if (fpsVisible)
	{
		char b[8] = { 0 };
		sprintf(b, "%3d", fps);
		SendMessage(hWndStatusBar, SB_SETTEXT, 0, (LPARAM)b);
	}
	else
		SendMessage(hWndStatusBar, SB_SETTEXT, 0, (LPARAM)"");
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

int mouseTimer = 0;
int lastMouseTimer = 0, lastX = 0, lastY = 0;
int justClicked = 0;
extern int winWidth, winHeight, scrWidth, scrHeight, scale, offsetX, offsetY;

int GetMouseState(int *x, int *y)
{
	if (mouseTimer == lastMouseTimer)
	{
		if (x != NULL) *x = lastX;
		if (y != NULL) *y = lastY;
		return (justClicked == 2) ? 1 : 0;
	}
	int buttons = SDL_GetMouseState(x, y);

	lastMouseTimer = mouseTimer;
	if (justClicked == 0 && buttons == 1)
		justClicked = 1;
	else if (justClicked == 1 && buttons == 0)
		justClicked = 2;
	else if (justClicked == 2)
		justClicked = 0;

	if (x == NULL || y == NULL)
		return (justClicked == 2) ? 1 : 0;

	int uX = *x, uY = *y;
	int nX = *x, nY = *y;

	nX = (nX - offsetX) / scale;
	nY = (nY - offsetY) / scale;

	//if (customMouse)
	//	SDL_ShowCursor((nX < 0 || nY < 0 || nX > 640 || nY > 480));

	*x = nX;
	*y = nY;
	lastX = nX;
	lastY = nY;

	return (justClicked == 2) ? 1 : 0;
}

void ResetPath()
{
	chdir(startingPath);
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
#ifdef _MSC_VER
		//_chdir(thing);
#else
		//TODO
		//chdir(thing);
#endif
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
	ShowOpenFileDialog(cmdInsertDisk, uiData, (((DiskDrive*)devices[uiData])->GetType() == ddDiskette ? "Disk images (*.img)|*.img" : "Disk images (*.vhd)|*.vhd"));
	if (uiCommand == 0)
		return;
	char key[16];
	sprintf(key, "%d", uiData);
	auto ret = ((DiskDrive*)devices[uiData])->Mount(uiString);
	if (ret == -1)
		SetStatus("Eject the diskette first, with Ctrl-Shift-U.");
	else if (ret != 0)
		SDL_Log("Error %d trying to open disk image.", ret);
	else
	{
		if (((DiskDrive*)devices[uiData])->GetType() == ddDiskette)
			ini.SetValue("devices/diskDrive", key, uiString);
		else
			ini.SetValue("devices/hardDrive", key, uiString);
		ResetPath();
		ini.SaveFile("settings.ini");
		if (hWndDevices != NULL) UpdateDevicePage(hWndDevices);
	}
}

void EjectDisk()
{
	((DiskDrive*)devices[uiData])->Unmount();
	char key[16]; sprintf(key, "%d", uiData);
	if (((DiskDrive*)devices[uiData])->GetType() == ddDiskette)
		ini.SetValue("devices/diskDrive", key, "");
	else
		ini.SetValue("devices/hardDrive", key, "");
	ResetPath();
	ini.SaveFile("settings.ini");
	SetStatus("Disk ejected.");
	if (hWndDevices != NULL) UpdateDevicePage(hWndDevices);
}
