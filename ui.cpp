#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "ui.h"

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
				ShowAbout();
			}
			else if (uiCommand == cmdMemViewer)
			{
				uiCommand = cmdNone;
				ShowMemViewer();
			}
			else if (uiCommand == cmdOptions)
			{
				uiCommand = cmdNone;
				ShowOptions();
			}
			else if (uiCommand == cmdDevices)
			{
				uiCommand = cmdNone;
				ShowDevices();
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
			/* MNS_MODELESS | */ MNS_AUTODISMISS,
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
		RECT rect;
		GetWindowRect(hWndStatusBar, &rect);
		statusBarHeight = rect.bottom - rect.top;

		headerFont = CreateFont(18, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Segoe UI");
		monoFont = CreateFont(16, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Courier New");
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
