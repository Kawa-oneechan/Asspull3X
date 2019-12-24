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

int DiskDrive::Mount(std::string filename)
{
	if (file != NULL)
		return -1;
	SDL_Log("Mounting disk image, %s ...", filename.c_str());
	file = fopen(filename.c_str(), "rb");
	if (file == 0)
		return errno;
	auto ext = strrchr(filename.c_str(), '.') + 1;
	fseek(file, 0, SEEK_END);
	capacity = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (SDL_strncasecmp(ext, "vhd", 3) == 0)
	{
		isVHD = true;
		capacity -= 512;
		fseek(file, capacity, SEEK_CUR);
		fseek(file, 56, SEEK_CUR); //skip over the rest, just get the geometry
		unsigned char x = 0;
		fread(&x, 1, 1, file); tracks = x;
		fread(&x, 1, 1, file); tracks = (tracks << 8) | x;
		fread(&x, 1, 1, file); heads = x;
		fread(&x, 1, 1, file); sectors = x;
		fseek(file, 0, SEEK_SET);
	}
	else
	{
		isVHD = false;
		//No CHS data in a diskette IMG, calculate it from total capacity.
		tracks = 80;
		heads = 2;
		sectors = capacity / 512;
		sectors = sectors / (tracks * heads);
	}
	SDL_Log("Mounted %s. %d * %d * %d * 512 = %d, should be ~%d.", (isVHD ? "VHD" : "IMG"), tracks, heads, sectors, tracks * heads * sectors * 512, capacity);
	return 0;
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
		case 0x10: return tracks >> 8;
		case 0x11: return tracks & 0xFF;
		case 0x12: return heads >> 8;
		case 0x13: return heads & 0xFF;
		case 0x14: return sectors >> 8;
		case 0x15: return sectors & 0xFF;
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
			//TODO: don't allow seeking out of bounds.
			fseek(file, sector * 512, SEEK_SET);
			error = false;
			if (value == 4)
				error = (fread(data, 1, 512, file) == 0);
			else if (value == 8)
			{
				fwrite(data, 1, 512, file);
				fflush(file);
				//TODO: update checksum for VHD.
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
