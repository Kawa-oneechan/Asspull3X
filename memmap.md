# Asspull IIIx Memory Map
## Address space
Address space is 28 bits:

    $0FFFFFFF
      |_______ bank

## Regions
| from    | to      | size    | name
| ------- |---------|---------|------
| 0000000 | 000FFFF | 000FFFF | BIOS
| 0010000 | 0FFFFFF | 0FF0000 | ROM
| 1000000 | 9FFFFFF | 8FFFFFF | RAM
| D7FFE00 | D7FFFFF | 0000200 | DISK
| D800000 |         |         | IO
| E000000 | EFFFFFF | 0FFFFFF | VRAM
| FF00000 | FFFFFFF | 00FFFFF | STACK

## VRAM
| from   | content
|--------|---------
| 000000 | Text/Bitmap/Tilemaps
| 080000 | Tileset
| 100000 | Palette
| 100200 | Font
| 108000 | Sprite tile/pal
| 108200 | Sprite position/flip/priority

### Size considerations
* text: up to 80×60=4800 16-bit cells: 4800×2 = 9600 => `$02580`
* bitmap: up to 640×480=307200 8-bit pixels => `$4B000`
* tilemap: 128×128=16384  16-bit cells: 16384×2 = 32768 => `$08000`
  * ...times 4 = 32768*4 => `$20000`
* tileset: 8×8 4bpp cells = 32, times 2048 = 1024 => `$08000`
* palette: up to 256 16-bit colors: 256×2 = 512 => `$00200`
* font: 12288 bytes => `$03000`
* sprites idea A: up to 256 16-bit entries = 512 => `$00200`
* sprites idea B: up to 256 32-bit entries = 1024 => `$00400`

## Tilemap cell
    PPPP vh.T TTTT TTTT
    |    || |____________ Tile #
    |    ||______________ Horizontal flip
    |    |_______________ Vertical flip
    |____________________ Palette #

## Sprite
    PPPP E..T TTTT TTTT
    |    |  |____________ Tile #
    |    |_______________ Enabled
    |____________________ Palette #
    PP.2 vhyx ...V VVVV VVVV ..HH HHHH HHHH
    |  | ||||    |             |___________  Horizontal position
    |  | ||||    |_________________________  Vertical position
    |  | ||||______________________________  Double width
    |  | |||_______________________________  Double height
    |  | ||________________________________  Horizontal flip
    |  | |_________________________________  Vertical flip
    |  |___________________________________  Double width and height
    |______________________________________  Priority
Alternative:

    PPvh hhww ~~~~
    | || | |___ Width: 8, 16, 32, 64
    ¦ ¦¦ |_____ Height: same

## Draw order
### Tilemap mode
1. Backdrop
2. Sprites with priority 2
3. Map 1
4. Sprites with priority 1 
5. Map 2
6. Sprites with priority 0
### Other modes:
1. Stuff
2. Sprites with any priority

## Register map
### 00000 Line
The current line being drawn as a `uint16`.
### 00004 ScreenMode
    B32S ..MM
    ||||   |___ Mode
    ||||_______ Enable sprites
    |||________ 240px tall instead of 480px
    ||_________ 320px wide instead of 640px
    |__________ Bold in text mode, 200 or 400px in bitmap mode
### 00005	Interrupts
    X... .VH.
    |     ||___ HBlank triggered
    |     |____ VBlank triggered
    |__________ Disable interrupts
Used by the BIOS dispatcher to determine what to call. Applications can set the disable bit to prevent them from firing.
### 00006	KeyIn
    .CAS KKKK
     ||| |_____ Keycode
     |||_______ Shift
     ||________ Alt
     |_________ Control
### 00008	TickCount
The amount of ticks since the system was turned on as a `uint32`.
### 0000C	ScreenFade
    W..A AAAA
    |  |_______ Amount
    |__________ To white
### 0000E	DebugOut
Pipe characters to `STDOUT`.
### 00010 TilemapSet1
### 00011 TilemapSet2
    E... TTTT
    |    |_____ Tile shift
    |__________ Enabled
Tile shift adds 64 << T to the tile # when rendering, so a shift value of 3 means a whole separate set of 512 tiles.
### 00012	TilemapScrollH1
### 00014	TilemapScrollH2
### 00016	TilemapScrollV1
### 00018	TilemapScrollV2
The tile map controls are a work in progress.
### 00020	DMASource
Either a pointer or a value.
### 00024	DMATarget
Certainly a pointer.
### 00028	DMALength
Certainly a value.
### 0002A	DMAControl
    ..WW VTSE
      |  ||||__ Enable now
      |  |||___ Increase source every loop
      |  ||____ Increase target every loop
      |  |_____ Use source as direct value, not as a pointer
      |________ Width of data to copy
### 00030	DiskSector
Selects the sector of the floppy diskette to read or write as a `uint16`.
### 00032	DiskControl
    .... WREP
         ||||__ Present (read only)
         |||___ Error state (read only)
         ||____ Read now
         |_____ Write now
#### 00040	MidiOut
Send a raw 32-bit message through the MIDI OUT port.
### 00080	HDMAControl1
    ...L LLLL LLLL ...S SSSS SSSS D.WW ...E
       |              |           | |     |__ Enable
       |              |           | |________ Width of data to copy
       |              |           |__________ Doublescan
       |              |______________________ Starting scanline
       |_____________________________________ Linecount
This repeats until `0009C HDMAControl8`.
### 000A0	HDMASource1
A pointer. This too repeats until `000BC HDMASource8`.
### 000C0	HDMADest1
A pointer. Again repeated until `000DC HDMADest8`.

---

## Blitter
| register | name        | format                                    |
| -------- | ----------- | ----------------------------------------- |
| 00100    | Function    | `.... .... .... .... .... .... .... FFFF` |
| 00104    | Source      | Pointer or value                          |
| 00108    | Destination | Pointer                                   |
| 0010C    | Length      | Value                                     |
| 00110    | Key         | Value                                     |
### Functions
#### 0001 - Blit
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

#### 0010 - Clear
    2222 2222 2222 1111 1111 1111 .WWS 0010
    |_________ Source width
* Copies the value of `source` to `dest`.
* If `width` is set to 0, sets `dest` to the low byte of the source value.
* If `width` is set to 1, sets `dest` to the lower short instead.
* If `width` is set to 2, sets `dest` to the full word.
* If `width` is set to 3, behavior is undefined.

#### 0011 Invert
    2222 2222 2222 1111 1111 1111 ...S 0011
Inverts the byte values at B, simple as that.

#### 0100 UnRLE
Performs a simple RLE decompression from `source` to `dest`. `length` is the *compressed* size.