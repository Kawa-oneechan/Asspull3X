#include "asspull.h"
#include <time.h>
#include <math.h>
#if WIN32
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
//undo some irrelevant Windows stuff
#ifdef TEXT
#undef TEXT
#endif
#else
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif

#define RENDERPIXELS_DEFINE

bool gfx320, gfx240, gfxTextBold, stretch200;
int gfxMode, gfxFade, scrollX[4], scrollY[4], tileShift[2], mapEnabled[4], mapBlend[4];
signed short mode7Matrix[4];

SDL_Window* sdlWindow = NULL;
SDL_Renderer* sdlRenderer = NULL;
SDL_Texture* sdlTexture = NULL;
unsigned int programId = 0;
bool customMouse = false, alwaysCustomMouse = false;

int winWidth = 640, winHeight = 480, scrWidth = 640, scrHeight = 480, scale = 1, offsetX = 0, offsetY = 0;

unsigned char* pixels;

void ProcessMode7(int* x, int* y)
{
	if (mode7Matrix[0] == 0x100 && mode7Matrix[1] == 0 && mode7Matrix[2] == 0 && mode7Matrix[3] == 0x100)
		return;

	auto A = (float)mode7Matrix[0] / 0x100;
	auto B = (float)mode7Matrix[1] / 0x100;
	auto C = (float)mode7Matrix[2] / 0x100;
	auto D = (float)mode7Matrix[3] / 0x100;

	auto Xi = (float)*x;
	auto Yi = (float)*y;

	//Full:
	//auto Xo = (A * (Xp + H - Xo)) + (B * (Yp + V - Yo) + Xo;
	//auto Yo = (C * (Xp + H - Xo)) + (D * (Yp + V - Yo) + Yo;
	auto Xo = (B * Yi) + (A * Xi);
	auto Yo = (C * Xi) + (D * Yi);

	*x = (int)Xo;
	*y = (int)Yo;
	
	//wrap
	*x %= 320; if (*x < 0) *x += 320;
	*y %= 240; if (*y < 0) *y += 240;
}

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

#ifdef RENDERPIXELS_DEFINE
#define RenderPixel(row, column, color) \
{ \
	auto snes = (ramVideo[PAL_ADDR + ((color) * 2) + 0] << 8) + ramVideo[PAL_ADDR + ((color) * 2) + 1]; \
	auto target = (((row) * 640) + (column)) * 4; \
	auto r = (snes >> 0) & 0x1F; \
	auto g = (snes >> 5) & 0x1F; \
	auto b = (snes >> 10) & 0x1F; \
	FADECODE; \
	pixels[target + 0] = (b << 3) + (b >> 2); \
	pixels[target + 1] = (g << 3) + (g >> 2); \
	pixels[target + 2] = (r << 3) + (r >> 2); \
}
#else
inline void RenderPixel(int row, int column, int color)
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
#endif

inline void RenderBlended(int row, int column, int color, bool subtractive)
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
			(ramVideo[SPR2_ADDR + 0 + (i * 2)] << 24) |
			(ramVideo[SPR2_ADDR + 1 + (i * 2)] << 16) |
			(ramVideo[SPR2_ADDR + 2 + (i * 2)] << 8) |
			(ramVideo[SPR2_ADDR + 3 + (i * 2)] << 0);
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
	if (gfx240)
		sourceLine /= 2;
	auto p = 0;
	for (auto col = 0; col < 640; col++)
	{
		auto newP = p;
		auto newSrc=  sourceLine;
		ProcessMode7(&newP, &newSrc);
		auto pixel = ramVideo[BMP_ADDR + newSrc * imgWidth + newP];
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

PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLVALIDATEPROGRAMPROC glValidateProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLUSEPROGRAMPROC glUseProgram;

const char* vertexShader = "varying vec4 v_color;"
"varying vec2 v_texCoord;"
"void main() {"
"gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
"v_color = gl_Color;"
"v_texCoord = vec2(gl_MultiTexCoord0);"
"}";

bool initGLExtensions() {
	glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
	glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
	glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
	glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
	glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
	glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
	glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
	glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)SDL_GL_GetProcAddress("glValidateProgram");
	glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
	glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");

	return glCreateShader && glShaderSource && glCompileShader && glGetShaderiv &&
		glGetShaderInfoLog && glDeleteShader && glAttachShader && glCreateProgram &&
		glLinkProgram && glValidateProgram && glGetProgramiv && glGetProgramInfoLog &&
		glUseProgram;
}

GLuint compileShader(const char* source, GLuint shaderType)
{
	if (source == NULL)
	{
		SDL_Log("Cannot compile shader, source is null.");
		return 0;
	}
	//SDL_Log("Compiling shader: %s", source);
	// Create ID for shader
	GLuint result = glCreateShader(shaderType);
	// Define shader text
	glShaderSource(result, 1, &source, NULL);
	// Compile shader
	glCompileShader(result);

	//Check vertex shader for errors
	GLint shaderCompiled = GL_FALSE;
	glGetShaderiv( result, GL_COMPILE_STATUS, &shaderCompiled );
	if( shaderCompiled != GL_TRUE )
	{
		SDL_Log("Error compiling shader: %d", result);
		GLint logLength;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0)
		{
			GLchar *log = (GLchar*)malloc(logLength);
			glGetShaderInfoLog(result, logLength, &logLength, log);
			SDL_Log("%s", log);
			free(log);
		}
		glDeleteShader(result);
		result = 0;
	}
	return result;
}

char* ReadTextFile(const char* filePath)
{
	FILE* file = fopen(filePath, "rb");
	if (!file) return NULL;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* dest = (char*)malloc(size + 1);
	fread(dest, 1, size, file);
	dest[size] = 0;
	fclose(file);
	return dest;
}

GLuint compileProgram(const char* fragFile)
{
	if (fragFile == NULL || fragFile[0] == 0)
		return 0;

	GLuint programId = 0;
	GLuint vtxShaderId, fragShaderId;

	programId = glCreateProgram();

	vtxShaderId = compileShader(vertexShader, GL_VERTEX_SHADER);

	auto source = ReadTextFile(fragFile);
	if (!alwaysCustomMouse)
		customMouse = (strstr(source, "{customMouseCursor}") != NULL);
	fragShaderId = compileShader(source, GL_FRAGMENT_SHADER);
	free(source);

	if(vtxShaderId && fragShaderId)
	{
		// Associate shader with program
		glAttachShader(programId, vtxShaderId);
		glAttachShader(programId, fragShaderId);
		glLinkProgram(programId);
		glValidateProgram(programId);

		// Check the status of the compile/link
		GLint logLen;
		glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLen);
		if(logLen > 0)
		{
			char* log = (char*) malloc(logLen * sizeof(char));
			// Show any errors as appropriate
			glGetProgramInfoLog(programId, logLen, &logLen, log);
			SDL_Log("Prog Info Log: %s", log);
			free(log);
		}
	}
	if(vtxShaderId) glDeleteShader(vtxShaderId);
	if(fragShaderId) glDeleteShader(fragShaderId);
	return programId;
}

void presentBackBuffer(SDL_Renderer *renderer, SDL_Window* win, SDL_Texture* backBuffer, GLuint programId)
{
	GLint oldProgramId;

	SDL_SetRenderTarget(renderer, NULL);
	SDL_RenderClear(renderer);

	SDL_GL_BindTexture(backBuffer, NULL, NULL);
	if(programId != 0)
	{
		glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgramId);
		glUseProgram(programId);
	}

	SDL_GetWindowSize(sdlWindow, &winWidth, &winHeight);
	if (winWidth < 640)
	{
		winWidth = 640;
		SDL_SetWindowSize(sdlWindow, 640, winHeight);
	}
	if (winHeight < 480)
	{
		winHeight = 480;
		SDL_SetWindowSize(sdlWindow, winWidth, 480);
	}
	auto maxScaleX = floorf(winWidth / 640.0f);
	auto maxScaleY = floorf(winHeight / 480.0f);
	scale = (int)__min(maxScaleX, maxScaleY);
	scrWidth = 640 * scale;
	scrHeight = 480 * scale;
	offsetX = (int)floorf((winWidth - scrWidth) * 0.5f);
	offsetY = (int)floorf((winHeight - scrHeight) * 0.5f);

	GLfloat minx = (GLfloat)offsetX;
	GLfloat miny = (GLfloat)offsetY;
	GLfloat maxx = minx + scrWidth;
	GLfloat maxy = miny + scrHeight;

	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(minx, miny);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2f(maxx, miny);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2f(minx, maxy);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2f(maxx, maxy);
	glEnd();
	SDL_GL_SwapWindow(win);

	if(programId != 0)
		glUseProgram(oldProgramId);
}

void VBlank()
{
	SDL_SetRenderTarget(sdlRenderer, sdlTexture);
	SDL_UpdateTexture(sdlTexture, NULL, pixels, 640 * 4);
	presentBackBuffer(sdlRenderer, sdlWindow, sdlTexture, programId);
	//SDL_UpdateWindowSurface(sdlWindow);
}

int InitVideo(bool fullScreen)
{
	SDL_Log("Creating window...");
	auto winWidth = SDL_atoi(ini->Get("video", "width", "640"));
	auto winHeight = SDL_atoi(ini->Get("video", "height", "480"));
	uint32_t flags = SDL_WINDOW_SHOWN;
	if (fullScreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else
		flags |= SDL_WINDOW_RESIZABLE;
	if ((sdlWindow = SDL_CreateWindow("Asspull IIIx", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, winWidth, winHeight, flags)) == NULL)
	{
		SDL_Log("Could not create window: %s", SDL_GetError());
		return -1;
	}


	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

	if ((sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE)) == NULL)
	{
		SDL_Log("Could not create renderer: %s", SDL_GetError());
		return -2;
	}

	auto thing = ini->Get("video", "stretch200", "false");
	if (thing[0] == 't' || thing[0] == 'T' || thing[0] == 1) stretch200 = true;
	thing = ini->Get("video", "alwaysCustomMouse", "false");
	if (thing[0] == 't' || thing[0] == 'T' || thing[0] == 1) alwaysCustomMouse = customMouse = true;

	initGLExtensions();
	programId = compileProgram(ini->Get("video", "shader", ""));

	if ((sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, 640, 480)) == NULL)
	{
		SDL_Log("Could not create renderer: %s", SDL_GetError());
		return -2;
	}

	pixels = (unsigned char*)malloc(640 * 480 * 4);

	SDL_ShowCursor(!customMouse);

	mode7Matrix[0] = 0x100;
	mode7Matrix[1] = 0x000;
	mode7Matrix[2] = 0x000;
	mode7Matrix[3] = 0x100;
	return 0;
}

int UninitVideo()
{
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyWindow(sdlWindow);
	return 0;
}

void Screenshot()
{
#ifdef WIN32
	char snap[128];
	__time64_t now;
	_time64(&now);
	sprintf(snap, "%llu.bmp", now);

	int winWidth, winHeight;
	SDL_GetWindowSize(sdlWindow, &winWidth, &winHeight);
	int scrWidth = (winWidth / 640) * 640;
	int scrHeight = (winHeight / 480) * 480;
	scrWidth = (int)(scrHeight * 1.33334f);
	int left = (winWidth - scrWidth) / 2;
	int top = (winHeight - scrHeight) / 2;
	int size = scrWidth * scrHeight * 3;

	char* shot = (char*)malloc(4 * scrWidth * scrHeight);
	glReadPixels(left, top, scrWidth, scrHeight, GL_BGR, GL_UNSIGNED_BYTE, shot);

	FILE* f = fopen(snap, "wb");
	if (!f) return;
	short s = 0x4D42; fwrite(&s, 2, 1, f);
	long l = size + 54; fwrite(&l, 4, 1, f);
	s = 0; fwrite(&s, 2, 2, f);
	l = 54; fwrite(&l, 4, 1, f);
	l = 40; fwrite(&l, 4, 1, f);
	l = scrWidth; fwrite(&l, 4, 1, f);
	l = scrHeight; fwrite(&l, 4, 1, f);
	s = 1; fwrite(&s, 2, 1, f);
	s = 24; fwrite(&s, 2, 1, f);
	l = 0; fwrite(&l, 4, 1, f);
	l = size; fwrite(&l, 4, 1, f);
	l = 7874; fwrite(&l, 4, 2, f);
	l = 0; fwrite(&l, 4, 2, f);
	fwrite(shot, size, 1, f);
	fclose(f);

	free(shot);
	SDL_Log("Snap! %s saved.", snap);
#else
	SDL_Log("Not on this platform just yet.");
#endif
}
