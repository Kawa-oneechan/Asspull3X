#include "asspull.h"
#include <vector>
#include <Windows.h>
#include "opl3.h"

#define FREQUENCY 44100
#define FORMAT AUDIO_S16MSB

extern "C" unsigned int m68k_read_memory_8(unsigned int address);

namespace Sound
{

	HMIDIOUT midiDevice = 0;

	int programs[16] = { 0 };
	std::vector<unsigned int> keyOns;

	int pcmSource[2], pcmLength[2], pcmPlayed[2], pcmVolume[4];
	bool pcmRepeat[2];

	static SDL_AudioDeviceID saDev;

	opl3_chip opl3 = { 0 };
	short mixStream[2048];

	void saCallback(void *userdata, unsigned char* stream, int len)
	{
		signed short* str = (signed short*)stream;
		len /= 2;

		for (int i = 0; i < len; i++)
			str[i] = 0;

		if (pauseState != 0)
			return;

		short samples[4] = { 0 };

		for (int i = 0; i < len; i += 8)
		{
			for (int channel = 0; channel < 2; channel++)
			{
				int l = channel * 2;
				int r = l + 1;

				if (pcmSource[channel] != NULL && pcmLength[channel] != 0)
				{
					if (pcmPlayed[channel])
					{
						samples[r] = samples[l] = (signed short)m68k_read_memory_8(pcmSource[channel] + (pcmLength[channel] - pcmPlayed[channel])) - 128;
						pcmPlayed[channel]--;
					}
				}

				if (pcmPlayed[channel] <= 0)
				{
					if (pcmRepeat[channel])
					{
						//Log(L"saCallback: starting over!");
						pcmPlayed[channel] = pcmLength[channel];
					}
					else
					{
						pcmSource[channel] = NULL;
						pcmLength[channel] = 0;
					}
				}

				samples[l] = (short)((float)samples[l] * ((float)pcmVolume[l] / 256.0f));
				samples[r] = (short)((float)samples[r] * ((float)pcmVolume[r] / 256.0f));
			}

			if (samples[0] < 0 && samples[2] < 0)
				mixStream[i + 0] = (samples[0] + samples[2]) - ((samples[0] * samples[2]) / INT8_MIN);
			else if (samples[0] > 0 && samples[2] > 0)
				mixStream[i + 0] = (samples[0] + samples[2]) - ((samples[0] * samples[2]) / INT8_MAX);
			else
				mixStream[i + 0] = samples[0] + samples[2];

			if (samples[1] < 0 && samples[3] < 0)
				mixStream[i + 1] = (samples[1] + samples[3]) - ((samples[1] * samples[3]) / INT8_MIN);
			else if (samples[1] > 0 && samples[3] > 0)
				mixStream[i + 1] = (samples[1] + samples[3]) - ((samples[1] * samples[3]) / INT8_MAX);
			else
				mixStream[i + 1] = samples[1] + samples[3];

			mixStream[i + 2] = mixStream[i + 0];
			mixStream[i + 3] = mixStream[i + 1];
			mixStream[i + 4] = mixStream[i + 0];
			mixStream[i + 5] = mixStream[i + 1];
			mixStream[i + 6] = mixStream[i + 0];
			mixStream[i + 7] = mixStream[i + 1];
		}

		SDL_MixAudioFormat(stream, (unsigned char*)mixStream, FORMAT, len * 2, SDL_MIX_MAXVOLUME / 4);

		if (opl3.rateratio != 0)
		{
			OPL3_GenerateStream(&opl3, mixStream, len / 2);
			for (int i = 0; i < len; i++)
				mixStream[i] = ((mixStream[i] & 0xFF00) >> 8) | ((mixStream[i] & 0xFF) << 8);
			SDL_MixAudioFormat(stream, (unsigned char*)mixStream, FORMAT, len * 2, SDL_MIX_MAXVOLUME * 2);
		}
	}

	int Initialize()
	{
		Shutdown();

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

		if (ini.GetBoolValue(L"audio", L"sound", true))
		{
			SDL_AudioSpec wanted = { 0 };
			wanted.format = FORMAT;
			wanted.freq = FREQUENCY;
			wanted.channels = 2;
			wanted.samples = 1024; //4096;
			wanted.callback = saCallback;
			SDL_AudioSpec got = { 0 };

			//TODO: extra setting?
			saDev = SDL_OpenAudioDevice(0, 0, &wanted, &got, 0);// SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
			SDL_PauseAudioDevice(saDev, 0);
		}

		OPL3_Reset(&opl3, FREQUENCY);

		return 0;
	}

	void Shutdown()
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
			midiDevice = NULL;
		}
		if (saDev)
		{
			SDL_CloseAudioDevice(saDev);
			saDev = NULL;
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

#pragma region MPU-401 stuff
	//Cribbed wholesale from DOSBox-X. Thanks~

#define SYSEX_SIZE 8192
	unsigned char MidiStatus = 0;
	unsigned char MidiCommandLength = 0;
	unsigned char MidiCommandPosition = 0;
	unsigned char MidiCommandBuffer[8] = { 0 };
	unsigned char MidiRealtimeBuffer[8] = { 0 };
	unsigned char MidiSysExBuffer[SYSEX_SIZE] = { 0 };
	unsigned int MidiSysExUsed = 0;
	unsigned int MidiSysExDelay = 0;
	unsigned long long MidiSysExStart = 0;

	unsigned const char MidiEventLengths[256] = {
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x10
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30

		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x40
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x70

		3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x80
		3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x90
		3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xA0
		3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xB0

		2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xC0
		2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xD0
		3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xE0
		0,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,0   // 0xF0
	};

	void SendSysEx(unsigned char* payload, int length)
	{
		MIDIHDR mHdr = { 0 };
		midiOutUnprepareHeader(midiDevice, &mHdr, sizeof(mHdr));

		mHdr.lpData = (char*)payload;
		mHdr.dwBufferLength = (DWORD)length;
		mHdr.dwBytesRecorded = (DWORD)length;
		mHdr.dwUser = 0;

		MMRESULT result = midiOutPrepareHeader(midiDevice, &mHdr, sizeof(mHdr));
		if (result != MMSYSERR_NOERROR) return;
		result = midiOutLongMsg(midiDevice, &mHdr, sizeof(mHdr));
		if (result != MMSYSERR_NOERROR)
			return;
	}

	void SendMidiByte(unsigned char data)
	{
		if (data >= 0xf8)
		{
			MidiRealtimeBuffer[0] = data;
			SendMidi(*(unsigned int*)MidiRealtimeBuffer);
			return;
		}
		if (MidiStatus == 0xF0)
		{
			if (!(data & 0x80))
			{
				if (MidiSysExUsed < (SYSEX_SIZE - 1))
					MidiSysExBuffer[MidiSysExUsed++] = data;
				return;
			}
			else {
				MidiSysExBuffer[MidiSysExUsed++] = 0xf7;

				if ((MidiSysExStart) && (MidiSysExUsed >= 4) && (MidiSysExUsed <= 9) && (MidiSysExBuffer[1] == 0x41) && (MidiSysExBuffer[3] == 0x16))
					Log(L"MIDI: Skipping invalid MT-32 SysEx midi message -- too short to contain a checksum.");
				else
				{
					SendSysEx(MidiSysExBuffer, MidiSysExUsed);
					if (MidiSysExStart)
					{
						if (MidiSysExBuffer[5] == 0x7F)
							MidiSysExDelay = 290; // All Parameters reset
						else
							MidiSysExDelay = (char)(((float)(MidiSysExUsed) * 1.25f) * 1000.0f / 3125.0f) + 2;
						MidiSysExStart = GetTickCount64();
					}
				}
			}
		}
		if (data & 0x80)
		{
			MidiStatus = data;
			MidiCommandPosition = 0;
			MidiCommandLength = MidiEventLengths[data];
			if (MidiStatus == 0xF0)
			{
				MidiSysExBuffer[0] = 0xF0;
				MidiSysExUsed = 1;
			}
		}
		if (MidiCommandLength) {
			MidiCommandBuffer[MidiCommandPosition++] = data;
			if (MidiCommandPosition >= MidiCommandLength) {
				SendMidi(*(unsigned int*)MidiCommandBuffer);
				MidiCommandPosition = 1; //use running status
			}
		}
	}
#pragma endregion

	void SendOPL(unsigned short message)
	{
		//	Log(L"REG_OPLOUT = 0x%04X;", message);
		OPL3_WriteReg(&opl3, message >> 8, message & 0xFF);
	}

	void ResetPCM()
	{
		pcmSource[0] = pcmSource[1] = NULL;
		pcmLength[0] = pcmLength[1] = 0;
		pcmVolume[0] = pcmVolume[1] = pcmVolume[2] = pcmVolume[3] = 256;
	}

}
