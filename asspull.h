#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#include <stdint.h>
#include <stdio.h>
#include <string>
#if _MSC_VER
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include "SimpleIni.h"

int Lerp(int a, int b, float f);
unsigned int RoundUp(unsigned int v);
int Slurp(unsigned char* dest, std::string filePath, unsigned int* size);
int Dump(std::string filePath, unsigned char* source, unsigned long size);

extern unsigned char* romBIOS;
extern unsigned char* romCartridge;
extern unsigned char* ramVideo;
extern unsigned char* ramInternal;

extern SDL_Window* sdlWindow;

extern int keyScan, joypad[2];

extern int gfxFade;
extern long ticks;

extern FILE* diskFile;

extern void RenderLine(int);
extern void VBlank();
extern int InitVideo();
extern int UninitVideo();

extern void HandleHdma(int line);
extern int InitMemory();

extern int InitSound();
extern void UninitSound();
extern void BufferAudioSample(signed char);

extern void HandleUI();
extern void ResetPath();

extern CSimpleIniA ini;

extern const unsigned char keyMap[];

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
	cmdAbout,
	cmdOptions,
	cmdDevices,
	cmdInsertDisk,
	cmdEjectDisk,
	cmdCreateDisk,
};

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

#define TEXT_SIZE	((80 * 60) * 2)	//640×480 mode has 8x8 character cells, so 80×60 characters.
#define BITMAP_SIZE	(640 * 480)		//640×480 mode in 256 colors.
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

//Sanity checks!
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
#error 640×480 Bitmap mode will overwrite the tilemap.
#endif
#if (BMP_ADDR + BITMAP_SIZE) > PAL_ADDR
#error 640×480 Bitmap mode will overwrite the palette.
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
	int Mount(std::string filename);
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
