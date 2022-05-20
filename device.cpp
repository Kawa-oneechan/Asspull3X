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
	visibleLength = 0;
	padLength = 80;
	pageLength = 0;
}

/* Silly idea inspired by reading the Epson T-1000 printer manual:
 * basically, more control codes.
 * The T-1000 manual includes a list of ESC control codes, such as
 * "\x1B e <n> <s>" which sets the tab sizes, or "\x1B C <n>" to
 * set the page length in lines, or "\x1B E" to use Emphasized font,
 * "\x1B - 1" to get underlines...
 * I'm thinking I could support a VERY SMALL SUBSET of VT100 codes,
 * of the "\x1B [ <...> m" kind, OR translate between Epson-ese and
 * VT100.
 *
 * Epson	A3X		VT100		Effect
 * E		E		[1m			Emphasis/bold
 * F		e		[22m		Remove emphasis
 * -1		U		[4m			Underline
 * -0		u		[24m		Remove underline
 */

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
			memset(line, 0, 120);
			visibleLength = 0;
			lineLength = 0;
			padLength = 80;
		}
		else if (visibleLength == 80 || (char)value == '\r')
		{
			WCHAR wLine[120] = { 0 };
			mbstowcs_s(NULL, wLine, line, 80);
			if (mode & 4)
				wprintf(L"\x1b[%d;90m\u2022\u00A6\x1b[%d;30m %*s \x1b[%d;90m\u00A6\u2022\x1B[0m\n", bg, bg, -padLength, wLine, bg);
			else
				wprintf(L"o| %*s |o\n", -padLength, wLine);
			Write(2, '\n');
			pageLength++;
			if (pageLength == 30)
				Write(2, '\f');
		}
		else
		{
			line[lineLength++] = (char)value;
			visibleLength++;
			if (line[lineLength - 2] == 0x1B)
			{
				switch (line[lineLength - 1])
				{
				case 'E':
				{
					line[lineLength - 1] = '[';
					line[lineLength++] = '1';
					line[lineLength++] = 'm';
					padLength += 4;
					break;
				}
				case 'e':
				{
					line[lineLength - 1] = '[';
					line[lineLength++] = '2';
					line[lineLength++] = '2';
					line[lineLength++] = 'm';
					padLength += 5;
					break;
				}
				case 'U':
				{
					line[lineLength - 1] = '[';
					line[lineLength++] = '4';
					line[lineLength++] = 'm';
					padLength += 4;
					break;
				}
				case 'u':
				{
					line[lineLength - 1] = '[';
					line[lineLength++] = '2';
					line[lineLength++] = '4';
					line[lineLength++] = 'm';
					padLength += 5;
					break;
				}
				}
			}
		}
	}
}

int LinePrinter::GetID() { return DEVID_LINEPRINTER; }

void LinePrinter::HBlank() {}

void LinePrinter::VBlank() {}
