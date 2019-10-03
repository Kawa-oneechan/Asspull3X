#include "asspull.h"

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

void pc_change(unsigned int new_pc)
{
}

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

void SaveSettings()
{
	FILE* settings = NULL;
	int err = fopen_s(&settings, "settings.txt", "w");
	if (!err)
	{
		fputs(biosPath, settings); fputc('\n', settings);
		fputs(romPath, settings); fputc('\n', settings);
		fputs(diskPath, settings); fputc('\n', settings);
	}
	fclose(settings);
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		return 0;
	if (InitVideo() < 0)
		return 0;
	//SDL_FillRect(sdlSurface, NULL, SDL_MapRGB(sdlSurface->format, 0x60, 0x40, 0x20));
	//SDL_UpdateWindowSurface(sdlWindow);

	if (InitMemory() < 0)
		return 0;

	if (InitSound() < 0)
		return 0;

	FILE* settings = NULL;
	int err = fopen_s(&settings, "settings.txt", "r");
	if (err)
	{
		strcpy_s(biosPath, 256, "roms\\ass-bios.apb");
		strcpy_s(romPath, 256, "-");
		strcpy_s(diskPath, 256, "-");
	}
	else
	{
		fgets(biosPath, 256, settings);
		fgets(romPath, 256, settings);
		fgets(diskPath, 256, settings);
		if (biosPath[strlen(biosPath) - 1] == '\n') biosPath[strlen(biosPath) - 1] = 0;
		if (romPath[strlen(romPath) - 1] == '\n') romPath[strlen(romPath) - 1] = 0;
		if (diskPath[strlen(diskPath) - 1] == '\n') diskPath[strlen(diskPath) - 1] = 0;
		fclose(settings);
	}
	SDL_Log("Loading BIOS, %s ...", biosPath);
	Slurp(romBIOS, biosPath);
	if (romPath[0] != '-')
	{
		SDL_Log("Loading ROM, %s ...", romPath);
		Slurp(romCartridge, romPath);
	}
	if (diskPath[0] != '-')
	{
		SDL_Log("Mounting diskette, %s ...", diskPath);
		auto err = fopen_s(&diskFile, diskPath, "rb+");
	}

	SDL_Log("Resetting Musashi...");
	m68k_init();
	m68k_set_pc_changed_callback(pc_change);
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68ki_set_sr(0);
	m68k_pulse_reset();

	SDL_Log("Asspull IIIx is ready.");
	SDL_Log("Press Ctrl-L to load a ROM or diskette.");
	SDL_Log("Press Ctrl-U to unload ROM, Ctrl-Shift-U to unload diskette.");
	SDL_Log("Press Ctrl-R to reset, Ctrl-Shift-R to unload and reset.");

	SDL_Event ev;
	
	auto mHz = 4;
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
							SaveSettings();
						}
						else if (SDL_strncasecmp(ext, "img", 3) == 0)
						{
							strcpy_s(diskPath, 256, thePath);
							if (diskFile != NULL)
							{
								SDL_Log("Unmounting diskette...");
								fclose(diskFile);
								diskFile = NULL;
							}
							SDL_Log("Mounting diskette, %s ...", diskPath);
							auto err = fopen_s(&diskFile, diskPath, "rb+");
							SaveSettings();
						}
						else
							SDL_Log("Don't know what to do with %s.", romPath);
						break;
					}
					else if (ev.key.keysym.sym == SDLK_u)
					{
						if (ev.key.keysym.mod & KMOD_LSHIFT)
						{
							if (diskFile != NULL)
							{
								SDL_Log("Unmounting diskette...");
								fclose(diskFile);
								diskFile = NULL;
								strcpy_s(diskPath, 256, "-");
								SaveSettings();
							}
						}
						else
						{
							SDL_Log("Unloading ROM...");
							memset(romCartridge, 0, 0x0FF0000);
							strcpy_s(romPath, 256, "-");
							SaveSettings();
						}
					}
					else if (ev.key.keysym.sym == SDLK_r)
					{
						if (ev.key.keysym.mod & KMOD_LSHIFT)
						{
							SDL_Log("Unloading ROM...");
							memset(romCartridge, 0, 0x0FF0000);
							if (diskFile != NULL)
							{
								SDL_Log("Unmounting diskette...");
								fclose(diskFile);
								diskFile = NULL;
							}
							SaveSettings();
						}
						SDL_Log("Resetting Musashi...");
						m68k_pulse_reset();
						break;
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
			if ((interrupts & 0x80) == 0)
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