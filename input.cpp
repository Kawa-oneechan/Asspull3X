#include "asspull.h"

/*
I/O CONTROLLER "FONDA"
----------------------
OUTPUTS
 * 0x0000: ID, 0x494F 'IO'
 * 0x0002: Keyboard buffer. Reading pops one byte value off the stack.
 * 0x0003: Keyboard shift states. & 1 for Shift, & 2 for Alt, & 4 for Ctrl.
 * 0x0010: Gamepad states. & 1 for digital-only P1, & 2 for analog P2,
 *         & 16 for digital P2, & 32 for analog P2.
 * 0x0012: P1 gamepad button state, part 1.
 * 0x0013: P1 gamepad button state, part 2.
 * 0x0014: P1 gamepad analog stick, axis 1.
 * 0x0015: P1 gamepad analog stick, axis 2.
 * 0x0016: P2 gamepad button state, part 1.
 * 0x0017: P2 gamepad button state, part 2.
 * 0x0018: P2 gamepad analog stick, axis 1.
 * 0x0019: P2 gamepad analog stick, axis 2.
 * 0x0020: Mouse state, 16 bits.
 * 0x0040: 256 bytes of live keyboard state.

INPUTS
 * 0x0000: Reset keyboard buffer.
 * 0x0004: MIDI out.
 * 0x0005: OPL3 out, 16 bits.
 * 0x0010: PCM 1 offset, 32 bits.
 * 0x0014: PCM 2 offset.
 * 0x0018: PCM 1 length and repeat, triggers playback.
 * 0x001C: PCM 2 length and repeat.
 * 0x0020: PCM 1 volume (TODO)

INTERRUPTS
 * VBlank: updates mouse state.
*/

InputOutputDevice* inputDev;
unsigned short joypad[2];
char joyaxes[2][2];

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

InputOutputDevice::InputOutputDevice()
{
	bufferCursor = 0;
	for (int i = 0; i < 32; i++)
		buffer[i] = 0;
	lastMouseX = lastMouseY = 0;
	mouseLatch = 0;
}

InputOutputDevice::~InputOutputDevice()
{
}

unsigned int InputOutputDevice::Read(unsigned int address)
{
	READ_DEVID(DEVID_INPUT);
	switch (address)
	{
	case 0x02: //Keyboard buffer
	{
		unsigned int key = 0;

		if (bufferCursor)
		{
			key = buffer[0];
			for (int i = 0; i < 31; i++)
				buffer[i] = buffer[i + 1];
			buffer[bufferCursor] = 0;
			bufferCursor--;
		}
		return key & 0xFF;
	}
	case 0x03: //Keyboard shifts
	{
		unsigned int key = 0;
		auto mods = SDL_GetModState();
		if (mods & KMOD_SHIFT) key |= 0x01;
		if (mods & KMOD_ALT) key |= 0x02;
		if (mods & KMOD_LCTRL) key |= 0x04;
		//if (mods & KMOD_RCTRL) key = 0; //reserved for the UI
		return key;
	}
	case 0x10: //Gamepad states
	{
		int num = SDL_NumJoysticks();
		unsigned char ret = 0x00;
		if (num == 0 && key2joy)
			ret = 0x01; //Digital-only pad via Key2Joy
		else if (num == 1)
			ret = 0x02; //TODO: is there a way to detect if there's a stick?
		if (num == 2)
			ret |= 0x20;
		return ret;
	}
	case 0x12: //Gamepad 1 digital 1
		return (joypad[0] >> 8) & 0xFF;
	case 0x13: //Gamepad 1 digital 2
		return (joypad[0] >> 0) & 0xFF;
	case 0x14: //Gamepad 1 analog 1
		return joyaxes[0][0];
	case 0x15: //Gamepad 1 analog 2
		return joyaxes[0][1];
	case 0x16: //Gamepad 2 digital 1
		return (joypad[1] >> 8) & 0xFF;
	case 0x17: //Gamepad 2 digital 2
		return (joypad[1] >> 0) & 0xFF;
	case 0x18: //Gamepad 2 analog 1
		return joyaxes[1][0];
	case 0x19: //Gamepad 2 analog 2
		return joyaxes[1][1];
	case 0x20: //Mouse
		return (mouseLatch >> 8) & 0xFF;
	case 0x21:
		return mouseLatch & 0xFF;
	}
	if (address >= 0x40 && address < 0x140)
	{
		bufferCursor = 0;
		const unsigned char *keys = ::SDL_GetKeyboardState(0);
		if (address - 0x40 == 77) return keys[SDL_SCANCODE_LSHIFT] | keys[SDL_SCANCODE_RSHIFT];
		if (address - 0x40 == 78) return keys[SDL_SCANCODE_LALT] | keys[SDL_SCANCODE_RALT];
		if (address - 0x40 == 79) return keys[SDL_SCANCODE_LCTRL];
		for (int i = 0; i < 100; i++)
			if (keyMap[i] == address - 0x40)
				return keys[i];
	}
	return 0;
}

void InputOutputDevice::Write(unsigned int address, unsigned int value)
{
	static unsigned int oplLatch = 0;
	static unsigned int pcmLatch = 0;
	switch (address)
	{
	case 0x00: //Reset keyboard buffer
	{
		for (int i = 0; i < 32; i++)
			buffer[i] = 0;
		bufferCursor = 0;
		return;
	}
	case 0x04: //MIDI Out
	{
		Sound::SendMidiByte(value);
		break;
	}
	case 0x05: //OPL3 out
	{
		oplLatch = value;
		break;
	}
	case 0x06:
	{
		Sound::SendOPL((oplLatch << 8) | value);
		oplLatch = 0;
		break;
	}
	case 0x10: //PCM Offset
	case 0x14:
	case 0x18: //PCM Length + Repeat
	case 0x1C:
	{
		pcmLatch = value << 24;
		break;
	}
	case 0x11:
	case 0x15:
	case 0x19:
	case 0x1D:
	{
		pcmLatch |= value << 16;
		break;
	}
	case 0x12:
	case 0x16:
	case 0x1A:
	case 0x1E:
	{
		pcmLatch |= value << 8;
		break;
	}
	case 0x13:
	case 0x17:
	{
		pcmLatch |= value << 0;
		Sound::pcmSource[address == 0x13 ? 0 : 1] = value;
		break;
	}
	case 0x1B:
	case 0x1F:
	{
		pcmLatch |= value << 0;
		auto channel = (address == 0x1B ? 0 : 1);
		Sound::pcmPlayed[channel] = Sound::pcmLength[channel] = value & 0x7FFFFFFF;
		Sound::pcmRepeat[channel] = (value & 0x80000000) != 0;
		break;
	}
	case 0x20: //PCM Volume
	case 0x21:
	case 0x22:
	case 0x23:
	{
		Sound::pcmVolume[address - 0x20] = value;
		break;
	}
	}
}

int InputOutputDevice::GetID() { return DEVID_INPUT; }

void InputOutputDevice::HBlank() {}

void InputOutputDevice::VBlank()
{
	if (!UI::mouseLocked)
	{
		lastMouseX = lastMouseY = 0;
		return;
	}

	POINT pos;

	int newX, newY, b;
	GetCursorPos(&pos);
	newX = pos.x;
	newY = pos.y;
	b = (!!GetAsyncKeyState(VK_LBUTTON)) | (!!GetAsyncKeyState(VK_RBUTTON) << 2);

	int x = newX - lastMouseX;
	int y = newY - lastMouseY;
	int dx = x < 0;
	int dy = y < 0;
	if (x < 0) x = -x;
	if (y < 0) y = -y;

	lastMouseX = newX;
	lastMouseY = newY;

	mouseLatch = ((b & 1) << 14) | ((b & 4) << 13) | (dy << 13) | (y << 7) | (dx << 6) | x;
}

void InputOutputDevice::Enqueue(SDL_Keysym sym)
{
	if (sym.scancode == SDL_SCANCODE_RCTRL) return;
	if (sym.scancode == SDL_SCANCODE_RALT) return;
	if (sym.scancode == SDL_SCANCODE_RSHIFT) return;
	if (sym.scancode == SDL_SCANCODE_LCTRL) return;
	if (sym.scancode == SDL_SCANCODE_LALT) return;
	if (sym.scancode == SDL_SCANCODE_LSHIFT) return;
	if (sym.scancode == SDL_SCANCODE_LGUI) return;
	if (sym.scancode == SDL_SCANCODE_RGUI) return;
	if (sym.scancode == SDL_SCANCODE_VOLUMEUP) return;
	if (sym.scancode == SDL_SCANCODE_VOLUMEDOWN) return;
	if (sym.scancode == SDL_SCANCODE_MUTE) return;

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
