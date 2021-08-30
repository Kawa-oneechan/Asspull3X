#include "asspull.h"
#include "resource.h"
#include "miniz.h"
#include <io.h>
#include <fcntl.h>
#include <commctrl.h>

extern "C" {
#include "musashi/m68k.h"
}

CSimpleIni ini;

bool reloadROM, reloadIMG;
extern bool fpsVisible, fpsCap;
extern unsigned int biosSize, romSize;
extern long rtcOffset;
extern int invertButtons;
extern int pauseState;

extern void InitializeUI();
extern void ShowOpenFileDialog(int, const WCHAR*);
extern bool ShowFileDlg(bool, WCHAR*, size_t, const WCHAR*);
extern void LoadROM(const WCHAR* path);
extern void MainLoop();
extern WCHAR* GetString(int);

WCHAR* paramLoad = NULL;

void GetSettings()
{
	ini.SetSpaces(false);
	ini.SetMultiKey(false);
	ini.SetMultiLine(false);
	ini.SetUnicode(true);
	ini.LoadFile(L"settings.ini");
	fpsCap = ini.GetBoolValue(L"video", L"fpsCap", false);
	fpsVisible = ini.GetBoolValue(L"video", L"showFps", true);
	reloadROM = ini.GetBoolValue(L"media", L"reloadRom", false);
	reloadIMG = ini.GetBoolValue(L"media", L"reloadImg", false);
	invertButtons = (int)ini.GetBoolValue(L"media", L"invertButtons", false);
	Discord::enabled = ini.GetBoolValue(L"media", L"discord", false);

	int argc = 0;
	auto argv = CommandLineToArgvW(GetCommandLine(), &argc);
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == L'-')
		{
			if (!wcscmp(argv[i], L"--fpscap"))
				fpsCap = false;
			else if (!wcscmp(argv[i], L"--noreload"))
				reloadROM = false;
			else if (!wcscmp(argv[i], L"--noremount"))
				reloadIMG = false;
			else if (!wcscmp(argv[i], L"--nodiscord"))
				Discord::enabled = false;
		}
		else if (i == 1)
		{
			paramLoad = argv[i];
		}
	}
	LocalFree(argv);

	rtcOffset = ini.GetLongValue(L"media", L"rtcOffset", 0xDEADC70C);
}

void ReportLoadingFail(int messageId, int err, int device, const WCHAR* fileName)
{
 	WCHAR b[1024] = { 0 };
	WCHAR e[1024] = { 0 };
	WCHAR f[1024] = { 0 };
	_wsplitpath_s(fileName, NULL, 0, NULL, 0, f, 1024, e, 1024);
	wcscat_s(f, 1024, e);
	_wcserror_s(e, 1024, err);
	wsprintf(b, GetString(messageId), f, device);
	TaskDialog(NULL, NULL, GetString(IDS_SHORTTITLE), b, e, TDCBF_OK_BUTTON, TD_WARNING_ICON, NULL);
}

void InitializeDevices()
{
	//Absolutely always load a disk drive as #0
	for (int i = 0; i < MAXDEVS; i++)
	{
		WCHAR key[32];
		WCHAR dft[64] = { 0 };
		const WCHAR* thing;
		//Always load a lineprinter as #1 by default
		if (i == 1) wcscpy_s(dft, 64, L"linePrinter");
		_itow_s(i, key, 32, 10);
		thing = ini.GetValue(L"devices", key, dft);
		if (i == 0) thing = L"diskDrive"; //Enforce a disk drive as #0.
		if (!wcslen(thing)) continue;
		if (!wcscmp(thing, L"diskDrive"))
		{
			devices[i] = (Device*)(new DiskDrive(0));
			Log(L"Attached a diskette drive as device #%d.", i);
			thing = ini.GetValue(L"devices/diskDrive", key, L"");
			if (reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					ReportLoadingFail(IDS_DISKIMAGEERROR, err, i, thing);
				else
					Log(L"Loaded \"%s\" into device #%d.", thing, i);
			}
		}
		else if (!wcscmp(thing, L"hardDrive"))
		{
			devices[i] = (Device*)(new DiskDrive(1));
			Log(L"Attached a hard disk drive as device #%d.", i);
			thing = ini.GetValue(L"devices/hardDrive", key, L"");
			if (reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					ReportLoadingFail(IDS_DISKIMAGEERROR, err, i, thing);
				else
					Log(L"Loaded \"%s\" into device #%d.", thing, i);
			}
		}
		else if (!wcscmp(thing, L"linePrinter"))
		{
			devices[i] = (Device*)(new LinePrinter());
			Log(L"Attached a line printer as device #%d.", i);
		}
		else Log(L"Don't know what a \"%s\" is to connect as device #%d.", thing, i);
	}
}

void Preload()
{
	const WCHAR* thing = ini.GetValue(L"media", L"bios", L""); //"roms\\ass-bios.apb");
	if (!wcslen(thing))
	{
		//thing = "roms\\ass-bios.apb";
		WCHAR thePath[FILENAME_MAX] = L"roms\\ass-bios.apb";
		if (ShowFileDlg(false, thePath, 256, L"A3X BIOS files (*.apb)|*.apb"))
		{
			thing = thePath;
			ini.SetValue(L"media", L"bios", thePath);
			ini.SaveFile(L"settings.ini", false);
		}
		else
		{
			return;
		}
	}
	Log(L"Loading BIOS, %s ...", thing);
	Slurp(romBIOS, thing, &biosSize);
	biosSize = RoundUp(biosSize);
	thing = ini.GetValue(L"media", L"lastROM", L"");
	if (reloadROM && wcslen(thing) && paramLoad == NULL)
	{
		Log(L"Loading ROM, %s ...", thing);
		LoadROM(thing);
		pauseState = 0;
	}
	else if (paramLoad != NULL)
	{
		Log(L"Command-line loading ROM, %s ...", paramLoad);
		LoadROM((const WCHAR*)paramLoad);
		pauseState = 0;
	}
}

#if _CONSOLE
#include <tchar.h>
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char** argv)
#endif
{
	argc, argv;
#if _CONSOLE
	SetConsoleCP(CP_UTF8);
	_setmode(_fileno(stdout), _O_U16TEXT);
#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_VIDEO_OPENGL | SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	GetSettings();

	if (InitVideo() < 0)
		return 0;

	InitializeDevices();

	InitializeUI();

	if (InitMemory() < 0)
		return 0;
	if (InitSound() < 0)
		return 0;

	SDL_Joystick *controller[2] = { NULL , NULL };
	if (SDL_NumJoysticks() > 0)
	{
		Log(L"Trying to hook up joystick...");
		controller[0] = SDL_JoystickOpen(0);
		if (SDL_NumJoysticks() > 1)
			controller[1] = SDL_JoystickOpen(1);
	}

	Discord::Init();

	Preload();

	MainLoop();

	Discord::Shutdown();

	UninitSound();
	UninitVideo();
	SDL_Quit();
	return 0;
}
