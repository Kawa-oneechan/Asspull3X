# Asspull IIIx Programmers Guide -- Registers

### 00000	REG_INTRMODE

    X... .VH.
    |     ||___ HBlank active
    |     |____ VBlank active
    |__________ Disable interrupts

Applications can set the disable bit to prevent interrupts from firing. The active bits can be used for polling independent of whether interrupts are disabled.

Available defines: `IMODE_DISABLE`, `IMODE_INVBLANK`, `IMODE_INHBLANK`

### 00001	REG_SCREENMODE

The exact meaning of the higher bits varies according to the screen mode.

    .... ..MM
           |___ Mode

In mode 0, text mode:

    AWHb B.00
    |||| |_____ Font variant (0 - thin, 1 - bold)
    ||||_______ Enable blinking
    |||________ Font height (0 - 8px, 1 - 16px) and line count (0 - 60 lines, 1 - 30)
    ||_________ Font width (0 - 8px, 1 - stretch to 16px) and column count (0 - 80 cols, 1 - 40 cols)
    |__________ Aspect mode (0 - 60/30 lines, 1 - 50/25 lines)

In modes 1 and 2, the bitmap modes:

    AWH. ..MM
    |||    |___ Mode (1 - 16 color, 2 - 256 color)
    |||________ Screen height (0 - 480 pixels, 1 - 240 pixels)
    ||_________ Screen width (0 - 640 pixels, 1 - 320 pixels)
    |__________ Aspect mode (0 - 480/240 pixels, 1 - 400/200 pixels)

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

    EEEE ..SS
    ||||   |___ Shift amount for objects
    ||||_______ Layer 1 enabled
    |||________ Layer 2 enabled
    ||_________ Layer 3 enabled
    |__________ Layer 4 enabled

Please refer to `0000B` for more on shift amounts.

### 0000A	REG_MAPBLEND

    SSSS EEEE
    |    |_____ Enable for these layers
    |__________ Subtract instead of add

### 0000B	REG_MAPSHIFT

	4433 2211
	| |  | |_____ Shift amount for layer 1
	| |  |_______ Shift amount for layer 2
	| |__________ Shift amount for layer 3
	|____________ Shift amount for layer 4

There are 2048 possible tiles, but map tiles can only address half of that range. The mapshift register shifts the window of tiles used by a given layer in 512 tile increments. A value of 0 means a 0 in the map uses tile 0, a value of 1 makes it use tile 512, and so on.

### 00010	REG_SCROLLX1

### 00012	REG_SCROLLY1

Scroll values for the tile map as `int16`. This repeats for each of the four layers up to `0001E REG_SCROLLY4`.

*The tile map controls are a work in progress.*

### 00020	REG_WINMASK

    ..O4 321B ..O4 321B
      ||    |   ||    |__ Background
      ||    |   ||_______ Each map layer
      ||    |   |________ Objects
      ||____|____________ The same, but inverted

Any pixel on the given layer that falls outside of the window clipping edges is skipped over. If this layer is the background, the resulting pixel will be black. The second set in the higher byte acts exactly the other way, skipping anything *inside* of the window.

### 00022	REG_WINLEFT
### 00024	REG_WINRIGHT

The left and right edges for the window clipping effect set up by `REG_WINMASK`.

### 00054	REG_CARET

    EB.. PPPP PPPP PPPP
    ||   |_______________ Position
    ||___________________ Block shape (0 for line)
    |____________________ Enabled

Only rendered in text mode.

### 00060	REG_TIMET

Returns or sets the current date and time in as seconds since the Unix epoch, from 1970-01-01 00:00:00. That is, this is a 32-bits `time_t` value. On first run, the emulator will reset the RTC to 1984-01-01 00:00:00 GMT. This *will* cause problems in the year 2038, but this is supposed to be a system from the early 90s.

### 00100	REG_DMASOURCE

Either a pointer to copy from, or a value to copy.

### 00104	REG_DMATARGET

A pointer to copy to.

### 00108	REG_DMALENGTH

The amount of data to copy.

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

| Register | Name            | Format                                    |
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
  * Though `key` is a 32-bit value, only the first 8 matter.
* If `4-bit source` is set:
  * If `4-bit target` is set, care is taken to work on nibbles.
  * If `4-bit target` is *not* set, the source data is expanded.
    * In this case, the 9th through 12th bits of `key` define a palette row to use.
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

Inverts the byte values at `dest`, simple as that.
