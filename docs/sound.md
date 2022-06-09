# Asspull IIIx Programmers Guide

The *Asspull IIIx* supports three sound output devices:

1. [Yamaha YMF262/OPL3](https://en.wikipedia.org/wiki/Yamaha_OPL#OPL3) for FM synthesis
2. [MIDI](https://en.wikipedia.org/wiki/MIDI) for perhaps a prettier alternative
3. Two separate [PCM](https://en.wikipedia.org/wiki/Pulse-code_modulation) audio channels for digital sound effects

## OPL3

The OPL3 can be driven by writing to `REG_OPLOUT`, connecting directly to the chip. The upper eight bits form the register, the lower eight the value.

In the *Clunibus* emulator, OPL3 support is provided by the [*Nuked OPL3* core](https://github.com/nukeykt/Nuked-OPL3).

When reading a guide on how to use the OPL3 (such as [this one](https://www.fit.vutbr.cz/~arnost/opl/opl3.html)), you can safely ignore any reference to "base addresses" -- it's *always* on `REG_OPLOUT`. Where on a PC you might use something like this:
```c
outp(0x388, 0xA4);
outp(0x389, 0x57);
outp(0x388, 0xB4);
outp(0x389, 0x31);
```
you would do this instead on the *Asspull IIIx*:
```c
REG_OPLOUT = 0xA457;
REG_OPLOUT = 0xB431;
```

A slightly adjusted version of an [IMF](http://www.vgmpf.com/Wiki/index.php?title=IMF) player by K1n9 Duk3 has been provided with the A3X SDK. You can use this to easily play background music through the OPL3.

## MIDI

The *Asspull IIIx* comes with a MIDI-OUT port as standard. You send raw MIDI message data one byte at a time. The amount of bytes needed to make a full message varies -- a "program change" message takes only two bytes, a "key on" message takes three, and a System Exclusive message may span several.

In the *Clunibus* emulator, this hooks up to Windows' own MIDI system and if you have multiple synthesizers to choose from you can do so from the emulator's Options window.

## PCM

Using the `REG_PCMxOFFSET` and `REG_PCMxLENGTH` pairs, you can play any two digital audio streams at once. First, set the offset register to point to the actual audio samples, then set the length register to the stream's length. Set the most significant bit on the length to make it automatically repeat. Sound playback will proceed the moment the length register is written to. Sounds can be interrupted by setting either the length or offset register to zero.

(*This means the longest sound that can be played can have a length of only 2,147,483,648 samples, which is still a rather absurd amount of data for this purpose. -- Kawa*)

Sound data must be stored as 8-bit unsigned, 11.025 kHz, monaural.

There are also *four* volume control registers, `REG_PCMxVOLUMEy`, where `y` is `L` or `R`. These allow you to position either sound anywhere in a two-speaker stereo field.

If you want to use *stereo* PCM audio for something like background music, using the OPL3 for sound effects perhaps, store and play the two channels separately.

For ease of use, the following array pointers are available:

| Register          | Array pointer  |
|-------------------|----------------|
| `REG_PCM1OFFSET`  | `PCMOFFSET[0]` |
| `REG_PCM2OFFSET`  | `PCMOFFSET[1]` |
| `REG_PCM1LENGTH`  | `PCMLENGTH[0]` |
| `REG_PCM1LENGTH`  | `PCMLENGTH[1]` |
| `REG_PCM1VOLUMEL` | `PCMVOLUME[0]` |
| `REG_PCM1VOLUMER` | `PCMVOLUME[1]` |
| `REG_PCM2VOLUMEL` | `PCMVOLUME[2]` |
| `REG_PCM2VOLUMER` | `PCMVOLUME[3]` |

A `PCM_REPEAT` constant is provided for readability.

The A3X SDK comes with an `ass-macros.i` file that can make importing sounds a little easier via the `incwav` macro. It will allow you to import plain `WAV` files, *under the assumption* they are "clean", with no extra metadata before or after the actual sample data, which is 8-bit unsigned mono 11.025 kHz. The macro will skip over the header data, import the length of the sample data while also swapping it over from little-endian (Intel) to big-endian (Motorola) byte order, then import all the actual sample data. This allows you to set the offset register to `(uint32_t)yourSoundData + 4`, the length to `(uint32_t*)yourSoundData`, and you're off, as demonstrated in the Test Suite's `pcm.c`.