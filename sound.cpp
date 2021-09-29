#include "asspull.h"
#include <vector>
#include <Windows.h>

HMIDIOUT midiDevice = 0;

int programs[16] = { 0 };
std::vector<unsigned int> keyOns;

extern int pcmSource, pcmLength, pcmPlayed;
extern bool pcmRepeat;

static SDL_AudioDeviceID saDev;

extern "C"
{
	unsigned int m68k_read_memory_8(unsigned int address);
}

extern bool quit;

char *pcmStream = NULL;

void saCallback(void *userdata, unsigned char* stream, int len)
{
	if (quit)
		return;

	for (int i = 0; i < len; i++)
		stream[i] = 0;

	if (pcmStream != NULL)
	{
		len = (len > pcmPlayed ? pcmPlayed : len);
		for (int i = 0; i < len; i++)
		{
			stream[i] = pcmStream[pcmLength - pcmPlayed]; //m68k_read_memory_8(pcmSource + (pcmLength - pcmPlayed));
			pcmPlayed--;

			stream[i] /= 2; //to make room for the OPL stream
		}

		if (pcmPlayed <= 0)
		{
			if (pcmRepeat)
			{
				//Log(L"saCallback: starting over!");
				pcmPlayed = pcmLength;
			}
			else
			{
				free(pcmStream);
				pcmStream = NULL;
			}
		}
	}
}

void BufferAudioSample(signed char sample)
{
	sample;
}

int InitSound()
{
	UninitSound();

	if (ini.GetBoolValue(L"audio", L"music", true))
	{
		int devID = (int)ini.GetLongValue(L"audio", L"midiDevice", 0);
		auto res = midiOutOpen(&midiDevice, devID, NULL, NULL, 0);
		if (res == MMSYSERR_BADDEVICEID)
		{
			Log(L"Could not open MIDI device #%d: bad device ID.", devID);
			return 1; //Negative would mean to stop loading but who cares?
		}
		else if (res == MMSYSERR_BADDEVICEID)
		{
			Log(L"Could not open MIDI device #%d: device already allocated.", devID);
			return 2; //If we *do* fail to open a device, we'll just run silent, pffft.
		}

		if (midiDevice)
			midiOutReset(midiDevice);

		for (auto i = 0; i < 16; i++)
		{
			auto message = (0xC0 | i) | (programs[i] << 8);
			midiOutShortMsg(midiDevice, message);
		}
	}

	SDL_AudioSpec wanted = { 0 };
	wanted.format = AUDIO_S8;
	wanted.freq = 11025;
	wanted.channels = 1;
	wanted.samples = 4096;
	wanted.callback = saCallback;
	SDL_AudioSpec got = { 0 };

	saDev = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(1, 0), 0, &wanted, &got, 0);// SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
	SDL_PauseAudioDevice(saDev, 0);

	return 0;
}

void UninitSound()
{
	if (midiDevice)
	{
		for (auto kon = keyOns.begin(); kon != keyOns.end(); ++kon)
		{
			auto message = *kon & ~0x10; //turn it into a koff
			midiOutShortMsg(midiDevice, message);
		}
		keyOns.clear();

		midiOutReset(midiDevice);
		midiOutClose(midiDevice);
	}

	SDL_CloseAudio();
}

void SendMidi(unsigned int message)
{
	if ((message & 0x0000F0) == 0xC0)
	{
		programs[(message & 0x0F)] = (message >> 8) & 0xFF;
	}

	if (midiDevice == 0) return;
	
	midiOutShortMsg(midiDevice, message);
	if ((message & 0x0000F0) == 0x90)
	{
		keyOns.push_back(message);
	}
	else if ((message & 0x0000F0) == 0x80)
	{
		message |= 0x10; //turn it into a keyon
		for (auto kon = keyOns.begin(); kon != keyOns.end(); ++kon)
		{
			if (*kon == message)
			{
				keyOns.erase(kon);
				break;
			}
		}
	}
}
