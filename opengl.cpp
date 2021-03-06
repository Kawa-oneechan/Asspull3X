#include "asspull.h"
#include <time.h>
#include <math.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

SDL_Window* sdlWindow = NULL;
SDL_Renderer* sdlRenderer = NULL;
SDL_Texture* sdlTexture = NULL;
SDL_Texture* sdlShader = NULL;
#define MAX_SHADERS 4
unsigned int programIds[MAX_SHADERS] = { 0 };
int numShaders = 1;
bool customMouse = false, alwaysCustomMouse = false;

int winWidth = 640, winHeight = 480, scrWidth = 640, scrHeight = 480, scale = 1, offsetX = 0, offsetY = 0;

extern unsigned char* pixels;
extern bool stretch200;
extern int statusBarHeight;

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

bool sdl2oh10 = false;

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

void presentBackBuffer(SDL_Renderer *renderer, SDL_Window* win)
{
	GLint oldProgramId;
	SDL_Texture* source = sdlTexture;
	SDL_Texture* target = sdlShader;

	for (int i = 0; i < numShaders; i++)
	{
		if (i == numShaders - 1)
			SDL_SetRenderTarget(renderer, NULL);
		else
			SDL_SetRenderTarget(renderer, target);
		SDL_RenderClear(renderer);

		unsigned int programId = programIds[i];

		SDL_GL_BindTexture(source, NULL, NULL);
		if(programId != 0)
		{
			glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgramId);
			glUseProgram(programId);
		}

		if (i == numShaders - 1)
		{
			SDL_GetWindowSize(sdlWindow, &winWidth, &winHeight);
			winHeight -= statusBarHeight;
			if (winWidth < 640)
			{
				winWidth = 640;
				SDL_SetWindowSize(sdlWindow, 640, winHeight + statusBarHeight);
			}
			if (winHeight < 480)
			{
				winHeight = 480;
				SDL_SetWindowSize(sdlWindow, winWidth, 480 + statusBarHeight);
			}

			auto maxScaleX = floorf(winWidth / 640.0f);
			auto maxScaleY = floorf(winHeight / 480.0f);
			scale = (int)__min(maxScaleX, maxScaleY);
			scrWidth = 640 * scale;
			scrHeight = 480 * scale;
			offsetX = (int)floorf((winWidth - scrWidth) * 0.5f);
			offsetY = (int)floorf((winHeight - scrHeight) * 0.5f);

			if (sdl2oh10)
			{
				offsetY += statusBarHeight;

				glViewport(offsetX, offsetY, scrWidth, scrHeight);

				glBegin(GL_TRIANGLE_STRIP);
					glTexCoord2f(0.0f, 0.0f);
					glVertex2f(0.0f, 0.0f);
					glTexCoord2f(1.0f, 0.0f);
					glVertex2f(640.0f, 0.0f);
					glTexCoord2f(0.0f, 1.0f);
					glVertex2f(0.0f, 480.0f);
					glTexCoord2f(1.0f, 1.0f);
					glVertex2f(640.0f, 480.0f);
				glEnd();
			}
			else
			{
				//Do NOT add the status bar height here, we work the other way up.

				glBegin(GL_TRIANGLE_STRIP);
					glTexCoord2f(0.0f, 0.0f);
					glVertex2f((GLfloat)offsetX, (GLfloat)offsetY);
					glTexCoord2f(1.0f, 0.0f);
					glVertex2f((GLfloat)offsetX + scrWidth, (GLfloat)offsetY);
					glTexCoord2f(0.0f, 1.0f);
					glVertex2f((GLfloat)offsetX, (GLfloat)offsetY + scrHeight);
					glTexCoord2f(1.0f, 1.0f);
					glVertex2f((GLfloat)offsetX + scrWidth, (GLfloat)offsetY + scrHeight);
				glEnd();
			}
		}
		else
		{
			glViewport(0, 0, 640, 480);

			if (sdl2oh10)
			{
				glBegin(GL_TRIANGLE_STRIP);
					glTexCoord2f(0.0f, 0.0f);
					glVertex2f(0.0f, 480.0f);
					glTexCoord2f(1.0f, 0.0f);
					glVertex2f(640.0f, 480.0f);
					glTexCoord2f(0.0f, 1.0f);
					glVertex2f(0.0f, 0.0f);
					glTexCoord2f(1.0f, 1.0f);
					glVertex2f(640.0f, 0.0f);
				glEnd();
			}
			else
			{
				glBegin(GL_TRIANGLE_STRIP);
					glTexCoord2f(0.0f, 0.0f);
					glVertex2f(0.0f, 0.0f);
					glTexCoord2f(1.0f, 0.0f);
					glVertex2f(640.0f, 0.0f);
					glTexCoord2f(0.0f, 1.0f);
					glVertex2f(0.0f, 480.0f);
					glTexCoord2f(1.0f, 1.0f);
					glVertex2f(640.0f, 480.0f);
				glEnd();
			}
		}

		if (source == sdlTexture)
		{
			source = sdlShader;
			target = sdlTexture;
		}
		else
		{
			source = sdlTexture;
			target = sdlShader;
		}
	}

	glUseProgram(oldProgramId);

	SDL_GL_SwapWindow(win);
}

void VBlank()
{
	SDL_SetRenderTarget(sdlRenderer, sdlTexture);
	SDL_UpdateTexture(sdlTexture, NULL, pixels, 640 * 4);
	presentBackBuffer(sdlRenderer, sdlWindow);
	//SDL_UpdateWindowSurface(sdlWindow);
}

extern HWND hWnd;

int InitVideo()
{
	SDL_version linked;
	SDL_GetVersion(&linked);
	if (linked.major == 2 && linked.minor == 0)
	{
		if (linked.patch > 7)
			sdl2oh10 = true;
		else if(linked.patch < 4)
		{
			//https://www.youtube.com/watch?v=5FjWe31S_0g
			char msg[512];
			sprintf(msg, "You are trying to run with an outdated version of SDL.\n\nYou have version %d.%d.%d.\nYou need version 2.0.4 or later.", linked.major, linked.minor, linked.patch);
			//SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Clunibus",  msg, NULL);
			MessageBox(hWnd, msg, "Clunibus", 0);
			return -3;
		}
	}

	SDL_Log("Creating window...");
	auto winWidth = ini.GetLongValue("video", "width", 640);
	auto winHeight = ini.GetLongValue("video", "height", 480);
	if ((sdlWindow = SDL_CreateWindow("Clunibus - Asspull IIIx Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, winWidth, winHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)) == NULL)
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

	stretch200 = ini.GetBoolValue("video", "stretch200", false);
	alwaysCustomMouse = customMouse = ini.GetBoolValue("video", "alwaysCustomMouse", false);

	initGLExtensions();
	numShaders = ini.GetLongValue("video", "shaders", -1);
	if (numShaders <= 0)
	{
		if (numShaders == 0)
			programIds[0] = 0;
		else
			programIds[0] = compileProgram(ini.GetValue("video", "shader", ""));
		numShaders = 1;
	}
	else
	{
		if (numShaders >= MAX_SHADERS)
		{
			SDL_Log("Too many shaders specified: can only do %d but %d were requested.", MAX_SHADERS, numShaders);
			numShaders = MAX_SHADERS;
		}
		for (int i = 0; i < numShaders; i++)
		{
			char key[16] = { 0 };
			sprintf(key, "shader%d", i + 1);
			programIds[i] = compileProgram(ini.GetValue("video", key, ""));
		}
	}

	if (sdl2oh10)
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, 640, 480, 0, -1, 1);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
	}

	if ((sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, 640, 480)) == NULL)
	{
		SDL_Log("Could not create texture: %s", SDL_GetError());
		return -2;
	}

	if ((sdlShader = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, 640, 480)) == NULL)
	{
		SDL_Log("Could not create texture: %s", SDL_GetError());
		return -2;
	}

	pixels = (unsigned char*)malloc(640 * 480 * 4);

	return 0;
}

int UninitVideo()
{
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyTexture(sdlShader);
	SDL_DestroyWindow(sdlWindow);
	return 0;
}

#include "miniz.h"

void Screenshot()
{
	char snap[128];
	__time64_t now;
	_time64(&now);
	sprintf(snap, "%llu.png", now);

	int size = scrWidth * scrHeight * 3;

	char* shot = (char*)malloc(4 * scrWidth * scrHeight);
	glReadPixels(offsetX, offsetY, scrWidth, scrHeight, GL_RGB, GL_UNSIGNED_BYTE, shot);

	size_t png_data_size = 0;
	void *pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(shot, scrWidth, scrHeight, 3, &png_data_size, MZ_DEFAULT_LEVEL, MZ_TRUE);
	if (!pPNG_data)
		SDL_Log("Failed to write PNG.");
	else
	{
		FILE *pFile = fopen(snap, "wb");
		fwrite(pPNG_data, 1, png_data_size, pFile);
		fclose(pFile);
		SDL_Log("Snap! %s saved.", snap);
	}
	mz_free(pPNG_data);
}
