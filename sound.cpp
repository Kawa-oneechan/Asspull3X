#include "asspull.h"
#include <vector>

#if _MSC_VER
#include <Windows.h>

#define LATENCY 64
#define BLOCKCOUNT 8

HMIDIOUT midiDevice = 0;
HWAVEOUT soundHandle = 0;
std::vector<WAVEHDR> headers;
int frameCount = 0;
int blockCount = 0;
int frameIndex = 0;
int blockIndex = 0;

void BufferAudioSample(signed char sample)
{
	auto block = (signed char*)headers[blockIndex].lpData;
	block[frameIndex] = (unsigned char)((int)sample + 128);
	if(++frameIndex >= frameCount)
	{
		frameIndex = 0;
		while(true)
		{
			auto result = waveOutWrite(soundHandle, &headers[blockIndex], sizeof(WAVEHDR));
			if(result != WAVERR_STILLPLAYING)
				break;
		}
		if(++blockIndex >= blockCount)
			blockIndex = 0;
    }
}

int InitSound()
{
	int devID = (int)ini.GetLongValue("media", "midiDevice", 0);
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
	res = waveOutOpen(&soundHandle, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
	if (res != MMSYSERR_NOERROR)
	{
		SDL_Log("Could not open audio device: error %d", res);
		return 3;
	}

    frameCount = LATENCY;
    blockCount = BLOCKCOUNT;
    frameIndex = 0;
    blockIndex = 0;

    headers.resize(blockCount);
    for(int i = 0; i < blockCount; i++)
	{
		auto& header = headers[i];
		memset((void*)&header, 0, sizeof(WAVEHDR));
		header.lpData = (LPSTR)LocalAlloc(LMEM_FIXED, frameCount * 1);
		header.dwBufferLength = frameCount * 1;
		waveOutPrepareHeader(soundHandle, &header, sizeof(WAVEHDR));
	}

	waveOutSetVolume(soundHandle, 0x80008000);
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

#pragma message("No sound support for non-Windows targets yet!")
void BufferAudioSample(signed char sample)
{
}


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
