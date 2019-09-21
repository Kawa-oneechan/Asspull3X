#include "asspull.h"

#define RENDERPIXELS_DEFINE 1
#define ENABLE_FADE 1
#define ENABLE_SCANLINES 0

extern "C" {

bool gfx320, gfx240, gfxTextBold, gfxSprites;
int gfxMode, gfxFade, scrollX, scrollY;

SDL_Window* sdlWindow = NULL;
SDL_Surface* sdlSurface = NULL;

unsigned char* pixels;

#if ENABLE_FADE
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
#else
#define FADECODE {}
#endif
#if ENABLE_SCANLINES
#define SCANLINECODE \
	if ((line % 2) == 1) \
	{ \
		r >>= 1; \
		g >>= 1; \
		b >>= 1; \
	}
#else
#define SCANLINECODE {}
#endif
#if RENDERPIXELS_DEFINE
#define RenderPixel(row, column, color) \
{ \
	auto target = (((row) * 640) + (column)) * 4; \
	auto snes = (ramVideo[0x100000 + ((color) * 2) + 0] << 8) + ramVideo[0x100000 + ((color) * 2) + 1]; \
	auto r = (snes >> 0) & 0x1F; \
	auto g = (snes >> 5) & 0x1F; \
	auto b = (snes >> 10) & 0x1F; \
	FADECODE; \
	SCANLINECODE; \
	pixels[target + 0] = (b << 3) + (b >> 2); \
	pixels[target + 1] = (g << 3) + (g >> 2); \
	pixels[target + 2] = (r << 3) + (r >> 2); \
}
#else
inline void RenderPixel(int row, int column, int color)
{
	//auto target = ((row * sdlSurface->w) + column) * format->BytesPerPixel;
	auto target = ((row * 640) + column) * 4;
	auto snes = (ramVideo[0x100000 + (color * 2) + 0] << 8) + ramVideo[0x100000 + (color * 2) + 1];
	auto r = (snes >> 0) & 0x1F;
	auto g = (snes >> 5) & 0x1F;
	auto b = (snes >> 10) & 0x1F;
	FADECODE;
	SCANLINECODE;
	pixels[target + 0] = (b << 3) + (b >> 2);
	pixels[target + 1] = (g << 3) + (g >> 2);
	pixels[target + 2] = (r << 3) + (r >> 2);
}
#endif

void RenderSprites16(int line)
{
	auto tileBase = 0x080000;
	auto spriteBaseA = 0x108000;
	auto spriteBaseB = 0x108200;
	auto imgWidth = gfx320 ? 320 : 640;
	auto step = gfx320 ? 4 : 2;
	for (auto i = 0; i < 256; i++)
	{
		auto spriteA = (ramVideo[spriteBaseA + 0 + (i * 2)] << 8) | (ramVideo[spriteBaseA + 1 + (i * 2)] << 0);
		if (spriteA == 0)
			continue;
		if ((spriteA & 0x800) != 0x800)
			continue;
		auto spriteB =
			(ramVideo[spriteBaseB + 0 + (i * 2)] << 24) |
			(ramVideo[spriteBaseB + 1 + (i * 2)] << 16) |
			(ramVideo[spriteBaseB + 2 + (i * 2)] << 8) |
			(ramVideo[spriteBaseB + 3 + (i * 2)] << 0);
		//var spriteB = ramVideo[spriteBaseB + 0 + (i * 2)] * 0x1000000;
		//spriteB += ramVideo[spriteBaseB + 1 + (i * 2)] * 0x10000;
		//spriteB += ramVideo[spriteBaseB + 2 + (i * 2)] * 0x100;
		//spriteB += ramVideo[spriteBaseB + 3 + (i * 2)] * 0x1;
		auto prio = (spriteB >> 30) & 3;
		auto tile = (spriteA >> 0) & 0x1FF;
		auto pal = (spriteA >> 12) & 0x0F;
		//var hPos = (spriteB >> 0) & 0x3FF;
		//var vPos = (spriteB >> 12) & 0x1FF;
		auto hPos = ((spriteB & 0x7FF) << 21) >> 21;
		auto vPos = ((spriteB & 0x7FF000) << 10) >> 22;

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

		if (hPos + effectiveWidth <= 0 || hPos > imgWidth)
			continue;

		if (hFlip)
			hPos += (gfx320 ? (effectiveWidth * 2) - 4 : effectiveWidth - 2);

		auto tileBasePic = tileBase + (tile * 32);
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
			auto renderWidth = (gfx320 ? 16 : 8);

			for (auto j = 0; j < renderWidth; j += step)
			{
				if (hPos + j < 0 || hPos + j > imgWidth)
					continue;

				auto hfJ = j;
				if (hFlip) hfJ = -j;

				auto twoPix = ramVideo[tilePic + (j / step)];
				auto l = (twoPix >> 0) & 0x0F;
				auto r = (twoPix >> 4) & 0x0F;
				if (hFlip)
				{
					auto lt = l;
					l = r;
					r = lt;
				}

				if (!gfx320)
				{
					if (l != 0) RenderPixel(line, hPos + hfJ  + 0, l);
					if (r != 0) RenderPixel(line, hPos + hfJ + 1, r);
				}
				else
				{
					if (l != 0)
					{
						RenderPixel(line, hPos + hfJ + 0, l);
						RenderPixel(line, hPos + hfJ + 1, l);
					}
					if (r != 0)
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
	auto mapBase = 0;
	auto width = gfx320 ? 40 : 80;
	auto height = gfx240 ? 16 : 8;
	auto bgY = line / height;
	auto tileIndex = mapBase + (((bgY % width) * width) * 2);
	auto font = 0x100200 + (gfxTextBold ? 0x800 : 0);
	if (gfx240)
		font = 0x101200 + (gfxTextBold ? 0x1000 : 0);
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
	if (gfxSprites) RenderSprites16(line);
}

void RenderBitmapMode1(int line)
{
	auto mapBase = 0;
	auto imgWidth = gfx320 ? 160 : 320; //image is 160 or 320 bytes wide
	auto sourceLine = gfxTextBold ? (int)(line * 0.835) : line;
	auto effective = mapBase + (gfx240 ? sourceLine / 2 : sourceLine);
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
	//if (gfxSprites) RenderSprites16(line);
}

void RenderBitmapMode2(int line)
{
	auto mapBase = 0;
	auto imgWidth = gfx320 ? 320 : 640;
	auto sourceLine = gfxTextBold ? (int)(line * 0.835) : line;
	auto effective = mapBase + (gfx240 ? sourceLine / 2 : sourceLine);
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
	//if (gfxSprites) RenderSpritesPal(line, 0); //RenderSprites256(line);
}

void RenderTileMode(int line)
{
	if (line % 2 == 1) return;
	auto sourceLine = line / 2;
	auto layer = 0;
	auto charBase = 0x080000;
	auto screenBase = 0;
	auto sizeX = 512;
	auto sizeY = 256;
	auto maskX = sizeX - 1;
	auto maskY = sizeY - 1;
	auto xxx = scrollX & maskX;
	auto yyy = (scrollY + sourceLine) & maskY;

	auto yShift = ((yyy >> 3) << 6);
	//yShift = 0;
	auto screenSource = screenBase + (0x000 * (xxx >> 8) + ((xxx & 511) >> 3) + yShift) * 2;
	for (auto x = 0; x < 320; x++)
	{
		//PPPP VH.T TTTT TTTT
		auto data = (ramVideo[screenSource] << 8) | ramVideo[screenSource+1];
		auto tile = data & 0x1FF;
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

		if (color > 0 || layer == 0)
		{

			color += pal * 16;

			RenderPixel(line, (x * 2) + 0, color);
			RenderPixel(line, (x * 2) + 1, color);
			RenderPixel(line + 1, (x * 2) + 0, color);
			RenderPixel(line + 1, (x * 2) + 1, color);
		}

		if (hFlip && tileX == 0) screenSource += 2;
		else if (tileX == 7) screenSource += 2;

		xxx++;
		if (xxx == 512)
		{
			if (sizeX > 512)
				screenSource = screenBase + (0x400 + yShift * 2);
			else
			{
				screenSource = screenBase + (yShift * 2);
				xxx = 0;
			}
		}
		else if (xxx >= sizeX)
		{
			xxx = 0;
			screenSource = screenBase + (yShift * 2);
		}
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
	if ((sdlSurface = SDL_GetWindowSurface(sdlWindow)) == NULL)
	{
		SDL_Log("Could not get surface: %s", SDL_GetError());
		return -2;
	}
	pixels = (unsigned char*)sdlSurface->pixels;

	if (sdlSurface->format->format != SDL_PIXELFORMAT_RGB888)
		SDL_Log("Surface format is wrong, should be 32-bit XRGB. Output may be fucky.");

	return 0;
}

int UninitVideo()
{
	SDL_FreeSurface(sdlSurface);
	SDL_DestroyWindow(sdlWindow);
	return 0;
}

}
