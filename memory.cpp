#include "asspull.h"
#include <time.h>

namespace Registers
{
	ScreenModeRegister ScreenMode;
	MapSetRegister MapSet;
	MapBlendRegister MapBlend;
	
	int WindowLeft, WindowRight, WindowMask;

	int Fade, Caret;
	int ScrollX[4], ScrollY[4];
}

extern "C"
{
unsigned int m68k_read_memory_8(unsigned int address);
unsigned int m68k_read_memory_16(unsigned int address);
unsigned int m68k_read_memory_32(unsigned int address);
void m68k_write_memory_8(unsigned int address, unsigned int value);
void m68k_write_memory_16(unsigned int address, unsigned int value);
void m68k_write_memory_32(unsigned int address, unsigned int value);
}

unsigned char* romBIOS = NULL;
unsigned char* romCartridge = NULL;
unsigned char* ramInternal = NULL;
unsigned char* ramVideo = NULL;

unsigned int biosSize = 0, romSize = 0;

int dmaSource, dmaTarget;
unsigned int dmaLength;
bool hdmaOn[8], hdmaDouble[8];
int hdmaSource[8], hdmaTarget[8], hdmaWidth[8], hdmaStart[8], hdmaCount[8];

void HandleBlitter(unsigned int function);
unsigned int blitLength;
int blitAddrA, blitAddrB, blitKey;

long ticks = 0;
time_t timelatch, timesetlatch;
long rtcOffset = 0;

extern int scale, offsetX, offsetY, statusBarHeight;

extern int line, interrupts;

void HandleHdma(int currentLine)
{
	for (auto i = 0; i < 8; i++)
	{
		if (!hdmaOn[i])
			continue;
		if (currentLine < hdmaStart[i])
			continue;
		if (currentLine > hdmaStart[i] + hdmaCount[i])
			continue;
		currentLine -= hdmaStart[i];
		auto l = currentLine / (hdmaDouble[i] ? 2 : 1);
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
	if ((romBIOS = new unsigned char[BIOS_SIZE]()) == NULL) return -1;
	if ((romCartridge = new unsigned char[CART_SIZE]()) == NULL) return -1;
	if ((ramInternal = new unsigned char[WRAM_SIZE]()) == NULL) return -1;
	if ((ramVideo = new unsigned char[VRAM_SIZE]()) == NULL) return -1;
	return 0;
}

void ResetMemory()
{
	memset(ramInternal, 0, WRAM_SIZE);
	memset(ramVideo, 0, VRAM_SIZE);
	memset(hdmaOn, 0, sizeof(bool) * 8);
	ticks = 0;
	Registers::Caret = 0;
	Registers::Fade = 0;
	Registers::MapBlend.Raw = 0;
	Registers::MapSet.Raw = 0;
	Registers::ScreenMode.Raw = 0;
	Registers::WindowLeft = Registers::WindowRight = Registers::WindowMask = 0;
	memset(Registers::ScrollX, 0, sizeof(int) * 4);
	memset(Registers::ScrollY, 0, sizeof(int) * 4);
	Sound::Reset();
}

unsigned int m68k_read_memory_8(unsigned int address)
{
	if (address >= REGS_ADDR && address < REGS_ADDR + REGS_SIZE)
	{
		auto reg = address & REGS_SIZE;
		switch (reg)
		{
			case 0x00: //Interrupts
				return interrupts;
			case 0x01: //Screen Mode
				return Registers::ScreenMode.Raw;
			case 0x08: //ScreenFade
				return Registers::Fade;
			case 0x09: //TilemapSet
				return Registers::MapSet.Raw;
		}
		return 0;
	}
	auto bank = (address & 0x0F000000) >> 24;
	auto addr = address & 0x00FFFFFF;
	switch (bank)
	{
		case 0x0:
			if (addr < BIOS_SIZE)
				return romBIOS[addr & (biosSize - 1)];
			return romCartridge[(addr - CART_ADDR) & (romSize - 1)];
		case 0x1:
			return ramInternal[addr & (WRAM_SIZE - 1)];
		case 0x2: //DEV
		{
			auto devnum = (addr / DEVBLOCK) % MAXDEVS;
			if (devices[devnum] != NULL)
				return devices[devnum]->Read(addr % DEVBLOCK);
			return 0;
		}
		case 0xE:
			addr &= 0x7FFFF;
			if (addr >= VRAM_SIZE)
				return 0;
			return ramVideo[addr];
	}
	return 0;
}

unsigned int m68k_read_memory_16(unsigned int address)
{
	if (address >= REGS_ADDR && address < REGS_ADDR + REGS_SIZE)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x02: //Line
				return line;
			case 0x10: //Horizontal scroll
			case 0x14:
			case 0x18:
			case 0x1C:
				return Registers::ScrollX[min((reg - 0x10) / 4, 4)];
			case 0x12: //Vertical scroll
			case 0x16:
			case 0x1A:
			case 0x1E:
				return Registers::ScrollY[min((reg - 0x12) / 4, 4)];
			case 0x20:
				return Registers::WindowMask;
			case 0x22:
				return Registers::WindowLeft;
			case 0x24:
				return Registers::WindowRight;
			case 0x54:
				return Registers::Caret;
		}
		return 0;
	}
	auto ret = m68k_read_memory_8(address) * 0x100;
	ret += m68k_read_memory_8(address + 1);
	return ret;
}

unsigned int m68k_read_memory_32(unsigned int address)
{
	if (address >= REGS_ADDR && address < REGS_ADDR + REGS_SIZE)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x04: //Ticks
				return (int)ticks;
			case 0x60: //Time_T (top half)
				timelatch = (rtcOffset == 0xDEADC70C) ? 0 : (time(NULL) + rtcOffset);
				return (int)(timelatch >> 32);
			case 0x64: //Time_T (bottom half)
				return (int)timelatch;
			case 0x100: //DMA Source
				return dmaSource;
			case 0x104: //DMA Target
				return dmaTarget;
			case 0x108: //DMA Length
				return dmaLength;
			//TODO: HDMA. Fun!
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
	if (address >= REGS_ADDR && address < REGS_ADDR + REGS_SIZE)
	{
		auto reg = address & 0x000FFFFF;
		auto u8 = (unsigned char)value;
		switch (reg)
		{
			case 0x00: //Interrupts
				interrupts = value;
				break;
			case 0x01: //ScreenMode
				Registers::ScreenMode.Raw = u8;
				break;
			case 0x08: //ScreenFade
				Registers::Fade = u8;
				break;
			case 0x09: //TilemapSet
				Registers::MapSet.Raw = value;
				break;
			case 0x0A: //TilemapBlend
				Registers::MapBlend.Raw = value;
				break;
			case 0x44: //MIDI Out
				Sound::SendMidiByte(value);
				break;
			case 0x80: //PCM Volume
			case 0x81:
			case 0x82:
			case 0x83:
			{
				Sound::pcmVolume[reg - 0x80] = value;
				break;
			}
			case 0x10A: //DMA Control
			{
				if ((value & 1) == 0) return;
				auto increaseSource = ((value >> 1) & 1) == 1;
				auto increaseTarget = ((value >> 2) & 1) == 1;
				auto asValue = ((value >> 3) & 1) == 1;
				auto dataWidth = (value >> 4) & 3;
				auto increaseStep = (dataWidth == 0) ? 1 : (dataWidth == 1) ? 2 : 4;
				if (asValue && dataWidth == 2 && dmaTarget >= 0x1000000 && dmaTarget < 0xA000000)
				{
					//Do it quicker!
					//TODO: allow this for all long-sized transfers that aren't into register space?
					//Log("DMA: detected a long-width memset into WRAM.");
					memset(&ramInternal[dmaTarget - 0x1000000], dmaSource, dmaLength);
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
			case 0x210: //Blitter key
				blitKey = value;
				break;
		}
		return;
	}
	auto bank = (address & 0x0F000000) >> 24;
	auto addr = address & 0x00FFFFFF;
	switch (bank)
	{
		case 0x0: /* BIOS is ROM */ break;
		case 0x1:
			if (addr < WRAM_SIZE)
				ramInternal[addr & (WRAM_SIZE - 1)] = (unsigned char)value;
			break;
		case 0x2: //DEV
			{
				auto devnum = (addr / DEVBLOCK) % MAXDEVS;
				if (devices[devnum] != NULL)
					devices[devnum]->Write(addr % DEVBLOCK, value);
			}
			break;
		case 0xE:
			addr &= 0x7FFFF;
			if (addr >= VRAM_SIZE)
				break;
			ramVideo[addr] = (unsigned char)value; break;
	}
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	if (address >= REGS_ADDR && address < REGS_ADDR + REGS_SIZE)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x10: //Horizontal scroll
			case 0x14:
			case 0x18:
			case 0x1C:
				Registers::ScrollX[(reg - 0x10) / 4] = value & 511;
				break;
			case 0x12: //Vertical scroll
			case 0x16:
			case 0x1A:
			case 0x1E:
				Registers::ScrollY[(reg - 0x12) / 4] = value & 511;
				break;
			case 0x20:
				Registers::WindowMask = value;
				break;
			case 0x22:
				Registers::WindowLeft = value;
				break;
			case 0x24:
				Registers::WindowRight = value;
				break;
			case 0x48: //OPL3 out
				Sound::SendOPL(value);
				break;
			case 0x54:
				Registers::Caret = value;
				break;
		}
		return;
	}
	m68k_write_memory_8(address + 0, (value >> 8));
	m68k_write_memory_8(address + 1, (value >> 0));
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	if (address >= REGS_ADDR && address < REGS_ADDR + REGS_SIZE)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x100: //DMA Source
				dmaSource = value;
				break;
			case 0x104: //DMA Target
				dmaTarget = value;
				break;
			case 0x108: //DMA Length
				dmaLength = value;
				break;
			case 0x60: //Time_T (top half)
				timelatch = time(NULL);
				if ((signed int)value == -1)
					value = 0;
				timesetlatch = value;
				break;
			case 0x64: //Time_T (bottom half)
				timesetlatch <<= 32;
				timesetlatch |= value;
				rtcOffset = (long)(timesetlatch - timelatch);
				{
					ini.SetLongValue(L"misc", L"rtcOffset", rtcOffset);
					UI::SaveINI();
				}
				break;
			case 0x70: //PCM Offset
			case 0x74:
			{
				auto channel = (reg - 0x70) / 4;
				Sound::pcmSource[channel] = value;
				break;
			}
			case 0x78: //PCM Length + Repeat
			case 0x7C:
			{
				auto channel = (reg - 0x78) / 4;
				Sound::pcmPlayed[channel] = Sound::pcmLength[channel] = value & 0x7FFFFFFF;
				Sound::pcmRepeat[channel] = (value & 0x80000000) != 0;
				break;
			}
			case 0x180: //HDMA Control
			case 0x184:
			case 0x188:
			case 0x18C:
			case 0x190:
			case 0x194:
			case 0x198:
			case 0x19C:
			{
				auto channel = (reg & 0xF) / 4;
				hdmaWidth[channel] = (value >> 4) & 3;
				hdmaDouble[channel] = ((value >> 7) & 1) == 1;
				hdmaOn[channel] = (value & 1) == 1;
				hdmaStart[channel] = (value >> 8) & 0x3FF;
				hdmaCount[channel] = (value >> 20) & 0x3FF;
				break;
			}
			case 0x1A0: //HDMA Source
			case 0x1A4:
			case 0x1A8:
			case 0x1AC:
			case 0x1B0:
			case 0x1B4:
			case 0x1B8:
			case 0x1BC:
			{
				auto channel = (reg & 0xF) / 4;
				hdmaSource[channel] = value;
				break;
			}
			case 0x1C0: //HDMA Target
			case 0x1C4:
			case 0x1C8:
			case 0x1CC:
			case 0x1D0:
			case 0x1D4:
			case 0x1D8:
			case 0x1DC:
			{
				auto channel = (reg & 0xF) / 4;
				hdmaTarget[channel] = value;
				break;
			}
			case 0x200: //Blitter function
				HandleBlitter(value);
				break;
			case 0x204: //Blitter address A
				blitAddrA = value;
				break;
			case 0x208: //Blitter address B
				blitAddrB = value;
				break;
			case 0x20C: //Blitter length
				blitLength = value;
				break;
			case 0x210: //Blitter key
				blitKey = value;
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
	case 2: //Set
	case 3: //Invert
	{
		auto strideSkip = ((function & 0x10) >> 4) == 1;
		auto colorKey = ((function & 0x20) >> 5) == 1;
		auto fourBitSource = ((function & 0x40) >> 6) == 1;
		auto fourBitTarget = ((function & 0x80) >> 7) == 1;
		auto width = ((function & 0x80) >> 6) & 4;
		auto sourceStride = ((function >> 8) & 0xFFF);
		auto targetStride = ((function >> 20) & 0xFFF);

		auto read = m68k_read_memory_8;
		if (width == 1) read = m68k_read_memory_16;
		else if (width == 2) read = m68k_read_memory_32;
		auto write = m68k_write_memory_8;
		if (width == 1) write = m68k_write_memory_16;
		else if (width == 2) write = m68k_write_memory_32;

		int val = 0;
		int striding = 0;

		while (blitLength > 0)
		{
			if (fun == 1) //Blit
			{
				val = read(blitAddrA);

				if (!fourBitSource && !fourBitTarget)
				{
					if (!(colorKey && val == blitKey))
						write(blitAddrB, val);
				}
				else if (fourBitSource && fourBitTarget)
				{
					auto old = read(blitAddrB);

					auto ai = (val >> 0) & 0x0F;
					auto bi = (val >> 4) & 0x0F;
					auto ao = (old >> 0) & 0x0F;
					auto bo = (old >> 4) & 0x0F;
					if (!(colorKey && ai == blitKey)) ao = ai;
					if (!(colorKey && bi == blitKey)) bo = bi;
					val = ao | (bo << 4);
					write(blitAddrB, val);
				}

				blitAddrA += (1 << width);
				blitAddrB += (1 << width);
			}
			else if (fun == 2) //Set
			{
				write(blitAddrB, blitAddrA);
				blitAddrB += (1 << width);
			}
			else if (fun == 3) //Invert
			{
				m68k_write_memory_8(blitAddrB, ~m68k_read_memory_8(blitAddrB));
				blitAddrB++;
			}

			if (strideSkip)
			{
				striding++;
				if (striding == sourceStride)
				{
					blitAddrB += (int)(targetStride - sourceStride);
					striding = 0;
				}
			}

			blitLength--;
		}
	}
	break;
	}
}
