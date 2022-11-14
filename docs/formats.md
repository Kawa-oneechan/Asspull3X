# ROM
A3X ROM files have the extension `.ap3`.

| Offset | Type       | Name              | Description                          |
|--------|------------|-------------------|--------------------------------------|
| 0x000  | `char[4]`  | Magic marker      | The string "ASS!" in plain ASCII     |
| 0x004  | code       | `JMP` instruction | Entry point for the BIOS to use      |
| 0x008  | `char[24]` | Internal name     | The name of the program              |
| 0x020  | `uint32`   | Checksum          | Sum of all bytes *except* these four |
| 0x024  | `char[4]`  | Game ID code      | See below                            |
| 0x028  |            |                   | Reserved                             |
| 0x02A  | code       | CRT0              | What the `JMP` jumps to              |

The CRT0 code ends up calling your `main` function.

The fourth character of the Game ID indicates a region/language, including but not limited to:

| Code | Country                | Expected language |
|------|------------------------|-------------------|
| `0`  | Not for public release | Irrelevant        |
| `w`  | Worldwide              | English           |
| `U`  | United States          | American English  |
| `e`  | Europe                 | Any or many       |
| `j`  | Japan                  | Japanese          |
| `E`  | United Kingdom         | English           |
| `F`  | France                 | French            |
| `D`  | Germany                | German            |
| `N`  | Netherlands            | Dutch             |
| `I`  | Italy                  | Italian           |
| `S`  | Spain                  | Spanish           |
| `s`  | United States          | Spanish           |
| `r`  | Russia                 | Russian           |
| `b`  | Brazil                 | Portuguese        |
| `p`  | Portugal               | Portuguese        |


If the region/language is `j`, the internal name is interpreted by the Clunibus emulator as Shift-JIS. If it's `r`, the internal name is interpreted as Windows-1251. Anything else is taken as Windows-1252.

# Disk

A3X programs to be run from disk have the extension `.app`. If the BIOS finds a file `start.app` on a diskette in the built-in diskette drive, that will be executed on boot.

| Offset | Type       | Name              | Description                       |
|--------|------------|-------------------|-----------------------------------|
| 0x000  | `char[4]`  | Magic marker      | The string "ASS!" in plain ASCII. |
| 0x004  | code       | `JMP` instruction | Entry point for the BIOS to use.  |
| 0x008  | `char[24]` | Internal name     | The name of the program.          |
| 0x020  | code       | CRT0              | What the `JMP` jumps to.          |

# Images

The A3X BIOS has built-in support for a specific bespoke image format.

As defined in `ass.h`:
```c
typedef struct TImageFile
{
	int32_t identifier;   // Should always be "AIMG".
	uint8_t bitDepth;     // Should be equal to 4 or 8, for 16 or 256 colors respectively.
	uint8_t flags;        // Specifies if the image is compressed, among other things.
	uint16_t width;       // The pixel width of the image.
	uint16_t height;      // The pixel height of the image.
	uint16_t stride;      // Specifies how many bytes make up a full line. Should be half the width for a 4bpp image.
	int32_t byteSize;     // The total amount of bytes making up the full image. Should be equal to stride times height.
	int32_t colorOffset;  // The offset from the start of the structure to the color data.
	int32_t imageOffset;  // The offset from the start of the structure to the image data.
	int32_t hdmaOffset;   // The offset from the start of the structure to the HDMA gradient data, if any.
} TImageFile;
```

Palette data is in 15-bit xBGR format. If the image is compressed, this applies on the *byte* level, not pixels.

For a compressed image:

1. Read a byte.
2. If the top two bits are clear, it's a literal byte value that can be copied to the destination as-is.
3. If the top two bits are set, the run length is stored in the other six and the next byte is the value to write.

That does mean that any byte value from 64 to 255 cannot be stored raw -- they must be stored as a one-byte run.

# Fonts

The A3X video memory has a dedicated space for fonting starting at , used primarily in text mode. By the rules of text mode, this is laid out as follows:

| Offset | Font      | Define            |
|--------|-----------|-------------------|
| 0x0000 | 8x8 thin  | `TEXTFONT_THIN8`  |
| 0x0800 | 8x8 bold  | `TEXTFONT_BOLD8`  |
| 0x1000 | 8x16 thin | `TEXTFONT_THIN16` |
| 0x2000 | 8x16 bold | `TEXTFONT_BOLD16` |

You can fit three 8x16 fonts in there if you want, six 8x8 fonts, whatever, but that will cause weird results in text mode. They certainly don't need to be thin and bold -- that's why the font variant bit in the [ScreenMode register](registers.md#00001reg_screenmode) is called such.

The `drawCharFont` pointer in the [Interface](bios.md#the_interface) can technically be set to anywhere that has font graphics in the expected format and size. For the built-in character drawing routines and text mode, that format is one bit per pixel, one byte per line.

