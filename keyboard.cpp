#include "asspull.h"

InputDevice* inputDev;

#define POLLDELAY 10

static unsigned int lastPollTime = 0;
static int lastPollResult = 0;

const unsigned char keyMap[] =
{
	0x00,
	0x00,
	0x00,
	0x00,
	0x1E, //a
	0x30, //b
	0x2E, //c
	0x20, //d
	0x12, //e
	0x21, //f
	0x22, //g
	0x23, //h
	0x17, //i
	0x24, //j
	0x25, //k
	0x26, //l
	0x32, //m
	0x31, //n
	0x18, //o
	0x19, //p
	0x10, //q
	0x13, //r
	0x1F, //s
	0x14, //t
	0x16, //u
	0x2F, //v
	0x11, //w
	0x2D, //x
	0x15, //y
	0x2C, //z
	0x02, //1
	0x03, //2
	0x04, //3
	0x05, //4
	0x06, //5
	0x07, //6
	0x08, //7
	0x09, //8
	0x0A, //9
	0x0B, //0
	0x1C, //enter
	0x01, //escape
	0x0E, //backspace
	0x0F, //tab
	0x39, //space
	0x0C, //-
	0x0D, //=
	0x1A, //[
	0x1B, //]
	0x2B, //backslash
	0x00,
	0x27, //;
	0x28, //'
	0x29, //`
	0x33, //,
	0x34, //.
	0x35, //slash
	0x3A, //caps
	0x3B, //f1
	0x3C, //f2
	0x3D, //f3
	0x3E, //f4
	0x3F, //f5
	0x40, //f6
	0x41, //f7
	0x42, //f8
	0x43, //f9
	0x44, //f10
	0x57, //f11
	0x58, //f12
	0x00, //prtscrn
	0x46, //scrollock
	0x00, //pause
	0xD2, //ins
	0xC7, //home
	0xC9, //pgup
	0xD3, //del
	0xCF, //end
	0xD1, //pgdn
	0xCD, //right
	0xCB, //left
	0xD0, //down
	0xC8, //up
	0x45, //numlock
	0xB5, //kp/
	0x37, //kp*
	0x4A, //kp-
	0x4E, //kp+
	0x9C, //kpenter
	0x4F, //kp1
	0x50, //kp2
	0x51, //kp3
	0x4B, //kp4
	0x4C, //kp5
	0x4D, //kp6
	0x47, //kp7
	0x48, //kp8
	0x49, //kp9
	0x52, //kp0
	0x53, //kp.
};

unsigned int PollKeyboard(bool force)
{
	auto newPollTime = SDL_GetTicks();
	if (!force && newPollTime < lastPollTime + POLLDELAY)
		return lastPollResult;
	SDL_PumpEvents();
	const unsigned char *keys = SDL_GetKeyboardState(0);
	lastPollResult = 0;
	for (auto i = 0; i < 100; i++)
	{
		if (keys[i] == 1)
		{
			lastPollResult = keyMap[i];
			break;
		}
	}
	auto mods = SDL_GetModState();
	if (mods & KMOD_SHIFT) lastPollResult |= 0x100;
	if (mods & KMOD_ALT) lastPollResult |= 0x200;
	if (mods & KMOD_LCTRL) lastPollResult |= 0x400;
	if (mods & KMOD_RCTRL) lastPollResult = 0; //reserved for the UI
	lastPollTime = newPollTime;
	return lastPollResult;
}


InputDevice::InputDevice()
{
	bufferCursor = 0;
	for (int i = 0; i < 32; i++)
		buffer[i] = 0;
}

InputDevice::~InputDevice()
{
}

unsigned int InputDevice::Read(unsigned int address)
{
	switch (address)
	{
	case 0x00: return 0x49;
	case 0x01: return 0x4F;
	case 0x02:
	case 0x03:
	{
		unsigned int key = 0;

		if (bufferCursor)
		{
			key = buffer[0];
			if (address == 0x03)
			{
				for (int i = 0; i < 31; i++)
					buffer[i] = buffer[i + 1];
				buffer[bufferCursor] = 0;
				bufferCursor--;
			}
		}

		if (address == 0x02) return key >> 8;
		else return key & 0xFF;
	}
	}
	if (address >= 0x10 && address < 0x110)
	{
		const unsigned char *keys = ::SDL_GetKeyboardState(0);
		return keys[address - 0x10];
	}
	return 0;
}

void InputDevice::Write(unsigned int address, unsigned int value)
{
	return;
}

int InputDevice::GetID() { return 0x494F; }

void InputDevice::Enqueue(SDL_Keysym sym)
{
	if (sym.scancode == SDL_SCANCODE_RCTRL) return;
	if (sym.scancode == SDL_SCANCODE_RALT) return;
	if (sym.scancode == SDL_SCANCODE_RSHIFT) return;
	if (sym.scancode == SDL_SCANCODE_LCTRL) return;
	if (sym.scancode == SDL_SCANCODE_LALT) return;
	if (sym.scancode == SDL_SCANCODE_LSHIFT) return;

	if (bufferCursor == 32)
	{
		wprintf(L"\x07");
		return;
	}

	unsigned int key = keyMap[sym.scancode];
	if (sym.mod & KMOD_SHIFT) key |= 0x100;
	if (sym.mod & KMOD_ALT) key |= 0x200;
	if (sym.mod & KMOD_LCTRL) key |= 0x400;
	if (sym.mod & KMOD_RCTRL) key = 0; //reserved for the UI
	buffer[bufferCursor++] = key;
}
