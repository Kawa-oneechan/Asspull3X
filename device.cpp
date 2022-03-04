#include "asspull.h"
#include "resource.h"

#define SECTOR_SIZE 512

Device* devices[MAXDEVS] = { 0 };

Device::Device(void) { }
Device::~Device(void) { }
unsigned int Device::Read(unsigned int address) { address; return (unsigned int)-1; }
void Device::Write(unsigned int address, unsigned int value) { address, value; }
int Device::GetID() { return 0; }

DiskDrive::DiskDrive(int newType)
{
	if ((data = new unsigned char[SECTOR_SIZE]()) == NULL) return;
	sector = 0;
	error = 0;
	capacity = 0;
	tracks = 0;
	heads = 0;
	sectors = 0;
	type = newType;
	file = NULL;
}

DiskDrive::~DiskDrive()
{
	if (data != NULL) free(data);
	if (file != NULL) fclose(file);
}

uint16_t ReadMoto16(FILE* file)
{
	uint16_t ret = 0;
	unsigned char x = 0;
	fread(&x, 1, 1, file); ret = x;
	fread(&x, 1, 1, file); ret = (ret << 8) | x;
	return ret;
}

uint32_t ReadMoto32(FILE* file)
{
	uint32_t ret = 0;
	unsigned char x = 0;
	fread(&x, 1, 1, file); ret = x;
	fread(&x, 1, 1, file); ret = (ret << 8) | x;
	fread(&x, 1, 1, file); ret = (ret << 8) | x;
	fread(&x, 1, 1, file); ret = (ret << 8) | x;
	return ret;
}

int DiskDrive::Mount(const WCHAR* filename)
{
	if (file != NULL)
		return -1;
	Log(UI::GetString(IDS_MOUNTINGDISK), filename); //"Mounting disk image, %s ..."
	if (_wfopen_s(&file, filename, L"r+b"))
		return errno;
	fseek(file, 0, SEEK_END);
	capacity = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (type == 0)
	{
		//Diskette
		tracks = 80;
		heads = 2;
		sectors = capacity / (tracks * heads);
	}
	else if (type == 1)
	{
		//VHD
		capacity -= 512;
		fseek(file, capacity, SEEK_SET);
		fseek(file, 8, SEEK_CUR); //skip cookie
		uint32_t i = ReadMoto32(file);
		if (i != 2)
		{
			UI::SetStatus(IDS_VHDBADBITS); //"Invalid VHD: bad feature bits (must be 2)"
			fclose(file);
			return 1;
		}
		i = ReadMoto32(file);
		if (i != 0x00010000)
		{
			UI::SetStatus(IDS_VHDBADVERSION); //"Invalid VHD: bad version (must be 1.00)."
			fclose(file);
			return 1;
		}
		fseek(file, 40, SEEK_CUR); //skip over the rest, just get the geometry
		tracks = ReadMoto16(file);
		unsigned char x = 0;
		fread(&x, 1, 1, file); heads = x;
		fread(&x, 1, 1, file); sectors = x;
		i = ReadMoto32(file);
		if (i != 2)
		{
			UI::SetStatus(IDS_VHDBADTYPE); //"Invalid VHD: bad disk type (must be FIXED)."
			fclose(file);
			return 1;
		}
		fseek(file, 0, SEEK_SET);
		Log(UI::GetString(IDS_MOUNTEDVHD), tracks, heads, sectors, tracks * heads * sectors * 512, capacity); //"Mounted VHD. %d * %d * %d * 512 = %d, should be ~%d."
	}
	return 0;
}

int DiskDrive::Unmount()
{
	if (file == NULL)
		return 1;
	Log(UI::GetString(IDS_UNMOUNTINGDISK)); //"Unmounting diskette..."
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
		case 0x05: return type;
		case 0x10: return tracks >> 8;
		case 0x11: return tracks & 0xFF;
		case 0x12: return heads >> 8;
		case 0x13: return heads & 0xFF;
		case 0x14: return sectors >> 8;
		case 0x15: return sectors & 0xFF;
	}
	if (address >= 512 && address < 512 + SECTOR_SIZE)
		return data[address - 512];
	return 0;
}

void DiskDrive::Write(unsigned int address, unsigned int value)
{
	if (type == ddDiskette)
		UI::diskIconTimer = 20;
	else
		UI::hddIconTimer = 20;

	switch (address)
	{
		case 0x02: sector = (unsigned short)((sector & 0xFF00) | (value << 8)); return;
		case 0x03: sector = (unsigned short)((sector & 0x00FF) | value); return;
		case 0x04:
		{
			if (file == NULL)
				return;
			//TODO: don't allow seeking out of bounds.
			fseek(file, sector * SECTOR_SIZE, SEEK_SET);
			error = false;
			if (value == 4)
				error = (fread(data, 1, SECTOR_SIZE, file) == 0);
			else if (value == 8)
			{
				fwrite(data, 1, SECTOR_SIZE, file);

				/*
				//Not really needed since we won't *edit* the header.
				if (type == ddHardDisk)
				{
					fseek(file, capacity + 64, SEEK_SET);
					WriteMoto32(file, 0); //less typing
					fseek(file, capacity, SEEK_SET);
					uint32_t checksum = 0;
					uint8_t b = 0;
					for (int i = 0; i < 54; i++) //go with the full 512-byte block???
					{
						fread(&x, 1, 1, file);
						checksum += x;
					}
					fseek(file, capacity + 64, SEEK_SET);
					WriteMoto32(file, ~checksum);
				}
				*/

				fflush(file);
			}
			return;
		}
	}
	if (address >= 512 && address < 512 + SECTOR_SIZE)
		data[address - 512] = (unsigned char)value;
}

int DiskDrive::GetID() { return 0x0144; }

int DiskDrive::GetType() { return type; }

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
	{
		//printf("%c", (char)value);
		char chr[1] = { (char)value };
		WCHAR wchr[6] = { 0 };
		mbstate_t throwAway = { 0 }; //only here to please code analysis :shrug:
		mbrtowc(wchr, chr, 1, &throwAway);
		wprintf(wchr);
	}
}

int LinePrinter::GetID() { return 0x4C50; }
