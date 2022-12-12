# Asspull IIIx Programmers Guide -- Devices

Each of the sixteen blocks of `8000` bytes starting from `2000000` may or may not map to a device. The first two bytes of each block identify what kind of device it is. If those bytes are the value `0000` or `FFFF` there is no device.

### Input/output devices

The I/O device is hard wired as device #0. As such, it can safely have its components defined.
It handles both input through the keyboard, mouse, and joypads, and output through the OPL3, MIDI-OUT, and dual PCM channels. As such, its registers act differently depending on if you're *reading* or *writing*.

#### 0000	Identifier

The input device identifies by the value `494F`, for "IO".

#### 0002	INP_KEYIN

Returns the last key pressed, or `00` if the input buffer is empty. This also pops the key out of the buffer.

*(Idea: allow "ungetting" a keypress by writing to `INP_KEYIN`?)*

#### 0003	INP_KEYSHIFT

Returns the state of the control, alt, and shift keys.

    .... .CAS
          |||__ Shift key is held
          ||___ Alt key is held
          |____ Control key is held

#### 0010	INP_JOYSTATES

Returns the availability and type of the two gamepads.

    2222 1111
    |    |_____ Type of the first gamepad
    |__________ Type of the second gamepad

0. Not attached
1. Digital input only
2. Analog sticks

#### 0012	INP_JOYPAD1

    .... RLSB YXBA RLDU
         | || |    |_____ Directions
         | || |__________ Action buttons
         | ||____________ Back/Select
         | |_____________ Start
         |_______________ Shoulder buttons

#### 0014	INP_JOYSTK1H
#### 0015	INP_JOYSTK1V

Analog stick movements are reported as a signed byte value.

The above is repeated on `0016`-`0019` for the second gamepad.

#### 0020	INP_MOUSE

    RLyY YYYY YxXX XXXX
    |||       ||      |__ Horizontal displacement
    |||       ||_________ Horizontal sign
    |||       |__________ Vertical displacement
    |||__________________ Vertical sign
    ||___________________ Buttons

#### 0040	INP_KEYMAP

A list of 256 individual bytes reflecting the state of up to 256 individual keys.

#### Key map

|      | x0   | x1   | x2   | x3   | x4   | x5   | x6   | x7   | x8   | x9    | xA   | xB   | xC   | xD   | xE   | xF   |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ----- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0x   |      | Esc  | 1    | 2    | 3    | 4    | 5    | 6    | 7    | 8     | 9    | 0    | -    | =    | Back | Tab  |
| 1x   | Q    | W    | E    | R    | T    | Y    | U    | I    | O    | P     | [    | ]    | Retn |      | A    | S    |
| 2x   | D    | F    | G    | H    | J    | K    | L    | ;    | '    | `     |      | \    | Z    | X    | C    | V    |
| 3x   | B    | N    | M    | ,    | .    | /    |      | KP*  |      | Space | CpLk | F1   | F2   | F3   | F4   | F5   |
| 4x   | F6   | F7   | F8   | F9   | F10  | NmLk | ScLk | KP7  | KP8  | KP9   | KP-  | KP4  | KP5  | KP6  | KP+  | KP1  |
| 5x   | KP2  | KP3  | KP0  | KP.  |      |      |      | F11  | F12  |       |      |      |      |      |      |      |
| 6x   |      |      |      |      |      |      |      |      |      |       |      |      |      |      |      |      |
| 7x   |      |      |      |      |      |      |      |      |      |       |      |      |      |      |      |      |
| 8x   |      |      |      |      |      |      |      |      |      |       |      |      |      |      |      |      |
| 9x   |      |      |      |      |      |      |      |      |      |       |      |      | KPEn |      |      |      |
| Ax   |      |      |      |      |      |      |      |      |      |       |      |      |      |      |      |      |
| Bx   |      |      |      |      |      | KP/  |      |      |      |       |      |      |      |      |      |      |
| Cx   |      |      |      |      |      |      |      | Home |      | PgUp  |      | Up   |      | Left |      | End  |
| Dx   | Down | PgDn | Ins  | Del  |      |      |      |      |      |       |      |      |      |      |      |      |
| Ex   |      |      |      |      |      |      |      |      |      |       |      |      |      |      |      |      |
| Fx   |      |      |      |      |      |      |      |      |      |       |      |      |      |      |      |      |

#### 0004 REG_MIDIOUT

Send a byte through the MIDI OUT port.

#### 0005 REG_OPLOUT

Sends a register-value pair to the OPL3/YMF262.

    RRRR RRRR VVVV VVVV
    |         |__________ Value
    |____________________ Register

#### 0010 REG_PCM1OFFSET

A pointer to a chunk of 8-bit 11025 Hz unsigned PCM audio for channel 1. Writing to this register only latches its value, it doesn't actually start playing anything. This is immediately followed by an identical register for channel 2, `0014 REG_PCM2OFFSET`.

The PCM offsets are also available via the `PCMOFFSET` array, and the first one as `REG_PCMOFFSET`.

#### 0018	REG_PCM1LENGTH

The first 31 bits are the length of the PCM audio chunk in `REG_PCM1OFFSET`. The most significant specifies if the sound should repeat automatically. Sound playback starts when this register is written to. This is immediately followed by an identical register for channel 2, `001C REG_PCM2LENGTH`.

The PCM lengths are also available via the `PCMLENGTH` array, and the first one as `REG_PCMLENGTH`.

#### 0020 REG_PCM1VOLUMEL

A value from zero to 255 denoting the volume to play PCM channel 1 at, on the left speaker. This is immediately followed by an identical register for the right speaker, `0021 REG_PCM1VOLUMER`, then the same pair for channel 2.

The PCM volume controls are also available via the `PCMVOLUME` array, and the first channels' as `REG_PCMVOLUMEL` and `REG_PCMVOLUMER`.

### Disk drive

#### 0000	Identifier

Disk drives identify by the value `0144`, as in "1.44 MB", even if it's a hard disk drive.

#### 0002	Sector number

#### 0004	Control

    ...B WREP
       | ||||__ Disk present (read only)
       | |||___ Error state (read only)
       | ||____ Read now
       | |_____ Write now
       |_______ Busy state (read only)

#### 0005	Drive type

Returns 0 if this is a diskette drive, 1 if it's a hard drive.

#### 0010	Tracks
#### 0012	Heads
#### 0014	Sectors

Read only, return the disk geometry. Roughly, the capacity of the disk can be found by multiplying these three values and 512 together. For example, for a diskette drive, this should always be 80 tracks, two heads, and 18 sectors. Given 512 bytes per sector this should add up to 1,474,560 bytes (512 * 18 * 2 * 80).

#### 0200	Sector buffer

The sector buffer comprises 512 bytes of the disk controller's internal RAM, used to hold a sector's worth of data to read or write.

### Line printer

#### 0000	Identifier

Line printers identify by the value `4C50`, for "LP".

#### 0002	Character out

Anything written here is piped directly to the printer. You can write with <em>emphasis</em> by writing `\x1B` `E` and turn emphasis back off with `\x1B` `e`. Likewise, you can write <u>underlined</u> by writing `\x1B` `U` and turn it off again with `\x1B` `u`. So as an example:

````
I'm not sure you should drink that. \n
It looks \x1BEbad\x1Be for you. \n
\n
Nonsense. It makes me feel \x1BUgreat!\1Bu \n
Like I could \x1BE\x1BUtake on the world!\x1Bu\x1Be
````

> I'm not sure you should drink that.
> It looks *bad* for you.
> 
> Nonsense. It makes me feel <u>great!</u>
> Like I could *<u>take on the world!</u>*
