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
extern void ShowOpenFileDialog(int, int, std::string);
extern bool ShowFileDlg(bool, char*, size_t, const char*);
extern void LoadROM(std::string path);
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
		std::string key;
		std::string dft;
		std::string thing;
		//Always load a lineprinter as #1 by default
		if (i == 1) dft = "linePrinter";
		//SDL_itoa(i, key, 10);
		key = std::to_string((long double)i);
		thing = ini.GetValue("devices", key.c_str(), dft.c_str());
		if (i == 0) thing = "diskDrive"; //Enforce a disk drive as #0.
		if (thing.empty()) continue;
		if (!thing.compare("diskDrive"))
		{
			SDL_Log("Attached a diskette drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive(0));
			thing = ini.GetValue("devices/diskDrive", key.c_str(), "");
			if (reloadIMG && thing.size() != 0)
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					SDL_Log("Error %d trying to open disk image \"%s\" for device #%d.", err, thing.c_str(), i);
			}
		}
		else if (!thing.compare("hardDrive"))
		{
			SDL_Log("Attached a hard disk drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive(1));
			thing = ini.GetValue("devices/hardDrive", key.c_str(), "");
			if (reloadIMG && thing.size() != 0)
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
					SDL_Log("Error %d trying to open disk image \"%s\" for device #%d.", err, thing.c_str(), i);
			}
		}
		else if (!thing.compare("linePrinter"))
		{
			SDL_Log("Attached a line printer as device #%d.", i);
			devices[i] = (Device*)(new LinePrinter());
		}
		else SDL_Log("Don't know what a \"%s\" is to connect as device #%d.", thing.c_str(), i);
	}
}

void Preload()
{
	std::string thing = ini.GetValue("media", "bios", ""); //"roms\\ass-bios.apb");
	if (thing.empty())
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
	SDL_Log("Loading BIOS, %s ...", thing.c_str());
	Slurp(romBIOS, thing, &biosSize);
	biosSize = RoundUp(biosSize);
	thing = ini.GetValue("media", "lastROM", "");
	if (reloadROM && !thing.empty() && paramLoad == NULL)
	{
		SDL_Log("Loading ROM, %s ...", thing.c_str());
		LoadROM(thing);
		pauseState = 0;
	}
	else if (paramLoad != NULL)
	{
		SDL_Log("Command-line loading ROM, %s ...", paramLoad);
		LoadROM(std::string(paramLoad));
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
