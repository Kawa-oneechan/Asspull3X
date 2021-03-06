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

int Slurp(unsigned char* dest, std::string filePath, unsigned int* size)
{
	FILE* file = fopen(filePath.c_str(), "rb");
	if (!file) return errno;
	fseek(file, 0, SEEK_END);
	long fs = ftell(file);
	if (size != 0) *size = (unsigned int)fs;
	fseek(file, 0, SEEK_SET);
	fread(dest, 1, fs, file);
	fclose(file);
	return 0;
}

int Dump(std::string filePath, unsigned char* source, unsigned long size)
{
	FILE* file = fopen(filePath.c_str(), "wb");
	if (!file) return errno;
	fwrite(source, size, 1, file);
	fclose(file);
	return 0;
}
