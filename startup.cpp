#include "asspull.h"
#include "resource.h"
#include "miniz.h"
#include <io.h>
#include <fcntl.h>
#include <commctrl.h>
#include <ShlObj.h>

extern "C" {
#include "musashi/m68k.h"
}

CSimpleIni ini;

extern unsigned int biosSize, romSize;
extern long rtcOffset;
extern int invertButtons;
extern int firstDiskDrive;

extern void LoadROM(const WCHAR* path);
extern void MainLoop();
extern FILE* logFile;

WCHAR paramLoad[512] = { 0 };

struct {
	WCHAR code[16];
	int num;
} langs[64];
int langCt = 0;

BOOL CALLBACK GetLangProc(HMODULE mod, LPCTSTR type, LPCTSTR name, WORD lang, LONG_PTR lParam)
{
	langs[langCt].num = lang;

	WCHAR work[32] = { 0 };
	GetLocaleInfo(MAKELCID(lang, SORT_DEFAULT), LOCALE_SENGLANGUAGE, work, 32);
	_wcslwr_s(work, 32);
	work[2] = L'-';
	work[3] = 0;
	GetLocaleInfo(MAKELCID(lang, SORT_DEFAULT), LOCALE_SABBREVCTRYNAME, work + 3, 32);
	_wcsupr_s(work + 3, 32 - 3);
	work[5] = 0;
	wcscpy(langs[langCt].code, work);

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
	wcscpy(UI::settingsFile, path);
	wcscat(UI::settingsFile, L"\\Clunibus.ini");
	auto atts = GetFileAttributes(UI::settingsFile);
	if (atts == INVALID_FILE_ATTRIBUTES)
	{
		wcscpy(UI::settingsFile, L"Clunibus.ini");
		atts = GetFileAttributes(UI::settingsFile);
		if (atts == INVALID_FILE_ATTRIBUTES)
		{
			//Local file doesn't exist? Then revert back to appdata and pretend.
			wcscpy(UI::settingsFile, path);
			wcscat(UI::settingsFile, L"\\Clunibus.ini");
		}
	}
	CoTaskMemFree(path);

	ini.LoadFile(UI::settingsFile);
	UI::fpsCap = ini.GetBoolValue(L"video", L"fpsCap", false);
	UI::fpsVisible = ini.GetBoolValue(L"video", L"showFps", true);
	UI::reloadROM = ini.GetBoolValue(L"misc", L"reloadRom", false);
	UI::reloadIMG = ini.GetBoolValue(L"misc", L"reloadImg", false);
	invertButtons = (int)ini.GetBoolValue(L"input", L"invertButtons", false);
	key2joy = (int)ini.GetBoolValue(L"input", L"key2joy", true);
	Discord::enabled = ini.GetBoolValue(L"misc", L"discord", false);

	EnumResourceLanguages(NULL, RT_MENU, MAKEINTRESOURCE(IDR_MAINMENU), GetLangProc, 0);
	int langID = (int)ini.GetLongValue(L"misc", L"lang", 0);
	if (langID == 0)
	{
		//try as a string
		auto locStr = ini.GetValue(L"misc", L"lang", L"en-us");
		for (auto &l : langs)
			if (!_wcsnicmp(locStr, l.code, 2))
				langID = l.num;
		for (auto &l : langs)
			if (!_wcsnicmp(locStr, l.code, 5))
				langID = l.num;
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
		}
		else if (i == 1)
		{
			wcscpy(paramLoad, argv[i]);
		}
	}
	LocalFree(argv);

	rtcOffset = ini.GetLongValue(L"misc", L"rtcOffset", 0xDEADC70C);
}

void InitializeDevices()
{
	//Absolutely always load a disk drive as #0
	for (int i = 0; i < MAXDEVS; i++)
	{
		WCHAR key[32];
		WCHAR dft[64] = { 0 };
		const WCHAR* thing;
		_itow(i, key, 10);
		thing = ini.GetValue(L"devices", key, dft);
		if (!wcslen(thing)) continue;
		if (!wcscmp(thing, L"diskDrive"))
		{
			devices[i] = (Device*)(new DiskDrive(0));
			Log(L"Attached a diskette drive as device #%d.", i);
			thing = ini.GetValue(L"devices/diskDrive", key, L"");
			if (UI::reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
				{
					if (UI::ReportLoadingFail(IDS_DISKIMAGEERROR, err, i, thing, true))
						UI::EjectDisk(i);
				}
				else
					Log(L"Loaded \"%s\" into device #%d.", thing, i);
			}
		}
		else if (!wcscmp(thing, L"hardDrive"))
		{
			devices[i] = (Device*)(new DiskDrive(1));
			Log(L"Attached a hard disk drive as device #%d.", i);
			thing = ini.GetValue(L"devices/hardDrive", key, L"");
			if (UI::reloadIMG && wcslen(thing))
			{
				auto err = ((DiskDrive*)devices[i])->Mount(thing);
				if (err)
				{
					if (UI::ReportLoadingFail(IDS_DISKIMAGEERROR, err, i, thing, true))
						UI::EjectDisk(i);
				}
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

	FindFirstDrive();
}

void Preload()
{
	const WCHAR* thing = ini.GetValue(L"media", L"bios", L""); //"roms\\ass-bios.apb");
	if (!wcslen(thing))
	{
		//thing = "roms\\ass-bios.apb";
		WCHAR thePath[FILENAME_MAX] = L"roms\\ass-bios.apb";
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
	Log(L"Loading BIOS %s...", thing);
	Slurp(romBIOS, thing, &biosSize);
	biosSize = RoundUp(biosSize);
	thing = ini.GetValue(L"media", L"rom", L"");
	if (UI::reloadROM && wcslen(thing) && paramLoad == NULL)
	{
		Log(L"Loading ROM %s...", thing);
		LoadROM(thing);
		pauseState = 0;
	}
	else if (paramLoad[0])
	{
		Log(L"Command-line loading ROM %s...", paramLoad);
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
	auto throwAway = _setmode(_fileno(stdout), _O_U16TEXT); throwAway;
#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_VIDEO_OPENGL | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	GetModuleFileName(NULL, UI::startingPath, 512);
	WCHAR* lastSlash = wcsrchr(UI::startingPath, L'\\');
	*lastSlash = 0;
	if (!wcsncmp(lastSlash - 5, L"Debug", 6))
	{
		lastSlash = wcsrchr(UI::startingPath, L'\\');
		*lastSlash = 0;
	}

	GetSettings();
	
	if (Video::Initialize() < 0)
		return 0;

	InitializeDevices();

	UI::Initialize();

	if (InitMemory() < 0)
		return 0;
	if (Sound::Initialize() < 0)
		return 0;

	SDL_Joystick *controller[2] = { NULL , NULL };
	if (SDL_NumJoysticks() > 0)
	{
		Log(L"Trying to hook up joystick...");
		controller[0] = SDL_JoystickOpen(0);
		if (SDL_NumJoysticks() > 1)
			controller[1] = SDL_JoystickOpen(1);
	}

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
