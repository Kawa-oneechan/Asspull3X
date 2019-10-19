#include <time.h>
#include "asspull.h"
#include "ini.h"

extern "C" {

#include "musashi\m68k.h"
#include "nativefiledialog\src\include\nfd.h"

static bool quit = 0;
int line = 0, interrupts = 0;
extern INLINE void m68ki_set_sr(unsigned int value);

char biosPath[256], romPath[256], diskPath[256];

//Map SDL scancodes
static const unsigned char keyMap[] =
{
	0x00,
	0x00,
	0x00,
	0x00,
	0x1E, //a
	0x30, //b
	0x2E, //c
	0x20, //d
	0x12, //e
	0x21, //f
	0x22, //g
	0x23, //h
	0x17, //i
	0x24, //j
	0x25, //k
	0x26, //l
	0x32, //m
	0x31, //n
	0x18, //o
	0x19, //p
	0x10, //q
	0x13, //r
	0x1F, //s
	0x14, //t
	0x16, //u
	0x2F, //v
	0x11, //w
	0x2D, //x
	0x15, //y
	0x2C, //z
	0x02, //1
	0x03, //2
	0x04, //3
	0x05, //4
	0x06, //5
	0x07, //6
	0x08, //7
	0x09, //8
	0x0A, //9
	0x0B, //0
	0x1C, //enter
	0x01, //escape
	0x0E, //backspace
	0x0F, //tab
	0x39, //space
	0x0C, //-
	0x0D, //=
	0x1A, //[
	0x1B, //]
	0x2B, //backslash
	0x00,
	0x27, //;
	0x28, //'
	0x29, //`
	0x33, //,
	0x34, //.
	0x35, //slash
	0x3A, //caps
	0x3B, //f1
	0x3C, //f2
	0x3D, //f3
	0x3E, //f4
	0x3F, //f5
	0x40, //f6
	0x41, //f7
	0x42, //f8
	0x43, //f9
	0x44, //f10
	0x57, //f11
	0x58, //f12
	0x00, //prtscrn
	0x46, //scrollock
	0x00, //pause
	0xD2, //ins
	0xC7, //home
	0xC9, //pgup
	0xD3, //del
	0xCF, //end
	0xD1, //pgdn
	0xCD, //right
	0xCB, //left
	0xD0, //down
	0xC8, //up
	0x45, //numlock
	0xB5, //kp/
	0x37, //kp*
	0x4A, //kp-
	0x4E, //kp+
	0x9C, //kpenter
	0x4F, //kp1
	0x50, //kp2
	0x51, //kp3
	0x4B, //kp4
	0x4C, //kp5
	0x4D, //kp6
	0x47, //kp7
	0x48, //kp8
	0x49, //kp9
	0x52, //kp0
	0x53, //kp.
};

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
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
		return 0;
	if (InitVideo() < 0)
		return 0;

	auto ini = new IniFile();
	ini->autoSave = true;
	ini->Load("settings.ini");
	auto thing = ini->Get("media", "bios", "roms\\ass-bios.apb"); strcpy_s(biosPath, 256, thing);
	thing = ini->Get("media", "lastROM", ""); strcpy_s(romPath, 256, thing);
	thing = ini->Get("media", "lastDisk", ""); strcpy_s(diskPath, 256, thing);
	thing = ini->Get("media", "midiDevice", ""); auto midiNum = SDL_atoi(thing);

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

	auto mHz = 8;
	auto screenFreq = 60;
	auto pixsPerRow = 640;
	auto lines = 480;
	auto trueLines = 525;
	auto trueWidth = 800;
	auto Hz = mHz * 1000000;
	auto vBlankEvery = Hz / screenFreq;
	auto hBlankEvery = vBlankEvery / lines;
	auto vBlankLasts = (trueLines) * hBlankEvery;
	auto hBlankLasts = (trueWidth - pixsPerRow);

	auto oldTicks = SDL_GetTicks();
	auto frames = 0;
	auto tickTock = 0;

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
				//SDL_Log("keyup: sym %d, mod 0x%x", ev.key.keysym.sym, ev.key.keysym.mod);
				if (ev.key.keysym.mod & KMOD_LCTRL)
				{
					if (ev.key.keysym.sym == SDLK_l)
					{
						nfdchar_t* thePath = NULL;
						nfdresult_t nfdResult = NFD_OpenDialog("ap3,img", NULL, &thePath);
						if (nfdResult != NFD_OKAY) break;
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
						break;
					}
					else if (ev.key.keysym.sym == SDLK_u)
					{
						if (ev.key.keysym.mod & KMOD_LSHIFT)
						{
							if (devices[0] != NULL && devices[0]->Read(0) == 0x01)
								((DiskDrive*)devices[0])->Unmount();
							strcpy_s(diskPath, 256, "");
							ini->Set("media", "lastDisk", diskPath);
						}
						else
						{
							SDL_Log("Unloading ROM...");
							memset(romCartridge, 0, 0x0FF0000);
							strcpy_s(romPath, 256, "");
							ini->Set("media", "lastROM", romPath);
						}
					}
					else if (ev.key.keysym.sym == SDLK_r)
					{
						if (ev.key.keysym.mod & KMOD_LSHIFT)
						{
							SDL_Log("Unloading ROM...");
							memset(romCartridge, 0, 0x0FF0000);
							if (devices[0] != NULL && devices[0]->Read(0) == 0x01)
								((DiskDrive*)devices[0])->Unmount();
							strcpy_s(romPath, 256, "");
							ini->Set("media", "lastROM", romPath);
						}
						SDL_Log("Resetting Musashi...");
						m68k_pulse_reset();
						break;
					}
					else if (ev.key.keysym.sym == SDLK_d)
					{
						SDL_Log("Dumping core...");
						Dump("wram.bin", ramInternal, 0x0400000);
						Dump("vram.bin", ramVideo, 0x0060000);
					}
					else if (ev.key.keysym.sym == SDLK_s)
					{
						char snap[128];
						__time64_t now;
						_time64(&now);
						sprintf_s(snap, 128, "%u.bmp", now);
						SDL_SaveBMP(sdlSurface, snap);
						SDL_Log("Snap! %s saved.", snap);
					}
				}
				keyScan = 0;
				break;
			}
		}

		auto newTicks = SDL_GetTicks();
		if (newTicks >= oldTicks + 1000)
		{
			oldTicks = newTicks;
			char buff[256];
			sprintf_s(buff, 256, "Asspull IIIx - %d FPS", frames);
			SDL_SetWindowTitle(sdlWindow, buff);
			frames = 0;
		}

		//if (interrupts & 0x80 == 0)
			m68k_execute(hBlankEvery);
		if (line < lines)
		{
			HandleHdma(line);
			RenderLine(line);
		}
		if (line == lines)
		{
			VBlank();
			frames++;
			if ((interrupts & 0x84) == 0) //if interrupts are enabled and not already in VBlank
			{
				interrupts |= 4; //set VBlank signal
				m68k_set_virq(M68K_IRQ_7, 1);
			}
		}
		m68k_execute(hBlankLasts);
		line++;
		if (line == trueLines)
		{
			tickTock ^= 1;
			if (tickTock == 0)
				ticks++;
			line = 0;
		}
	}

	UninitSound();
	UninitVideo();
	SDL_Quit();
	return 0;
}

}