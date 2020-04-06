#include "asspull.h"
#include <math.h>

bool gfx320, gfx240, gfxTextBold, stretch200;
int gfxMode, gfxFade, scrollX[4], scrollY[4], tileShift[2], mapEnabled[4], mapBlend[4];
int caret;

unsigned char* pixels;

#define FADECODE \
	if (gfxFade > 0) \
	{ \
		auto f = (gfxFade & 31); \
		if ((gfxFade & 0x80) == 0x80) \
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

static inline void RenderPixel(int row, int column, int color)
{
	auto snes = (ramVideo[PAL_ADDR + ((color) * 2) + 0] << 8) + ramVideo[PAL_ADDR + ((color) * 2) + 1];
	auto target = ((row * 640) + column) * 4;
	auto r = (snes >> 0) & 0x1F;
	auto g = (snes >> 5) & 0x1F;
	auto b = (snes >> 10) & 0x1F;
	FADECODE;
	pixels[target + 0] = (b << 3) + (b >> 2);
	pixels[target + 1] = (g << 3) + (g >> 2);
	pixels[target + 2] = (r << 3) + (r >> 2);
}

static inline void RenderBlended(int row, int column, int color, bool subtractive)
{
	auto snes = (ramVideo[PAL_ADDR + ((color) * 2) + 0] << 8) + ramVideo[PAL_ADDR + ((color) * 2) + 1];
	auto target = ((row * 640) + column) * 4;
	auto r = (snes >> 0) & 0x1F;
	auto g = (snes >> 5) & 0x1F;
	auto b = (snes >> 10) & 0x1F;
	FADECODE;
	if (!subtractive)
	{
		b = (pixels[target + 0] + ((b << 3) + (b >> 2))) / 2;
		g = (pixels[target + 1] + ((g << 3) + (g >> 2))) / 2;
		r = (pixels[target + 2] + ((r << 3) + (r >> 2))) / 2;
		pixels[target + 0] = (b > 255) ? 255 : b;
		pixels[target + 1] = (g > 255) ? 255 : g;
		pixels[target + 2] = (r > 255) ? 255 : r;
	}
	else
	{
		b = (((b << 3) + (b >> 2)) - pixels[target + 0]) / 2;
		g = (((g << 3) + (g >> 2)) - pixels[target + 1]) / 2;
		r = (((r << 3) + (r >> 2)) - pixels[target + 2]) / 2;
		pixels[target + 0] = (b < 0) ? 0 : b;
		pixels[target + 1] = (g < 0) ? 0 : g;
		pixels[target + 2] = (r < 0) ? 0 : r;
	}
}

void RenderSprites(int line, int withPriority)
{
	auto imgWidth = gfx320 ? 320 : 640;
	auto step = gfx320 ? 4 : 2;
	for (auto i = 0; i < 256; i++)
	{
		auto spriteA = (ramVideo[SPR1_ADDR + 0 + (i * 2)] << 8) | (ramVideo[SPR1_ADDR + 1 + (i * 2)] << 0);
		if (spriteA == 0)
			continue;
		if ((spriteA & 0x800) != 0x800)
			continue;
		auto spriteB =
			(ramVideo[SPR2_ADDR + 0 + (i * 4)] << 24) |
			(ramVideo[SPR2_ADDR + 1 + (i * 4)] << 16) |
			(ramVideo[SPR2_ADDR + 2 + (i * 4)] << 8) |
			(ramVideo[SPR2_ADDR + 3 + (i * 4)] << 0);
		auto prio = (spriteB >> 29) & 7;
		if (withPriority > -1 && prio != withPriority)
			continue;
		auto tile = (spriteA >> 0) & 0x1FF;
		auto blend = (spriteA >> 9) & 3;
		auto pal = (spriteA >> 12) & 0x0F;
		short hPos = ((spriteB & 0x7FF) << 21) >> 21;
		short vPos = ((spriteB & 0x7FF000) << 10) >> 22;
		if (hPos & 0x200) //extend sign because lolbitfields
			hPos |= 0xFC00;
		else
			hPos &= 0x3FF;
		if (vPos & 0x100)
			vPos |= 0xFE00;
		else
			vPos &= 0x1FF;
		if (gfx320) hPos *= 2;
		if (gfx240) vPos *= 2;

		auto doubleWidth = ((spriteB >> 24) & 1) == 1;
		auto doubleHeight = ((spriteB >> 25) & 1) == 1;
		auto hFlip = ((spriteB >> 26) & 1) == 1;
		auto vFlip = ((spriteB >> 27) & 1) == 1;
		auto doubleSize = ((spriteB >> 28) & 1) == 1;

		auto tileHeight = 1;
		if (doubleHeight) tileHeight *= 2;
		if (doubleSize) tileHeight *= 2;

		auto tileWidth = 1;
		if (doubleWidth) tileWidth *= 2;
		if (doubleSize) tileWidth *= 2;

		auto effectiveHeight = tileHeight * 8;
		auto effectiveWidth = tileWidth * 8;
		effectiveHeight *= ((gfx240 && gfxMode > 0) ? 2 : 1);

		if (line < vPos || line >= vPos + effectiveHeight)
			continue;

		auto renderWidth = (gfx320 ? 16 : 8);
		if (hPos + (effectiveWidth * (gfx320 ? 2 : 1)) <= 0 || hPos > 640)
			continue;

		if (hFlip)
			hPos += (gfx320 ? (effectiveWidth * 2) - 4 : effectiveWidth - 2);

		auto tileBasePic = TILES_ADDR + (tile * 32);
		for (auto col = 0; col < tileWidth; col++)
		{
			auto tilePic = tileBasePic;
			auto part = (line - vPos);
			if (vFlip) part = effectiveHeight - 1 - part;

			if (gfx240 && gfxMode > 0)
				part /= 2;
			auto tileLine = part / 8;
			if (tileWidth == 2) tilePic += tileLine * 32;
			if (tileWidth == 4) tilePic += tileLine * 96;

			tilePic += part * 4;

			for (auto j = 0; j < renderWidth; j += step)
			{
				auto hfJ = j;
				if (hFlip) hfJ = -j;

				auto twoPix = ramVideo[tilePic + (j / step)];
				if (twoPix == 0)
					continue;
				auto l = (twoPix >> 0) & 0x0F;
				auto r = (twoPix >> 4) & 0x0F;
				if (l) l += pal * 16;
				if (r) r += pal * 16;
				if (hFlip)
				{
					auto lt = l;
					l = r;
					r = lt;
				}

				if (blend == 0)
				{
					if (!gfx320)
					{
						if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderPixel(line, hPos + hfJ + 0, l);
						if (r != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderPixel(line, hPos + hfJ + 1, r);
					}
					else
					{
						if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640)
						{
							RenderPixel(line, hPos + hfJ + 0, l);
							RenderPixel(line, hPos + hfJ + 1, l);
						}
						if (r != 0 && hPos + hfJ + 2 >= 0 && hPos + hfJ + 2 < 640)
						{
							RenderPixel(line, hPos + hfJ + 2, r);
							RenderPixel(line, hPos + hfJ + 3, r);
						}
					}
				}
				else
				{
					bool sub = ((blend & 2) == 2);
					if (!gfx320)
					{
						if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderBlended(line, hPos + hfJ + 0, l, sub);
						if (r != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640) RenderBlended(line, hPos + hfJ + 1, r, sub);
					}
					else
					{
						if (l != 0 && hPos + hfJ >= 0 && hPos + hfJ < 640)
						{
							RenderBlended(line, hPos + hfJ + 0, l, sub);
							RenderBlended(line, hPos + hfJ + 1, l, sub);
						}
						if (r != 0 && hPos + hfJ + 2 >= 0 && hPos + hfJ + 2 < 640)
						{
							RenderBlended(line, hPos + hfJ + 2, r, sub);
							RenderBlended(line, hPos + hfJ + 3, r, sub);
						}
					}
				}
			}

			if (!hFlip)
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
	auto width = gfx320 ? 40 : 80;
	auto height = gfx240 ? 16 : 8;
	auto bgY = line / height;
	auto tileIndex = TEXT_ADDR + (((bgY % width) * width) * 2);
	auto font = FONT_ADDR + (gfxTextBold ? 0x800 : 0);
	if (gfx240)
		font = FONT_ADDR + 0x1000 + (gfxTextBold ? 0x1000 : 0);
	auto tileY = line % (gfx240 ? 16 : 8); //8;

	//if (gfxTextHigh)
	//	tileY = (line2 / 2) % 8;
	for (auto col = 0; col < width; col++)
	{
		auto chr = ramVideo[tileIndex++];
		auto att = ramVideo[tileIndex++];
		//if (chr == 0)
		//	continue;
		auto glyph = font + (chr * (gfx240 ? 16 : 8)); //8);
		auto scan = ramVideo[glyph + tileY];
		if (!gfx320)
		{
			for (auto bit = 0; bit < 8; bit++)
				RenderPixel(line, (col * 8) + bit, ((scan >> bit) & 1) == 1 ? (att & 0xF) : (att >> 4));
		}
		else
		{
			for (auto bit = 0; bit < 8; bit++)
			{
				RenderPixel(line, (col * 16) + (bit * 2) + 0, ((scan >> bit) & 1) == 1 ? (att & 0xF) : (att >> 4));
				RenderPixel(line, (col * 16) + (bit * 2) + 1, ((scan >> bit) & 1) == 1 ? (att & 0xF) : (att >> 4));
			}
		}
	}

	if (caret & 0x8000 && (SDL_GetTicks() % 500) < 250)
	{
		int cp = caret & 0x3FFF;
		int cr = cp / width;
		int ch = caret & 0x4000 ? -1 : height - 3;
		if (bgY == cr && line % height > ch)
		{
			int cc = cp % width;
			int ca = ramVideo[TEXT_ADDR + (cp * 2) + 1];
			int cw = gfx320 ? 16 : 8;
			for (auto col = 0; col < cw; col++)
			{
				RenderPixel(line, (cc * cw) + col, ca & 0xF);
			}
		}
	}
	RenderSprites(line, -1);
}

void RenderBitmapMode1(int line)
{
	auto imgWidth = gfx320 ? 160 : 320; //image is 160 or 320 bytes wide
	auto sourceLine = gfxTextBold ? (int)(line * 0.835) : line;
	if (gfxTextBold && !stretch200)
	{
		if (line < 40 || line > 439)
		{
			for (auto col = 0; col < 640 * 4; col++)
				pixels[(line * 640 * 4) + col] = 0;
			return;
		}
		sourceLine = line - 40;
	}
	auto effective = BMP_ADDR + (gfx240 ? sourceLine / 2 : sourceLine);
	auto p = 0;
	for (auto col = 0; col < 640; col += (gfx320 ? 4 : 2))
	{
		auto twoPix = ramVideo[effective * imgWidth + p];
		p++;
		auto l = (twoPix >> 4) & 0x0F;
		auto r = (twoPix >> 0) & 0x0F;
		if (!gfx320)
		{
			RenderPixel(line, col + 0, l);
			RenderPixel(line, col + 1, r);
		}
		else
		{
			RenderPixel(line, col + 0, l);
			RenderPixel(line, col + 1, l);
			RenderPixel(line, col + 2, r);
			RenderPixel(line, col + 3, r);
		}
	}
	RenderSprites(line, -1);
}

void RenderBitmapMode2(int line)
{
	auto imgWidth = gfx320 ? 320 : 640;
	auto sourceLine = gfxTextBold ? (int)(line * 0.835) : line;
	if (gfxTextBold && !stretch200)
	{
		if (line < 40 || line > 439)
		{
			for (auto col = 0; col < 640 * 4; col++)
				pixels[(line * 640 * 4) + col] = 0;
			return;
		}
		sourceLine = line - 40;
	}
	auto effective = BMP_ADDR + (gfx240 ? sourceLine / 2 : sourceLine);
	auto p = 0;
	for (auto col = 0; col < 640; col++)
	{
		auto pixel = ramVideo[effective * imgWidth + p];
		p++;
		if (!gfx320)
		{
			RenderPixel(line, col, pixel);
		}
		else
		{
			RenderPixel(line, col++, pixel);
			RenderPixel(line, col, pixel);
		}
	}
	RenderSprites(line, -1);
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
				RenderPixel(line, (x * 2) + 0, 0);
				RenderPixel(line, (x * 2) + 1, 0);
				RenderPixel(line + 1, (x * 2) + 0, 0);
				RenderPixel(line + 1, (x * 2) + 1, 0);
			}
			RenderSprites(line, 4);
			RenderSprites(line + 1, 4);
			continue;
		}

		screenBase = MAP1_ADDR + (layer * MAP_SIZE);

		if (!mapEnabled[layer])
		{
			RenderSprites(line, 3 - layer);
			RenderSprites(line + 1, 3 - layer);
			continue;
		}

		auto xxx = scrollX[layer] & maskX;
		auto yyy = (scrollY[layer] + sourceLine) & maskY;

		auto yShift = ((yyy >> 3) << 6);
		//yShift = 0;
		auto screenSource = screenBase + (0x000 * (xxx >> 8) + ((xxx & 511) >> 3) + yShift) * 2;
		auto shift = 0; // 128 << (tileShift[layer] - 1);

		//Grab the first tile
		//PPPP VH.T TTTT TTTT
		auto data = (ramVideo[screenSource] << 8) | ramVideo[screenSource+1];
		auto tile = (data & 0x3FF) + shift;
		auto tileY = yyy & 7;
		auto hFlip = (data & 0x0400) == 0x0400;
		auto vFlip = (data & 0x0800) == 0x0800;
		auto pal = data >> 12;
		auto tileBase = charBase + (tile << 5) + (tileY << 2);

		for (auto x = 0; x < 320; x++)
		{
			auto tileX = xxx & 7;

			if (hFlip) tileX = 7 - tileX;
			if (vFlip) tileY = 7 - tileY;

			auto color = (int)ramVideo[tileBase + (tileX >> 1)];
			if ((tileX & 1) == 1) color >>= 4;
			color &= 0x0F;

			if (color)
			{
				if (color) color += pal * 16;
				if (mapBlend[layer])
				{
					RenderBlended(line, (x * 2) + 0, color, (mapBlend[layer] & 2) == 2);
					RenderBlended(line, (x * 2) + 1, color, (mapBlend[layer] & 2) == 2);
					RenderBlended(line + 1, (x * 2) + 0, color, (mapBlend[layer] & 2) == 2);
					RenderBlended(line + 1, (x * 2) + 1, color, (mapBlend[layer] & 2) == 2);
				}
				else
				{
					RenderPixel(line, (x * 2) + 0, color);
					RenderPixel(line, (x * 2) + 1, color);
					RenderPixel(line + 1, (x * 2) + 0, color);
					RenderPixel(line + 1, (x * 2) + 1, color);
				}
			}

			if (hFlip) tileX = 7 - tileX;
			if (tileX == 7)
			{
				screenSource += 2;
				data = (ramVideo[screenSource] << 8) | ramVideo[screenSource+1];
				tile = (data & 0x3FF) + shift;
				hFlip = (data & 0x0400) == 0x0400;
				vFlip = (data & 0x0800) == 0x0800;
				pal = data >> 12;
				tileBase = charBase + (tile << 5) + (tileY << 2);
			}

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
		RenderSprites(line, 3 - layer);
		RenderSprites(line + 1, 3 - layer);
	}
}

void RenderLine(int line)
{
	switch (gfxMode)
	{
	case 0: RenderTextMode(line); break;
	case 1: RenderBitmapMode1(line); break;
	case 2: RenderBitmapMode2(line); break;
	case 3: RenderTileMode(line); break;
	}
}
