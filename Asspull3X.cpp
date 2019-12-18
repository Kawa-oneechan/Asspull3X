#include "asspull.h"

extern "C" {
#include "musashi/m68k.h"
}

static bool quit = 0;
int line = 0, interrupts = 0;
extern void Screenshot();
extern int uiCommand, uiData, uiKey;
extern char uiString[512];
extern char uiFPS[];
extern void SetStatus(const char*);
extern void _devUpdateDiskette(int);
extern void ShowOpenFileDialog(int, int, const char*);

extern unsigned int biosSize, romSize;

IniFile* ini;

int pauseState = 0;
unsigned char* pauseScreen;
extern unsigned char* pixels;
bool fpsCap;
#if WIN32 && _CONSOLE
#include <tchar.h>
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_VIDEO_OPENGL | SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	ini = new IniFile();
	ini->autoSave = true;
	ini->Load("settings.ini");
	auto thing = ini->Get("video", "fpscap", "true"); if (thing[0] == 't' || thing[0] == 'T' || thing[0] == 1) fpsCap = true;
	bool fullScreen = false;
	thing = ini->Get("video", "fullScreen", "false"); if (thing[0] == 't' || thing[0] == 'T' || thing[0] == 1) fullScreen = true;
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if (argv[i][1] == 'f')
				fullScreen = true;
		}
	}

	if (InitVideo(fullScreen) < 0)
		return 0;

	//Absolutely always load a disk drive as #0
	for (int i = 0; i < MAXDEVS; i++)
	{
		char key[8];
		char dft[24] = "";
		//Always load a lineprinter as #1 by default
		if (i == 1) strcpy(dft, "linePrinter");
		SDL_itoa(i, key, 10);
		thing = ini->Get("devices", key, dft);
		if (i == 0) thing = "diskDrive"; //Enforce a disk drive as #0.
		if (thing[0] == 0) continue;
		if (!strcmp(thing, "diskDrive"))
		{
			SDL_Log("Attached a disk drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive());
			thing = ini->Get("devices/diskDrive", key, "");
			if (thing[0] != 0)
			{
				auto err = ((DiskDrive*)devices[0])->Mount(thing);
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

	if (InitMemory() < 0)
		return 0;
	if (InitSound() < 0)
		return 0;

	pauseScreen = (unsigned char*)malloc(640 * 480 * 4);

	SDL_Joystick *controller[2] = { NULL , NULL };
	if (SDL_NumJoysticks() > 0)
	{
		SDL_Log("Trying to hook up joystick...");
		controller[0] = SDL_JoystickOpen(0);
		if (SDL_NumJoysticks() > 1)
			controller[1] = SDL_JoystickOpen(1);
	}

	thing = ini->Get("media", "bios", "roms\\ass-bios.apb");
	SDL_Log("Loading BIOS, %s ...", thing);
	Slurp(romBIOS, thing, &biosSize);
	biosSize = RoundUp(biosSize);
	thing = ini->Get("media", "lastROM", "");
	if (thing[0] != 0)
	{
		SDL_Log("Loading ROM, %s ...", thing);
		Slurp(romCartridge, thing, &romSize);
		romSize = RoundUp(romSize);
	}

	SDL_Log("Resetting Musashi...");
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68k_pulse_reset();

	SDL_Log("Asspull IIIx is ready.");
	SDL_Log("Press Ctrl-L to load a ROM, Ctrl-Shift-L to load a diskette.");
	SDL_Log("Press Ctrl-U to unload ROM, Ctrl-Shift-U to unload diskette.");
	SDL_Log("Press Ctrl-R to reset, Ctrl-Shift-R to unload and reset.");
	SDL_Log("Press Ctrl-D to dump RAM, Ctrl-S to take a screenshot.");

	SDL_Event ev;

	const auto mHz = 8;
	const auto screenFreq = 60;
	const auto pixsPerRow = 640;
	const auto lines = 480;
	const auto trueLines = 525;
	const auto trueWidth = 800;
	const auto Hz = mHz * 1000000;
	const auto vBlankEvery = Hz / screenFreq;
	const auto hBlankEvery = vBlankEvery / lines;
	//const auto vBlankLasts = (trueLines) * hBlankEvery;
	const auto hBlankLasts = (trueWidth - pixsPerRow);

	auto startTime = 0;
	auto endTime = 0;
	auto delta = 0;
	auto frames = 0;

	while (!quit)
	{
		while (SDL_PollEvent(&ev) != 0)
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				quit = true;
				break;

			case SDL_JOYBUTTONDOWN:
				if (ev.jbutton.which < 2)
					joypad[ev.jbutton.which] |= 16 << ev.jbutton.button;
				break;
			case SDL_JOYBUTTONUP:
				if (ev.jbutton.which < 2)
					joypad[ev.jbutton.which] &= ~(16 << ev.jbutton.button);
				break;
			case SDL_JOYHATMOTION:
				if (ev.jhat.which < 2 && ev.jhat.hat == 0)
					joypad[ev.jbutton.which] = (joypad[ev.jbutton.which] & ~15) | ev.jhat.value;
				break;
			case SDL_KEYUP:
				uiKey = keyMap[ev.key.keysym.scancode];
				if (ev.key.keysym.mod & KMOD_SHIFT) uiKey |= 0x100;
				if (ev.key.keysym.mod & KMOD_ALT) uiKey |= 0x200;
				if (ev.key.keysym.mod & KMOD_CTRL) uiKey |= 0x400;
				if (ev.key.keysym.mod & KMOD_LCTRL)
				{
					if (ev.key.keysym.sym == SDLK_l)
						uiCommand = (ev.key.keysym.mod & KMOD_SHIFT) ? cmdInsertDisk : cmdLoadRom;
					else if (ev.key.keysym.sym == SDLK_u)
						uiCommand = (ev.key.keysym.mod & KMOD_SHIFT) ? cmdEjectDisk : cmdUnloadRom;
					else if (ev.key.keysym.sym == SDLK_r)
						uiCommand = cmdReset;
					else if (ev.key.keysym.sym == SDLK_d)
						uiCommand = cmdDump;
					else if (ev.key.keysym.sym == SDLK_s)
						uiCommand = cmdScreenshot;
					else if (ev.key.keysym.sym == SDLK_p)
					{
						if (pauseState == 0)
							pauseState = 1;
						else if (pauseState == 2)
							pauseState = 0;
					}
				}
				keyScan = 0;
				break;
			}
		}

		if (uiCommand != cmdNone)
		{
			if (uiCommand == cmdLoadRom)
			{
				if (uiString[0] == 0)
					ShowOpenFileDialog(cmdLoadRom, 0, "*.ap3");
				else
				{
					SDL_Log("Loading ROM, %s ...", uiString);
					auto gottaReset = (*(uint32_t*)romCartridge == 0x21535341);
					memset(romCartridge, 0, CART_SIZE);
					Slurp(romCartridge, uiString, &romSize);
					romSize = RoundUp(romSize);
					ini->Set("media", "lastROM", uiString);
					if (gottaReset)
						m68k_pulse_reset();
				}
			}
			else if (uiCommand == cmdInsertDisk)
			{
				if (devices[uiData] == NULL || devices[uiData]->GetID() != 0x0144)
					SetStatus("No disk drive.");					
				else if (((DiskDrive*)devices[uiData])->IsMounted())
					SetStatus("Eject the diskette first.");
				else
				{
					if (uiString[0] == 0)
						ShowOpenFileDialog(cmdInsertDisk, uiData, "*.img");
					else
					{
						auto ret = ((DiskDrive*)devices[uiData])->Mount(uiString);
						if (ret == -1)
							SetStatus("Eject the diskette first, with Ctrl-Shift-U.");
						else if (ret != 0)
							SDL_Log("Error %d trying to open disk image.", ret);
						else
							ini->Set("devices/diskDrive", "0", uiString);
						_devUpdateDiskette(uiData);
					}
				}
			}
			else if (uiCommand == cmdUnloadRom)
			{
				SDL_Log("Unloading ROM...");
				memset(romCartridge, 0, CART_SIZE);
				ini->Set("media", "lastROM", "");
				gfxFade = 31;
				SetStatus("Cart pulled.");
			}
			else if (uiCommand == cmdEjectDisk)
			{
				if (devices[uiData] == NULL || devices[uiData]->GetID() != 0x0144)
					SetStatus("No disk drive.");
				else if (((DiskDrive*)devices[uiData])->IsMounted())
				{
					((DiskDrive*)devices[uiData])->Unmount();
					ini->Set("devices/diskDrive", "0", "");
					SetStatus("Disk ejected.");
				}
				_devUpdateDiskette(uiData);
			}
			else if (uiCommand == cmdReset)
			{
				if (uiData)
				{
					SDL_Log("Unloading ROM...");
					memset(romCartridge, 0, CART_SIZE);
					((DiskDrive*)devices[0])->Unmount();
					ini->Set("media", "lastROM", "");
				}
				SDL_Log("Resetting Musashi...");
				SetStatus("System reset.");
				m68k_pulse_reset();
			}
			else if (uiCommand == cmdQuit)
			{
				quit = true;
			}
			else if (uiCommand == cmdDump)
			{
				SDL_Log("Dumping core...");
				Dump("wram.bin", ramInternal, WRAM_SIZE);
				Dump("vram.bin", ramVideo, VRAM_SIZE);
			}
			else if (uiCommand == cmdScreenshot)
			{
				Screenshot();
			}
			uiCommand = uiData = 0;
			uiString[0] = 0;
		}

		if (pauseState != 2)
		{
			m68k_execute(hBlankEvery);
			if (line < lines)
			{
				HandleHdma(line);
				RenderLine(line);
			}
			if (line == lines)
			{
				if (pauseState == 1) //pausing now!
				{
					memcpy(pauseScreen, pixels, 640 * 480 * 4);
					for (auto i = 0; i < 640 * 480 * 4; i += 4)
					{
						pauseScreen[i + 0] /= 2;
						pauseScreen[i + 1] /= 2;
						pauseScreen[i + 2] /= 2;
					}
					pauseState = 2;
				}
				HandleUI();
				VBlank();

				if (!startTime)
					startTime = SDL_GetTicks();
				else
					delta = endTime - startTime;

				auto averageFPS = frames / (SDL_GetTicks() / 1000.0f);
				if (averageFPS > 2000000)
					averageFPS = 0;
				sprintf(uiFPS, "%d", (int)averageFPS);
				if (fpsCap && delta < 20)
					SDL_Delay(20 - delta);
				startTime = endTime;
				endTime = SDL_GetTicks();

				frames++;
				if (pauseState != 2)
				{
					if ((interrupts & 0x84) == 0) //if interrupts are enabled and not already in VBlank
					{
						interrupts |= 4; //set VBlank signal
						m68k_set_virq(M68K_IRQ_7, 1);
					}
				}
			}
			if (pauseState != 2)
			{
				//Would trigger HBlank here but that kills the system???
				/* if ((interrupts & 0x82) == 0) //if interrupts are enabled and not already in HBlank
				{
					interrupts |= 2; //set HBlank signal
					m68k_set_virq(M68K_IRQ_7, 1);
				} */
				m68k_execute(hBlankLasts);
			}
		}
		else if (pauseState == 2)
		{
			memcpy(pixels, pauseScreen, 640 * 480 * 4);
			HandleUI();
			VBlank();
		}
		line++;
		if (line == trueLines)
		{
			m68k_set_virq(M68K_IRQ_7, 0);
			ticks++;
			/*
			if (tadaCt < TADA_size)
			{
				//any less than 128 a n d i t g e t s i n t e r e s t i n g
				for (int lol = 0; lol < 128; lol++)
					BufferAudioSample(TADA[tadaCt++]);
			}
			else
				BufferAudioSample(0);
			*/
			line = 0;
		}
	}

	UninitSound();
	UninitVideo();
	SDL_Quit();
	return 0;
}
