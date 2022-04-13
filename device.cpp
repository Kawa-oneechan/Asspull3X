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


LinePrinter::LinePrinter()
{
	memset(line, 0, 80);
	lineLength = 0;
	pageLength = 0;
}

LinePrinter::~LinePrinter() { }

unsigned int LinePrinter::Read(unsigned int address)
{
	READ_DEVID(DEVID_LINEPRINTER);
	//switch (address)
	return 0;
}

void LinePrinter::Write(unsigned int address, unsigned int value)
{
	if (address == 2)
	{
		DWORD mode = 0;
		GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
		int bg = pageLength % 4 < 2 ? 107 : 47;
		if ((char)value == '\f')
		{
			if (mode & 4)
				wprintf(L"\x1b[%d;90m\u2022\u00A6----------------------------------------------------------------------------------\u00A6\u2022\x1B[0m\n", bg);
			else
				wprintf(L"o|----------------------------------------------------------------------------------|o\n");
			pageLength = 0;
		}
		else if ((char)value == '\n')
		{
			memset(line, 0, 80);
			lineLength = 0;
		}
		else if (lineLength == 80 || (char)value == '\r')
		{
			WCHAR wLine[80] = { 0 };
			mbstowcs_s(NULL, wLine, line, 80);
			if (mode & 4)
				wprintf(L"\x1b[%d;90m\u2022\u00A6\x1b[%d;30m %-80s \x1b[%d;90m\u00A6\u2022\x1B[0m\n", bg, bg, wLine, bg);
			else
				wprintf(L"o| %-80s |o\n", wLine);
			Write(2, '\n');
			pageLength++;
			if (pageLength == 30)
				Write(2, '\f');
		}
		else
		{
			line[lineLength++] = (char)value;
		}
	}
}

int LinePrinter::GetID() { return DEVID_LINEPRINTER; }

void LinePrinter::HBlank() {}

void LinePrinter::VBlank() {}
