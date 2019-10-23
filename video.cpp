#include "asspull.h"

#define RENDERPIXELS_DEFINE

#define TEXT	0x00000
#define BITMAP	0x00000
#define MAP1	0x00000
#define MAP2	0x08000
#define TILESET	0x10000
#define PALETTE	0x50000
#define FONT	0x50200
#define SPRITE1	0x54000
#define SPRITE2 0x54200

extern "C" {

bool gfx320, gfx240, gfxTextBold;
int gfxMode, gfxFade, scrollX[2], scrollY[2], tileShift[2], mapEnabled[2];

SDL_Window* sdlWindow = NULL;
SDL_Surface* sdlSurface = NULL;
SDL_Surface* sdlWinSurface = NULL;

unsigned short* pixels;

//by Alcaro
unsigned short add_rgb(unsigned short rgb, unsigned short add)
{
  unsigned short low_bit = ((1<<10) | (1<<5) | (1<<0));
  add = add * low_bit;
  unsigned short out_low = rgb ^ add; // this is the correct value in the low bit, others are garbage
  unsigned short out_add = rgb + add;
  unsigned short overflowed = (out_add^out_low) & (low_bit<<5); // if low^add is nonzero at positions 5, 10 or 15, it overflowed
  out_add -= overflowed; // remove overflow from 5/10/15
  out_add |= (overflowed - (overflowed>>5)); // and saturate the overflowed channels
  return out_add;
}

#ifdef RENDERPIXELS_DEFINE
#define RenderPixel(row, column, color) \
{ \
	auto snes = (ramVideo[PALETTE + ((color) * 2) + 0] << 8) + ramVideo[PALETTE + ((color) * 2) + 1]; \
	if (gfxFade != 0) \
	{ \
		if (gfxFade & 0x80) \
			snes = add_rgb(snes, gfxFade & 31); \
		else \
			snes = ~add_rgb(~snes, gfxFade & 31); \
	} \
	pixels[((row) * 640) + (column)] = snes; \
}
#else
inline void RenderPixel(int row, int column, int color)
{
	auto snes = (ramVideo[PALETTE + ((color) * 2) + 0] << 8) + ramVideo[PALETTE + ((color) * 2) + 1];
	if (gfxFade != 0)
	{
		if (gfxFade & 0x80)
			snes = add_rgb(snes, gfxFade & 31);
		else
			snes = ~add_rgb(~snes, gfxFade & 31);
	}
	pixels[((row * 640) + column)] = snes;
}
#endif

void RenderSprites(int line, int withPriority)
{
	auto imgWidth = gfx320 ? 320 : 640;
	auto step = gfx320 ? 4 : 2;
	for (auto i = 0; i < 256; i++)
	{
		auto spriteA = (ramVideo[SPRITE1 + 0 + (i * 2)] << 8) | (ramVideo[SPRITE1 + 1 + (i * 2)] << 0);
		if (spriteA == 0)
			continue;
		if ((spriteA & 0x800) != 0x800)
			continue;
		auto spriteB =
			(ramVideo[SPRITE2 + 0 + (i * 2)] << 24) |
			(ramVideo[SPRITE2 + 1 + (i * 2)] << 16) |
			(ramVideo[SPRITE2 + 2 + (i * 2)] << 8) |
			(ramVideo[SPRITE2 + 3 + (i * 2)] << 0);
		auto prio = (spriteB >> 30) & 3;
		if (withPriority > -1 && prio != withPriority)
			continue;
		auto tile = (spriteA >> 0) & 0x1FF;
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

		auto tileBasePic = TILESET + (tile * 32);
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

				if (!gfx320)
				{
					//if (hPos + j < 0 || hPos + j > 640)	
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
	auto tileIndex = TEXT + (((bgY % width) * width) * 2);
	auto font = FONT + (gfxTextBold ? 0x800 : 0);
	if (gfx240)
		font = FONT + 0x1000 + (gfxTextBold ? 0x1000 : 0);
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
	RenderSprites(line, -1);
}

void RenderBitmapMode1(int line)
{
	auto imgWidth = gfx320 ? 160 : 320; //image is 160 or 320 bytes wide
	auto sourceLine = gfxTextBold ? (int)(line * 0.835) : line;
	auto effective = BITMAP + (gfx240 ? sourceLine / 2 : sourceLine);
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
	auto effective = BITMAP + (gfx240 ? sourceLine / 2 : sourceLine);
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
	auto charBase = TILESET;
	auto screenBase = MAP1;
	auto sizeX = 512;
	auto sizeY = 256;
	auto maskX = sizeX - 1;
	auto maskY = sizeY - 1;

	for (int layer = -1; layer < 2; layer++)
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
			RenderSprites(line, 2);
			RenderSprites(line + 1, 2);
			continue;
		}

		if (!mapEnabled[layer])
		{
			RenderSprites(line, 1 - layer);
			RenderSprites(line + 1, 1 - layer);
			continue;
		}

		auto xxx = scrollX[layer] & maskX;
		auto yyy = (scrollY[layer] + sourceLine) & maskY;

		auto yShift = ((yyy >> 3) << 6);
		//yShift = 0;
		auto screenSource = screenBase + (0x000 * (xxx >> 8) + ((xxx & 511) >> 3) + yShift) * 2;
		auto shift = 128 << (tileShift[layer] - 1);

		for (auto x = 0; x < 320; x++)
		{
			//PPPP VH.T TTTT TTTT
			auto data = (ramVideo[screenSource] << 8) | ramVideo[screenSource+1];
			auto tile = (data & 0x1FF) + shift;
			auto tileX = xxx & 7;
			auto tileY = yyy & 7;

			auto hFlip = (data & 0x0400) == 0x0400;
			auto vFlip = (data & 0x0800) == 0x0800;

			auto pal = data >> 12;

			if (hFlip) tileX = 7 - tileX;
			if (vFlip) tileY = 7 - tileY;

			auto color = (int)ramVideo[charBase + (tile << 5) + (tileY << 2) + (tileX >> 1)];
			if ((tileX & 1) == 1) color >>= 4;
			color &= 0x0F;

			if (color)
			{
				if (color) color += pal * 16;
				RenderPixel(line, (x * 2) + 0, color);
				RenderPixel(line, (x * 2) + 1, color);
				RenderPixel(line + 1, (x * 2) + 0, color);
				RenderPixel(line + 1, (x * 2) + 1, color);
			}

			if (hFlip) tileX = 7 - tileX;
			if (tileX == 7) screenSource += 2;

			xxx++;
			if (xxx == 512)
			{
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
		RenderSprites(line, 1 - layer);
		RenderSprites(line + 1, 1 - layer);
		screenBase += 0x8000;
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

void VBlank()
{
	SDL_BlitSurface(sdlSurface, NULL, sdlWinSurface, NULL);
	SDL_UpdateWindowSurface(sdlWindow);
}

int InitVideo()
{
	SDL_Log("Creating window...");
	if ((sdlWindow = SDL_CreateWindow("Asspull IIIx", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_SHOWN)) == NULL)
	{
		SDL_Log("Could not create window: %s", SDL_GetError());
		return -1;
	}
	if ((sdlWinSurface = SDL_GetWindowSurface(sdlWindow)) == NULL)
	{
		SDL_Log("Could not get window surface: %s", SDL_GetError());
		return -2;
	}
	if ((sdlSurface = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 16, SDL_PIXELFORMAT_BGR555)) == NULL)
	{
		SDL_Log("Could not get surface: %s", SDL_GetError());
		return -2;
	}
	pixels = (unsigned short*)sdlSurface->pixels;

	if (sdlSurface->format->format != SDL_PIXELFORMAT_RGB888)
		SDL_Log("Surface format is wrong, should be 32-bit XRGB. Output may be fucky.");

	return 0;
}

int UninitVideo()
{
	SDL_FreeSurface(sdlSurface);
	SDL_FreeSurface(sdlWinSurface);
	SDL_DestroyWindow(sdlWindow);
	return 0;
}

}
