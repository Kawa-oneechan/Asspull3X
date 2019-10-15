#include "asspull.h"

Device* devices[MAXDEVS] = { 0 };

Device::Device(void) {}
Device::~Device(void) {}
unsigned int Device::Read(unsigned int address) { return (unsigned int)-1; }
void Device::Write(unsigned int address, unsigned int value) { }

FILE* diskFile; //TODO: make this part of DiskDrive

DiskDrive::DiskDrive()
{
	if ((data = (unsigned char*)malloc(0x0000200)) == NULL) return;
	SDL_memset(data, 0, 0x0000200);
	sector = 0;
	error = 0;
}

DiskDrive::~DiskDrive()
{
	if (data != NULL) free(data);
	if (diskFile != NULL) fclose(diskFile);
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
			ret |= (diskFile == NULL) ? 0 : 1;
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
			if (diskFile == NULL)
				return;
			fseek(diskFile, sector * 512, SEEK_SET);
			error = false;
			if (value == 4)
				error = (fread(data, 1, 512, diskFile) == 0);
			else if (value == 8)
			{
				fwrite(data, 1, 512, diskFile);
				fflush(diskFile);
			}
			return;
		}
	}
	if (address >= 512 && address < 1024)
		data[address - 512] = (unsigned char)value;;
}

