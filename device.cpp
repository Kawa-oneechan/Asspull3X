#include "asspull.h"

Device* devices[MAXDEVS] = { 0 };

Device::Device(void) { }
Device::~Device(void) { }
unsigned int Device::Read(unsigned int address) { return (unsigned int)-1; }
void Device::Write(unsigned int address, unsigned int value) { }
int Device::GetID() { return 0; }

DiskDrive::DiskDrive()
{
	if ((data = (unsigned char*)malloc(0x0000200)) == NULL) return;
	SDL_memset(data, 0, 0x0000200);
	sector = 0;
	error = 0;
	file = NULL;
}

DiskDrive::~DiskDrive()
{
	if (data != NULL) free(data);
	if (file != NULL) fclose(file);
}

int DiskDrive::Mount(const char* filename)
{
	if (file != NULL)
		return -1;
	SDL_Log("Mounting diskette, %s ...", filename);
	FILE* file = fopen(filename, "rb");
	return errno;
}

int DiskDrive::Unmount()
{
	if (file == NULL)
		return 1;
	SDL_Log("Unmounting diskette...");
	fclose(file);
	file = NULL;
	return 0;
}

unsigned int DiskDrive::Read(unsigned int address)
{
	switch (address)
	{
		case 0x00: return 0x01;
		case 0x01: return 0x44;
		case 0x02: return sector >> 8;
		case 0x03: return sector & 0xFF;
		case 0x04:
		{
			auto ret = error << 1;
			ret |= (file == NULL) ? 0 : 1;
			return ret;
		}
	}
	if (address >= 512 && address < 1024)
		return data[address - 512];
	return 0;
}

void DiskDrive::Write(unsigned int address, unsigned int value)
{
	switch (address)
	{
		case 0x02: sector = (sector & 0xFF00) | (value << 8); return;
		case 0x03: sector = (sector & 0x00FF) | value; return;
		case 0x04:
		{
			if (file == NULL)
				return;
			fseek(file, sector * 512, SEEK_SET);
			error = false;
			if (value == 4)
				error = (fread(data, 1, 512, file) == 0);
			else if (value == 8)
			{
				fwrite(data, 1, 512, file);
				fflush(file);
			}
			return;
		}
	}
	if (address >= 512 && address < 1024)
		data[address - 512] = (unsigned char)value;;
}

int DiskDrive::GetID() { return 0x0144; }

bool DiskDrive::IsMounted() { return file != NULL; }

LinePrinter::LinePrinter() { }

LinePrinter::~LinePrinter() { }

unsigned int LinePrinter::Read(unsigned int address)
{
	switch (address)
	{
		case 0x00: return 0x4C;
		case 0x01: return 0x50;
	}
	return 0;
}

void LinePrinter::Write(unsigned int address, unsigned int value)
{
	if (address == 2)
		printf("%c", (char)value);
}

int LinePrinter::GetID() { return 0x4C50; }
