#pragma once
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT 1
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <SDL.h>
#include "SimpleIni.h"
#include "resource.h"
#include <Windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <Uxtheme.h>
#include <vsstyle.h>

extern int Lerp(int a, int b, float f);
extern unsigned int RoundUp(unsigned int v);
extern int Slurp(unsigned char* dest, const WCHAR* filePath, unsigned int* size);
extern int Dump(const WCHAR* filePath, unsigned char* source, unsigned long size);
extern void Log(WCHAR* message, ...);

extern unsigned char* romBIOS;
extern unsigned char* romCartridge;
extern unsigned char* ramVideo;
extern unsigned char* ramInternal;

extern int key2joy;
extern int firstDiskDrive;
extern void FindFirstDrive();

#define SCREENBUFFERSIZE (640 * 480 * 4)

namespace UI
{
	extern int uiCommand;

	extern WCHAR startingPath[FILENAME_MAX];
	extern WCHAR lastPath[FILENAME_MAX];

	extern HWND hWndMain;
	extern HINSTANCE hInstance;
	extern HWND hWndStatusBar;
	extern int statusBarHeight;
	extern int diskIconTimer, hddIconTimer;

	extern int mouseTimer;
	extern int statusTimer;
	extern WCHAR uiStatus[512];
	extern WCHAR uiString[512];
	extern bool fpsVisible, fpsCap, reloadROM, reloadIMG;

	extern bool wasPaused;

	extern bool ShowFileDlg(bool toSave, WCHAR* target, size_t max, const WCHAR* filter);
	extern bool ReportLoadingFail(int messageId, int err, int device, const WCHAR* fileName, bool offerToForget = false);

	extern void InsertDisk(int devId);
	extern void EjectDisk(int devId);

	extern WCHAR settingsFile[FILENAME_MAX];

	extern FILE* logFile;

	extern WCHAR* GetString(int stab);
	extern void SetStatus(const WCHAR*);
	extern void SetStatus(int);
	extern void ResizeStatusBar();
	extern void SetFPS(int fps);
	extern void ShowOpenFileDialog(int, const WCHAR*);
	extern void LetItSnow();
	extern void Initialize();
	extern void Update();
	extern void ResetPath();
	extern void SetTitle(const char*);
	extern void Complain(int message);
	extern void SaveINI();

	namespace Presentation
	{
		extern HBRUSH hbrBack, hbrStripe, hbrList;
		extern COLORREF rgbBack, rgbStripe, rgbText, rgbHeader, rgbList, rgbListBk;
		extern HFONT headerFont, monoFont;

		extern void DrawWindowBk(HWND hwndDlg, bool stripe);
		extern void DrawWindowBk(HWND hwndDlg, bool stripe, PAINTSTRUCT* ps, HDC hdc);
		extern void DrawCheckbox(HWND hwndDlg, LPNMCUSTOMDRAW dis);
		extern bool DrawDarkButton(HWND hwndDlg, LPNMCUSTOMDRAW dis);
		extern bool DrawComboBox(LPDRAWITEMSTRUCT dis);
		extern void GetIconPos(HWND hwndDlg, int ctlID, RECT* iconRect, int leftOffset, int topOffset);
		namespace Windows10
		{
			extern bool IsWin10();
		}
		extern void SetThemeColors();
	}

	namespace Images
	{
		extern HIMAGELIST hIml;
		extern HBITMAP GetImageListImage(int);
		extern HBITMAP LoadPNGResource(int);
	}

	namespace About
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace MemoryViewer
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace Options
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace DeviceManager
	{
		extern void UpdatePage();
		extern void Show();
		extern HWND hWnd;
	}
	namespace PalViewer
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace TileViewer
	{
		extern void Show();
		extern HWND hWnd;
	}
	namespace Shaders
	{
		extern void Show();
		extern HWND hWnd;
	}
}

//Move these into InputDevice?
extern char joypad[4];
extern char joyaxes[4];

extern long ticks;

enum pauseStates
{
	pauseNot,
	pauseEntering,
	pauseYes,
};
extern pauseStates pauseState;

namespace Registers
{
	struct ScreenModeRegister
	{
		union
		{
			struct
			{
				unsigned char Mode : 2;
				unsigned char : 2;
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
				unsigned char Shift : 4;
				unsigned char Enabled : 4;
			};
			struct
			{
				unsigned char Shift12 : 2;
				unsigned char Shift34: 2;
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

	extern int Fade, Caret;
	extern int ScrollX[4], ScrollY[4];
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
};

#define MAXSHADERS 8

#define MAXDEVS 16
#define DEVBLOCK	0x8000

//Define memory map
#define BIOS_SIZE	0x00020000
#define CART_SIZE	0x00FE0000
#define WRAM_SIZE	0x00400000
#define DEVS_SIZE	(DEV_BLOCK * MAXDEVS) //0x0080000
#define REGS_SIZE	0x000FFFFF
#define VRAM_SIZE	0x00080000
#define STAC_SIZE	0x00010000

#define TEXT_SIZE	((80 * 60) * 2)	//640x480 mode has 8x8 character cells, so 80�60 characters.
#define BITMAP_SIZE	(640 * 480)		//640x480 mode in 256 colors.
#define MAP_SIZE	(((512 / 8) * (512 / 8)) * 4)	//Each tile is a 16-bit value.
#define TILES_SIZE	((((8 * 8) / 2) * 512) + (128 << 3))
#define PAL_SIZE	(512 * 2)	//512 xBGR-1555 colors.
#define FONT_SIZE	(((8 * 256) * 2) + ((16 * 256) * 2))	//Two 8x8 fonts, two 8x16 fonts.
#define OBJ1_SIZE	(256 * 2)	//256 16-bit entries.
#define OBJ2_SIZE	(256 * 4)	//256 32-bit entries.

#define BIOS_ADDR	0x00000000
#define CART_ADDR	0x00020000
#define WRAM_ADDR	0x01000000
#define STAC_ADDR	0x013F0000
#define DEVS_ADDR	0x02000000
#define REGS_ADDR	0x0D000000
#define VRAM_ADDR	0x0E000000

#define TEXT_ADDR	0x000000
#define BMP_ADDR	0x000000
#define MAP1_ADDR	0x000000
#define MAP2_ADDR	(MAP1_ADDR + MAP_SIZE)
#define MAP3_ADDR	(MAP2_ADDR + MAP_SIZE)
#define MAP4_ADDR	(MAP3_ADDR + MAP_SIZE)
#define TILES_ADDR	0x050000
#define PAL_ADDR	0x060000
#define FONT_ADDR	0x060400
#define OBJ1_ADDR	0x064000
#define OBJ2_ADDR	0x064200

#pragma region Sanity checks!
#if (BIOS_SIZE + CART_SIZE) != 0x1000000
#error Total ROM size (BIOS + CART) should be exactly 0x1000000 (16 MiB).
#endif
#if (BIOS_ADDR + BIOS_SIZE) > CART_ADDR
#error BIOS encroaches on CART space.
#endif
#if (CART_ADDR + CART_SIZE) > WRAM_ADDR
#error CART encroaches on WRAM space.
#endif
#if (WRAM_ADDR + WRAM_SIZE) > DEVS_ADDR
#error WRAM encroaches on DEVS space.
#endif
#if (STAC_ADDR + STAC_SIZE) > (WRAM_ADDR + WRAM_SIZE)
#error STAC breaks out of WRAM space.
#endif
#if (DEVS_ADDR + DEVS_SIZE) > REGS_ADDR
#error DEVS encroaches on REGS space.
#endif
#if (REGS_ADDR + REGS_SIZE) > VRAM_ADDR
#error REGS encroaches on VRAM space.
#endif
#if (FONT_SIZE != 12288)
#error FONT size is off.
#endif
#if (MAP4_ADDR + MAP_SIZE) > TILES_ADDR
#error MAP4 encroaches on TILES.
#endif
#if (BMP_ADDR + BITMAP_SIZE) > TILES_ADDR
#error 640x480 Bitmap mode will overwrite the tileset.
#endif
#if (BMP_ADDR + BITMAP_SIZE) > PAL_ADDR
#error 640x480 Bitmap mode will overwrite the palette.
#endif
#if (TILES_ADDR + TILES_SIZE) > PAL_ADDR
#error TILES encroaches on PAL.
#endif
#if (PAL_ADDR + PAL_SIZE) > FONT_ADDR
#error PAL encroaches on FONT.
#endif
#if (FONT_ADDR + FONT_SIZE) > OBJ1_ADDR
#error FONT encroaches on OBJ1.
#endif
#if (OBJ1_ADDR + OBJ1_SIZE) > OBJ2_ADDR
#error OBJ1 encroaches on OBJ2.
#endif
#if (OBJ2_ADDR + OBJ2_SIZE) > VRAM_SIZE
#error Insufficient VRAM!
#endif
#pragma endregion

class Device
{
public:
	Device(void);
	virtual ~Device(void) = 0;
	virtual unsigned int Read(unsigned int address);
	virtual void Write(unsigned int address, unsigned int value);
	virtual int GetID();
	virtual void HBlank();
	virtual void VBlank();
};

class DiskDrive : Device
{
private:
	FILE* file;
	unsigned char* data;
	unsigned short sector;
	unsigned long capacity;
	unsigned int tracks, heads, sectors;
	int error;
	int type;
public:
	DiskDrive(int newType);
	~DiskDrive(void);
	int Mount(const WCHAR* filename);
	int Unmount();
	unsigned int Read(unsigned int address);
	void Write(unsigned int address, unsigned int value);
	int GetID();
	void HBlank();
	void VBlank();
	int GetType();
	bool IsMounted();
};
enum diskDriveTypes
{
	ddDiskette,
	ddHardDisk,
};

class LinePrinter : Device
{
public:
	LinePrinter(void);
	~LinePrinter(void);
	unsigned int Read(unsigned int address);
	void Write(unsigned int address, unsigned int value);
	int GetID();
	void HBlank();
	void VBlank();
};

class InputDevice : Device
{
private:
	unsigned int buffer[32];
	unsigned int bufferCursor;
	int lastMouseX, lastMouseY;
	unsigned int mouseLatch;
public:
	InputDevice(void);
	~InputDevice(void);
	unsigned int Read(unsigned int address);
	void Write(unsigned int address, unsigned int value);
	int GetID();
	void HBlank();
	void VBlank();
	void Enqueue(SDL_Keysym sym);
};

extern Device* devices[MAXDEVS];
extern InputDevice* inputDev;

namespace Discord
{
	extern bool enabled;

	extern void Initialize();
	extern void SetPresence(char* gameName);
	extern void Shutdown();
}
