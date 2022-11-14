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

All text library calls can be reached with the `TEXT` shorthand `#define`, like in `TEXT->ClearScreen();`

#### `int Write(const char* format, ...)`

Writes formatted text to the screen. Equivalent to `printf(format, ...)`, which is in fact a `#define`. Returns the amount of characters actually written.

#### `int Format(char *buffer, const char* format, ...)`

Writes formatted text to memory. Equivalent to `sprintf(buffer, format, ...)` which is *not* a `#define`. Returns the amount of characters actually written.

#### `int VFormat(char *buffer, const char* format, va_list args)`

Equivalent to `vsprintf(buffer, format, args)`.

#### `void WriteChar(char ch)`

Writes a single character to the screen. Equivalent to `putch(ch)` which is *not* a `#define`.

#### `void SetCursorPosition(int left, int top)`

#### `void SetTextColor(int back, int fore)`

#### `void ClearScreen()`

### Drawing

All drawing library calls can be reached with the `DRAW` shorthand `#define`.

#### `void ResetPalette()`

#### `void DisplayPicture(TImageFile* picData)`

#### `void Fade(bool in, bool toWhite)`

| Effect             | `in`    | `toWhite` |
|--------------------|---------|-----------|
| Fade in from black | `true`  | `false`   |
| Fade out to black  | `false` | `false`   |
| Fade in from white | `true`  | `true`    |
| Fade out to white  | `false` | `true`    |

#### `void SetupDrawChar()`

Ensures `interface->DrawChar` is set correctly. Failure to call this at least once may cause the system to hang.

#### `void DrawString(const char* str, int x, int y, int color)`

#### `void DrawFormat(const char* format, int x, int y, int color, ...)`

#### `void DrawChar(char ch, int x, int y, int color)`

#### `void DrawLine(int x0, int y0, int x1, int y1, int color, uint8_t* dest)`

#### `void FloodFill(int x, int y, int color, uint8_t* dest)`

### Miscellaneous

All miscellaneous library calls can be reached with the `MISC` shorthand `#define`.

#### `void WaitForVBlank()`

#### `void WaitForVBlanks(int vbls)`

#### `void DmaCopy(void* dst, const void* src, size_t size, int step)`

#### `void DmaClear(void* dst, int src, size_t size, int step)`

#### `void MidiReset()`

#### `void OplReset()`

#### `void RleUnpack(int8_t* dst, int8_t* src, size_t size)`

#### `char* GetLocaleStr(ELocale category, int item)`

Returns strings according to `interface->locale`.

| Category   | Item  | Returns                 | Example        |
|------------|-------|-------------------------|----------------|
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

#### `EFileError CloseFile(TFileHandle* handle);`

#### `int ReadFile(TFileHandle* handle, void* target, size_t length);`

#### `int WriteFile(TFileHandle* handle, void* source, size_t length);`

#### `uint32_t SeekFile(TFileHandle* handle, uint32_t offset, int origin);`

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

#### `const char* FileErrStr(EFileError);`

#### `uint8_t GetNumDrives();`

#### `uint32_t GetFree(char disk);`

