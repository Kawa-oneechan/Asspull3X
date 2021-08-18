# Asspull IIIx Programmers Guide -- Registers

### 00000	REG_INTRMODE

    X... .V..
    |     |____ VBlank triggered
    |__________ Disable interrupts

Used by the BIOS dispatcher to determine what to call. Applications can set the disable bit to prevent them from firing. Only VBlank is supported for now.

Available defines: `IMODE_DISABLE`, `IMODE_INVBLANK`

### 00001	REG_SCREENMODE

The exact meaning of the higher bits varies according to the screen mode.

    .... ..MM
           |___ Mode

In mode 0, text mode:

    BWHb ..00
    ||||_______ Enable blinking
    |||________ Font height (0 - 8px, 1 - 16px) and line count (0 - 60 lines, 1 - 30)
    ||_________ Font width (0 - 8px, 1 - stretch to 16px) and column count (0 - 80 cols, 1 - 40 cols)
    |__________ Font variant (0 - thin, 1 - bold)

In modes 1 and 2, the bitmap modes:

    AWH. ..MM
    |||    |___ Mode (1 - 16 color, 2 - 256 color)
    |||________ Screen height (0 - 480 pixels, 1 - 240 pixels)
    ||_________ Screen width (0 - 640 pixels, 1 - 320 pixels)
    |__________ Aspect mode (0 - 400 pixels, 1 - 200 pixels)

Mode 3, tile mode, has no further settings. It is *always* 320 by 240.

Available defines: `SMODE_TEXT` `SMODE_BMP16`, `SMODE_BMP256`, `SMODE_TILE`, `SMODE_BLINK`, `SMODE_240`, `SMODE_320`, `SMODE_BOLD`, `SMODE_200`

### 00002	REG_LINE

The current line being drawn as a `uint16`.

### 00004	REG_TICKCOUNT

The amount of ticks since the system was turned on as a `uint32`.

### 00008	REG_SCREENFADE

    W..A AAAA
    |  |_______ Amount
    |__________ To white

### 00009	REG_MAPSET

    EEEE TTTT
    |||| | |___ Tile shift for layers 1 and 3
    |||| |_____ Tile shift for layer 2 and 4
    ||||_______ Layer 1 enabled
    |||________ Layer 2 enabled
    ||_________ Layer 3 enabled
    |__________ Layer 4 enabled

Tile shift adds `128 << T` to the tile # when rendering, so a shift value of 3 means a whole separate set of 1024 tiles.

### 0000A	REG_MAPBLEND

    SSSS EEEE
    |    |_____ Enable for these layers
    |__________ Subtract instead of add

### 00010	REG_SCROLLX1

### 00012	REG_SCROLLY1

Scroll values for the tile map as `int16`. This repeats for each of the four layers up to `001E REG_SCROLLY4`.

*The tile map controls are a work in progress.*

### 00040	REG_KEYIN

    .... .CAS KKKK KKKK
          ||| |__________ Keycode
          |||____________ Shift
          ||_____________ Alt
          |______________ Control

### 00042	REG_JOYPAD1

### 00043	REG_JOYPAD2

First, *write* anything to the joypad register you want to poll.

    First read:  YXBA RLDU   -- the D-pad and action buttons
    Second read: .... RLSB   -- Back, Start, left and right shoulder buttons
    Third read:  signed byte -- Analog stick up/down
    Fourth read: signed btye -- Analog stick left/right

Different connected joypads *may not* have all features. You should always write to reset before reading, or you *will* misinterpret inputs.

The joypads are also available via the `JOYPADS` array and `REG_JOYPAD`.

#### 00044	REG_MIDIOUT

Send a raw 32-bit message through the MIDI OUT port.

### 00048	REG_AUDIOOUT

Sends raw 8-bit 11025 Hz unsigned PCM audio through the audio port.

### 00050	REG_MOUSE

    RL.. ...Y YYYY YYYY   .... ..XX XXXX XXXX
    ||      |                    |______________ Horizontal position (0-639)
    ||      |___________________________________ Vertical position (0-479)
    ||__________________________________________ Buttons

### 00054	REG_CARET

    EB.. PPPP PPPP PPPP
    ||   |_______________ Position
    ||___________________ Block shape (0 for line)
    |____________________ Enabled

Only rendered in text mode.

### 00060	REG_TIMET

The only 64-bit value in the system. Emulator-wise, reading the first half latches the current host system time so there's no sudden shifts when you read the second half. Writing the first half likewise latches, and the real time clock isn't actually *set* until the second half is written. If reading returns a null value, the RTC hasn't been set and doesn't tick.

### 00080	REG_DEBUGOUT

Pipe characters to `STDOUT`. *Should really be redone as a Line Printer thing.* 

### 00100	REG_DMASOURCE

Either a pointer or a value.

### 00104	REG_DMATARGET

Certainly a pointer.

### 00108	REG_DMALENGTH

Certainly a value.

### 0010A	REG_DMACONTROL

    ..WW VTSE
      |  ||||__ Enable now
      |  |||___ Increase source every loop
      |  ||____ Increase target every loop
      |  |_____ Use source as direct value, not as a pointer
      |________ Width of data to copy

### 00180	REG_HDMACONTROL1

    ...L LLLL LLLL ...S SSSS SSSS D.WW ...E
       |              |           | |     |__ Enable
       |              |           | |________ Width of data to copy
       |              |           |__________ Doublescan
       |              |______________________ Starting scanline
       |_____________________________________ Linecount

This repeats until `0019C REG_HDMACONTROL8`.

### 001A0	REG_HDMASOURCE1

A pointer. This too repeats until `001BC REG_HDMASOURCE8`.

### 001C0	REG_HDMATARGET1

A pointer. Again repeated until `001DC REG_HDMATARGET8`.

---

## Blitter

| register | name            | format                                    |
| -------- | --------------- | ----------------------------------------- |
| 00200    | REG_BLITCONTROL | `.... .... .... .... .... .... .... FFFF` |
| 00204    | REG_BLITSOURCE  | Pointer or value                          |
| 00208    | REG_BLITTARGET  | Pointer                                   |
| 0020C    | REG_BLITLENGTH  | Value                                     |
| 00210    | REG_BLITKEY     | Value                                     |

### 1	Blit

    2222 2222 2222 1111 1111 1111 44KS 0001
    |              |              ||||_______ Enable strideskip
    |              |              |||________ Enable colorkey
    |              |              ||_________ Source is 4-bits
    |              |              |__________ Target is 4-bits
    |              |_________________________ Source stride
    |________________________________________ Target stride

* Copies `length` bytes from `source` to `dest`. If `strideskip` is enabled, copies from `source` for `stride` bytes, then skips over `target stride - source stride` bytes, until `length` bytes are copied in total.
* If `colorkey` is enabled, pixels matching `key` are skipped.
* If `4-bit source` is set:
  * If `4-bit target` is set, care is taken to work on nibbles.
  * If `4-bit target` is *not* set, the source data is expanded.
* If `4-bit source` is not set:
  * If `4-bit target` is set, behavior is undefined.
  * If `4-bit target` not set, nothing special is done.

### 2	Clear

    2222 2222 2222 1111 1111 1111 .WWS 0010
    |_________ Source width

* Copies the value of `source` to `dest`.
* If `width` is set to 0, sets `dest` to the low byte of the source value.
* If `width` is set to 1, sets `dest` to the lower short instead.
* If `width` is set to 2, sets `dest` to the full word.
* If `width` is set to 3, behavior is undefined.

### 3	Invert

    2222 2222 2222 1111 1111 1111 ...S 0011

Inverts the byte values at B, simple as that.

### 4	UnRLE

Performs a simple RLE decompression from `source` to `dest`. `length` is the *compressed* size.