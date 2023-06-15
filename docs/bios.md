# BIOS Interface

To use the Interface, include the Asspull main header file and define a global `IBios` pointer named `interface`. The shared startup code will set this to the right value just before invoking `main`.

```cpp
#include "../ass.h"
IBios* interface;
```

## The Interface

The `IBios` structure looks like this:

```cpp
typedef struct IBios
{
    long assBang;
    int16_t biosVersion;
    int16_t extensions;
    void(*exception)(void);
    void(*vBlank)(void);
    void(*hBlank)(void);
    void(*drawChar)(char ch, int32_t x, int32_t y, int32_t color);
    ITextLibrary* textLibrary;
    IDrawingLibrary* drawingLibrary;
    IMiscLibrary* miscLibrary;
    IDiskLibrary* diskLibrary;
    char* drawCharFont;
    uint16_t drawCharHeight;
    uint8_t* linePrinter;
    TIOState io;
    TLocale locale;
} IBios;
```

* `assBang`: the word "ASS!" spelled out in ASCII.
* `biosVersion`: the BIOS/interface revision in 8.8 fixed point, so `0x0102` means version 1.2.
* `extensions`: reserved.
* `exception`: a function pointer to a generic exception handler, not currently used.
* `vBlank`: a function pointer called by the BIOS' interrupt dispatcher when not null. May be set by programs as needed.
* `hBlank`: as above, but for HBlank.
* `drawChar`: a function pointer to a routine that draws characters,  Automatically set to BIOS-provided routines by the `SetupDrawChar` interface call.
* `textLibrary`, `drawingLibrary`, `miscLibrary`, and `diskLibrary`: substructures with function pointers to various interface calls.
* `drawCharFont`: a pointer to the font graphics data that `drawChar` may use.
* `drawCharHeight`: used by `drawChar`, least significant byte holds the cell height and most significant the line height.
* `linePrinter`: a pointer to the [line printer device](devices.md#line-printer), if one is attached. The BIOS sets this on startup.

## Libraries

### Text

All text library calls can be reached with the `TEXT` shorthand `#define`, like in `TEXT->ClearScreen();`. Except for `VFormat` and `ClearScreen`, these only work in text mode.

#### `int Write(const char* format, ...)`

Writes formatted text to the screen in text mode. Equivalent to `printf(format, ...)`, which is in fact a `#define`. Returns the amount of characters actually written.

#### `int Format(char *buffer, const char* format, ...)`

Writes formatted text to memory. Equivalent to `sprintf(buffer, format, ...)` which is *not* a `#define`. Returns the amount of characters actually written.

#### `int VFormat(char *buffer, const char* format, va_list args)`

Equivalent to `vsprintf(buffer, format, args)`.

#### `void WriteChar(char ch)`

Writes a single character to the screen in text mode. Equivalent to `putch(ch)` which is *not* a `#define`.

#### `void SetCursorPosition(int left, int top)`

Sets the cursor position in text mode. The next character will appear there.

#### `void SetTextColor(int back, int fore)`

Set the color to use in text mode. The next character will appear in this color. Defaults to 7 on 0.

#### `void ClearScreen()`

Clears the text mode screen using spaces in the current color. When called in any other mode, writes 307,200 zeroes into video memory.

### Drawing

All drawing library calls can be reached with the `DRAW` shorthand `#define`. Except for `ResetPalette`, these only work in bitmap modes.

#### `void ResetPalette()`

Loads the default sixteen colors into palette memory.

#### `void DisplayPicture(TImageFile* picData)`

Given an A3X format image, this will:

1. set the screen mode accordingly, if it's one of the standard sizes;
2. load colors, if needed;
3. unpacks or copies the pixel data;
4. sets up any HDMA gradients if needed.

#### `void Fade(bool in, bool toWhite)`

| Effect             | `in`    | `toWhite` |
| ------------------ | ------- | --------- |
| Fade in from black | `true`  | `false`   |
| Fade out to black  | `false` | `false`   |
| Fade in from white | `true`  | `true`    |
| Fade out to white  | `false` | `true`    |

#### `void SetupDrawChar()`

Ensures `interface->DrawChar` is set correctly. Failure to call this at least once may cause the system to hang.

#### `void DrawString(const char* str, int x, int y, int color)`

Writes text to the bitmap screen at the specified location.

#### `void DrawFormat(const char* format, int x, int y, int color, ...)`

Writes *formatted* text to the bitmap screen.

#### `void DrawChar(char ch, int x, int y, int color)`

Writes a single character to the bitmap screen.

#### `void DrawLine(int x0, int y0, int x1, int y1, int color, uint8_t* dest)`

Draws a line on presumably an image buffer the size of the screen using the Bresenham algorithm. If `dest` is null, the screen image is presumed.

#### `void FloodFill(int x, int y, int color, uint8_t* dest)`

Fills an area of presumably an image buffer the size of the screen using a [scanline with stack algorithm](https://lodev.org/cgtutor/floodfill.html#Scanline_Floodfill_Algorithm_With_Stack). If `dest` is null, the screen image is presumed.

### Miscellaneous

All miscellaneous library calls can be reached with the `MISC` shorthand `#define`.

#### `void WaitForVBlank()`

Spins in place until the screen reaches the bottom of the visible area.

#### `void WaitForVBlanks(int vbls)`

Waits for the specified number of frames.

#### `void DmaCopy(void* dst, const void* src, size_t size, int step)`

Quickly copies a contiguous block of memory from `src` to `dst`. The `step` determines if the data is in bytes, shorts, or longs. The `size` should be measured accordingly -- if your data consists of 100 shorts, which is 200 bytes, `size` should be 100 regardless and `step` should be 1 or `DMA_SHORT`.

#### `void DmaClear(void* dst, int src, size_t size, int step)`

Quickly sets a contiguous block of memory to a given value.

#### `void MidiReset()`

Resets all MIDI controllers and releases all notes currently playing.

#### `void OplReset()`

Resets the OPL3 registers.

#### `void RleUnpack(int8_t* dst, int8_t* src, size_t size)`

Decompresses a run-length encoded data block at `src` to `dst`. The `size` refers to the source data.

If a byte in the source data has the two most significant bits set, the remaining six determine the amount of times to write the next byte. If the most significant bits are clear, that single byte is copied directly.

#### `char* GetLocaleStr(ELocale category, int item)`

Returns strings according to `interface->locale`.

| Category   | Item | Returns                 | Example         |
| ---------- | ---- | ----------------------- | --------------- |
| `LC_CODE`  | n/a  | The locale's identifier | "en_US"         |
| `LC_DAYS`  | 0-6  | Abbreviated day names   | "Mon"           |
| `LC_MONS`  | 0-11 | Abbreviated month names | "Jan"           |
| `LC_DAYF`  | 0-6  | Full day names          | "Monday"        |
| `LC_MONF`  | 0-11 | Full month names        | "January"       |
| `LC_DATES` | n/a  | Short date format       | "%Y-%m-%d"      |
| `LC_DATEF` | n/a  | Long date format        | "%A, %B %d, %Y" |
| `LC_TIMES` | n/a  | Short time format       | "%H:%M"         |
| `LC_TIMEF` | n/a  | Long time format        | "%H:%M:%S"      |
| `LC_CURR`  | n/a  | Currency symbol         | "$"             |

Other things can be taken directly from `interface->locale`.

#### `void* HeapAlloc(size_t size)`

Works like `malloc` in standard C, and there's a `#define` so you can use that name instead. If `size` is zero, `NULL` is returned.

#### `void HeapFree(void* ptr);`

Works like `free` in standard C, and there's a `#define` so you can use that name instead.

#### `void* HeapReAlloc(void* ptr, size_t size);`

Works like `realloc` in standard C, and there's a `#define` so you can use that name instead.

#### `void* HeapCAlloc(size_t nelem, size_t elsize);`

Works like `calloc` in standard C, and there's a `#define` so you can use that name instead.

### Disk I/O

All disk I/O library calls can be reached with the `DISK` shorthand `#define`. Because these don't work like in standard C, `ass-std.c` can be included in the project to provide more standard-like wrappers.

#### `EFileError OpenFile(TFileHandle* handle, const char* path, char mode);`

`mode` can be one of the following:

| Value | Define                                    | Effect           | POSIX  |
| ----- | ----------------------------------------- | ---------------- | ------ |
| 0x00  | `FA_OPEN_EXISTING`                        | Just open        | None   |
| 0x01  | `FA_READ`                                 | Read access      | `"r"`  |
| 0x02  | `FA_WRITE`                                | Write access     | None   |
| 0x03  | `FA_READ \| FA_WRITE`                     | Read/write       | `"r+"` |
| 0x04  | `FA_CREATE_NEW`                           | Use new file     | None   |
| 0x08  | `FA_CREATE_ALWAYS`                        | Replace old file | None   |
| 0x0A  | `FA_CREATE_ALWAYS \| FA_WRITE`            | Replace to write | `"w"`  |
| 0x0B  | `FA_CREATE_ALWAYS \| FA_WRITE \| FA_READ` | Replace for both | `"w+"` |
| 0x10  | `FA_OPEN_ALWAYS`                          | Create if needed | None   |
| 0x30  | `FA_OPEN_APPEND`                          | Do not truncate  | None   |
| 0x32  | `FA_OPEN_APPEND \| FA_WRITE`              | Open to append   | `"a"`  |
| 0x33  | `FA_OPEN_APPEND \| FA_WRITE \| FA_READ`   | ...but also read | `"a+"` |

Basically only the entries with POSIX equivalents are useful on their own.

For `FA_CREATE_NEW`, an `FE_Denied` error is returned if the file already exists. For `FA_CREATE_ALWAYS`, the file is truncated instead if it already exists.

This is a wrapper for [FatFS' `f_open`](http://elm-chan.org/fsw/ff/doc/open.html).

#### `EFileError CloseFile(TFileHandle* handle);`

Closes the given file handle. This is a wrapper for [FatFS' `f_close`](http://elm-chan.org/fsw/ff/doc/close.html).

#### `int ReadFile(TFileHandle* handle, void* target, size_t length);`

Reads up to `length` bytes from the given file to the `target`. Returns the actual number of bytes read. This is *almost* a wrapper for [FatFS' `f_read`](http://elm-chan.org/fsw/ff/doc/read.html): if `length` is zero, `ReadFile` tries to read the whole thing. If the return value is less than zero, its absolute value is the error code.

#### `int WriteFile(TFileHandle* handle, void* source, size_t length);`

Writes up to `length` bytes from `source` to the given file and returns the amount actually written. This is *almost* wrapper for [FatFS' `f_write`](http://elm-chan.org/fsw/ff/doc/write.html). If the return value is less than zero, its absolute value is the error code.

#### `uint32_t SeekFile(TFileHandle* handle, uint32_t offset, int origin);`

Sets the read/write pointer for the given file to `offset`.  `origin` can be `SEEK_CUR`, `SEEK_END`, or `SEEK_SET` -- if it's `SEEK_CUR`, `offset` is added to the current pointer. If it's `SEEK_END`, `offset` is added to the very end of the file. Seeking beyond the end of a file causes the file to grow if the file is writeable.

This is *almost* a wrapper for [FatFS' `f_lseek`](http://elm-chan.org/fsw/ff/doc/lseek.html), which only supports `SEEK_SET`.

#### `EFileError TruncateFile(TFileHandle* handle);`

#### `EFileError FlushFile(TFileHandle*);`

#### `uint32_t FilePosition(TFileHandle* handle);`

#### `bool FileEnd(TFileHandle* handle);`

#### `size_t FileSize(TFileHandle* handle);`

#### `EFileError OpenDir(TDirHandle* handle, const char* path);`

#### `EFileError CloseDir(TDirHandle* handle);`

#### `EFileError ReadDir(TDirHandle* handle, TFileInfo* info);`

#### `EFileError FindFirst(TDirHandle* handle, TFileInfo* info, const char* path, const char* pattern);`

#### `EFileError FindNext(TDirHandle* handle, TFileInfo* info);`

#### `EFileError FileStat(const char* path, TFileInfo* info);`

#### `EFileError UnlinkFile(const char* path);`

#### `EFileError RenameFile(const char* from, const char* to);`

#### `EFileError FileTouch(const char* path, TFileInfo* info);`

#### `EFileError FileAttrib(const char* path, char attrib);`

#### `EFileError MakeDir(const char* path);`

#### `EFileError ChangeDir(const char* path);`

#### `EFileError GetCurrentDir(char* path, size_t buflen);`

#### `EFileError GetLabel(char disk, char* buffer, uint32_t* vsn);`

Given a drive letter, writes the volume name to `buffer` (must be at least 11 characters long) and the volume serial number to `vsn`. Both targets are allowed to be null if you don't need either.

#### `const char* FileErrStr(EFileError);`

Returns a string with a readable English equivalent of the given error. For example, `FE_InvalidPath` returns `"The path name format is invalid."`.

#### `uint32_t GetFree(char disk);`

Given a drive letter, returns the free space in bytes. Behind the scenes, free space is given in clusters (512 byte blocks), so this may be a little off.
