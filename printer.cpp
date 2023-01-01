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
	if (address == 2)
	{
		FILE* prnFile = nullptr;
		fopen_s(&prnFile, "printer.html", "a, ccs=UNICODE");
		fseek(prnFile, 0, SEEK_END);
		auto fs = ftell(prnFile);
		if (fs == 0)
			fwprintf(prnFile, L"<style>*{white-space:pre;font-family:monospace;}</style>");

		if ((char)value == '\r')
		{
		}
		else if ((char)value == '\f')
		{
			fwprintf(prnFile, L"<hr>");
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
						line[lineLength - 2] = '<';
						line[lineLength - 1] = 'b';
						line[lineLength++] = '>';
						padLength += 3;
						visibleLength -= 2;
						break;
					}
					case 'e':
					{
						line[lineLength - 2] = '<';
						line[lineLength - 1] = '/';
						line[lineLength++] = 'b';
						line[lineLength++] = '>';
						padLength += 4;
						visibleLength -= 2;
						break;
					}
					case 'U':
					{
						line[lineLength - 2] = '<';
						line[lineLength - 1] = 'u';
						line[lineLength++] = '>';
						padLength += 3;
						visibleLength -= 2;
						break;
					}
					case 'u':
					{
						line[lineLength - 2] = '<';
						line[lineLength - 1] = '/';
						line[lineLength++] = 'u';
						line[lineLength++] = '>';
						padLength += 4;
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
				fwprintf(prnFile, L"%s\n", wLine);
				pageLength++;
				//if (pageLength == 30) Write(2, '\f');
				memset(line, 0, 120);
				visibleLength = 0;
				lineLength = 0;
				padLength = 80;
			}
		}
		fclose(prnFile);
	}
}

int LinePrinter::GetID() { return DEVID_LINEPRINTER; }

void LinePrinter::HBlank() {}

void LinePrinter::VBlank() {}
