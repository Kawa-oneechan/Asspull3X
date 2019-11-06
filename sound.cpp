#include "asspull.h"

#if WIN32

#include <Windows.h>

int midiNum = 0;
HMIDIOUT midiDevice = 0;

int InitSound(int device)
{
	midiNum = device;
	midiOutOpen(&midiDevice, midiNum, NULL, NULL, 0);
	midiOutReset(midiDevice);
	return 0;
}

int UninitSound()
{
	midiOutReset(midiDevice);
	midiOutClose(midiDevice);
	return 0;
}

void SendMidi(unsigned int message)
{
	midiOutShortMsg(midiDevice, message);
}

#else

#pragma message("No MIDI support for non-Windows targets yet!")
int InitSound()
{
	return 0;
}

int UninitSound()
{
	return 0;
}

void SendMidi(unsigned int message)
{
}

#endif
