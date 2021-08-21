#include "asspull.h"
#include "miniz.h"
#include <io.h>
#include <fcntl.h>

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
extern void ShowOpenFileDialog(int, int, const WCHAR*);
extern bool ShowFileDlg(bool, WCHAR*, size_t, const WCHAR*);
extern void LoadROM(const WCHAR* path);
extern void MainLoop();

WCHAR* paramLoad = NULL;

#if _CONSOLE
void GetSettings(int argc, WCHAR** argv)
#else
void GetSettings(int argc, char** argv)
#endif
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

	for (int i = 1; i < argc; i++)
	{
#if _CONSOLE
		WCHAR* thisArg = argv[i];
#else
		WCHAR thisArg[512] = { 0 };
		mbstowcs(thisArg, argv[i], 512);
#endif
		if (thisArg[0] == L'-')
		{
			if (!wcscmp(thisArg, L"--fpscap"))
				fpsCap = false;
			else if (!wcscmp(thisArg, L"--noreload"))
				reloadROM = false;
			else if (!wcscmp(thisArg, L"--noremount"))
				reloadIMG = false;
			else if (!wcscmp(thisArg, L"--nodiscord"))
				Discord::enabled = false;
		}
		else if (i == 1)
		{
			paramLoad = thisArg;
		}
	}

	rtcOffset = ini.GetLongValue(L"media", L"rtcOffset", 0xDEADC70C);
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
			SDL_LogW(L"Attached a diskette drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive(0));
			thing = ini.GetValue(L"devices/diskDrive", key, L"");
			if (reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					SDL_LogW(L"Error %d trying to open disk image \"%s\" for device #%d.", err, thing, i);
			}
		}
		else if (!wcscmp(thing, L"hardDrive"))
		{
			SDL_LogW(L"Attached a hard disk drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive(1));
			thing = ini.GetValue(L"devices/hardDrive", key, L"");
			if (reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					SDL_LogW(L"Error %d trying to open disk image \"%s\" for device #%d.", err, thing, i);
			}
		}
		else if (!wcscmp(thing, L"linePrinter"))
		{
			SDL_LogW(L"Attached a line printer as device #%d.", i);
			devices[i] = (Device*)(new LinePrinter());
		}
		else SDL_LogW(L"Don't know what a \"%s\" is to connect as device #%d.", thing, i);
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
	SDL_LogW(L"Loading BIOS, %s ...", thing);
	Slurp(romBIOS, thing, &biosSize);
	biosSize = RoundUp(biosSize);
	thing = ini.GetValue(L"media", L"lastROM", L"");
	if (reloadROM && wcslen(thing) && paramLoad == NULL)
	{
		SDL_LogW(L"Loading ROM, %s ...", thing);
		LoadROM(thing);
		pauseState = 0;
	}
	else if (paramLoad != NULL)
	{
		SDL_LogW(L"Command-line loading ROM, %s ...", paramLoad);
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

#if _CONSOLE
	SetConsoleCP(CP_UTF8);
	_setmode(_fileno(stdout), _O_U16TEXT);
#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_VIDEO_OPENGL | SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	GetSettings(argc, argv);

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
		SDL_LogW(L"Trying to hook up joystick...");
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
