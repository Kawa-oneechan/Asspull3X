#include "asspull.h"
#include "resource.h"
#include "miniz.h"

extern "C" {
#include "musashi/m68k.h"
}

bool fpsCap;
bool quit = 0;
int line = 0, interrupts = 0;
int invertButtons = 0;
extern void ResetAudio();
extern void ReportLoadingFail(int messageId, int err, int device, const WCHAR* fileName);

extern unsigned int biosSize, romSize;
extern long rtcOffset;

void LoadROM(const WCHAR* path)
{
	unsigned int fileSize = 0;

	WCHAR lpath[512];
	for (int i = 0; i < 512; i++)
	{
		lpath[i] =  towlower(path[i]);
		if (path[i] == 0)
			break;
	}

	auto ext = wcsrchr(lpath, L'.') + 1;
	if (!wcscmp(ext, L"ap3"))
	{
		memset(romCartridge, 0, CART_SIZE);
		auto err = Slurp(romCartridge, path, &romSize);
		if (err)
			ReportLoadingFail(IDS_ROMLOADERROR, err, -1, path);
	}
	else if (!wcscmp(ext, L"a3z"))
	{
		mz_zip_archive zip;
		memset(&zip, 0, sizeof(zip));
		char zipPath[512] = { 0 };
		wcstombs(zipPath, path, 512);
		mz_zip_reader_init_file(&zip, zipPath, 0);

		bool foundSomething = false;
		for (int i = 0; i < (int)mz_zip_reader_get_num_files(&zip); i++)
		{
			mz_zip_archive_file_stat fs;
			if (!mz_zip_reader_file_stat(&zip, i, &fs))
			{
				mz_zip_reader_end(&zip);
				return;
			}

			if (!strchr(fs.m_filename, '.'))
				continue;

			auto ext2 = strrchr(fs.m_filename, '.') + 1;
			if (!_stricmp(ext2, "ap3"))
			{
				foundSomething = true;
				romSize = (unsigned int)fs.m_uncomp_size;
				memset(romCartridge, 0, CART_SIZE);
				mz_zip_reader_extract_to_mem(&zip, i, romCartridge, romSize, 0);
				break;
			}
		}
		mz_zip_reader_end(&zip);
		if (!foundSomething)
		{
			UI::SetStatus(IDS_NOTHINGINZIP); //"No single AP3 file found in archive."
			return;
		}
	}

#if _CONSOLE
	fileSize = romSize;
	romSize = RoundUp(romSize);
	if (romSize != fileSize)
		Log(GetString(IDS_BADSIZE), fileSize, fileSize, romSize, romSize); //"File size is not a power of two: is %d (0x%08X), should be %d (0x%08X)."

	unsigned int c1 = 0;
	unsigned int c2 = (romCartridge[0x20] << 24) | (romCartridge[0x21] << 16) | (romCartridge[0x22] << 8) | (romCartridge[0x23] << 0);
	for (unsigned int i = 0; i < romSize; i++)
	{
		if (i == 0x20)
			i += 4; //skip the checksum itself
		c1 += romCartridge[i];
	}
	if (c1 != c2)
		Log(GetString(IDS_BADCHECKSUM), c2, c1); //"Checksum mismatch: is 0x%08X, should be 0x%08X."
#endif

	ini.SetValue(L"media", L"lastROM", path);
	UI::ResetPath();
	ini.SaveFile(UI::settingsFile, false);

	char romName[32] = { 0 };
	memcpy(romName, romCartridge + 8, 24);
	Discord::UpdateDiscordPresence(romName);
	UI::SetTitle(romName);
}

int pauseState = 0;
unsigned char* pauseScreen;

void MainLoop()
{
	pauseScreen = new unsigned char[640 * 480 * 4]();

	Log(UI::GetString(IDS_RESETTING));
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	ResetAudio();
	m68k_pulse_reset();

#if _CONSOLE
	wprintf(GetString(IDS_ASSPULLISREADY)); //"Asspull IIIx is ready."...
#endif

	SDL_Event ev;

	const auto mHz = 16;
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

	bool gottaReset = false;

	UI::SetStatus(IDS_CLICKTORELEASE); //"Middle-click or RCtrl-P to pause emulation."

	while (!quit)
	{
		if (SDL_PollEvent(&ev) != 0)
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				quit = true;
				break;

			case SDL_JOYBUTTONDOWN:
				if (ev.jbutton.which < 2)
				{
					//If the button is 0-3 it's ABXY. Any higher must be LB/RB, Back, or Start.
					if (ev.jbutton.button < 4)
						joypad[ev.jbutton.which] |= 16 << (ev.jbutton.button ^ invertButtons);
					else
						joypad[ev.jbutton.which + 2] |= 1 << (ev.jbutton.button - 4);
				}
				break;
			case SDL_JOYBUTTONUP:
				if (ev.jbutton.which < 2)
				{
					if (ev.jbutton.button < 4)
						joypad[ev.jbutton.which] &= ~(16 << (ev.jbutton.button ^ invertButtons));
					else
						joypad[ev.jbutton.which + 2] &= ~(1 << (ev.jbutton.button - 4));
				}
				break;
			case SDL_JOYHATMOTION:
				if (ev.jhat.which < 2 && ev.jhat.hat == 0)
					joypad[ev.jhat.which] = (joypad[ev.jhat.which] & ~15) | ev.jhat.value;
				break;
			case SDL_JOYAXISMOTION:
				if (ev.jaxis.axis > 2) ev.jaxis.axis -= 3; //map right stick and trigger to left
				if (ev.jaxis.which < 2)
					joyaxes[(ev.jaxis.which * 2) + ev.jaxis.axis] = ev.jaxis.value >> 8;
				break;
			case SDL_KEYUP:
			{
				if (ev.key.keysym.mod & KMOD_RCTRL)
				{
					if (ev.key.keysym.sym == SDLK_l)
						UI::uiCommand = (ev.key.keysym.mod & KMOD_SHIFT) ? cmdInsertDisk : cmdLoadRom;
					else if (ev.key.keysym.sym == SDLK_u)
						UI::uiCommand = (ev.key.keysym.mod & KMOD_SHIFT) ? cmdEjectDisk : cmdUnloadRom;
					else if (ev.key.keysym.sym == SDLK_r)
						UI::uiCommand = cmdReset;
					else if (ev.key.keysym.sym == SDLK_d)
						UI::uiCommand = cmdDump;
					else if (ev.key.keysym.sym == SDLK_s)
						UI::uiCommand = cmdScreenshot;
					else if (ev.key.keysym.sym == SDLK_p)
					{
						if (pauseState == 0)
							pauseState = 1;
						else if (pauseState == 2)
							pauseState = 0;
					}
				}
				break;
			}
			case SDL_MOUSEBUTTONUP:
				if (ev.button.button == 2)
				{
					if (pauseState == 0)
						pauseState = 1;
					else if (pauseState == 2)
						pauseState = 0;
				}
			case SDL_WINDOWEVENT:
				if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
					UI::ResizeStatusBar();
			}
		}

		if (gottaReset)
		{
			UI::uiCommand = cmdReset;
			gottaReset = false;
		}

		if (UI::uiCommand != cmdNone)
		{
			if (UI::uiCommand == cmdLoadRom)
			{
				UI::ShowOpenFileDialog(cmdLoadRom, UI::GetString(IDS_CARTFILTER)); //"Asspull IIIx ROMS (*.ap3)|*.ap3"
				if (UI::uiCommand == 0) continue;
				Log(UI::GetString(IDS_LOADINGROM), UI::uiString); //"Loading ROM, %s ..."
				gottaReset = (*(uint32_t*)romCartridge == 0x21535341);
				LoadROM(UI::uiString);
			}
			else if (UI::uiCommand == cmdInsertDisk)
			{
				if (devices[0] == NULL || devices[0]->GetID() != 0x0144)
					UI::SetStatus(IDS_NODISKDRIVE); //"No disk drive."
				else if (((DiskDrive*)devices[0])->IsMounted())
					UI::SetStatus(IDS_UNMOUNTFIRST); //"Unmount the medium first."
				else
					UI::InsertDisk(0);
			}
			else if (UI::uiCommand == cmdUnloadRom)
			{
				Log(UI::GetString(IDS_UNLOADINGROM)); //"Unloading ROM..."
				memset(romCartridge, 0, CART_SIZE);
				ini.SetValue(L"media", L"lastROM", L"");
				UI::ResetPath();
				ini.SaveFile(UI::settingsFile, false);
				Video::gfxFade = 31;
				UI::SetStatus(IDS_CARTEJECTED); //"Cart pulled."
				Discord::UpdateDiscordPresence(NULL);
				UI::SetTitle(NULL);
			}
			else if (UI::uiCommand == cmdEjectDisk)
			{
				if (devices[0] == NULL || devices[0]->GetID() != 0x0144)
					UI::SetStatus(IDS_NODISKDRIVE); //"No disk drive."
				else if (((DiskDrive*)devices[0])->IsMounted())
					UI::EjectDisk(0);
			}
			else if (UI::uiCommand == cmdReset)
			{
				if (SDL_GetModState() & KMOD_SHIFT)
				{
					Log(UI::GetString(IDS_UNLOADINGROM)); //"Unloading ROM..."
					memset(romCartridge, 0, CART_SIZE);
					ini.SetValue(L"media", L"lastROM", L"");
					UI::ResetPath();
					ini.SaveFile(UI::settingsFile, true);
					Discord::UpdateDiscordPresence(NULL);
					UI::SetTitle(NULL);
				}
				Log(UI::GetString(IDS_SYSTEMRESET)); //"Resetting Musashi..."
				UI::SetStatus(IDS_SYSTEMRESET); //"System reset."
				ResetAudio();
				m68k_pulse_reset();
			}
			else if (UI::uiCommand == cmdQuit)
			{
				quit = true;
			}
			else if (UI::uiCommand == cmdDump)
			{
				Dump(L"wram.bin", ramInternal, WRAM_SIZE);
				Dump(L"vram.bin", ramVideo, VRAM_SIZE);
				UI::SetStatus(IDS_DUMPEDRAM); //"Dumping core..."
			}
			else if (UI::uiCommand == cmdScreenshot)
			{
				Video::Screenshot();
			}
			UI::uiCommand = 0;
			UI::uiString[0] = 0;
		}

		if (pauseState != 2)
		{
			m68k_execute(hBlankEvery);
			if (line < lines)
			{
				HandleHdma(line);
				Video::RenderLine(line);
			}
			m68k_execute(hBlankLasts);
			if (line == lines)
			{
				if (pauseState == 1) //pausing now!
				{
					memcpy(pauseScreen, Video::pixels, 640 * 480 * 4);
					for (auto i = 0; i < 640 * 480 * 4; i += 4)
					{
						pauseScreen[i + 0] /= 2;
						pauseScreen[i + 1] /= 2;
						pauseScreen[i + 2] /= 2;
					}
					pauseState = 2;
				}
				UI::HandleUI();
				Video::VBlank();

				if (!startTime)
					startTime = SDL_GetTicks();
				else
					delta = endTime - startTime;

				auto averageFPS = frames / (SDL_GetTicks() / 1000.0f);
				if (averageFPS > 2000000)
					averageFPS = 0;
				UI::SetFPS((int)averageFPS);
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
		}
		else if (pauseState == 2)
		{
			memcpy(Video::pixels, pauseScreen, 640 * 480 * 4);
			UI::LetItSnow();
			Video::VBlank();
		}
		line++;
		if (line == trueLines)
		{
			m68k_set_virq(M68K_IRQ_7, 0);
			ticks++;
			line = 0;
		}
	}

	ResetAudio();
	for (int i = 0; i < MAXDEVS; i++)
		if (devices[i] != NULL) delete devices[i];
	delete[] pauseScreen;
	delete[] romBIOS;
	delete[] romCartridge;
	delete[] ramInternal;
	delete[] ramVideo;
}
