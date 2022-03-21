# Asspull IIIx Programmers Guide -- Devices

Each of the sixteen blocks of `8000` bytes starting from `2000000` may or may not map to a device. The first two bytes of each block identify what kind of device it is. If those bytes are the value `0000` or `FFFF` there is no device.

### Input devices

The input device is hard wired as device #0. As such, it can safely have its components defined.

#### 0000	Identifier

The input device identifies by the value `494F`, for "IO", even though it's only input.

#### 0002	INP_KEYIN

Returns the last key pressed, or `00` if the input buffer is empty. This also pops the key out of the buffer.

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

Read only, return the disk geometry.

#### 0200	Sector buffer

The sector buffer comprises 512 bytes of the disk controller's internal RAM, used to hold a sector's worth of data to read or write.

### Line printer

#### 0000	Identifier

Line printers identify by the value `4C50`, for "LP".

#### 0002	Character out

Anything written here is piped directly to the printer.
