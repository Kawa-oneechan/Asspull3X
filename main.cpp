#include "asspull.h"
#include "resource.h"
#include "miniz.h"

extern "C" {
#include "musashi/m68k.h"
}

bool quit = 0;
int line = 0, interrupts = 0;
int invertButtons = 0;
int key2joy = 0;

extern unsigned int biosSize, romSize;
extern long rtcOffset;

int firstDiskDrive = -1;

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
			UI::ReportLoadingFail(IDS_ROMLOADERROR, err, -1, path);
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
		Log(UI::GetString(IDS_BADSIZE), fileSize, fileSize, romSize, romSize); //"File size is not a power of two: is %d (0x%08X), should be %d (0x%08X)."

	unsigned int c1 = 0;
	unsigned int c2 = (romCartridge[0x20] << 24) | (romCartridge[0x21] << 16) | (romCartridge[0x22] << 8) | (romCartridge[0x23] << 0);
	for (unsigned int i = 0; i < romSize; i++)
	{
		if (i == 0x20)
			i += 4; //skip the checksum itself
		c1 += romCartridge[i];
	}
	if (c1 != c2)
		Log(UI::GetString(IDS_BADCHECKSUM), c2, c1); //"Checksum mismatch: is 0x%08X, should be 0x%08X."
#endif

	ini.SetValue(L"media", L"rom", path);
	UI::SaveINI();

	char romName[32] = { 0 };
	memcpy(romName, romCartridge + 8, 24);
	Discord::SetPresence(romName);
	UI::SetTitle(romName);
}

void FindFirstDrive()
{
	int old = firstDiskDrive;
	firstDiskDrive = -1;
	bool firstIsHDD = false;
	for (int i = 0; i < MAXDEVS; i++)
	{
		if (devices[i] != nullptr && devices[i]->GetID() == DEVID_DISKDRIVE)
		{
			if (((DiskDrive*)devices[i])->GetType() == ddHardDisk && firstDiskDrive == -1)
			{
				firstIsHDD = true;
				firstDiskDrive = i;
				continue;
			}
			firstDiskDrive = i;
			//if (old != i) Log(L"First disk drive is now #%d.", i);
			return;
		}
	}
}

pauseStates pauseState = pauseNot;
unsigned char* pauseScreen;

void MainLoop()
{
	if ((pauseScreen = new unsigned char[SCREENBUFFERSIZE]()) == nullptr)
	{
		UI::Complain(IDS_PAUSEFAIL);
		return;
	}

	Log(UI::GetString(IDS_RESETTING));
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	Sound::Reset();
	m68k_pulse_reset();

#if _CONSOLE
	wprintf(UI::GetString(IDS_ASSPULLISREADY)); //"Asspull IIIx is ready."...
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
			{
				quit = true;
				break;
			}
			case SDL_JOYBUTTONDOWN:
			{
				if (ev.jbutton.which < 2)
				{
					//If the button is 0-3 it's ABXY. Any higher must be LB/RB, Back, or Start.
					if (ev.jbutton.button < 4)
						joypad[ev.jbutton.which] |= 16 << (ev.jbutton.button ^ invertButtons);
					else
						joypad[ev.jbutton.which + 2] |= 1 << (ev.jbutton.button - 4);
				}
				break;
			}
			case SDL_JOYBUTTONUP:
			{
				if (ev.jbutton.which < 2)
				{
					if (ev.jbutton.button < 4)
						joypad[ev.jbutton.which] &= ~(16 << (ev.jbutton.button ^ invertButtons));
					else
						joypad[ev.jbutton.which + 2] &= ~(1 << (ev.jbutton.button - 4));
				}
				break;
			}
			case SDL_JOYHATMOTION:
			{
				if (ev.jhat.which < 2 && ev.jhat.hat == 0)
					joypad[ev.jhat.which] = (joypad[ev.jhat.which] & ~15) | ev.jhat.value;
				break;
			}
			case SDL_JOYAXISMOTION:
			{
				if (ev.jaxis.axis > 2) ev.jaxis.axis -= 3; //map right stick and trigger to left
				if (ev.jaxis.which < 2)
					joyaxes[(ev.jaxis.which * 2) + ev.jaxis.axis] = ev.jaxis.value >> 8;
				break;
			}
			case SDL_KEYDOWN:
			{
				bool k2joyed = true;
				if (key2joy && !(ev.key.keysym.mod & KMOD_RCTRL))
				{
					switch (ev.key.keysym.sym)
					{
					case SDLK_UP: joypad[0] |= 1; break;
					case SDLK_RIGHT: joypad[0] |= 2; break;
					case SDLK_DOWN: joypad[0] |= 4; break;
					case SDLK_LEFT: joypad[0] |= 8; break;
					case SDLK_z: joypad[0] |= 16; break;
					case SDLK_x: joypad[0] |= 32; break;
					case SDLK_a: joypad[0] |= 64; break;
					case SDLK_s: joypad[0] |= 128; break;
					case SDLK_d: joypad[2] |= 1; break;
					case SDLK_f: joypad[2] |= 2; break;
					case SDLK_c: joypad[2] |= 4; break;
					case SDLK_v: joypad[2] |= 8; break;
					default: k2joyed = false;
					}
					if (k2joyed) break;
				}
				else if (ev.key.repeat)
				{
					inputDev->Enqueue(ev.key.keysym);
				}
				break;
			}
			case SDL_KEYUP:
			{
				bool k2joyed = true;
				if (key2joy && !(ev.key.keysym.mod & KMOD_RCTRL))
				{
					switch (ev.key.keysym.sym)
					{
					case SDLK_UP: joypad[0] &= ~1; break;
					case SDLK_RIGHT: joypad[0] &= ~2; break;
					case SDLK_DOWN: joypad[0] &= ~4; break;
					case SDLK_LEFT: joypad[0] &= ~8; break;
					case SDLK_z: joypad[0] &= ~16; break;
					case SDLK_x: joypad[0] &= ~32; break;
					case SDLK_a: joypad[0] &= ~64; break;
					case SDLK_s: joypad[0] &= ~128; break;
					case SDLK_d: joypad[2] &= ~1; break;
					case SDLK_f: joypad[2] &= ~2; break;
					case SDLK_c: joypad[2] &= ~4; break;
					case SDLK_v: joypad[2] &= ~8; break;
					default: k2joyed = false;
					}
					if (k2joyed) break;
				}
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
					else if (ev.key.keysym.sym == SDLK_a)
					{
						Video::stretch200 = !Video::stretch200;
						if (UI::Options::hWnd)
							CheckDlgButton(UI::Options::hWnd, IDC_ASPECT, Video::stretch200);
						ini.SetBoolValue(L"video", L"stretch200", Video::stretch200);
						ini.SaveFile(UI::settingsFile, false);
					}
					else if (ev.key.keysym.sym == SDLK_k)
					{
						key2joy = !key2joy;
						if (UI::Options::hWnd)
							CheckDlgButton(UI::Options::hWnd, IDC_KEY2JOY, key2joy);
						ini.SetBoolValue(L"input", L"key2joy", key2joy == 1);
						ini.SaveFile(UI::settingsFile, false);
					}
					else if (ev.key.keysym.sym == SDLK_p)
					{
						if (pauseState == pauseNot)
							pauseState = pauseEntering;
						else if (pauseState == pauseYes)
							pauseState = pauseNot;
					}
				}
				else
				{
					inputDev->Enqueue(ev.key.keysym);
				}
				break;
			}
			case SDL_MOUSEBUTTONUP:
				if (ev.button.button == 2)
				{
					/*
					if (pauseState == pauseNot)
						pauseState = pauseEntering;
					else if (pauseState == pauseYes)
						pauseState = pauseNot;
					*/
					UI::mouseLocked = !UI::mouseLocked;
					SDL_ShowCursor(!UI::mouseLocked);
					SDL_CaptureMouse((SDL_bool)UI::mouseLocked);
				}
			case SDL_WINDOWEVENT:
				if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
					UI::ResizeStatusBar();
				else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					UI::mouseLocked = false;
					SDL_ShowCursor(true);
					SDL_CaptureMouse(SDL_FALSE);
				}
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
				if (firstDiskDrive == -1)
					UI::SetStatus(IDS_NODISKDRIVE); //"No disk drive."
				else if (((DiskDrive*)devices[firstDiskDrive])->IsMounted())
					UI::SetStatus(IDS_UNMOUNTFIRST); //"Unmount the medium first."
				else
					UI::InsertDisk(firstDiskDrive);
			}
			else if (UI::uiCommand == cmdUnloadRom)
			{
				Log(UI::GetString(IDS_UNLOADINGROM)); //"Unloading ROM..."
				memset(romCartridge, 0, CART_SIZE);
				ini.SetValue(L"media", L"rom", L"");
				UI::SaveINI();
				UI::SetStatus(IDS_CARTEJECTED); //"Cart pulled."
				Discord::SetPresence(NULL);
				UI::SetTitle(NULL);
			}
			else if (UI::uiCommand == cmdEjectDisk)
			{
				if (firstDiskDrive == -1)
					UI::SetStatus(IDS_NODISKDRIVE); //"No disk drive."
				else if (((DiskDrive*)devices[firstDiskDrive])->IsMounted())
					UI::EjectDisk(firstDiskDrive);
			}
			else if (UI::uiCommand == cmdReset)
			{
				if (SDL_GetModState() & KMOD_SHIFT)
				{
					Log(UI::GetString(IDS_UNLOADINGROM)); //"Unloading ROM..."
					memset(romCartridge, 0, CART_SIZE);
					ini.SetValue(L"media", L"rom", L"");
					UI::SaveINI();
					Discord::SetPresence(NULL);
					UI::SetTitle(NULL);
				}
				Log(UI::GetString(IDS_SYSTEMRESET)); //"Resetting Musashi..."
				UI::SetStatus(IDS_SYSTEMRESET); //"System reset."
				ResetMemory();
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
				for (int i = 0; i < MAXDEVS; i++)
					if (devices[i] != NULL) devices[i]->HBlank();
			}
			m68k_execute(hBlankLasts);
			if (line == lines)
			{
				if (pauseState == pauseEntering) //pausing now!
				{
					memcpy(pauseScreen, Video::pixels, SCREENBUFFERSIZE);
					for (auto i = 0; i < SCREENBUFFERSIZE; i += 4)
					{
						//what if we made it blue instead lol
						//pauseScreen[i + 0] /= 2;
						pauseScreen[i + 1] /= 2;
						pauseScreen[i + 2] /= 2;
					}
					pauseState = pauseYes;
				}
				UI::Update();
				Video::VBlank();
				for (int i = 0; i < MAXDEVS; i++)
					if (devices[i] != NULL) devices[i]->VBlank();

				if (!startTime)
					startTime = SDL_GetTicks();
				else
					delta = endTime - startTime;

				auto averageFPS = frames / (SDL_GetTicks() / 1000.0f);
				if (averageFPS > 2000000)
					averageFPS = 0;
				UI::SetFPS((int)averageFPS);
				if (UI::fpsCap && delta < 20)
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
			memcpy(Video::pixels, pauseScreen, SCREENBUFFERSIZE);
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

	Sound::Reset();
	for (int i = 0; i < MAXDEVS; i++)
		if (devices[i] != NULL) delete devices[i];
	delete[] pauseScreen;
	delete[] romBIOS;
	delete[] romCartridge;
	delete[] ramInternal;
	delete[] ramVideo;
}
