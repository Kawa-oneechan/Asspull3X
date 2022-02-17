#include "asspull.h"
#include <time.h>
#include <math.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <commctrl.h>
#include "resource.h"

#include "miniz.h"

namespace Video
{
	SDL_Window* sdlWindow = NULL;
	SDL_Renderer* sdlRenderer = NULL;
	SDL_Texture* sdlTexture = NULL;
	SDL_Texture* sdlShader = NULL;
	unsigned int programIds[MAXSHADERS] = { 0 };
	int numShaders = 1;
	float iTime = 0;

	int winWidth = 640, winHeight = 480, scrWidth = 640, scrHeight = 480, scale = 1, offsetX = 0, offsetY = 0;

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
	PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
	PFNGLUNIFORM1FPROC glUniform1f;

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
		glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
		glUniform1f = (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f");

		return glCreateShader && glShaderSource && glCompileShader && glGetShaderiv &&
			glGetShaderInfoLog && glDeleteShader && glAttachShader && glCreateProgram &&
			glLinkProgram && glValidateProgram && glGetProgramiv && glGetProgramInfoLog &&
			glUseProgram && glGetUniformLocation && glUniform1f;
	}

	void ComplainAboutShaders(const WCHAR* file, char* log, size_t logLength)
	{
		WCHAR hdr[512];
		WCHAR msg[512];
		wcscpy(hdr, UI::GetString(IDS_SHADERCOMPILE_A)); //"An error occured while compiling shaders."
		wsprintf(msg, UI::GetString(IDS_SHADERCOMPILE_B), file); //"The file %s contains at least one syntax error, and will be skipped."

		WCHAR *wLog = (WCHAR*)malloc((logLength + 4) * 2);
		mbstowcs_s(NULL, wLog, logLength, log, _TRUNCATE);
		Log(L"-----------------------");
		Log(wLog);
		Log(L"-----------------------");

		TASKDIALOGCONFIG td = {
			sizeof(TASKDIALOGCONFIG), UI::hWndMain, NULL,
			TDF_EXPAND_FOOTER_AREA, TDCBF_OK_BUTTON,
			UI::GetString(IDS_SHORTTITLE),
			NULL,
			hdr, msg,
			0, NULL, IDOK,
			0, NULL, 0,
			NULL,
			wLog
		};
		td.pszMainIcon = TD_WARNING_ICON;
		TaskDialogIndirect(&td, NULL, NULL, NULL);

		free(wLog);
	}

	GLuint compileShader(const char* source, const WCHAR* file, GLuint shaderType)
	{
		if (source == NULL)
		{
			Log(UI::GetString(IDS_MOUNTINGDISK)); //"Mounting disk image, %s ..."
			return 0;
		}
		//Log("Compiling shader: %s", source);
		// Create ID for shader
		GLuint result = glCreateShader(shaderType);
		// Define shader text
		glShaderSource(result, 1, &source, NULL);
		// Compile shader
		glCompileShader(result);

		//Check vertex shader for errors
		GLint shaderCompiled = GL_FALSE;
		glGetShaderiv(result, GL_COMPILE_STATUS, &shaderCompiled);
		if (shaderCompiled != GL_TRUE)
		{
			Log(UI::GetString(IDS_SHADERERROR)); //"Error compiling shader!"
			GLint logLength;
			glGetShaderiv(result, GL_INFO_LOG_LENGTH, &logLength);
			if (logLength > 0)
			{
				GLchar *log = (GLchar*)malloc(logLength + 4);
				glGetShaderInfoLog(result, logLength, &logLength, log);
				ComplainAboutShaders(file, log, logLength);
				free(log);
			}
			glDeleteShader(result);
			result = 0;
		}
		return result;
	}

	char* ReadTextFile(const WCHAR* filePath)
	{
		FILE* file = NULL;
		if (_wfopen_s(&file, filePath, L"r+b"))
			return NULL;
		if (file == NULL)
			return NULL;
		fseek(file, 0, SEEK_END);
		long size = ftell(file);
		if (size == 0)
		{
			fclose(file);
			return "";
		}
		fseek(file, 0, SEEK_SET);
		auto dest = new char[size]();
		if (dest == NULL)
		{
			fclose(file);
			return NULL;
		}
		fread(dest, 1, size, file);
		dest[size - 1] = 0;
		fclose(file);
		return dest;
	}

	GLuint compileProgram(const WCHAR* fragFile)
	{
		if (fragFile == NULL || fragFile[0] == 0)
			return 0;

		GLuint programId = 0;
		GLuint vtxShaderId, fragShaderId;

		programId = glCreateProgram();

		vtxShaderId = compileShader(vertexShader, fragFile, GL_VERTEX_SHADER);

		auto source = ReadTextFile(fragFile);
		fragShaderId = compileShader(source, fragFile, GL_FRAGMENT_SHADER);
		delete[] source;

		if (vtxShaderId && fragShaderId)
		{
			// Associate shader with program
			glAttachShader(programId, vtxShaderId);
			glAttachShader(programId, fragShaderId);
			glLinkProgram(programId);
			glValidateProgram(programId);

			// Check the status of the compile/link
			GLint logLength;
			glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
			if (logLength > 0)
			{
				char* log = (char*)malloc(logLength + 4);
				if (log != NULL)
				{
					// Show any errors as appropriate
					glGetProgramInfoLog(programId, logLength, &logLength, log);
					WCHAR *wLog = (WCHAR*)malloc((logLength + 4) * 2);
					mbstowcs_s(NULL, wLog, logLength, log, _TRUNCATE);
					free(log);
					Log(L"Prog Info Log: %s", wLog);
					free(wLog);
				}
			}
		}
		if (vtxShaderId) glDeleteShader(vtxShaderId);
		if (fragShaderId) glDeleteShader(fragShaderId);
		return programId;
	}

	void presentBackBuffer(SDL_Renderer *renderer, SDL_Window* win)
	{
		GLint oldProgramId = -1;
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

			if (programId != 0)
			{
				glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgramId);
				glUseProgram(programId);

				auto timeU = glGetUniformLocation(programId, "iTime");
				if (timeU != -1)
					glUniform1f(timeU, iTime);
			}

			if (i == numShaders - 1)
			{
				SDL_GetWindowSize(sdlWindow, &winWidth, &winHeight);
				winHeight -= UI::statusBarHeight;
				if (winWidth < 640)
				{
					winWidth = 640;
					SDL_SetWindowSize(sdlWindow, 640, winHeight + UI::statusBarHeight);
				}
				if (winHeight < 480)
				{
					winHeight = 480;
					SDL_SetWindowSize(sdlWindow, winWidth, 480 + UI::statusBarHeight);
				}

				auto maxScaleX = floorf(winWidth / 640.0f);
				auto maxScaleY = floorf(winHeight / 480.0f);
				scale = (int)__min(maxScaleX, maxScaleY);
				scrWidth = 640 * scale;
				scrHeight = 480 * scale;
				offsetX = (int)floorf((winWidth - scrWidth) * 0.5f);
				offsetY = (int)floorf((winHeight - scrHeight) * 0.5f);

				offsetY += UI::statusBarHeight;

				float imgH = 1.0f;
				if (numShaders == 1 && (stretch200 && (Registers::ScreenMode.Mode == 1 || Registers::ScreenMode.Mode == 2) && Registers::ScreenMode.Aspect))
					imgH = 0.830f;

				glViewport(offsetX, offsetY, scrWidth, scrHeight);
				glBegin(GL_TRIANGLE_STRIP);
				glTexCoord2f(0.0f, 0.0f);
				glVertex2f(0.0f, 0.0f);
				glTexCoord2f(1.0f, 0.0f);
				glVertex2f(640.0f, 0.0f);
				glTexCoord2f(0.0f, imgH);
				glVertex2f(0.0f, 480.0f);
				glTexCoord2f(1.0f, imgH);
				glVertex2f(640.0f, 480.0f);
				glEnd();
			}
			else
			{
				float imgH = 1.0f;
				if (i == 0 && stretch200 && (Registers::ScreenMode.Mode == 1 || Registers::ScreenMode.Mode == 2) && Registers::ScreenMode.Aspect)
					imgH = 0.830f;

				glViewport(0, 0, 640, 480);
				glBegin(GL_TRIANGLE_STRIP);
				glTexCoord2f(0.0f, 0.0f);
				glVertex2f(0.0f, 480.0f);
				glTexCoord2f(1.0f, 0.0f);
				glVertex2f(640.0f, 480.0f);
				glTexCoord2f(0.0f, imgH);
				glVertex2f(0.0f, 0.0f);
				glTexCoord2f(1.0f, imgH);
				glVertex2f(640.0f, 0.0f);
				glEnd();
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

		iTime += 0.01f;

		presentBackBuffer(sdlRenderer, sdlWindow);
		//SDL_UpdateWindowSurface(sdlWindow);
	}

	void InitShaders()
	{
		numShaders = ini.GetLongValue(L"video", L"shaders", 0);
		if (numShaders == 0)
		{
			programIds[0] = 0;
			numShaders = 1;
		}
		else
		{
			if (numShaders >= MAXSHADERS)
			{
				Log(UI::GetString(IDS_TOOMANYSHADERS), MAXSHADERS, numShaders); //"Too many shaders specified: can only do %d but %d were requested."
				numShaders = MAXSHADERS;
			}
			for (int i = 0; i < numShaders; i++)
			{
				WCHAR key[16] = { 0 };
				wsprintf(key, L"shader%d", i + 1);
				programIds[i] = compileProgram(ini.GetValue(L"video", key, L""));
			}
		}
	}

	int Initialize()
	{
		//Log("Creating window...");
		winWidth = ini.GetLongValue(L"video", L"width", 640);
		winHeight = ini.GetLongValue(L"video", L"height", 480);
		if ((sdlWindow = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, winWidth, winHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)) == NULL)
		{
			Log(UI::GetString(IDS_WINDOWFAILED), SDL_GetError()); //"Could not create window: %s"
			return -1;
		}

		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

		if ((sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE)) == NULL)
		{
			Log(UI::GetString(IDS_RENDERERFAILED), SDL_GetError()); //"Could not create renderer: %s"
			return -2;
		}

		stretch200 = ini.GetBoolValue(L"video", L"stretch200", false);

		initGLExtensions();

		InitShaders();

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, 640, 480, 0, -1, 1);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		if ((sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, 640, 480)) == NULL)
		{
			Log(UI::GetString(IDS_TEXTUREFAILED), SDL_GetError()); //"Could not create texture: %s"
			return -2;
		}

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		if ((sdlShader = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, 640, 480)) == NULL)
		{
			Log(UI::GetString(IDS_TEXTUREFAILED), SDL_GetError()); //"Could not create texture: %s"
			return -2;
		}

		if ((pixels = new unsigned char[640 * 480 * 4]()) == nullptr)
		{
			UI::Complain(IDS_SCREENFAIL);
			return -2;
		}

		return 0;
	}

	int Shutdown()
	{
		delete[] pixels;
		SDL_DestroyTexture(sdlTexture);
		SDL_DestroyTexture(sdlShader);
		SDL_DestroyRenderer(sdlRenderer);
		SDL_DestroyWindow(sdlWindow);
		return 0;
	}

	void Screenshot()
	{
		WCHAR snap[128];
		__time64_t now;
		_time64(&now);
		wsprintf(snap, L"%I64u.png", now);

		auto shot = new char[4 * scrWidth * scrHeight]();
		glReadPixels(offsetX, offsetY, scrWidth, scrHeight, GL_RGB, GL_UNSIGNED_BYTE, shot);

		size_t png_data_size = 0;
		void *pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(shot, scrWidth, scrHeight, 3, &png_data_size, MZ_DEFAULT_LEVEL, MZ_TRUE);
		delete[] shot;

		if (!pPNG_data)
		{
			UI::SetStatus(IDS_PNGFAILED); //"Failed to write PNG."
		}
		else
		{
			FILE* pFile = NULL;
			if (_wfopen_s(&pFile, snap, L"wb"))
			{
				UI::SetStatus(IDS_PNGFAILED); //"Failed to write PNG."
				return;
			}
			if (pFile == NULL)
			{
				UI::SetStatus(IDS_PNGFAILED); //"Failed to write PNG."
				return;
			}
			fwrite(pPNG_data, 1, png_data_size, pFile);
			fclose(pFile);
			WCHAR m[512] = { 0 };
			wsprintf(m, UI::GetString(IDS_SCREENSHOT), snap);
			UI::SetStatus(m); //"Snap! %s saved."
		}
		mz_free(pPNG_data);
	}
}
