#include "asspull.h"

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
	FILE* file = _wfopen(filePath, L"rb");
	if (!file) return errno;
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
	FILE* file = _wfopen(filePath, L"wb");
	if (!file) return errno;
	fwrite(source, size, 1, file);
	fclose(file);
	return 0;
}

/*
void SDL_LogW(WCHAR* message, ...)
{
	va_list args;
	WCHAR wcs[1024];
	char mbcs[1024];
	va_start(args, message);
	wvsprintf(wcs, message, args);
	va_end(args);
	wcstombs(mbcs, wcs, 1024);
	SDL_Log(mbcs);
}
*/
