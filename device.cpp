#include "asspull.h"
#include "resource.h"

#define SECTOR_SIZE 512

Device* devices[MAXDEVS] = { 0 };

Device::Device(void) { }
Device::~Device(void) { }
unsigned int Device::Read(unsigned int address) { address; return (unsigned int)-1; }
void Device::Write(unsigned int address, unsigned int value) { address, value; }
int Device::GetID() { return 0; }
void Device::HBlank() {}
void Device::VBlank() {}


LinePrinter::LinePrinter() { }

LinePrinter::~LinePrinter() { }

unsigned int LinePrinter::Read(unsigned int address)
{
	READ_DEVID(DEVID_LINEPRINTER);
	switch (address)
	{
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

int LinePrinter::GetID() { return DEVID_LINEPRINTER; }

void LinePrinter::HBlank() {}

void LinePrinter::VBlank() {}
