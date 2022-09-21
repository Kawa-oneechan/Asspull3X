#include "asspull.h"
#include "resource.h"

extern "C" {
#include "musashi/m68k.h"
}

bool quit = 0;
int line = 0, interrupts = 0;
int key2joy = 0;

SDL_GameControllerButton buttonMap[16];

extern unsigned int biosSize, romSize;
extern long rtcOffset;

int firstDiskDrive = -1;

WCHAR currentROM[FILENAME_MAX], currentSRAM[FILENAME_MAX];

SDL_GameController *controller[2] = { NULL , NULL };

int IllegalMangrasp(int i)
{
	static unsigned int lastPC = -1;
	auto pc = m68k_get_reg(NULL, M68K_REG_PC);
	if (lastPC == pc)
		return 0;
	lastPC = pc;
	auto ppc = m68k_get_reg(NULL, M68K_REG_PPC);
	auto sp = m68k_get_reg(NULL, M68K_REG_SP);
	auto a0 = m68k_get_reg(NULL, M68K_REG_A0);
	auto a1 = m68k_get_reg(NULL, M68K_REG_A1);
	auto a2 = m68k_get_reg(NULL, M68K_REG_A2);
	auto a3 = m68k_get_reg(NULL, M68K_REG_A3);
	auto a4 = m68k_get_reg(NULL, M68K_REG_A4);
	auto a5 = m68k_get_reg(NULL, M68K_REG_A5);
	auto a6 = m68k_get_reg(NULL, M68K_REG_A6);
	auto a7 = m68k_get_reg(NULL, M68K_REG_A7);
	auto d0 = m68k_get_reg(NULL, M68K_REG_D0);
	auto d1 = m68k_get_reg(NULL, M68K_REG_D1);
	auto d2 = m68k_get_reg(NULL, M68K_REG_D2);
	auto d3 = m68k_get_reg(NULL, M68K_REG_D3);
	auto d4 = m68k_get_reg(NULL, M68K_REG_D4);
	auto d5 = m68k_get_reg(NULL, M68K_REG_D5);
	auto d6 = m68k_get_reg(NULL, M68K_REG_D6);
	auto d7 = m68k_get_reg(NULL, M68K_REG_D7);

	Log(logError, L"Illegal instruction");
	Log(L"PC: 0x%08X from 0x%08X, SP: 0x%08X", pc, ppc, sp);
	Log(L"A0: 0x%08X, A1: 0x%08X, A2: 0x%08X, A3: 0x%08X", a0, a1, a2, a3);
	Log(L"A4: 0x%08X, A5: 0x%08X, A6: 0x%08X, A7: 0x%08X", a4, a5, a6, a7);
	Log(L"D0: 0x%08X, D1: 0x%08X, D2: 0x%08X, D3: 0x%08X", d0, d1, d2, d3);
	Log(L"D4: 0x%08X, D5: 0x%08X, D6: 0x%08X, D7: 0x%08X", d4, d5, d6, d7);

	//pauseState = pauseEntering;
	return 0;
}

int InterruptAck(int level)
{
	m68k_set_virq(level, 0);
	return M68K_INT_ACK_AUTOVECTOR;
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
	m68k_set_cpu_type(M68K_CPU_TYPE_68030);
	m68k_set_illg_instr_callback(IllegalMangrasp);
	m68k_set_int_ack_callback(InterruptAck);
	Sound::Reset();
	m68k_pulse_reset();

#if _CONSOLE
	wprintf(UI::GetString(IDS_ASSPULLISREADY)); //"Asspull IIIx is ready."...
#endif

	SDL_Event ev;

	const auto cpuClock = 32'000'000;

	// VGA standard 640x480@60Hz
	const auto pixelClock = 25'175'000;
	const auto screenFreq = 60; // technically 59.94 Hz
	const auto pixsPerRow = 640;
	const auto lines = 480;
	const auto trueLines = 525;
	const auto trueWidth = 800;
	const auto hBlankPixs = trueWidth - pixsPerRow;

	const auto commonDivisor = GreatestCommonDivisor(cpuClock, pixelClock);
	const auto cpuScale = pixelClock / commonDivisor;
	const auto pixScale = cpuClock / commonDivisor;

	auto startTime = 0;
	auto endTime = 0;
	auto delta = 0;
	auto frames = 0;
	auto cycles = 0LL;

	bool gottaReset = false;

	auto executeForPixels = [&](int pixels)
	{
		cycles += (long long)pixels * pixScale;
		auto requestedCycles = (int)(cycles / cpuScale);
		auto executedCycles = m68k_execute(requestedCycles);
		cycles -= (long long)executedCycles * cpuScale;
	};

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
			case SDL_JOYDEVICEADDED:
			{
				//Log(L"Controller %d attached.", ev.cdevice.which);
				controller[ev.cdevice.which] = SDL_GameControllerOpen(ev.cdevice.which);
				break;
			}

			case SDL_KEYDOWN:
			{
				bool k2joyed = true;
				if (key2joy && !(ev.key.keysym.mod & KMOD_RCTRL))
				{
					switch (ev.key.keysym.sym)
					{
					case SDLK_UP: joypad[0] |= 0x0001; break;
					case SDLK_RIGHT: joypad[0] |= 0x0002; break;
					case SDLK_DOWN: joypad[0] |= 0x0004; break;
					case SDLK_LEFT: joypad[0] |= 0x0008; break;
					case SDLK_z: joypad[0] |= 0x0010; break;
					case SDLK_x: joypad[0] |= 0x0020; break;
					case SDLK_a: joypad[0] |= 0x0040; break;
					case SDLK_s: joypad[0] |= 0x0080; break;
					case SDLK_d: joypad[0] |= 0x0100; break;
					case SDLK_f: joypad[0] |= 0x0200; break;
					case SDLK_c: joypad[0] |= 0x0400; break;
					case SDLK_v: joypad[0] |= 0x0800; break;
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
					case SDLK_UP: joypad[0] &= ~0x0001; break;
					case SDLK_RIGHT: joypad[0] &= ~0x0002; break;
					case SDLK_DOWN: joypad[0] &= ~0x0004; break;
					case SDLK_LEFT: joypad[0] &= ~0x0008; break;
					case SDLK_z: joypad[0] &= ~0x0010; break;
					case SDLK_x: joypad[0] &= ~0x0020; break;
					case SDLK_a: joypad[0] &= ~0x0040; break;
					case SDLK_s: joypad[0] &= ~0x0080; break;
					case SDLK_d: joypad[0] &= ~0x0100; break;
					case SDLK_f: joypad[0] &= ~0x0200; break;
					case SDLK_c: joypad[0] &= ~0x0400; break;
					case SDLK_v: joypad[0] &= ~0x0800; break;
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
					else if (ev.key.keysym.sym == SDLK_F10)
					{
						UI::HideUI(!UI::hideUI);
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

		//So this ground-up rewrite circumvents this weird hotplugging Thing...
		//... and it opens the way to remapping beyond ABXY/BAYX!
		for (int i = 0; i < 2; i++)
		{
			if (controller[i])
			{
				if (!SDL_GameControllerGetAttached(controller[i]))
				{
					//Log(L"Controller %d detached.", i);
					SDL_GameControllerClose(controller[i]);
					controller[i] = NULL;
					continue;
				}
				joypad[i] = 0;
				for (int j = 0; j < 12; j++)
					joypad[i] |= SDL_GameControllerGetButton(controller[i], buttonMap[j]) << j;
				joyaxes[i][0] = SDL_GameControllerGetAxis(controller[i], SDL_CONTROLLER_AXIS_LEFTX) >> 8;
				joyaxes[i][1] = SDL_GameControllerGetAxis(controller[i], SDL_CONTROLLER_AXIS_LEFTY) >> 8;
				if (abs(joyaxes[i][0]) < 4) joyaxes[i][0] = 0;
				if (abs(joyaxes[i][1]) < 4) joyaxes[i][1] = 0;
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
				gottaReset = (*(uint32_t*)romCartridge == 0x21535341);
				SaveCartRAM();
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
				SaveCartRAM();
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
				SaveFile(L"wram.bin", ramInternal, WRAM_SIZE);
				SaveFile(L"vram.bin", ramVideo, VRAM_SIZE);
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
			if (line < lines)
			{
				executeForPixels(pixsPerRow);

				HandleHdma(line);
				Video::RenderLine(line);
				for (int i = 0; i < MAXDEVS; i++)
					if (devices[i] != NULL) devices[i]->HBlank();

				if ((interrupts & 0x80) == 0) //if interrupts are enabled
				{
					m68k_set_virq(M68K_IRQ_4, 1);
				}

				interrupts |= 2; //set HBlank signal

				executeForPixels(hBlankPixs);

				interrupts &= ~2; //clear HBlank signal
			}
			else
			{
				executeForPixels(trueWidth);
			}

			if (line + 1 == lines)
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
					if ((interrupts & 0x80) == 0) //if interrupts are enabled
					{
						m68k_set_virq(M68K_IRQ_6, 1);
					}

					interrupts |= 4; //set VBlank signal
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
			interrupts &= ~4; //clear VBlank signal
			ticks++;
			line = 0;
		}
	}

	SaveCartRAM();
	Sound::Reset();
	UI::Tooltips::DestroyTooltips();
	for (int i = 0; i < MAXDEVS; i++)
		if (devices[i] != NULL) delete devices[i];
	delete[] pauseScreen;
	delete[] romBIOS;
	delete[] romCartridge;
	delete[] ramCartridge;
	delete[] ramInternal;
	delete[] ramVideo;
}
