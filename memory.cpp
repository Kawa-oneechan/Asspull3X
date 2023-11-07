#include "asspull.h"
#include <time.h>

namespace Registers
{
	int Interrupts;

	ScreenModeRegister ScreenMode;
	MapSetRegister MapSet;
	MapBlendRegister MapBlend;
	DMAControlRegister DMAControl;
	HDMAControlRegister HDMAControls[8];
	BlitControlRegister BlitControls;
	BlitKeyRegister BlitKey;

	int WindowLeft, WindowRight, WindowMask;

	int Fade, Caret;
	int ScrollX[4], ScrollY[4];
	unsigned int MapTileShift = 0;

	long RTCOffset;
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
unsigned char* ramCartridge = NULL;
unsigned char* ramInternal = NULL;
unsigned char* ramVideo = NULL;

unsigned int biosSize = 0, romSize = 0;

void HandleBlitter();

long ticks = 0;
int dmaLines = 0;

extern int line;

void HandleHdma(int currentLine)
{
	for (auto i = 0; i < 8; i++)
	{
		if (!Registers::HDMAControls[i].Enable)
			continue;
		if ((unsigned int)currentLine < Registers::HDMAControls[i].Start)
			continue;
		if ((unsigned int)currentLine > Registers::HDMAControls[i].Start + Registers::HDMAControls[i].Count)
			continue;
		currentLine -= Registers::HDMAControls[i].Start;
		auto l = currentLine / (Registers::HDMAControls[i].DoubleScan ? 2 : 1);
		auto width = Registers::HDMAControls[i].Width;
		auto target = Registers::HDMAControls[i].Target;
		auto source = Registers::HDMAControls[i].Source;
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
	romBIOS = new unsigned char[BIOS_SIZE]();
	romCartridge = new unsigned char[CART_SIZE]();
	ramCartridge = new unsigned char[SRAM_SIZE]();
	ramInternal = new unsigned char[WRAM_SIZE]();
	ramVideo = new unsigned char[VRAM_SIZE]();
	return 0;
}

void ResetMemory()
{
	memset(ramInternal, 0, WRAM_SIZE);
	memset(ramVideo, 0, VRAM_SIZE);
	ticks = 0;
	Registers::Caret = 0;
	Registers::Fade = 0;
	Registers::MapBlend.Raw = 0;
	Registers::MapSet.Raw = 0;
	Registers::ScreenMode.Raw = 0;
	Registers::WindowLeft = Registers::WindowRight = Registers::WindowMask = 0;
	Registers::MapTileShift = 0;
	for (int i = 0; i < 8; i++)
	{
		Registers::HDMAControls[i].Raw = 0;
		Registers::HDMAControls[i].Source = 0;
		Registers::HDMAControls[i].Target = 0;
	}
	memset(Registers::ScrollX, 0, sizeof(int) * 4);
	memset(Registers::ScrollY, 0, sizeof(int) * 4);
	Sound::Reset();
}

unsigned int m68k_read_memory_8(unsigned int address)
{
	if (address >= REGS_ADDR && address < REGS_ADDR + REGS_SIZE)
	{
		auto reg = address & 0x000FFFFF;
		switch (reg)
		{
			case 0x00: //Interrupts
				return Registers::Interrupts;
			case 0x01: //Screen Mode
				return Registers::ScreenMode.Raw;
			case 0x08: //ScreenFade
				return Registers::Fade;
			case 0x09: //TilemapSet
				return Registers::MapSet.Raw;
			case 0x0B: //MapShift
				return Registers::MapTileShift;
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
			if (addr >= SRAM_ADDR)
				return ramCartridge[addr - SRAM_ADDR];
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
			addr &= (VRAM_SIZE - 1);
//			if (addr >= VRAM_SIZE)
//				return 0;
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
			case 0x60: //Time_T
				return (int)time(NULL) + Registers::RTCOffset;
			case 0x100: //DMA Source
				return Registers::DMAControl.Source;
			case 0x104: //DMA Target
				return Registers::DMAControl.Target;
			case 0x108: //DMA Length
				return Registers::DMAControl.Length;
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
				Registers::Interrupts = value;
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
			case 0x0B: //MapShift
				Registers::MapTileShift = value;
				break;
			case 0x10A: //DMA Control
			{
				/*
				if ((value & 1) == 0) return;
				auto increaseSource = ((value >> 1) & 1) == 1;
				auto increaseTarget = ((value >> 2) & 1) == 1;
				auto asValue = ((value >> 3) & 1) == 1;
				auto dataWidth = (value >> 4) & 3;
				*/
				Registers::DMAControl.Raw = value;
				auto width = Registers::DMAControl.Width;
				auto increaseStep = (width == 0) ? 1 : (width == 1) ? 2 : 4;
				dmaLines = Registers::DMAControl.Length >> (2 + width);
				//Log(L"DMA: $%08X to $%08X, length $%08X, %d wide, %s. %s %s.", dmaSource, dmaTarget, dmaLength, increaseStep, asValue ? L"memset" : L"memcpy", increaseSource ? L"s++" : L"   ", increaseTarget ? L"t++" : L"");
				while (Registers::DMAControl.Length > 0)
				{
					if (Registers::DMAControl.DirectValue)
					{
						if (width == 0)
							m68k_write_memory_8(Registers::DMAControl.Target, Registers::DMAControl.Source);
						else if (width == 1)
							m68k_write_memory_16(Registers::DMAControl.Target, Registers::DMAControl.Source);
						else if (width == 2)
							m68k_write_memory_32(Registers::DMAControl.Target, Registers::DMAControl.Source);
					}
					else
					{
						if (width == 0)
							m68k_write_memory_8(Registers::DMAControl.Target, m68k_read_memory_8(Registers::DMAControl.Source));
						else if (width == 1)
							m68k_write_memory_16(Registers::DMAControl.Target, m68k_read_memory_16(Registers::DMAControl.Source));
						else if (width == 2)
							m68k_write_memory_32(Registers::DMAControl.Target, m68k_read_memory_32(Registers::DMAControl.Source));
					}
					Registers::DMAControl.Length--;
					if (Registers::DMAControl.IncreaseSource)
						Registers::DMAControl.Source += increaseStep;
					if (Registers::DMAControl.IncreaseTarget)
						Registers::DMAControl.Target += increaseStep;
				}
				break;
			}
			case 0x210: //Blitter key
				Registers::BlitKey.Color = value;
				break;
		}
		return;
	}
	auto bank = (address & 0x0F000000) >> 24;
	auto addr = address & 0x00FFFFFF;
	switch (bank)
	{
		case 0x0:
			if (addr >= SRAM_ADDR)
				ramCartridge[addr - SRAM_ADDR] = (unsigned char)value;
			break;
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
			addr &= (VRAM_SIZE - 1);
//			if (addr >= VRAM_SIZE)
//				break;
			ramVideo[addr] = (unsigned char)value;
			break;
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
				Registers::DMAControl.Source = value;
				break;
			case 0x104: //DMA Target
				Registers::DMAControl.Target = value;
				break;
			case 0x108: //DMA Length
				Registers::DMAControl.Length = value;
				break;
			case 0x60: //Time_T
				Registers::RTCOffset = (long)(value - time(NULL));
				{
					ini.SetLongValue(L"misc", L"rtcOffset", Registers::RTCOffset);
					UI::SaveINI();
				}
				break;
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
				Registers::HDMAControls[channel].Raw = value;
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
				Registers::HDMAControls[channel].Source = value;
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
				Registers::HDMAControls[channel].Target = value;
				break;
			}
			case 0x200: //Blitter function
				Registers::BlitControls.Raw = value;
				HandleBlitter();
				break;
			case 0x204: //Blitter address A
				Registers::BlitControls.Source = value;
				break;
			case 0x208: //Blitter address B
				Registers::BlitControls.Target = value;
				break;
			case 0x20C: //Blitter length
				Registers::BlitControls.Length = value;
				break;
			case 0x210: //Blitter key
				Registers::BlitKey.Raw = value;
				break;
		}
		return;
	}
	m68k_write_memory_8(address + 0, (value >> 24));
	m68k_write_memory_8(address + 1, (value >> 16));
	m68k_write_memory_8(address + 2, (value >> 8));
	m68k_write_memory_8(address + 3, (value >> 0));
}

void HandleBlitter()
{
	if (Registers::BlitControls.Function == Registers::BlitFunctions::None)
		return;

	auto width = (Registers::BlitControls.Function == 1) ? 0 : Registers::BlitControls.Width;

	auto read = m68k_read_memory_8;
	if (width == 1) read = m68k_read_memory_16;
	else if (width == 2) read = m68k_read_memory_32;
	auto write = m68k_write_memory_8;
	if (width == 1) write = m68k_write_memory_16;
	else if (width == 2) write = m68k_write_memory_32;

	int val = 0;
	int striding = 0;

	while (Registers::BlitControls.Length > 0)
	{
		switch (Registers::BlitControls.Function)
		{
		case Registers::BlitFunctions::None:
		{
			break;
		}
		case Registers::BlitFunctions::Copy:
		{
			val = read(Registers::BlitControls.Source);

			if (Registers::BlitControls.Source4bit)
			{
				if (Registers::BlitControls.Target4bit)
				{
					//4-to-4
					auto old = read(Registers::BlitControls.Target);

					auto ai = (val >> 0) & 0x0F;
					auto bi = (val >> 4) & 0x0F;
					auto ao = (old >> 0) & 0x0F;
					auto bo = (old >> 4) & 0x0F;
					if (!(Registers::BlitControls.Key && ai == Registers::BlitKey.Color)) ao = ai;
					if (!(Registers::BlitControls.Key && bi == Registers::BlitKey.Color)) bo = bi;
					val = ao | (bo << 4);
					write(Registers::BlitControls.Target, val);
				}
				else
				{
					//4-to-8
					auto a = (val >> 0) & 0x0F;
					auto b = (val >> 4) & 0x0F;
					if (!(Registers::BlitControls.Key && a == Registers::BlitKey.Color))
						write(Registers::BlitControls.Target, a | (Registers::BlitKey.Palette << 4));
					Registers::BlitControls.Target += (1 << width);
					if (!(Registers::BlitControls.Key && b == Registers::BlitKey.Color))
						write(Registers::BlitControls.Target, b | (Registers::BlitKey.Palette << 4));
					if (Registers::BlitControls.StrideSkip) striding++;
				}
			}
			else
			{
				//8-to-8
				if (!(Registers::BlitControls.Key && val == Registers::BlitKey.Color))
					write(Registers::BlitControls.Target, val);

				//there is no 8-to-4 yet.
			}

			Registers::BlitControls.Source += (1 << width);
			Registers::BlitControls.Target += (1 << width);
			break;
		}
		case Registers::BlitFunctions::Set:
		{
			write(Registers::BlitControls.Target, Registers::BlitControls.Source);
			Registers::BlitControls.Target += (1 << width);
			break;
		}
		case Registers::BlitFunctions::Invert:
		{
			m68k_write_memory_8(Registers::BlitControls.Target, ~m68k_read_memory_8(Registers::BlitControls.Target));
			Registers::BlitControls.Target++;
			break;
		}
		}

		if (Registers::BlitControls.StrideSkip)
		{
			striding++;
			if (striding == Registers::BlitControls.SourceStride)
			{
				Registers::BlitControls.Target += (int)(Registers::BlitControls.TargetStride - Registers::BlitControls.SourceStride);
				striding = 0;
			}
		}

		Registers::BlitControls.Length--;
	}

	Registers::BlitControls.Raw = 0;
}
