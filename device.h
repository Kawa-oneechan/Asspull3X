#pragma once
#include <SDL.h>
#include <stdio.h>
#include <Windows.h>

#define MAXDEVS 16
#define DEVBLOCK 0x8000

class Device
{
public:
	Device(void);
	virtual ~Device(void) = 0;
	virtual unsigned int Read(unsigned int address);
	virtual void Write(unsigned int address, unsigned int value);
	virtual int GetID();
	virtual void HBlank();
	virtual void VBlank();
};

class DiskDrive : Device
{
private:
	FILE* file;
	unsigned char* data;
	unsigned short sector;
	unsigned long capacity;
	unsigned int tracks, heads, sectors;
	int error;
	int type;
public:
	DiskDrive(int newType);
	~DiskDrive(void);
	int Mount(const WCHAR* filename);
	int Unmount();
	unsigned int Read(unsigned int address);
	void Write(unsigned int address, unsigned int value);
	int GetID();
	void HBlank();
	void VBlank();
	int GetType();
	bool IsMounted();
};
#define DEVID_DISKDRIVE 0x0144
enum diskDriveTypes
{
	ddDiskette,
	ddHardDisk,
};

class LinePrinter : Device
{
private:
	char line[120];
	int lineLength, visibleLength, padLength;
	int pageLength;
public:
	LinePrinter(void);
	~LinePrinter(void);
	unsigned int Read(unsigned int address);
	void Write(unsigned int address, unsigned int value);
	int GetID();
	void HBlank();
	void VBlank();
};
#define DEVID_LINEPRINTER 0x4C50

class InputDevice : Device
{
private:
	unsigned int buffer[32];
	unsigned int bufferCursor;
	int lastMouseX, lastMouseY;
	unsigned int mouseLatch;
public:
	InputDevice(void);
	~InputDevice(void);
	unsigned int Read(unsigned int address);
	void Write(unsigned int address, unsigned int value);
	int GetID();
	void HBlank();
	void VBlank();
	void Enqueue(SDL_Keysym sym);
};
#define DEVID_INPUT 0x494F

#define READ_DEVID(x) \
	if (address == 0) return (x >> 8); \
	else if (address == 1) return (x & 0xFF);

extern Device* devices[MAXDEVS];
extern InputDevice* inputDev;

namespace Discord
{
	extern bool enabled;

	extern void Initialize();
	extern void SetPresence(char* gameName);
	extern void Shutdown();
}