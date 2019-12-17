#include "asspull.h"

#if WIN32
#include <Windows.h>

#define AUDIOCHONK 1024

HMIDIOUT midiDevice = 0;
HWAVEOUT soundHandle = 0;
WAVEHDR waveHeader;

signed char buffer[AUDIOCHONK];
int audioCursor = 0;

void BufferAudioSample(signed char sample)
{
	buffer[audioCursor] = sample;
	audioCursor++;
	if (audioCursor == AUDIOCHONK)
	{
		audioCursor = 0;
		waveOutWrite(soundHandle, &waveHeader, sizeof(WAVEHDR));
	}
}

void CALLBACK waveOutCallBack(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{

}

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

	WAVEFORMATEX format = {};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 1;
	format.nSamplesPerSec = 11025;
	format.nBlockAlign = 1;
	format.wBitsPerSample = 8;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize = 0;
	res = waveOutOpen(&soundHandle, WAVE_MAPPER, &format, (DWORD_PTR)waveOutCallBack, 0, CALLBACK_FUNCTION);
	if (res != MMSYSERR_NOERROR)
	{
		SDL_Log("Could not open audio device: error %d", res);
		return 3;
	}
	waveHeader.lpData = (LPSTR)buffer;
	waveHeader.dwBufferLength = AUDIOCHONK;
	waveHeader.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
	res = waveOutPrepareHeader(soundHandle, &waveHeader, sizeof(WAVEHDR));
	waveOutSetVolume(soundHandle, -1);
	waveOutRestart(soundHandle);

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
