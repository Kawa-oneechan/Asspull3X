#pragma once
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT 1
#include <stdint.h>
#include <stdio.h>
#include <SDL.h>
#include "support/SimpleIni.h"
#include "resource.h"
#include <Windows.h>
#include <commctrl.h>

#include "memory.h"
#include "device.h"
#include "ui.h"

enum logCategories
{
	logNormal,
	logWarning,
	logError,
};

extern int Lerp(int a, int b, float f);
extern int GreatestCommonDivisor(int a, int b);
extern unsigned int RoundUp(unsigned int v);
extern int LoadFile(unsigned char* dest, const WCHAR* filePath, unsigned int* size);
extern int SaveFile(const WCHAR* filePath, unsigned char* source, unsigned long size);
extern void LoadROM(const WCHAR* path);
extern void FindFirstDrive();
extern void SaveCartRAM();
extern void Log(WCHAR* message, ...);
extern void Log(logCategories cat, WCHAR* message, ...);
extern SDL_GameControllerButton SDL_GameControllerGetButtonFromStringW(const WCHAR*);
extern const WCHAR* SDL_GameControllerGetStringForButtonW(SDL_GameControllerButton);

extern unsigned char* romBIOS;
extern unsigned char* romCartridge;
extern unsigned char* ramCartridge;
extern unsigned char* ramVideo;
extern unsigned char* ramInternal;

extern int key2joy;
extern int firstDiskDrive;
extern void FindFirstDrive();

#define SCREENBUFFERSIZE (640 * 480 * 4)

//Move these into InputOutputDevice?
extern unsigned short joypad[2];
extern char joyaxes[2][2];

extern long ticks;

enum pauseStates
{
	pauseTurnedOff,
	pauseNot,
	pauseEntering,
	pauseYes,
};
extern pauseStates pauseState;

namespace Registers
{
	extern int Interrupts;

	struct ScreenModeRegister
	{
		union
		{
			struct
			{
				unsigned char Mode : 2;
				unsigned char : 1;
				unsigned char Bold : 1;
				unsigned char Blink : 1;
				unsigned char HalfHeight : 1;
				unsigned char HalfWidth : 1;
				unsigned char Aspect : 1;
			};
			unsigned char Raw;
		};
	};
	extern ScreenModeRegister ScreenMode;

	struct MapSetRegister
	{
		union
		{
			struct
			{
				unsigned char Shift : 2;
				unsigned char : 2;
				unsigned char Enabled : 4;
			};
			struct
			{
				unsigned char Shift : 2;
				unsigned char : 2;
				unsigned char Map1 : 1;
				unsigned char Map2 : 1;
				unsigned char Map3 : 1;
				unsigned char Map4 : 1;
			};
			unsigned char Raw;
		};
	};
	extern MapSetRegister MapSet;

	struct MapBlendRegister
	{
		union
		{
			struct
			{
				unsigned char Enabled : 4;
				unsigned char Subtract : 4;
			};
			struct
			{
				unsigned char Enable1 : 1;
				unsigned char Enable2 : 1;
				unsigned char Enable3 : 1;
				unsigned char Enable4 : 1;
				unsigned char Subtract1 : 1;
				unsigned char Subtract2 : 1;
				unsigned char Subtract3 : 1;
				unsigned char Subtract4 : 1;
			};
			unsigned char Raw;
		};
	};
	extern MapBlendRegister MapBlend;

	struct DMAControlRegister
	{
		union
		{
			struct
			{
				unsigned int Enable : 1;
				unsigned int IncreaseSource : 1;
				unsigned int IncreaseTarget : 1;
				unsigned int DirectValue : 1;
				unsigned int Width : 2;
			};
			unsigned int Raw;
		};
		int Source;
		int Target;
		unsigned int Length;
	};
	extern DMAControlRegister DMAControl;

	struct HDMAControlRegister
	{
		union
		{
			struct
			{
				unsigned int Enable : 1;
				unsigned int : 3;
				unsigned int Width : 2;
				unsigned int : 1;
				unsigned int DoubleScan : 1;
				unsigned int Start : 9;
				unsigned int : 3;
				unsigned int Count : 9;
			};
			unsigned int Raw;
		};
		int Source;
		int Target;
	};
	extern HDMAControlRegister HDMAControls[8];

	enum BlitFunctions
	{
		None, Copy, Set, Invert
	};
	struct BlitControlRegister
	{
		union
		{
			struct
			{
				BlitFunctions Function : 3;
				unsigned int : 1;
				unsigned int StrideSkip : 1;
			};
			struct
			{
				unsigned int : 3;
				unsigned int : 1;
				unsigned int : 1;
				unsigned int Key : 1;
				unsigned int Source4bit : 1;
				unsigned int Target4bit : 1;
				unsigned int SourceStride : 12;
				unsigned int TargetStride : 12;
			};
			struct
			{
				unsigned int : 3;
				unsigned int : 1;
				unsigned int : 1;
				unsigned int Width : 2;
			};
			unsigned int Raw;
		};
		int Source;
		int Target;
		unsigned Length;
	};
	extern BlitControlRegister BlitControls;

	struct BlitKeyRegister
	{
		union
		{
			struct
			{
				unsigned int Color : 8;
				unsigned int Palette : 4;
			};
			unsigned int Raw;
		};
	};
	extern BlitKeyRegister BlitKey;

	extern int WindowLeft, WindowRight, WindowMask;

	extern int Fade, Caret;
	extern int ScrollX[4], ScrollY[4];
	extern unsigned int MapTileShift;

	extern long RTCOffset;
}

namespace Video
{
	extern SDL_Window* sdlWindow;
	extern bool stretch200;

	extern unsigned char* pixels;

	extern void RenderLine(int);
	extern void VBlank();
	extern int Initialize();
	extern int Shutdown();

	extern void InitShaders();
	extern void Screenshot();
}

extern void HandleHdma(int line);
extern int InitMemory();
extern void ResetMemory();

namespace Sound
{
	extern int pcmSource[2], pcmLength[2], pcmPlayed[2], pcmVolume[4];
	extern bool pcmRepeat[2];

	extern int Initialize();
	extern void Shutdown();
	extern void SendMidiByte(unsigned char part);
	extern void SendOPL(unsigned short message);
	extern void Reset();
}

namespace Discord
{
	extern bool enabled;

	extern void Initialize();
	extern void SetPresence(char* gameName);
	extern void Shutdown();
}

extern CSimpleIni ini;

enum uiCommands
{
	cmdNone,
	cmdLoadRom,
	cmdUnloadRom,
	cmdReset,
	cmdScreenshot,
	cmdDump,
	cmdQuit,
	cmdMemViewer,
	cmdPalViewer,
	cmdAbout,
	cmdOptions,
	cmdShaders,
	cmdDevices,
	cmdTileViewer,
	cmdInsertDisk,
	cmdEjectDisk,
	cmdCreateDisk,
	cmdReloadRom,
	cmdTurnOnOrOff,
	cmdButtonMapper,
};

#define MAXSHADERS 8

