#include "asspull.h"

extern "C" {
#include "musashi\m68k.h"
}
#include "nativefiledialog\src\include\nfd.h"

static bool quit = 0;
int line = 0, interrupts = 0;
extern "C" INLINE void m68ki_set_sr(unsigned int value);
extern void Screenshot();
extern int uiCommand, uiData;
extern char uiFPS[];
extern void SetStatus(char*);

IniFile* ini;

char biosPath[256], romPath[256], diskPath[256];

int pauseState = 0;
unsigned char* pauseScreen;
extern unsigned char* pixels;

int Slurp(unsigned char* dest, const char* filePath)
{
	FILE* file = NULL;
	int err = fopen_s(&file, filePath, "rb");
	if (err)
		return err;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	fread(dest, 1, size, file);
	fclose(file);
	return 0;
}

int Dump(const char* filePath, unsigned char* source, unsigned long size)
{
	FILE* file = NULL;
	int err = fopen_s(&file, filePath, "wb");
	if (err)
		return err;
	fwrite(source, size, 1, file);
	fclose(file);
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef WITH_OPENGL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_VIDEO_OPENGL | SDL_INIT_GAMECONTROLLER) < 0)
#else
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
#endif
		return 0;

	ini = new IniFile();
	ini->autoSave = true;
	ini->Load("settings.ini");
	auto thing = ini->Get("media", "bios", "roms\\ass-bios.apb"); strcpy_s(biosPath, 256, thing);
	thing = ini->Get("media", "lastROM", ""); strcpy_s(romPath, 256, thing);
	thing = ini->Get("media", "lastDisk", ""); strcpy_s(diskPath, 256, thing);
	thing = ini->Get("media", "midiDevice", ""); auto midiNum = SDL_atoi(thing);

	if (InitVideo() < 0)
		return 0;

	for (int i = 0; i < MAXDEVS; i++)
	{
		char key[8];
		char dft[24] = "";
		//Always load a diskdrive and lineprinter as #0 and #1 by default
		if (i == 0) strcpy_s(dft, 24, "diskDrive");
		else if (i == 1) strcpy_s(dft, 24, "linePrinter");
		SDL_itoa(i, key, 10);
		thing = ini->Get("devices", key, dft);
		if (thing[0] == 0) continue;
		if (!strcmp(thing, "diskDrive"))
		{
			SDL_Log("Attached a disk drive as device #%d.", i);
			devices[i] = (Device*)(new DiskDrive());
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
	if (InitSound(midiNum) < 0)
		return 0;

	pauseScreen = (unsigned char*)malloc(640 * 480 * 4);

	SDL_Joystick *controller = NULL;
	if (SDL_NumJoysticks() > 0)
	{
		SDL_Log("Trying to hook up joystick...");
		controller = SDL_JoystickOpen(0);
	}

	SDL_Log("Loading BIOS, %s ...", biosPath);
	Slurp(romBIOS, biosPath);
	if (romPath[0] != 0)
	{
		SDL_Log("Loading ROM, %s ...", romPath);
		Slurp(romCartridge, romPath);
	}
	if (diskPath[0] != 0 && devices[0] != NULL && devices[0]->Read(0) == 0x01)
	{
		SDL_Log("Mounting diskette, %s ...", diskPath);
		auto err = ((DiskDrive*)devices[0])->Mount(diskPath);
		if (err)
			SDL_Log("Error %d trying to open disk image.", err);
	}

	SDL_Log("Resetting Musashi...");
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68ki_set_sr(0);
	m68k_pulse_reset();

	SDL_Log("Asspull IIIx is ready.");
	SDL_Log("Press Ctrl-L to load a ROM or diskette.");
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
	const auto vBlankLasts = (trueLines) * hBlankEvery;
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
				if (ev.jbutton.which == 0)
					joypad |= 16 << ev.jbutton.button;
				break;
			case SDL_JOYBUTTONUP:
				if (ev.jbutton.which == 0)
					joypad &= ~(16 << ev.jbutton.button);
				break;
			case SDL_JOYHATMOTION:
				if (ev.jhat.which == 0 && ev.jhat.hat == 0)
					joypad = (joypad & ~15) | ev.jhat.value;
				break;
			case SDL_KEYDOWN:
				if (ev.key.keysym.scancode < 100)
				{
					keyScan = keyMap[ev.key.keysym.scancode];
					//SDL_Log("KEY: Map 0x%02X (%d) to 0x%02X", ev.key.keysym.scancode, ev.key.keysym.scancode, keyScan);
					if (ev.key.keysym.mod & KMOD_SHIFT) keyScan |= 0x100;
					if (ev.key.keysym.mod & KMOD_ALT) keyScan |= 0x200;
					if (ev.key.keysym.mod & KMOD_CTRL) keyScan |= 0x400;
				}
				break;
			case SDL_KEYUP:
				if (ev.key.keysym.mod & KMOD_LCTRL)
				{
					if (ev.key.keysym.sym == SDLK_l)
						uiCommand = cmdLoadRom;
					else if (ev.key.keysym.sym == SDLK_u)
						uiCommand = (ev.key.keysym.mod & KMOD_SHIFT) ? cmdEject : cmdUnloadRom;
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

		//TODO: enumerate the uiCommands.
		if (uiCommand != cmdNone)
		{
			if (uiCommand == cmdLoadRom)
			{
				//TODO: split off the disk image parts
				nfdchar_t* thePath = NULL;
				nfdresult_t nfdResult = NFD_OpenDialog("ap3,img", NULL, &thePath);
				if (nfdResult == NFD_OKAY)
				{
					auto ext = strrchr(thePath, '.') + 1;
					if (SDL_strncasecmp(ext, "ap3", 3) == 0)
					{
						strcpy_s(romPath, 256, thePath);
						SDL_Log("Loading ROM, %s ...", romPath);
						Slurp(romCartridge, romPath);
						ini->Set("media", "lastROM", romPath);
					}
					else if (SDL_strncasecmp(ext, "img", 3) == 0)
					{
						strcpy_s(diskPath, 256, thePath);
						if (devices[0] != NULL && devices[0]->Read(0) == 0x01)
						{
							auto ret = ((DiskDrive*)devices[0])->Mount(diskPath);
							if (ret == -1)
								SDL_Log("Unmount the diskette first, with Ctrl-Shift-U.");
							else if (ret != 0)
								SDL_Log("Error %d trying to open disk image.", ret);
							else
								ini->Set("media", "lastDisk", diskPath);
						}
					}
					else
						SDL_Log("Don't know what to do with %s.", romPath);
				}
			}
			else if (uiCommand == cmdUnloadRom)
			{
				SDL_Log("Unloading ROM...");
				memset(romCartridge, 0, CART_SIZE);
				strcpy_s(romPath, 256, "");
				ini->Set("media", "lastROM", romPath);
			}
			else if (uiCommand == cmdEject)
			{
				SDL_Log("Ejecting disk...");
				if (devices[0] != NULL && devices[0]->Read(0) == 0x01)
					((DiskDrive*)devices[0])->Unmount();
				strcpy_s(diskPath, 256, "");
				ini->Set("media", "lastDisk", diskPath);
			}
			else if (uiCommand == cmdReset)
			{
				if (uiData)
				{
					SDL_Log("Unloading ROM...");
					memset(romCartridge, 0, CART_SIZE);
					if (devices[0] != NULL && devices[0]->Read(0) == 0x01)
						((DiskDrive*)devices[0])->Unmount();
					strcpy_s(romPath, 256, "");
					ini->Set("media", "lastROM", romPath);
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
		}

		//if (interrupts & 0x80 == 0)
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
				sprintf_s(uiFPS, 32, "%d", (int)averageFPS);
				if (delta < 20)
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
				m68k_execute(hBlankLasts);
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
			ticks++;
			line = 0;
		}
	}

	UninitSound();
	UninitVideo();
	SDL_Quit();
	return 0;
}
