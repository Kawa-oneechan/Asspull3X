#include "asspull.h"

extern "C" {
#include "musashi\m68k.h"
}
#include "nativefiledialog\src\include\nfd.h"

static bool quit = 0;
int line = 0, interrupts = 0;
extern void Screenshot();
extern int uiCommand, uiData;
extern char uiFPS[];
extern void SetStatus(char*);

extern unsigned int biosSize, romSize;

IniFile* ini;

char biosPath[256], romPath[256];

int pauseState = 0;
unsigned char* pauseScreen;
extern unsigned char* pixels;
bool fpsCap;

unsigned int RoundUp(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

int Slurp(unsigned char* dest, const char* filePath, unsigned int* size)
{
	FILE* file = NULL;
	int err = fopen_s(&file, filePath, "rb");
	if (err)
		return err;
	fseek(file, 0, SEEK_END);
	long fs = ftell(file);
	if (size != 0) *size = (unsigned int)fs;
	fseek(file, 0, SEEK_SET);
	fread(dest, 1, fs, file);
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

#ifdef _CONSOLE
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char*argv[])
#endif
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_VIDEO_OPENGL | SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	ini = new IniFile();
	ini->autoSave = true;
	ini->Load("settings.ini");
	auto thing = ini->Get("media", "bios", "roms\\ass-bios.apb"); strcpy_s(biosPath, 256, thing);
	thing = ini->Get("media", "lastROM", ""); strcpy_s(romPath, 256, thing);
	thing = ini->Get("media", "midiDevice", ""); auto midiNum = SDL_atoi(thing);
	thing = ini->Get("video", "fpscap", "true"); if (thing[0] == 't' || thing[0] == 'T' || thing[0] == 1) fpsCap = true;

	if (InitVideo() < 0)
		return 0;

	//Absolutely always load a disk drive as #0
	for (int i = 0; i < MAXDEVS; i++)
	{
		char key[8];
		char dft[24] = "";
		//Always load a lineprinter as #1 by default
		if (i == 1) strcpy_s(dft, 24, "linePrinter");
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
	Slurp(romBIOS, biosPath, &biosSize);
	biosSize = RoundUp(biosSize);
	if (romPath[0] != 0)
	{
		SDL_Log("Loading ROM, %s ...", romPath);
		Slurp(romCartridge, romPath, &romSize);
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
				nfdchar_t* thePath = NULL;
				nfdresult_t nfdResult = NFD_OpenDialog("ap3", NULL, &thePath);
				if (nfdResult == NFD_OKAY)
				{
					auto ext = strrchr(thePath, '.') + 1;
					if (SDL_strncasecmp(ext, "ap3", 3) == 0)
					{
						strcpy_s(romPath, 256, thePath);
						SDL_Log("Loading ROM, %s ...", romPath);
						auto gottaReset = (*(uint32_t*)romCartridge == 0x21535341);
						memset(romCartridge, 0, CART_SIZE);
						Slurp(romCartridge, romPath, &romSize);
						romSize = RoundUp(romSize);
						ini->Set("media", "lastROM", romPath);
						if (gottaReset)
							m68k_pulse_reset();
					}
					else
						SDL_Log("Don't know what to do with %s.", romPath);
				}
			}
			else if (uiCommand == cmdInsertDisk)
			{
				nfdchar_t* thePath = NULL;
				nfdresult_t nfdResult = NFD_OpenDialog("img", NULL, &thePath);
				if (nfdResult == NFD_OKAY)
				{
					auto ext = strrchr(thePath, '.') + 1;
					if (SDL_strncasecmp(ext, "img", 3) == 0)
					{
						auto ret = ((DiskDrive*)devices[uiData])->Mount(thePath);
						if (ret == -1)
							SetStatus("Eject the diskette first, with Ctrl-Shift-U.");
						else if (ret != 0)
							SDL_Log("Error %d trying to open disk image.", ret);
						else
							ini->Set("devices/diskDrive", "0", thePath);
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
				gfxFade = 31;
				SetStatus("Cart pulled.");
			}
			else if (uiCommand == cmdEjectDisk)
			{
				((DiskDrive*)devices[uiData])->Unmount();
				ini->Set("devices/diskDrive", "0", "");
				SetStatus("Disk ejected.");
			}
			else if (uiCommand == cmdReset)
			{
				if (uiData)
				{
					SDL_Log("Unloading ROM...");
					memset(romCartridge, 0, CART_SIZE);
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
			line = 0;
		}
	}

	UninitSound();
	UninitVideo();
	SDL_Quit();
	return 0;
}
