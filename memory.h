#pragma once

//Define memory map
#define BIOS_SIZE	0x00020000
#define CART_SIZE	0x00FC0000
#define SRAM_SIZE	0x00020000
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
#define SRAM_ADDR	0x00FE0000
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
#if (BIOS_SIZE + CART_SIZE + SRAM_SIZE) != 0x1000000
#error Total ROM size (BIOS + CART + SRAM) should be exactly 0x1000000 (16 MiB).
#endif
#if (BIOS_ADDR + BIOS_SIZE) > CART_ADDR
#error BIOS encroaches on CART space.
#endif
#if (CART_ADDR + CART_SIZE) > SRAM_ADDR
#error CART encroaches on SRAM space.
#endif
#if (SRAM_ADDR + SRAM_SIZE) > WRAM_ADDR
#error SRAM encroaches on WRAM space.
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