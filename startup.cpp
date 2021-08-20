#include "asspull.h"
#include "miniz.h"

extern "C" {
#include "musashi/m68k.h"
}

CSimpleIniA ini;

bool reloadROM, reloadIMG;
extern bool fpsVisible, fpsCap;
extern unsigned int biosSize, romSize;
extern long rtcOffset;
extern int invertButtons;
extern int pauseState;

extern void InitializeUI();
extern void ShowOpenFileDialog(int, int, const char*);
extern bool ShowFileDlg(bool, char*, size_t, const char*);
extern void LoadROM(const char* path);
extern void MainLoop();

char* paramLoad = NULL;

void GetSettings(int argc, char* argv[])
{
	ini.SetSpaces(false);
	ini.SetMultiKey(false);
	ini.SetMultiLine(false);
	ini.SetUnicode(false);
	ini.LoadFile("settings.ini");
	fpsCap = ini.GetBoolValue("video", "fpsCap", false);
	fpsVisible = ini.GetBoolValue("video", "showFps", true);
	reloadROM = ini.GetBoolValue("media", "reloadRom", false);
	reloadIMG = ini.GetBoolValue("media", "reloadImg", false);
	invertButtons = (int)ini.GetBoolValue("media", "invertButtons", false);
	Discord::enabled = ini.GetBoolValue("media", "discord", false);

	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if (!strcmp(argv[i], "--fpscap"))
				fpsCap = false;
			else if (!strcmp(argv[i], "--noreload"))
				reloadROM = false;
			else if (!strcmp(argv[i], "--noremount"))
				reloadIMG = false;
			else if (!strcmp(argv[i], "--nodiscord"))
				Discord::enabled = false;
		}
		else if (i == 1)
		{
			paramLoad = argv[i];
		}
	}

	rtcOffset = ini.GetLongValue("media", "rtcOffset", 0xDEADC70C);
}

void InitializeDevices()
{
	//Absolutely always load a disk drive as #0
	for (int i = 0; i < MAXDEVS; i++)
	{
		char key[32];
		char dft[64] = { 0 };
		const char* thing;
		//Always load a lineprinter as #1 by default
		if (i == 1) strcpy_s(dft, 64, "linePrinter");
		itoa(i, key, 10);
		thing = ini.GetValue("devices", key, dft);
		if (i == 0) thing = "diskDrive"; //Enforce a disk drive as #0.
		if (!strlen(thing)) continue;
		if (!strcmp(thing, "diskDrive"))
		{
			SDL_Log("Attached a diskette drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive(0));
			thing = ini.GetValue("devices/diskDrive", key, "");
			if (reloadIMG && strlen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					SDL_Log("Error %d trying to open disk image \"%s\" for device #%d.", err, thing, i);
			}
		}
		else if (!strcmp(thing, "hardDrive"))
		{
			SDL_Log("Attached a hard disk drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive(1));
			thing = ini.GetValue("devices/hardDrive", key, "");
			if (reloadIMG && strlen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					SDL_Log("Error %d trying to open disk image \"%s\" for device #%d.", err, thing, i);
			}
		}
		else if (!strcmp(thing, "linePrinter"))
		{
			SDL_Log("Attached a line printer as device #%d.", i);
			devices[i] = (Device*)(new LinePrinter());
		}
		else SDL_Log("Don't know what a \"%s\" is to connect as device #%d.", thing, i);
	}
}

void Preload()
{
	const char* thing = ini.GetValue("media", "bios", ""); //"roms\\ass-bios.apb");
	if (!strlen(thing))
	{
		//thing = "roms\\ass-bios.apb";
		char thePath[FILENAME_MAX] = "roms\\ass-bios.apb";
		if (ShowFileDlg(false, thePath, 256, "A3X BIOS files (*.apb)|*.apb"))
		{
			thing = thePath;
			ini.SetValue("media", "bios", thePath);
			ini.SaveFile("settings.ini");
		}
		else
		{
			return;
		}
	}
	SDL_Log("Loading BIOS, %s ...", thing);
	Slurp(romBIOS, thing, &biosSize);
	biosSize = RoundUp(biosSize);
	thing = ini.GetValue("media", "lastROM", "");
	if (reloadROM && strlen(thing) && paramLoad == NULL)
	{
		SDL_Log("Loading ROM, %s ...", thing);
		LoadROM(thing);
		pauseState = 0;
	}
	else if (paramLoad != NULL)
	{
		SDL_Log("Command-line loading ROM, %s ...", paramLoad);
		LoadROM((const char*)paramLoad);
		pauseState = 0;
	}
}

#if _CONSOLE
#include <tchar.h>
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
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
		SDL_Log("Trying to hook up joystick...");
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
