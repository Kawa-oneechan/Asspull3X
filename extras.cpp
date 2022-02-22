#include "asspull.h"

FILE* logFile = NULL;

int Lerp(int a, int b, float f)
{
	return (int)((b - a) * f + a);
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

int Slurp(unsigned char* dest, const WCHAR* filePath, unsigned int* size)
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

int Dump(const WCHAR* filePath, unsigned char* source, unsigned long size)
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

void Log(WCHAR* message, ...)
{
	va_list args;
	va_start(args, message);
	WCHAR buf[1024] = { 0 };
	vswprintf(buf, 1024, message, args);
	va_end(args);
#if _CONSOLE
	wprintf(buf);
	wprintf(L"\n");
#endif
	if (logFile == NULL)
	{
		WCHAR fileName[FILENAME_MAX];
		wcscpy(fileName, UI::startingPath);
		wcscat(fileName, L"\\clunibus.log");
		logFile = _wfsopen(fileName, L"w, ccs=UTF-8", _SH_DENYWR);
	}
	if (logFile)
	{
		fwprintf(logFile, buf);
		fwprintf(logFile, L"\n");
		fflush(logFile);
	}
}
