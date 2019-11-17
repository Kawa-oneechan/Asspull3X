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

extern unsigned int biosSize, romSize;

IniFile* ini;

char biosPath[256], romPath[256], diskPath[256];

int pauseState = 0;
unsigned char* pauseScreen;
extern unsigned char* pixels;
bool fpsCap;

static const unsigned char bespokeDiskMBS[] =
{
	0xEB,0x3C,0x90,0x41,0x53,0x53,0x50,0x55,0x4C,0x4C,0x21,0x00,0x02,0x01,0x01,0x00,
	0x02,0xE0,0x00,0x40,0x0B,0xF0,0x09,0x00,0x12,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x46,0x41,0x54,0x31,0x32,0x20,0x20,0x20,0x33,0xC9,
	0x8E,0xD1,0xBC,0xF0,0x7B,0x8E,0xD9,0xB8,0x00,0x20,0x8E,0xC0,0xFC,0xBD,0x00,0x7C,
	0x38,0x4E,0x24,0x7D,0x24,0x8B,0xC1,0x99,0xE8,0x3C,0x01,0x72,0x1C,0x83,0xEB,0x3A,
	0x66,0xA1,0x1C,0x7C,0x26,0x66,0x3B,0x07,0x26,0x8A,0x57,0xFC,0x75,0x06,0x80,0xCA,
	0x02,0x88,0x56,0x02,0x80,0xC3,0x10,0x73,0xEB,0x33,0xC9,0x8A,0x46,0x10,0x98,0xF7,
	0x66,0x16,0x03,0x46,0x1C,0x13,0x56,0x1E,0x03,0x46,0x0E,0x13,0xD1,0x8B,0x76,0x11,
	0x60,0x89,0x46,0xFC,0x89,0x56,0xFE,0xB8,0x20,0x00,0xF7,0xE6,0x8B,0x5E,0x0B,0x03,
	0xC3,0x48,0xF7,0xF3,0x01,0x46,0xFC,0x11,0x4E,0xFE,0x61,0xBF,0x00,0x00,0xE8,0xE6,
	0x00,0x72,0x39,0x26,0x38,0x2D,0x74,0x17,0x60,0xB1,0x0B,0xBE,0xA1,0x7D,0xF3,0xA6,
	0x61,0x74,0x32,0x4E,0x74,0x09,0x83,0xC7,0x20,0x3B,0xFB,0x72,0xE6,0xEB,0xDC,0xA0,
	0xFB,0x7D,0xB4,0x7D,0x8B,0xF0,0xAC,0x98,0x40,0x74,0x0C,0x48,0x74,0x13,0xB4,0x0E,
	0xBB,0x07,0x00,0xCD,0x10,0xEB,0xEF,0xA0,0xFD,0x7D,0xEB,0xE6,0xA0,0xFC,0x7D,0xEB,
	0xE1,0xCD,0x16,0xCD,0x19,0x26,0x8B,0x55,0x1A,0x52,0xB0,0x01,0xBB,0x00,0x00,0xE8,
	0x3B,0x00,0x72,0xE8,0x5B,0x8A,0x56,0x24,0xBE,0x0B,0x7C,0x8B,0xFC,0xC7,0x46,0xF0,
	0x3D,0x7D,0xC7,0x46,0xF4,0x29,0x7D,0x8C,0xD9,0x89,0x4E,0xF2,0x89,0x4E,0xF6,0xC6,
	0x06,0x96,0x7D,0xCB,0xEA,0x03,0x00,0x00,0x20,0x0F,0xB6,0xC8,0x66,0x8B,0x46,0xF8,
	0x66,0x03,0x46,0x1C,0x66,0x8B,0xD0,0x66,0xC1,0xEA,0x10,0xEB,0x5E,0x0F,0xB6,0xC8,
	0x4A,0x4A,0x8A,0x46,0x0D,0x32,0xE4,0xF7,0xE2,0x03,0x46,0xFC,0x13,0x56,0xFE,0xEB,
	0x4A,0x52,0x50,0x06,0x53,0x6A,0x01,0x6A,0x10,0x91,0x8B,0x46,0x18,0x96,0x92,0x33,
	0xD2,0xF7,0xF6,0x91,0xF7,0xF6,0x42,0x87,0xCA,0xF7,0x76,0x1A,0x8A,0xF2,0x8A,0xE8,
	0xC0,0xCC,0x02,0x0A,0xCC,0xB8,0x01,0x02,0x80,0x7E,0x02,0x0E,0x75,0x04,0xB4,0x42,
	0x8B,0xF4,0x8A,0x56,0x24,0xCD,0x13,0x61,0x61,0x72,0x0B,0x40,0x75,0x01,0x42,0x03,
	0x5E,0x0B,0x49,0x75,0x06,0xF8,0xC3,0x41,0xBB,0x00,0x00,0x60,0x66,0x6A,0x00,0xEB,
	0xB0,0x53,0x54,0x41,0x52,0x54,0x2E,0x41,0x50,0x50,0x20,0x20,0x0D,0x0A,0x54,0x68,
	0x69,0x73,0x20,0x69,0x73,0x20,0x6E,0x6F,0x74,0x20,0x66,0x6F,0x72,0x20,0x79,0x6F,
	0x75,0x72,0x20,0x50,0x43,0x2E,0x20,0x50,0x75,0x6C,0x6C,0x20,0x69,0x74,0x20,0x6F,
	0x75,0x74,0x2C,0x20,0x74,0x68,0x65,0x6E,0x20,0x70,0x72,0x65,0x73,0x73,0x20,0x61,
	0x6E,0x79,0x20,0x6B,0x65,0x79,0x20,0x74,0x6F,0x20,0x72,0x65,0x73,0x74,0x61,0x72,
	0x74,0x2E,0x0D,0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xAC,0xCB,0xD8,0x55,0xAA,
};
static const unsigned char bespokeVolumeLabel[] =
{
	0x54,0x48,0x45,0x20,0x41,0x53,0x53,0x44,0x49,0x53,0x4B,0x08,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x78,0xB3,0x69,0x4F
};

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
	thing = ini->Get("media", "lastDisk", ""); strcpy_s(diskPath, 256, thing);
	thing = ini->Get("media", "midiDevice", ""); auto midiNum = SDL_atoi(thing);
	thing = ini->Get("video", "fpscap", "true"); if (thing[0] == 't' || thing[0] == 'T' || thing[0] == 1) fpsCap = true;

	if (InitVideo() < 0)
		return 0;

	//Absolutely always load a disk drive as #0
	devices[0] = (Device*)(new DiskDrive());
	for (int i = 1; i < MAXDEVS; i++)
	{
		char key[8];
		char dft[24] = "";
		//Always load a lineprinter as #1 by default
		if (i == 1) strcpy_s(dft, 24, "linePrinter");
		SDL_itoa(i, key, 10);
		thing = ini->Get("devices", key, dft);
		if (thing[0] == 0) continue;
		if (!strcmp(thing, "diskDrive"))
		{
			SDL_Log("Can only have one disk drive, at device #0.");
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
	if (diskPath[0] != 0)
	{
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

		//TODO: enumerate the uiCommands.
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
						strcpy_s(diskPath, 256, thePath);
						auto ret = ((DiskDrive*)devices[0])->Mount(diskPath);
						if (ret == -1)
							SetStatus("Eject the diskette first, with Ctrl-Shift-U.");
						else if (ret != 0)
							SDL_Log("Error %d trying to open disk image.", ret);
						else
							ini->Set("media", "lastDisk", diskPath);
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
				((DiskDrive*)devices[0])->Unmount();
				strcpy_s(diskPath, 256, "");
				ini->Set("media", "lastDisk", diskPath);
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
			else if (uiCommand == cmdCreateDisk)
			{
				nfdchar_t* thePath = NULL;
				nfdresult_t nfdResult = NFD_SaveDialog("img", NULL, &thePath);
				if (nfdResult == NFD_OKAY)
				{
					FILE* file = NULL;
					auto ret = fopen_s(&file, thePath, "wb");
					fseek(file, 1474560 - 1, SEEK_SET);
					fputc(0, file);
					fseek(file, 0, SEEK_SET);
					fwrite(bespokeDiskMBS, sizeof(bespokeDiskMBS), 1, file);
					fseek(file, 0x0027, SEEK_SET);
					unsigned short volumeID = rand();
					fwrite(&volumeID, 4, 1, file);
					fseek(file, 0x2600, SEEK_SET);
					fwrite(bespokeVolumeLabel, sizeof(bespokeVolumeLabel), 1, file);
					fclose(file);
					if (((DiskDrive*)devices[0])->Mount(thePath) == 0)
						ini->Set("media", "lastDisk", thePath);
				}
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
