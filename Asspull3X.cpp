#include "asspull.h"

extern "C" {

#include "musashi\m68k.h"
#include "nativefiledialog\src\include\nfd.h"

static bool quit = 0;
int line = 0, interrupts = 0;
extern INLINE void m68ki_set_sr(unsigned int value);

char biosPath[256], romPath[256], diskPath[256];

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
				keyScan = ev.key.keysym.scancode;
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