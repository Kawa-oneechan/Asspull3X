#include "asspull.h"

#if WIN32
#include <Windows.h>

HMIDIOUT midiDevice = 0;

int InitSound()
{
	auto thing = ini->Get("media", "midiDevice", "0");
	int devID = (int)strtol(thing, NULL, 10);
	auto res = midiOutOpen(&midiDevice, devID, NULL, NULL, 0);
	if (res == MMSYSERR_BADDEVICEID)
	{
		SDL_Log("Could not open MIDI device #%d: bad device ID.", devID);
		return 1; //Negative would mean to stop loading but who cares?
	}
	else if (res == MMSYSERR_BADDEVICEID)
	{
		SDL_Log("Could not open MIDI device #%d: device already allocated.", devID);
		return 2; //If we *do* fail to open a device, we'll just run silent, pffft.
	}
	if (midiDevice)
		midiOutReset(midiDevice);
	return 0;
}

void UninitSound()
{
	if (midiDevice)
	{
		midiOutReset(midiDevice);
		midiOutClose(midiDevice);
	}
}

void SendMidi(unsigned int message)
{
	if (midiDevice)
		midiOutShortMsg(midiDevice, message);
}

#else

#pragma message("No MIDI support for non-Windows targets yet!")
int InitSound()
{
	return 0;
}

void UninitSound()
{
}

void SendMidi(unsigned int message)
{
}

#endif
