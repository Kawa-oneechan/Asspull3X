#include "asspull.h"
#include "support/miniz.h"

extern unsigned int biosSize, romSize;
extern WCHAR currentROM[FILENAME_MAX], currentSRAM[FILENAME_MAX];

FILE* logFile = NULL;

int Lerp(int a, int b, float f)
{
	return (int)((b - a) * f + a);
}

int GreatestCommonDivisor(int a, int b)
{
	while (b != 0)
	{
		int t = b;
		b = a % b;
		a = t;
	}
	return a;
}

unsigned int RoundUp(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

void ApplyBPS(char* patch, long patchSize)
{
	unsigned int srcOff = 4, dstOff = 0;

	auto decode = [&]() -> unsigned int
	{
		unsigned int data = 0, shift = 1;
		while (true)
		{
			auto x = patch[srcOff++];
			data += (x & 0x7f) * shift;
			if (x & 0x80) break;
			shift <<= 7;
			data += shift;
		}
		return data;
	};

	if (*((uint32_t*)patch) != 0x31535042 || decode() != romSize)
	{
		Log(logError, UI::GetString(IDS_BPSHEADERFAILED));
		return;
	}

	auto targetSize = decode();
	auto target = new unsigned char[targetSize]();
	/*
	if (target == nullptr)
	{
		Log(logError, L"Couldn't allocate space for patch.");
		return;
	}
	*/
	srcOff += decode();

	unsigned int srcRelOff = 0, dstRelOff = 0;
	while ((signed)srcOff < patchSize - 12)
	{
		auto length = decode();
		auto mode = length & 3;
		length = (length >> 2) + 1;
		switch (mode)
		{
		case 0:
			while (length--)
			{
				target[dstOff] = romCartridge[dstOff];
				dstOff++;
			}
			break;
		case 1:
			while (length--)
			{
				target[dstOff] = patch[srcOff];
				dstOff++;
				srcOff++;
			}
			break;
		case 2:
		case 3:
			int offset = decode();
			offset = offset & 1 ? -(offset >> 1) : (offset >> 1);
			if (mode == 2)
			{
				srcRelOff += offset;
				while (length--)
				{
					target[dstOff] = romCartridge[srcRelOff];
					dstOff++;
					srcRelOff++;
				}
			}
			else
			{
				dstRelOff += offset;
				while (length--)
				{
					target[dstOff] = target[dstRelOff];
					dstOff++;
					dstRelOff++;
				}
			}
		}
	}

	//Do I care about the checksums?

	memcpy_s(romCartridge, romSize, target, targetSize);
	delete[] target;
	Log(UI::GetString(IDS_BPSPATCHAPPLIED));
}

int LoadFile(unsigned char* dest, const WCHAR* filePath, unsigned int* size)
{
	FILE* file = NULL;
	if (_wfopen_s(&file, filePath, L"r+b"))
		return errno;
	if (file == NULL)
		return -1;
	fseek(file, 0, SEEK_END);
	long fs = ftell(file);
	if (size != 0) *size = (unsigned int)fs;
	fseek(file, 0, SEEK_SET);
	fread(dest, 1, fs, file);
	fclose(file);
	return 0;
}

int SaveFile(const WCHAR* filePath, unsigned char* source, unsigned long size)
{
	FILE* file = NULL;
	if (_wfopen_s(&file, filePath, L"wb"))
		return errno;
	if (file == NULL)
		return -1;
	fwrite(source, size, 1, file);
	fclose(file);
	return 0;
}

void LoadROM(const WCHAR* path)
{
	unsigned int fileSize = 0;

	if (!path[0])
		return;

	WCHAR lpath[512];
	for (int i = 0; i < 512; i++)
	{
		lpath[i] = towlower(path[i]);
		if (path[i] == 0)
			break;
	}

	auto ext = wcsrchr(lpath, L'.') + 1;
	if (!wcscmp(ext, L"ap3"))
	{
		Log(logNormal, UI::GetString(IDS_LOADINGROM), path); //"Loading ROM, %s ..."
		memset(romCartridge, 0, CART_SIZE);
		auto err = LoadFile(romCartridge, path, &romSize);
		if (err)
			UI::ReportLoadingFail(IDS_ROMLOADERROR, err, -1, path);
		else
			wcscpy(currentROM, path);
	}
	else if (!wcscmp(ext, L"a3z") || !wcscmp(ext, L"zip"))
	{
		mz_zip_archive zip;
		memset(&zip, 0, sizeof(zip));
		char zipPath[512] = { 0 };
		wcstombs(zipPath, path, 512);
		mz_zip_reader_init_file(&zip, zipPath, 0);

		bool foundSomething = false;
		for (int i = 0; i < (int)mz_zip_reader_get_num_files(&zip); i++)
		{
			mz_zip_archive_file_stat fs;
			if (!mz_zip_reader_file_stat(&zip, i, &fs))
			{
				mz_zip_reader_end(&zip);
				return;
			}

			if (!strchr(fs.m_filename, '.'))
				continue;

			auto ext2 = strrchr(fs.m_filename, '.') + 1;
			if (!_stricmp(ext2, "ap3"))
			{
				foundSomething = true;
				romSize = (unsigned int)fs.m_uncomp_size;
				memset(romCartridge, 0, CART_SIZE);
				Log(logNormal, UI::GetString(IDS_LOADINGROM), lpath); //"Loading ROM, %s ..."
				mz_zip_reader_extract_to_mem(&zip, i, romCartridge, romSize, 0);
				break;
			}
		}
		mz_zip_reader_end(&zip);
		if (!foundSomething)
		{
			UI::SetStatus(IDS_NOTHINGINZIP); //"No single AP3 file found in archive."
			return;
		}
	}

	WCHAR baseName[256] = { 0 };
	for (auto i = 0; lpath[i] != L'.'; i++)
		baseName[i] = lpath[i];
	auto baseNameLen = lstrlen(baseName);
	lstrcat(baseName, L".bps");
	FILE* bpsFile = NULL;
	_wfopen_s(&bpsFile, baseName, L"r+b");
	if (bpsFile != NULL)
	{
		fseek(bpsFile, 0, SEEK_END);
		long bpsSize = ftell(bpsFile);
		auto bpsData = new char[bpsSize];
		fseek(bpsFile, 0, SEEK_SET);
		fread(bpsData, 1, bpsSize, bpsFile);
		fclose(bpsFile);
		ApplyBPS(bpsData, bpsSize);
	}
	baseName[baseNameLen] = 0;
	
	fileSize = romSize;
	romSize = RoundUp(romSize);
	//Invertigo: need to set romSize regardless of build, lest mirroring in m68k_read_memory_8 break.
#if _CONSOLE
	if (romSize != fileSize)
		Log(logWarning, UI::GetString(IDS_BADSIZE), fileSize, fileSize, romSize, romSize); //"File size is not a power of two: is %d (0x%08X), should be %d (0x%08X)."

	unsigned int c1 = 0;
	unsigned int c2 = (romCartridge[0x20] << 24) | (romCartridge[0x21] << 16) | (romCartridge[0x22] << 8) | (romCartridge[0x23] << 0);
	for (unsigned int i = 0; i < romSize; i++)
	{
		if (i == 0x20)
			i += 4; //skip the checksum itself
		c1 += romCartridge[i];
	}
	if (c1 != c2)
		Log(logWarning, UI::GetString(IDS_BADCHECKSUM), c2, c1); //"Checksum mismatch: is 0x%08X, should be 0x%08X."
#endif

	ini.SetValue(L"media", L"rom", path);
	UI::SaveINI();

	auto sramSize = romCartridge[0x28] * 512;
	if (sramSize)
	{
		wcscpy(currentSRAM, path);
		ext = wcsrchr(currentSRAM, L'.') + 1;
		*ext = 0;
		wcscat(currentSRAM, L"srm");
		Log(logNormal, UI::GetString(IDS_LOADINGSRAM), currentSRAM); //"Loading SRAM, %s ..."
		LoadFile(ramCartridge, currentSRAM, nullptr);
	}

	char romName[32] = { 0 };
	memcpy_s(romName, 32, romCartridge + 8, 24);
	Discord::SetPresence(romName);
	WCHAR wideName[256] = { 0 };
	if (romCartridge[0x27] == 'j')
		MultiByteToWideChar(932, 0, romName, -1, wideName, 256);
	else if (romCartridge[0x27] == 'r')
		MultiByteToWideChar(1251, 0, romName, -1, wideName, 256);
	else
		mbstowcs_s(NULL, wideName, romName, 256);
	UI::SetTitle(wideName);
}

void FindFirstDrive()
{
	int old = firstDiskDrive;
	firstDiskDrive = -1;
	bool firstIsHDD = false;
	for (int i = 0; i < MAXDEVS; i++)
	{
		if (devices[i] != nullptr && devices[i]->GetID() == DEVID_DISKDRIVE)
		{
			if (((DiskDrive*)devices[i])->GetType() == ddHardDisk && firstDiskDrive == -1)
			{
				firstIsHDD = true;
				firstDiskDrive = i;
				continue;
			}
			firstDiskDrive = i;
			//if (old != i) Log(L"First disk drive is now #%d.", i);
			return;
		}
	}
}

void SaveCartRAM()
{
	auto sramSize = romCartridge[0x28] * 512;
	if (sramSize)
		SaveFile(currentSRAM, ramCartridge, sramSize);
}

void Log(logCategories cat, WCHAR* message, ...)
{
	va_list args;
	va_start(args, message);
	WCHAR buf[1024] = { 0 };
	vswprintf(buf, 1024, message, args);
	va_end(args);
	if (buf[0] == 0)
		return;
#if _CONSOLE
	DWORD mode = 0;
	auto std = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleMode(std, &mode);
	if (mode & 4)
	{
		switch (cat)
		{
		case logNormal: break; //no special treatment.
		case logWarning: wprintf(L"\x1b[93m\u26A0 \x1B[1m"); break;
		case logError: wprintf(L"\x1b[101m\u26D4 \x1B[1m"); break;
		}
		wprintf(L"%s\x1B[0m\n", buf);
	}
	else
	{
		switch (cat)
		{
		case logNormal: break;
		case logWarning: SetConsoleTextAttribute(std, 14); wprintf(L"\u26A0 "); break;
		case logError: SetConsoleTextAttribute(std, 12); wprintf(L"\u26D4 "); break;
		}
		SetConsoleTextAttribute(std, 7);
		wprintf(L"%s\n", buf);
	}
#endif
	if (logFile == NULL)
	{
		WCHAR fileName[FILENAME_MAX] = { 0 };
		wcscpy(fileName, UI::startingPath);
		wcscat(fileName, L"\\clunibus.log");
		logFile = _wfsopen(fileName, L"w, ccs=UTF-8", _SH_DENYWR);
	}
	if (logFile)
	{
		//strip out "ESC[...m"
		WCHAR *o = buf;
		WCHAR *n = buf;
		while (*o)
		{
			if (*o != '\x1b')
				*n++ = *o++;
			else
			{
				while (*o != 'm')
					o++;
				o++;
			}
		}
		*n = 0;

		fwprintf(logFile, L"%s\n", buf);
		fflush(logFile);
	}
}

void Log(WCHAR* message, ...)
{
	va_list args;
	va_start(args, message);
	WCHAR buf[1024] = { 0 };
	vswprintf(buf, 1024, message, args);
	va_end(args);
	Log(logNormal, buf);
}


//Wide versions of the SDL routines, to save a conversion step.

static const WCHAR* map_StringForControllerButton[] = {
	L"a",
	L"b",
	L"x",
	L"y",
	L"back",
	L"guide",
	L"start",
	L"leftstick",
	L"rightstick",
	L"leftshoulder",
	L"rightshoulder",
	L"dpup",
	L"dpdown",
	L"dpleft",
	L"dpright",
	L"misc1",
	L"paddle1",
	L"paddle2",
	L"paddle3",
	L"paddle4",
	L"touchpad",
	NULL
};

SDL_GameControllerButton SDL_GameControllerGetButtonFromStringW(const WCHAR *pchString)
{
	int entry;
	if (!pchString || !pchString[0])
		return SDL_CONTROLLER_BUTTON_INVALID;

	for (entry = 0; map_StringForControllerButton[entry]; ++entry)
		if (wcscmp(pchString, map_StringForControllerButton[entry]) == 0)
			return (SDL_GameControllerButton)entry;
	return SDL_CONTROLLER_BUTTON_INVALID;
}

const WCHAR* SDL_GameControllerGetStringForButtonW(SDL_GameControllerButton axis)
{
	if (axis > SDL_CONTROLLER_BUTTON_INVALID && axis < SDL_CONTROLLER_BUTTON_MAX)
		return map_StringForControllerButton[axis];
	return NULL;
}