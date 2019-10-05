#include "asspull.h"

extern "C" {

extern void SendMidi(unsigned int message);

unsigned int  m68k_read_memory_8(unsigned int address);
unsigned int  m68k_read_memory_16(unsigned int address);
unsigned int  m68k_read_memory_32(unsigned int address);
void m68k_write_memory_8(unsigned int address, unsigned int value);
void m68k_write_memory_16(unsigned int address, unsigned int value);
void m68k_write_memory_32(unsigned int address, unsigned int value);

unsigned char* romBIOS = NULL;
unsigned char* romCartridge = NULL;
unsigned char* ramInternal = NULL;
unsigned char* disk = NULL;
unsigned char* ramVideo = NULL;

FILE* diskFile = NULL;
int diskError, diskSector;

int keyScan;

int dmaSource, dmaTarget;
unsigned int dmaLength;
bool hdmaOn[8], hdmaDouble[8];
int hdmaSource[8], hdmaTarget[8], hdmaWidth[8], hdmaStart[8], hdmaCount[8];

void HandleBlitter(unsigned int function);
unsigned int blitLength;
int blitAddrA, blitAddrB;

long ticks = 0;

extern bool gfx320, gfx240, gfxTextBold, gfxSprites;
extern int gfxMode, gfxFade, scrollX[2], scrollY[2], tileShift[2], mapEnabled[2];

extern int line, interrupts;

void HandleHdma(int line)
{
	for (auto i = 0; i < 8; i++)
	{
		if (!hdmaOn[i])
			continue;
		if (line < hdmaStart[i])
			continue;
		if (line > hdmaStart[i] + hdmaCount[i])
			continue;
		line -= hdmaStart[i];
		auto l = line / (hdmaDouble[i] ? 2 : 1);
		auto width = hdmaWidth[i];
		auto target = hdmaTarget[i];
		auto source = hdmaSource[i];
		switch (width)
		{
			case 0:
				source += l;
				m68k_write_memory_8(target, m68k_read_memory_8(source));
				break;
			case 1:
				source += l * 2;
				m68k_write_memory_16(target, m68k_read_memory_16(source));
				break;
			case 2:
				source += l * 4;
				m68k_write_memory_32(target, m68k_read_memory_32(source));
				break;
		}
	}
}

int InitMemory()
{
	if ((romBIOS = (unsigned char*)malloc(0x0020000)) == NULL) return -1;
	SDL_memset(romBIOS, 0, 0x0020000);
	if ((romCartridge = (unsigned char*)malloc(0x0FF0000)) == NULL) return -1;
	SDL_memset(romCartridge, 0, 0x0FF0000);
	if ((ramInternal = (unsigned char*)malloc(0xA000000)) == NULL) return -1;
	SDL_memset(ramInternal, 0, 0xA000000);
	if ((disk = (unsigned char*)malloc(0x0000200)) == NULL) return -1;
	SDL_memset(disk, 0, 0x0000200);
	if ((ramVideo = (unsigned char*)malloc(0x0200000)) == NULL) return -1;
	SDL_memset(ramVideo, 0, 0x0200000);
	return 0;
}

unsigned int m68k_read_memory_8(unsigned int address)
{
	if (address >= 0x0D800000 && address < 0x0E000000)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
		case 4: //Screen Mode
			return (gfxMode |
				(gfxSprites ? 1 << 8 : 0) |
				(gfx240 ? 1 << 5 : 0) |
				(gfx320 ? 1 << 6 : 0) |
				(gfxTextBold ? 1 << 7 : 0));
		case 5: //VBlankMode
			return interrupts;
		case 0x32: //Disk Control
			auto ret = diskError << 1;
			ret |= (diskFile == NULL) ? 0 : 1;
			return ret;
		}
		return 0;
	}
	auto bank = (address & 0x0F000000) >> 24;
	auto addr =  address & 0x00FFFFFF;
	switch (bank)
	{
		case 0x0:
			if (addr < 0x00020000)
				return romBIOS[addr];
			return romCartridge[addr - 0x00020000];
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x8:
		case 0x9:
			return ramInternal[addr + (0x1000000 * (bank - 1))];
		case 0xD:
			{
				if (addr >= 0x7FFE00)
					return disk[addr - 0x7FFE00];
				else
					return 0;
			}
		case 0xE:
			if (addr >= 0x0200000)
				return 0;
			return ramVideo[addr];
	}
	return 0;
}

unsigned int m68k_read_memory_16(unsigned int address)
{
	if (address >= 0x0D800000 && address < 0x0E000000)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0: //Line
				return line;
			case 0x6: //Keyscan
				return keyScan;
			case 0x30: //Disk sector
				if (diskFile == NULL)
					return 0;
				return diskSector;
		}
		return 0;
	}
	auto ret = m68k_read_memory_8(address) * 0x100;
	ret += m68k_read_memory_8(address + 1);
	return ret;
}

unsigned int m68k_read_memory_32(unsigned int address)
{
	if (address >= 0x0D800000 && address < 0x0E000000)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x8: //Ticks
				return (int)ticks;
		}
		return 0;
	}
	auto ret = m68k_read_memory_8(address) * 0x1000000;
	ret += m68k_read_memory_8(address + 1) * 0x10000;
	ret += m68k_read_memory_8(address + 2) * 0x100;
	ret += m68k_read_memory_8(address + 3) * 0x1;
	return ret;
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
	if (address >= 0x0D800000 && address < 0x0E000000)
	{
		auto reg = address & 0x000FFFFF;
		auto u8 = (unsigned char)value;
		switch (reg)
		{
			case 4: //ScreenMode
				gfxTextBold = ((u8 >> 7) & 1) == 1;
				gfx320 = ((u8 >> 6) & 1) == 1;
				gfx240 = ((u8 >> 5) & 1) == 1;
				gfxSprites = ((u8 >> 4) & 1) == 1;
				gfxMode = u8 & 0x0F;
				break;
			case 5: //VBlankMode
				interrupts = value;
				break;
			case 0xC: //ScreenFade
				gfxFade = u8;
				break;
			case 0xE: //Debug
				//Console.Write((char)value);
				printf("%c", (char)value);
				break;
			case 0x10:
			case 0x11:
				mapEnabled[reg - 0x10] = value >> 7;
				tileShift[reg - 0x10] = value & 15;
				break;
			case 0x2A: //DMA Control
				{
				if ((value & 1) == 0) return;
				auto increaseSource = ((value >> 1) & 1) == 1;
				auto increaseTarget = ((value >> 2) & 1) == 1;
				auto asValue = ((value >> 3) & 1) == 1;
				auto dataWidth = (value >> 4) & 3;
				auto increaseStep = (dataWidth == 0) ? 1 : (dataWidth == 1) ? 2 : 4;
				if (dmaTarget == 0xE000000)
					dmaTarget += 0;
				if (asValue && dataWidth == 2 && dmaTarget >= 0x1000000 && dmaTarget < 0xA000000)
				{
					//Do it quicker!
					//TODO: allow this for all long-sized transfers that aren't into register space?
					//SDL_Log("DMA: detected a long-width memset into WRAM.");
					SDL_memset((void*)&ramInternal[dmaTarget - 0x1000000], dmaSource, dmaLength);
					break;
				}
				while (dmaLength > 0)
				{
					if (asValue)
					{
						if (dataWidth == 0) m68k_write_memory_8(dmaTarget, dmaSource);
						else if (dataWidth == 1) m68k_write_memory_16(dmaTarget, dmaSource);
						else if (dataWidth == 2) m68k_write_memory_32(dmaTarget, dmaSource);
					}
					else
					{
						if (dataWidth == 0) m68k_write_memory_8(dmaTarget, m68k_read_memory_8(dmaSource));
						else if (dataWidth == 1) m68k_write_memory_16(dmaTarget, m68k_read_memory_16(dmaSource));
						else if (dataWidth == 2) m68k_write_memory_32(dmaTarget, m68k_read_memory_32(dmaSource));
					}
					dmaLength--;
					if (increaseSource) dmaSource += increaseStep;
					if (increaseTarget) dmaTarget += increaseStep;
				}
				break;
				}
			case 0x32: //Disk Control
				if (diskFile == NULL)
					break;
				fseek(diskFile, diskSector * 512, SEEK_SET);
				//diskStream.Seek(diskSector * 512, System.IO.SeekOrigin.Begin);
				diskError = false;
				if (value == 4)
				{
					diskError = (fread(disk, 1, 512, diskFile) == 0);
					//diskError = diskStream.Read(disk, 0, 512) == 0;
				}
				else if (value == 8)
				{
					fwrite(disk, 1, 512, diskFile);
					fflush(diskFile);
					//diskStream.Write(disk, 0, 512);
					//diskStream.Flush();
				}
				break;
			case 0x110: //Blitter key
				//blitKey = (byte)value;
				break;
		}
		return;
	}
	auto bank = (address & 0x0F000000) >> 24;
	auto addr =  address & 0x00FFFFFF;
	switch (bank)
	{
		case 0x0: /* BIOS is ROM */ break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x8:
		case 0x9:
			ramInternal[addr + (0x1000000 * (bank - 1))] = (unsigned char)value; break;
		case 0xD:
			{
				if (addr >= 0x7FFE00)
					disk[addr - 0x7FFE00] =(unsigned char)value;
				break;
			}
		case 0xE:
			if (addr >= 0x0200000)
				break;
			ramVideo[addr] = (unsigned char)value; break;
		//default: memory[address & 0x0FFFFFFF] = (byte)value; break;
	}
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	if (address >= 0x0D800000 && address < 0x0E000000)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x12: //Horizontal scroll
			case 0x14:
				scrollX[(reg - 0x12) / 2] = value & 511;
				break;
			case 0x16: //Vertical scroll
			case 0x18:
				scrollY[(reg - 0x16) / 2] = value & 511;
				break;
			case 0x30: //Disk sector
				if (diskFile != NULL)
					diskSector = value;
				break;
		}
		return;
	}
	m68k_write_memory_8(address + 0, (value >> 8));
	m68k_write_memory_8(address + 1, (value >> 0));
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	if (address >= 0x0D800000 && address < 0x0E000000)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x20: //DMA Source
				dmaSource = value;
				break;
			case 0x24: //DMA Target
				dmaTarget = value;
				break;
			case 0x28: //DMA Length
				dmaLength = value;
				break;
			case 0x40: //MIDI Out
				if (value > 0)
					SendMidi(value);
				break;
			case 0x80: //HDMA Control
			case 0x84:
			case 0x88:
			case 0x8C:
			case 0x90:
			case 0x94:
			case 0x98:
			case 0x9C:
				{
				auto channel = (reg & 0xF) / 4;
				hdmaWidth[channel] = (value >> 4) & 3;
				hdmaDouble[channel] = ((value >> 7) & 1) == 1;
				hdmaOn[channel] = (value & 1) == 1;
				hdmaStart[channel] = (value >> 8) & 0x3FF;
				hdmaCount[channel] = (value >> 20) & 0x3FF;
				break;
				}
			case 0xA0: //HDMA Source
			case 0xA4:
			case 0xA8:
			case 0xAC:
			case 0xB0:
			case 0xB4:
			case 0xB8:
			case 0xBC:
				{
				auto channel = (reg & 0xF) / 4;
				hdmaSource[channel] = value;
				break;
				}
			case 0xC0: //HDMA Target
			case 0xC4:
			case 0xC8:
			case 0xCC:
			case 0xD0:
			case 0xD4:
			case 0xD8:
			case 0xDC:
				{
				auto channel = (reg & 0xF) / 4;
				hdmaTarget[channel] = value;
				break;
				}

			case 0x100: //Blitter function
				HandleBlitter(value);
				break;
			case 0x104: //Blitter address A
				blitAddrA = value;
				break;
			case 0x108: //Blitter address B
				blitAddrB =  value;
				break;
			case 0x10C: //Blitter length
				blitLength =  value;
				break;
		}
		return;
	}
	m68k_write_memory_8(address + 0, (value >> 24));
	m68k_write_memory_8(address + 1, (value >> 16));
	m68k_write_memory_8(address + 2, (value >> 8));
	m68k_write_memory_8(address + 3, (value >> 0));
}

void HandleBlitter(unsigned int function)
{
	auto fun = function & 0xF;
	switch (fun)
	{
		case 0: return;
		case 1: //Blit
		case 2: //Clear
		case 3: //Invert
			{
				auto strideSkip = ((function & 0x10) >> 4) == 1; //1 2 3
				auto colorKey = ((function & 0x20) >> 5) == 1; //1
				auto source4 = ((function & 0x40) >> 6) == 1; //1
				auto width = ((function & 0x60) >> 5); //2
				auto target4 = ((function & 0x80) >> 7) == 1; //1
				auto sourceStride = ((function >> 8) & 0xFFF); //1 2 3
				auto targetStride = ((function >> 20) & 0xFFF); //1 2 3

				auto read = m68k_read_memory_8;
				if (width == 1) read = m68k_read_memory_16;
				else if (width == 2) read = m68k_read_memory_32;
				auto write = m68k_write_memory_8;
				if (width == 1) write = m68k_write_memory_16;
				else if (width == 2) write = m68k_write_memory_32;

				if (fun == 1) //Blit
				{
					//throw new NotImplementedException();
				}
				else if (fun == 2) //Clear
				{
					/*
					Copies the value of ADDRESS A to B.
					If STRIDESKIP is enabled, copies SOURCE STRIDE bytes,
					then skips over TARGET STRIDE - SOURCE STRIDE bytes,
					until LENGTH bytes are copied in total.
					If WIDTH is set to 0, sets B to the low byte of the source value.
					If WIDTH is set to 1, sets B to the lower short instead.
					If WIDTH is set to 2, sets B to the full word.
					If WIDTH is set to 3, behavior is undefined.
					*/
					while (blitLength > 0)
					{
						if (strideSkip)
						{
							for (unsigned int i = 0; i < sourceStride && blitLength > 0; i++, blitAddrB += (1 << width), blitLength--)
								write(blitAddrB, blitAddrA);
							blitAddrB += (int)(targetStride - sourceStride) << width;
						}
						else
						{
							write(blitAddrB, read(blitAddrB));
							blitAddrB += (1 << width);
							blitLength--;
						}
					}
				}
				else if (fun == 3) //Invert
				{
					while (blitLength > 0)
					{
						if (strideSkip)
						{
							for (unsigned int i = 0; i < sourceStride && blitLength > 0; i++, blitAddrB++, blitLength--)
								m68k_write_memory_8(blitAddrB, ~m68k_read_memory_8(blitAddrB));
							blitAddrB += (int)(targetStride - sourceStride);
						}
						else
						{
							m68k_write_memory_8(blitAddrB, ~m68k_read_memory_8(blitAddrB));
							blitAddrB++;
							blitLength--;
						}
					}
				}
			}
			break;
		case 4: //UnRLE
			unsigned int i = 0;
			unsigned char rle = 0;
			char data = 0;
			while (i < blitLength)
			{
				rle = m68k_read_memory_8(blitAddrA++);
				rle++;
				data = m68k_read_memory_8(blitAddrA++);
				for (; rle > 0; rle--, i++)
					m68k_write_memory_8(blitAddrB++, data);
			}
			break;
	}
}

}