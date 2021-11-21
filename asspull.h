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
	extern void InitializeUI();
	extern void HandleUI();
	extern void ResetPath();
	extern void SetTitle(const char*);

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
		extern void UpdatePage(HWND hwndDlg);
		extern void Show();
		extern HWND hWnd;
	}
	namespace PalViewer
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

extern int keyScan;
extern char joypad[4];
extern char joyaxes[4];

extern long ticks;
extern int pauseState;

namespace Video
{
	extern SDL_Window* sdlWindow;

	extern bool gfx320, gfx240, gfxTextBold, gfxTextBlink, stretch200;
	extern int gfxMode, gfxFade, scrollX[4], scrollY[4], tileShift[2], mapEnabled[4], mapBlend[4];
	extern int caret;

	extern unsigned char* pixels;

	extern void RenderLine(int);
	extern void VBlank();
	extern int InitVideo();
	extern int UninitVideo();

	extern void InitShaders();
	extern void Screenshot();
}

extern void HandleHdma(int line);
extern int InitMemory();

extern int InitSound();
extern void UninitSound();

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
#define PAL_SIZE	(256 * 2)	//256 xBGR-1555 colors.
#define FONT_SIZE	(((8 * 256) * 2) + ((16 * 256) * 2))	//Two 8x8 fonts, two 8x16 fonts.
#define SPR1_SIZE	(256 * 2)	//256 16-bit entries.
#define SPR2_SIZE	(256 * 4)	//256 32-bit entries.

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
#define FONT_ADDR	0x060200
#define SPR1_ADDR	0x064000
#define SPR2_ADDR	0x064200

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
#if (FONT_ADDR + FONT_SIZE) > SPR1_ADDR
#error FONT encroaches on SPR1.
#endif
#if (SPRITE1 + SPR1_SIZE) > SPR2_ADDR
#error SPR1 encroaches on SPR2.
#endif
#if (SPR2_ADDR + SPR2_SIZE) > VRAM_SIZE
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
};

extern Device* devices[MAXDEVS];

namespace Discord
{
	extern bool enabled;

	extern void Init();
	extern void UpdateDiscordPresence(char* gameName);
	extern void Shutdown();
}
