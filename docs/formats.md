# ROM
A3X ROM files have the extension `.ap3`.

| Offset | Type       | Name              | Description                           |
| ------ | ---------- | ----------------- | ------------------------------------- |
| 0      | `char[4]`  | Magic marker      | The string "ASS!" in plain ASCII.     |
| 4      | code       | `JMP` instruction | Entry point for the BIOS to use.      |
| 8      | `char[24]` | Internal name     | The name of the program.              |
| 0x20   | `uint32`   | Checksum          | Sum of all bytes *except* these four. |
| 0x24   | `char[4]`  | Game ID code      | See below.                            |
| 0x25   | `uint8`    | SRAM size         | How much battery-backed RAM there is. |
| 0x2A   | code       | CRT0              | What the `JMP` jumps to.              |

The CRT0 code ends up calling your `main` function.

The fourth character of the Game ID indicates a region/language, including but not limited to:

| Code | Country                | Expected language |
| ---- | ---------------------- | ----------------- |
| `0`  | Not for public release | Irrelevant        |
| `w`  | Worldwide              | English           |
| `U`  | United States          | American English  |
| `e`  | European               | Any or many       |
| `j`  | Japan                  | Japanese          |
| `E`  | United Kingdom         | English           |
| `F`  | France                 | French            |
| `D`  | Germany                | German            |
| `N`  | Netherlands            | Dutch             |
| `I`  | Italy                  | Italian           |
| `S`  | Spain                  | Spanish           |
| `s`  | United States          | Spanish           |

# Disk

A3X programs to be run from disk have the extension `.app`. If the BIOS finds a file `start.app` on a diskette in the built-in diskette drive, that will be executed on boot.

| Offset | Type       | Name              | Description                       |
| ------ | ---------- | ----------------- | --------------------------------- |
| 0      | `char[4]`  | Magic marker      | The string "ASS!" in plain ASCII. |
| 4      | code       | `JMP` instruction | Entry point for the BIOS to use.  |
| 8      | `char[24]` | Internal name     | The name of the program.          |
| 0x20   | code       | CRT0              | What the `JMP` jumps to.          |

# Images

The A3X BIOS has built-in support for a specific bespoke image format.

As defined in `ass.h`:
```c
typedef struct TImageFile
{
	int32_t Identifier;		// Should always be "AIMG".
	uint8_t BitDepth;		// Should be equal to 4 or 8, for 16 or 256 colors respectively.
	uint8_t Flags;			// Specifies if the image is compressed, among other things.
	uint16_t Width;			// The pixel width of the image.
	uint16_t Height;		// The pixel height of the image.
	uint16_t Stride;		// Specifies how many bytes make up a full line. Should be half the width for a 4bpp image.
	int32_t ByteSize;		// The total amount of bytes making up the full image. Should be equal to stride times height.
	int32_t ColorOffset;	// The offset from the start of the structure to the color data.
	int32_t ImageOffset;	// The offset from the start of the structure to the image data.
} TImageFile;
```

Palette data is in 15-bit xBGR format. If the image is compressed, this applies on the *byte* level, not pixels.

For a compressed image:

1. Read a byte.
2. If the top two bits are clear, it's a literal byte value that can be copied to the destination as-is.
3. If the top two bits are set, the run length is stored in the other six and the next byte is the value to write.

That does mean that any byte value from 64 to 255 cannot be stored raw -- they must be stored as a one-byte run.
