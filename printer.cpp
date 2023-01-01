#include "asspull.h"

/*
LINE PRINTER "KATHY"
--------------------

OUTPUTS
 * 0x0000: ID, 0x4C50 'LP'

INPUTS
 * 0x0002: character data.

INTERRUPTS
 * None

Lines are buffered up to 80 characters long, and actually printed
when either the buffer is filled up OR an \n is written.
Note that the line buffer is actually 120 characters long to make
room for formatting sequences -- switching bold or underline should
NOT count to the 80 character limit.

CONTROL CODES
 * \n: print line in buffer, then clear it.
 * \f: form feed -- skip ahead to next page.
 * \33E: enable boldtype, VT100 "[1m".
 * \33e: disable boldtype, VT100 "[22m".
 * \33U: enable underline, VT100 "[4m".
 * \33u: disable underline, VT100 "[24m".
*/

LinePrinter::LinePrinter()
{
	memset(line, 0, 120);
	lineLength = 0;
	visibleLength = 0;
	padLength = 80;
	pageLength = 0;
}

LinePrinter::~LinePrinter() { }

unsigned int LinePrinter::Read(unsigned int address)
{
	READ_DEVID(DEVID_LINEPRINTER);
	//switch (address)
	return 0;
}

#include <io.h>
#include <fcntl.h>

void LinePrinter::Write(unsigned int address, unsigned int value)
{
	//TODO: look into printing to PNG files.

	if (address == 2)
	{
		auto hWnd = GetConsoleWindow();
		if (hWnd == NULL)
		{
			//Apparently we have no console window. Debug with a detached process, Release build?
			AllocConsole();
			FILE* pOut = nullptr;
			freopen_s(&pOut, "CON", "w", stdout);
			SetConsoleCP(CP_UTF8);
			auto throwAway = _setmode(_fileno(stdout), _O_U16TEXT); throwAway;
			DWORD mode = 0;
			GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
			mode |= 4;
			SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode);
		}

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
		else if ((char)value == '\r')
		{
		}
		else
		{
			if ((char)value != '\n')
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
						visibleLength -= 2;
						break;
					}
					case 'e':
					{
						line[lineLength - 1] = '[';
						line[lineLength++] = '2';
						line[lineLength++] = '2';
						line[lineLength++] = 'm';
						padLength += 5;
						visibleLength -= 2;
						break;
					}
					case 'U':
					{
						line[lineLength - 1] = '[';
						line[lineLength++] = '4';
						line[lineLength++] = 'm';
						padLength += 4;
						visibleLength -= 2;
						break;
					}
					case 'u':
					{
						line[lineLength - 1] = '[';
						line[lineLength++] = '2';
						line[lineLength++] = '4';
						line[lineLength++] = 'm';
						padLength += 5;
						visibleLength -= 2;
						break;
					}
					}
				}
			}
			if (visibleLength == 80 || (char)value == '\n')
			{
				WCHAR wLine[128] = { 0 };
				mbstowcs_s(NULL, wLine, line, 80 + padLength);
				if (mode & 4)
					wprintf(L"\x1b[%d;90m\u2022\u00A6\x1b[%d;30m %*s \x1b[%d;90m\u00A6\u2022\x1B[0m\n", bg, bg, -padLength, wLine, bg);
				else
					wprintf(L"o| %*s |o\n", -padLength, wLine);
				pageLength++;
				if (pageLength == 30)
					Write(2, '\f');
				memset(line, 0, 120);
				visibleLength = 0;
				lineLength = 0;
				padLength = 80;
			}
		}
	}
}

int LinePrinter::GetID() { return DEVID_LINEPRINTER; }

void LinePrinter::HBlank() {}

void LinePrinter::VBlank() {}
