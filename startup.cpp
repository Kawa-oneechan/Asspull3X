#include "asspull.h"
#include "resource.h"
#include <io.h>
#include <fcntl.h>
#include <commctrl.h>
#include <ShlObj.h>
#include <time.h>

extern "C" {
#include "musashi/m68k.h"
}

CSimpleIni ini;

extern unsigned int biosSize, romSize;
extern long rtcOffset;
extern int firstDiskDrive;

extern void LoadROM(const WCHAR* path);
extern void MainLoop();
extern FILE* logFile;

extern SDL_GameControllerButton buttonMap[12];
const WCHAR* buttonNames[] =
{
	L"up", L"right", L"down", L"left",
	L"a", L"b", L"x", L"y",
	L"leftshoulder", L"rightshoulder",
	L"start", L"select",
};
const WCHAR* buttonDefaults[] =
{
	L"dpup", L"dpright", L"dpdown", L"dpleft",
	L"a", L"b", L"x", L"y",
	L"leftshoulder", L"rightshoulder",
	L"start", L"back",
};

WCHAR paramLoad[512] = { 0 };

struct {
	WCHAR code[16];
	int num;
} langs[64];
int langCt = 0;
int langID = 0;

extern void AssociateFiletypes();

BOOL CALLBACK GetLangProc(HMODULE mod, LPCTSTR type, LPCTSTR name, WORD lang, LONG_PTR lParam)
{
	langs[langCt].num = lang;
	GetLocaleInfo(MAKELCID(lang, SORT_DEFAULT), LOCALE_SNAME, langs[langCt].code, 16);
	langCt++;
	if (langCt == 64)
		return FALSE;
	return TRUE;
}

void GetSettings()
{
	ini.SetSpaces(false);
	ini.SetMultiKey(false);
	ini.SetMultiLine(false);
	ini.SetUnicode(true);

	PWSTR path = NULL;
	auto res = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path); res;
	wcscpy(UI::settingsFile, L"Clunibus.ini");
	auto atts = GetFileAttributes(UI::settingsFile);
	if (atts == INVALID_FILE_ATTRIBUTES)
	{
		wcscpy(UI::settingsFile, path);
		wcscat(UI::settingsFile, L"\\Clunibus.ini");
		atts = GetFileAttributes(UI::settingsFile);
	}
	CoTaskMemFree(path);

	ini.LoadFile(UI::settingsFile);
	UI::fpsCap = ini.GetBoolValue(L"video", L"fpsCap", false);
	UI::fpsVisible = ini.GetBoolValue(L"video", L"showFps", true);
	UI::reloadROM = ini.GetBoolValue(L"misc", L"reloadRom", false);
	UI::reloadIMG = ini.GetBoolValue(L"misc", L"reloadImg", false);
	key2joy = (int)ini.GetBoolValue(L"input", L"key2joy", true);
	Discord::enabled = ini.GetBoolValue(L"misc", L"discord", false);

	EnumResourceLanguages(NULL, RT_MENU, MAKEINTRESOURCE(IDR_MAINMENU), GetLangProc, 0);
	langID = (int)ini.GetLongValue(L"misc", L"lang", 0);
	if (langID == 0)
	{
		//try as a string
		auto locStr = ini.GetValue(L"misc", L"lang", L"en-us");
		for (int i = 0; i < langCt; i++)
			if (!_wcsnicmp(locStr, langs[i].code, 2))
				langID = langs[i].num;
		for (int i = 0; i < langCt; i++)
			if (!_wcsnicmp(locStr, langs[i].code, 5))
				langID = langs[i].num;
	}
	if (langID != 0)
	{
		//no need to check if a resource with this langID exists, it'll just default out.
		SetThreadUILanguage(langID);
	}

	int argc = 0;
	auto argv = CommandLineToArgvW(GetCommandLine(), &argc);
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == L'-')
		{
			if (!wcscmp(argv[i], L"--fpscap"))
				UI::fpsCap = false;
			else if (!wcscmp(argv[i], L"--noreload"))
				UI::reloadROM = false;
			else if (!wcscmp(argv[i], L"--noremount"))
				UI::reloadIMG = false;
			else if (!wcscmp(argv[i], L"--nodiscord"))
				Discord::enabled = false;
			else if (!wcscmp(argv[i], L"--fullscreen"))
				UI::startFullscreen = true;
			else if (!wcscmp(argv[i], L"--poweron"))
				pauseState = pauseNot;
			else if (!wcscmp(argv[i], L"--associate"))
			{
				AssociateFiletypes();
				_exit(1);
			}
		}
		else if (i == 1)
		{
			wcscpy(paramLoad, argv[i]);
		}
	}
	LocalFree(argv);

	auto rtcEpoch = 441763200 - (long)time(NULL);
	rtcOffset = ini.GetLongValue(L"misc", L"rtcOffset", rtcEpoch);
	if (rtcOffset == rtcEpoch)
		ini.SetLongValue(L"misc", L"rtcOffset", rtcEpoch);

	for (int i = 0; i < 12; i++)
	{
		auto button = ini.GetValue(L"input", buttonNames[i], buttonDefaults[i]);
		buttonMap[i] = SDL_GameControllerGetButtonFromStringW(button);
	}
}

void InitializeDevices()
{
	//Absolutely always load Input as #0
	inputDev = new InputOutputDevice();
	devices[0] = inputDev;

	for (int i = 1; i < MAXDEVS; i++)
	{
		WCHAR key[32];
		WCHAR dft[64] = { 0 };
		const WCHAR* thing;
		_itow(i, key, 10);
		thing = ini.GetValue(L"devices", key, dft);
		if (!wcslen(thing)) continue;
		if (!wcscmp(thing, L"diskDrive"))
		{
			devices[i] = new DiskDrive(0);
			//Log(L"Attached a \x1b[1mdiskette drive\x1b[0m as device \x1b[1m#%d\x1b[0m.", i);
			thing = ini.GetValue(L"devices/diskDrive", key, L"");
			if (UI::reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
				{
					if (UI::ReportLoadingFail(IDS_DISKIMAGEERROR, err, i, thing, true))
						UI::EjectDisk(i);
				}
				//else
				//	Log(logNormal, L"Loaded \x1b[96m%s\x1b[0m into device \x1b[1m#%d\x1b[0m.", thing, i);
			}
		}
		else if (!wcscmp(thing, L"hardDrive"))
		{
			devices[i] = new DiskDrive(1);
			//Log(logNormal, L"Attached a \x1b[1mhard disk drive\x1b[0m as device \x1b[1m#%d\x1b[0m.", i);
			thing = ini.GetValue(L"devices/hardDrive", key, L"");
			if (UI::reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
				{
					if (UI::ReportLoadingFail(IDS_DISKIMAGEERROR, err, i, thing, true))
						UI::EjectDisk(i);
				}
				//else
				//	Log(logNormal, L"Loaded \x1b[96m%s\x1b[0m into device \x1b[1m#%d\x1b[0m.", thing, i);
			}
		}
		else if (!wcscmp(thing, L"linePrinter"))
		{
			devices[i] = new LinePrinter();
			//Log(logNormal, L"Attached a \x1b[1mline printer\x1b[0m as device \x1b[1m#%d\x1b[0m.", i);
		}
		//else Log(logWarning, L"Don't know what \x1b[101m\"%s\"\x1b[0m is to connect as device \x1b[1m#%d\x1b[0m.", thing, i);
	}

	FindFirstDrive();
}

void Preload()
{
	const WCHAR* thing = ini.GetValue(L"media", L"bios", L""); //"roms\\ass-bios.apb");
	WCHAR thePath[FILENAME_MAX] = L"roms\\ass-bios.apb";
	if (!wcslen(thing))
	{
askForBIOS:
		//thing = "roms\\ass-bios.apb";
		if (UI::ShowFileDlg(false, thePath, 256, L"A3X BIOS files (*.apb)|*.apb"))
		{
			thing = thePath;
			ini.SetValue(L"media", L"bios", thePath);
			ini.SaveFile(UI::settingsFile, false);
		}
		else
		{
			return;
		}
	}
	Log(logNormal, UI::GetString(IDS_LOADINGBIOS), thing);
	if (LoadFile(romBIOS, thing, &biosSize))
		goto askForBIOS;
	biosSize = RoundUp(biosSize);
	thing = ini.GetValue(L"media", L"rom", L"");
	if (UI::reloadROM && wcslen(thing) && paramLoad[0] == 0)
	{
		//Log(logNormal, L"Loading ROM \x1b[96m%s\x1b[0m...", thing);
		LoadROM(thing);
		//pauseState = pauseNot;
	}
	else if (paramLoad[0])
	{
		//Log(logNormal, L"Command-line loading ROM \x1b[96m%s\x1b[0m...", paramLoad);
		LoadROM((const WCHAR*)paramLoad);
		pauseState = pauseNot;
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

	GetModuleFileName(NULL, UI::startingPath, 256);
	WCHAR* lastSlash = wcsrchr(UI::startingPath, L'\\');
	*lastSlash = 0;
	if (!wcsncmp(lastSlash - 5, L"Debug", 6))
	{
		lastSlash = wcsrchr(UI::startingPath, L'\\');
		*lastSlash = 0;
	}

	GetSettings();

#if _CONSOLE
	SetConsoleCP(CP_UTF8);
	auto throwAway = _setmode(_fileno(stdout), _O_U16TEXT); throwAway;
	DWORD mode = 0;
	GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
	mode |= 4;
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode);
#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_VIDEO_OPENGL | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	if (Video::Initialize() < 0)
		return 0;

	InitializeDevices();

	UI::Initialize();

	if (InitMemory() < 0)
		return 0;
	if (Sound::Initialize() < 0)
		return 0;

	Discord::Initialize();

	Preload();

	MainLoop();

	Discord::Shutdown();

	Sound::Shutdown();
	Video::Shutdown();
	SDL_Quit();
	if (logFile != NULL)
		fclose(logFile);
	return 0;
}
