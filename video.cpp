#include "asspull.h"
#include <math.h>

namespace Video
{
	struct ObjectA
	{
		union
		{
			struct
			{
				unsigned short Tile : 9;
				unsigned short Blend : 2;
				unsigned short Enabled : 1;
				unsigned short Palette : 4;
			};
			unsigned short Raw;
		};
	};
	struct ObjectB
	{
		union
		{
			struct
			{
				signed int Horizontal : 11;
				unsigned int : 1;
				signed int Vertical : 10;
				unsigned int : 2;
				unsigned int DoubleWidth : 1;
				unsigned int DoubleHeight : 1;
				unsigned int FlipHoriz : 1;
				unsigned int FlipVert : 1;
				unsigned int DoubleUp : 1;
				unsigned int Priority : 3;
			};
			unsigned int Raw;
		};
	};
	struct MapTile {
		union
		{
			struct
			{
				unsigned short Tile : 10;
				unsigned short FlipHoriz : 1;
				unsigned short FlipVert : 1;
				unsigned short Palette : 4;
			};
			unsigned short Raw;
		};
	};

	bool stretch200;

	unsigned char* pixels;

#define BLINK ((SDL_GetTicks() % 600) < 300)

#define FADECODE \
	if (Registers::Fade > 0) \
	{ \
		auto f = (Registers::Fade & 31); \
		if ((Registers::Fade & 0x80) == 0x80) \
		{ \
			r = ((r + f > 31) ? 31 : r + f); \
			g = ((g + f > 31) ? 31 : g + f); \
			b = ((b + f > 31) ? 31 : b + f); \
		} \
		else \
		{ \
			r = ((r - f < 0) ? 0 : r - f); \
			g = ((g - f < 0) ? 0 : g - f); \
			b = ((b - f < 0) ? 0 : b - f); \
		} \
	}

#define WINDOWCODE \
	if (Registers::WindowMask) \
	{ \
		int winMul =  Registers::ScreenMode.HalfWidth ? 2 : 1; \
		if ( \
			((Registers::WindowMask & (1 << win)) \
				&& (column < (Registers::WindowLeft * winMul) \
				|| column > (Registers::WindowRight * winMul))) || \
			(((Registers::WindowMask >> 8) & (1 << win)) \
				&& (column >= (Registers::WindowLeft * winMul) && \
				column <= (Registers::WindowRight * winMul))) \
			) \
		{ \
			if (win == 0) \
				snes = 0; \
			else \
				return; \
		} \
	}

	static inline void RenderPixel(int row, int column, int color, int win)
	{
		auto snes = (ramVideo[PAL_ADDR + ((color) * 2) + 0] << 8) + ramVideo[PAL_ADDR + ((color) * 2) + 1];
		auto target = ((row * 640) + column) * 4;
		WINDOWCODE;
		auto r = (snes >> 0) & 0x1F;
		auto g = (snes >> 5) & 0x1F;
		auto b = (snes >> 10) & 0x1F;
		FADECODE;
		pixels[target + 0] = (unsigned char)((b << 3) + (b >> 2));
		pixels[target + 1] = (unsigned char)((g << 3) + (g >> 2));
		pixels[target + 2] = (unsigned char)((r << 3) + (r >> 2));
	}

	static inline void RenderBlended(int row, int column, int color, bool subtractive, int win)
	{
		auto snes = (ramVideo[PAL_ADDR + ((color) * 2) + 0] << 8) + ramVideo[PAL_ADDR + ((color) * 2) + 1];
		auto target = ((row * 640) + column) * 4;
		WINDOWCODE;
		auto r = (snes >> 0) & 0x1F;
		auto g = (snes >> 5) & 0x1F;
		auto b = (snes >> 10) & 0x1F;
		FADECODE;
		if (!subtractive)
		{
			b = (pixels[target + 0] + ((b << 3) + (b >> 2))) / 2;
			g = (pixels[target + 1] + ((g << 3) + (g >> 2))) / 2;
			r = (pixels[target + 2] + ((r << 3) + (r >> 2))) / 2;
			pixels[target + 0] = (unsigned char)((b > 255) ? 255 : b);
			pixels[target + 1] = (unsigned char)((g > 255) ? 255 : g);
			pixels[target + 2] = (unsigned char)((r > 255) ? 255 : r);
		}
		else
		{
			b = (((b << 3) + (b >> 2)) - pixels[target + 0]) / 2;
			g = (((g << 3) + (g >> 2)) - pixels[target + 1]) / 2;
			r = (((r << 3) + (r >> 2)) - pixels[target + 2]) / 2;
			pixels[target + 0] = (unsigned char)((b < 0) ? 0 : b);
			pixels[target + 1] = (unsigned char)((g < 0) ? 0 : g);
			pixels[target + 2] = (unsigned char)((r < 0) ? 0 : r);
		}
	}

	void RenderObjects(int line, int withPriority)
	{
		auto step = Registers::ScreenMode.HalfWidth ? 4 : 2;
		for (auto i = 0; i < 256; i++)
		{
			ObjectA objA;
			objA.Raw = (unsigned short)((ramVideo[OBJ1_ADDR + 0 + (i * 2)] << 8) | (ramVideo[OBJ1_ADDR + 1 + (i * 2)] << 0));
			if (objA.Raw == 0)
				continue;
			if (!objA.Enabled)
				continue;
			ObjectB objB;
			objB.Raw = (unsigned int)(
				(ramVideo[OBJ2_ADDR + 0 + (i * 4)] << 24) |
				(ramVideo[OBJ2_ADDR + 1 + (i * 4)] << 16) |
				(ramVideo[OBJ2_ADDR + 2 + (i * 4)] << 8) |
				(ramVideo[OBJ2_ADDR + 3 + (i * 4)] << 0));
			if (withPriority > -1 && objB.Priority != withPriority)
				continue;

			short hPos = objB.Horizontal * (Registers::ScreenMode.HalfWidth ? 2 : 1);
			short vPos = objB.Vertical * (Registers::ScreenMode.HalfHeight ? 2 : 1);

			short tileHeight = 1;
			if (objB.DoubleHeight) tileHeight *= 2;
			if (objB.DoubleUp) tileHeight *= 2;

			short tileWidth = 1;
			if (objB.DoubleWidth) tileWidth *= 2;
			if (objB.DoubleUp) tileWidth *= 2;

			short effectiveHeight = tileHeight * 8;
			short effectiveWidth = tileWidth * 8;
			effectiveHeight *= ((Registers::ScreenMode.HalfHeight && Registers::ScreenMode.Mode > 0) ? 2 : 1);

			if (line < vPos || line >= vPos + effectiveHeight)
				continue;

			short renderWidth = (Registers::ScreenMode.HalfWidth ? 16 : 8);
			if (hPos + (effectiveWidth * (Registers::ScreenMode.HalfWidth ? 2 : 1)) <= 0 || hPos > 640)
				continue;

			if (objB.FlipHoriz)
				hPos += (Registers::ScreenMode.HalfWidth ? (effectiveWidth * 2) - 4 : effectiveWidth - 2);

			auto tileBasePic = TILES_ADDR + (objA.Tile * 32);
			for (auto col = 0; col < tileWidth; col++)
			{
				auto tilePic = tileBasePic;
				auto part = (line - vPos);
				if (objB.FlipVert) part = effectiveHeight - 1 - part;

				if (Registers::ScreenMode.HalfHeight && Registers::ScreenMode.Mode > 0)
					part /= 2;
				auto tileLine = part / 8;
				if (tileWidth == 2) tilePic += tileLine * 32;
				if (tileWidth == 4) tilePic += tileLine * 96;

				tilePic += part * 4;

				for (auto j = 0; j < renderWidth; j += step)
				{
					auto hfJ = j;
					if (objB.FlipHoriz) hfJ = -j;

					auto twoPix = ramVideo[tilePic + (j / step)];
					if (twoPix == 0)
						continue;
					auto l = (twoPix >> 0) & 0x0F;
					auto r = (twoPix >> 4) & 0x0F;
					if (l) l += objA.Palette * 16;
					if (r) r += objA.Palette * 16;
					if (objB.FlipHoriz)
					{
						auto lt = l;
						l = r;
						r = lt;
					}

					if (objA.Blend == 0)
					{
						if (!Registers::ScreenMode.HalfWidth)
						{
							if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderPixel(line, hPos + hfJ + 0, l + 256, 5);
							if (r != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderPixel(line, hPos + hfJ + 1, r + 256, 5);
						}
						else
						{
							if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640)
							{
								RenderPixel(line, hPos + hfJ + 0, l + 256, 5);
								RenderPixel(line, hPos + hfJ + 1, l + 256, 5);
							}
							if (r != 0 && hPos + hfJ + 2 >= 0 && hPos + hfJ + 2 < 640)
							{
								RenderPixel(line, hPos + hfJ + 2, r + 256, 5);
								RenderPixel(line, hPos + hfJ + 3, r + 256, 5);
							}
						}
					}
					else
					{
						bool sub = ((objA.Blend & 2) == 2);
						if (!Registers::ScreenMode.HalfWidth)
						{
							if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderBlended(line, hPos + hfJ + 0, l + 256, sub, 5);
							if (r != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderBlended(line, hPos + hfJ + 1, r + 256, sub, 5);
						}
						else
						{
							if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640)
							{
								RenderBlended(line, hPos + hfJ + 0, l + 256, sub, 5);
								RenderBlended(line, hPos + hfJ + 1, l + 256, sub, 5);
							}
							if (r != 0 && hPos + hfJ + 2 >= 0 && hPos + hfJ + 2 < 640)
							{
								RenderBlended(line, hPos + hfJ + 2, r + 256, sub, 5);
								RenderBlended(line, hPos + hfJ + 3, r + 256, sub, 5);
							}
						}
					}
				}

				if (!objB.FlipHoriz)
					hPos += renderWidth;
				else
					hPos -= renderWidth;
				if (tileHeight == 1)
					tileBasePic += 32;
				else
				{
					if (tileWidth == 2)
						tileBasePic += 16 * tileWidth;
					if (tileWidth == 4)
						tileBasePic += 8 * tileWidth;
				}
			}
		}
	}

	void RenderTextMode(int line)
	{
		auto width = Registers::ScreenMode.HalfWidth ? 40 : 80;
		auto height = Registers::ScreenMode.HalfHeight ? 50 : 100;
		auto celHeight = Registers::ScreenMode.HalfHeight ? 16 : 8;
		auto bgY = line / celHeight;
		auto tileIndex = TEXT_ADDR + (((bgY % height) * width) * 2);
		auto font = FONT_ADDR + (Registers::ScreenMode.Aspect ? 0x800 : 0);
		if (Registers::ScreenMode.HalfHeight)
			font = FONT_ADDR + 0x1000 + (Registers::ScreenMode.Aspect ? 0x1000 : 0);
		auto tileY = line % (Registers::ScreenMode.HalfHeight ? 16 : 8); //8;

		//if (gfxTextHigh)
		//	tileY = (line2 / 2) % 8;
		for (auto col = 0; col < width; col++)
		{
			auto chr = ramVideo[tileIndex++];
			auto att = ramVideo[tileIndex++];
			//if (chr == 0)
			//	continue;
			auto glyph = font + (chr * (Registers::ScreenMode.HalfHeight ? 16 : 8)); //8);
			auto scan = ramVideo[glyph + tileY];

			if (Registers::ScreenMode.Blink && (att & 0x80))
			{
				att = (att & 0x7F);
				if (BLINK)
					att = (att & 0x70) | ((att & 0x70) >> 4);
			}

			if (!Registers::ScreenMode.HalfWidth)
			{
				for (auto bit = 0; bit < 8; bit++)
					RenderPixel(line, (col * 8) + bit, ((scan >> bit) & 1) == 1 ? (att & 0xF) : (att >> 4), 0);
			}
			else
			{
				for (auto bit = 0; bit < 8; bit++)
				{
					RenderPixel(line, (col * 16) + (bit * 2) + 0, ((scan >> bit) & 1) == 1 ? (att & 0xF) : (att >> 4), 0);
					RenderPixel(line, (col * 16) + (bit * 2) + 1, ((scan >> bit) & 1) == 1 ? (att & 0xF) : (att >> 4), 0);
				}
			}
		}

		if (Registers::Caret & 0x8000 && BLINK)
		{
			int cp = Registers::Caret & 0x3FFF;
			int cr = cp / width;
			int ch = Registers::Caret & 0x4000 ? -1 : celHeight - 3;
			if (bgY == cr && line % celHeight > ch)
			{
				int cc = cp % width;
				int ca = ramVideo[TEXT_ADDR + (cp * 2) + 1];
				int cw = Registers::ScreenMode.HalfWidth ? 16 : 8;
				for (auto col = 0; col < cw; col++)
				{
					RenderPixel(line, (cc * cw) + col, ca & 0xF, 0);
				}
			}
		}
		RenderObjects(line, -1);
	}

	void RenderBitmapMode1(int line)
	{
		auto imgWidth = Registers::ScreenMode.HalfWidth ? 160 : 320; //image is 160 or 320 bytes wide
		auto sourceLine = line; // gfxTextBold ? (int)(line * 0.835) : line;
		if (Registers::ScreenMode.Aspect && !stretch200)
		{
			if (line < 40 || line > 439)
			{
				for (auto col = 0; col < 640 * 4; col++)
					pixels[(line * 640 * 4) + col] = 0;
				return;
			}
			sourceLine = line - 40;
		}
		auto effective = BMP_ADDR + (Registers::ScreenMode.HalfHeight ? sourceLine / 2 : sourceLine);
		auto p = 0;
		for (auto col = 0; col < 640; col += (Registers::ScreenMode.HalfWidth ? 4 : 2))
		{
			auto twoPix = ramVideo[effective * imgWidth + p];
			p++;
			auto l = (twoPix >> 0) & 0x0F;
			auto r = (twoPix >> 4) & 0x0F;
			if (!Registers::ScreenMode.HalfWidth)
			{
				RenderPixel(line, col + 0, l, 0);
				RenderPixel(line, col + 1, r, 0);
			}
			else
			{
				RenderPixel(line, col + 0, l, 0);
				RenderPixel(line, col + 1, l, 0);
				RenderPixel(line, col + 2, r, 0);
				RenderPixel(line, col + 3, r, 0);
			}
		}
		RenderObjects(line, -1);
	}

	void RenderBitmapMode2(int line)
	{
		auto imgWidth = Registers::ScreenMode.HalfWidth ? 320 : 640;
		auto sourceLine = line; // gfxTextBold ? (int)(line * 0.835) : line;
		if (Registers::ScreenMode.Aspect && !stretch200)
		{
			if (line < 40 || line > 439)
			{
				for (auto col = 0; col < 640 * 4; col++)
					pixels[(line * 640 * 4) + col] = 0;
				return;
			}
			sourceLine = line - 40;
		}
		auto effective = BMP_ADDR + (Registers::ScreenMode.HalfHeight ? sourceLine / 2 : sourceLine);
		auto p = 0;
		for (auto col = 0; col < 640; col++)
		{
			auto pixel = ramVideo[effective * imgWidth + p];
			p++;
			if (!Registers::ScreenMode.HalfWidth)
			{
				RenderPixel(line, col, pixel, 0);
			}
			else
			{
				RenderPixel(line, col++, pixel, 0);
				RenderPixel(line, col, pixel, 0);
			}
		}
		RenderObjects(line, -1);
	}

	void RenderTileMode(int line)
	{
		if (line % 2 == 1) return;
		auto sourceLine = line / 2;
		const auto charBase = TILES_ADDR;
		auto screenBase = MAP1_ADDR;
		const auto sizeX = 512;
		const auto sizeY = 512;
		const auto maskX = sizeX - 1;
		const auto maskY = sizeY - 1;

		for (int layer = -1; layer < 4; layer++)
		{
			if (layer == -1)
			{
				for (auto x = 0; x < 320; x++)
				{
					RenderPixel(line, (x * 2) + 0, 0, 0);
					RenderPixel(line, (x * 2) + 1, 0, 0);
					RenderPixel(line + 1, (x * 2) + 0, 0, 0);
					RenderPixel(line + 1, (x * 2) + 1, 0, 0);
				}
				RenderObjects(line, 4);
				RenderObjects(line + 1, 4);
				continue;
			}

			screenBase = MAP1_ADDR + (layer * MAP_SIZE);

			if (!(Registers::MapSet.Enabled & (1 << layer)))
			{
				RenderObjects(line, 3 - layer);
				RenderObjects(line + 1, 3 - layer);
				continue;
			}

			auto xxx = Registers::ScrollX[layer] & maskX;
			auto yyy = (Registers::ScrollY[layer] + sourceLine) & maskY;

			auto yShift = ((yyy >> 3) << 6);
			//yShift = 0;
			auto screenSource = screenBase + (0x000 * (xxx >> 8) + ((xxx & 511) >> 3) + yShift) * 2;
			auto shift = 0; // 128 << (tileShift[layer] - 1);

			for (auto x = 0; x < 320; x++)
			{
				MapTile mapTile;
				mapTile.Raw = (ramVideo[screenSource] << 8) | ramVideo[screenSource + 1];
				auto tileX = xxx & 7;
				auto tileY = yyy & 7;

				if (mapTile.FlipHoriz) tileX = 7 - tileX;
				if (mapTile.FlipVert) tileY = 7 - tileY;

				auto color = (int)ramVideo[charBase + ((mapTile.Tile + shift) << 5) + (tileY << 2) + (tileX >> 1)];
				if ((tileX & 1) == 1) color >>= 4;
				color &= 0x0F;

				if (color)
				{
					if (color) color += mapTile.Palette * 16;
					if (Registers::MapBlend.Enabled & (1 << layer))
					{
						auto sub = (Registers::MapBlend.Subtract & (1 << layer)) != 0;
						RenderBlended(line, (x * 2) + 0, color, sub, layer + 1);
						RenderBlended(line, (x * 2) + 1, color, sub, layer + 1);
						RenderBlended(line + 1, (x * 2) + 0, color, sub, layer + 1);
						RenderBlended(line + 1, (x * 2) + 1, color, sub, layer + 1);
					}
					else
					{
						RenderPixel(line, (x * 2) + 0, color, layer + 1);
						RenderPixel(line, (x * 2) + 1, color, layer + 1);
						RenderPixel(line + 1, (x * 2) + 0, color, layer + 1);
						RenderPixel(line + 1, (x * 2) + 1, color, layer + 1);
					}
				}

				if (mapTile.FlipHoriz) tileX = 7 - tileX;
				if (tileX == 7) screenSource += 2;

				xxx++;
				if (xxx == 512)
				{
					//TODO: check if this is needed for 512 px *tall* maps.
					//if (sizeX > 512)
					//	screenSource = screenBase + (0x400 + yShift * 2);
					//else
					//{
					screenSource = screenBase + (yShift * 2);
					xxx = 0;
					//}
				}
				else if (xxx >= sizeX)
				{
					xxx = 0;
					screenSource = screenBase + (yShift * 2);
				}
			}
			RenderObjects(line, 3 - layer);
			RenderObjects(line + 1, 3 - layer);
		}
	}

	void RenderLine(int line)
	{
		switch (Registers::ScreenMode.Mode)
		{
		case 0: RenderTextMode(line); break;
		case 1: RenderBitmapMode1(line); break;
		case 2: RenderBitmapMode2(line); break;
		case 3: RenderTileMode(line); break;
		}
	}

}
