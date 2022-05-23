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
	long AssBang;
	int16_t biosVersion;
	int16_t extensions;
	void(*Exception)(void);
	void(*VBlank)(void);
	void(*reserved)(void);
	void(*DrawChar)(char ch, int32_t x, int32_t y, int32_t color);
	ITextLibrary* textLibrary;
	IDrawingLibrary* drawingLibrary;
	IMiscLibrary* miscLibrary;
	IDiskLibrary* diskLibrary;
	char* DrawCharFont;
	uint16_t DrawCharHeight;
} IBios;
```

* `AssBang`: the word "ASS!" spelled out in ASCII.
* `biosVersion`: the BIOS/interface revision in 8.8 fixed point, so `0x0102` means version 1.2.
* `extensions`: reserved.
* `Exception`: a function pointer to a generic exception handler, not currently used.
* `VBlank`: a function pointer called by ~~the BIOS' interrupt dispatcher~~ `MISC->WaitForVblank` when not null. May be set by programs as needed.
* `reserved`: exactly that.
* `DrawChar`: a function pointer to a routine that draws characters,  Automatically set to BIOS-provided routines by the `SetBitmapMode` interface calls.
* `textLibrary`, `drawingLibrary`, `miscLibrary`, and `diskLibrary`: substructures with function pointers to various interface calls.
* `DrawCharFont`: a pointer to the font graphics data that `DrawChar` may use.
* `DrawCharHeight`: used by `DrawChar`, least significant byte holds the cell height and most significant the line height.

## Libraries

### Text

All text library calls can be reached with the `TEXT` shorthand `#define`, like in `TEXT->ClearScreen();`

#### `int Write(const char* format, ...)`

Writes formatted text to the screen. Equivalent to `printf(format, ...)`, which is in fact a `#define`. Returns the amount of characters actually written.

#### `int Format(char *buffer, const char* format, ...)`

Writes formatted text to memory. Equivalent to `sprintf(buffer, format, ...)` which is *not* a `#define`. Returns the amount of characters actually written.

#### `void WriteChar(char ch)`

Writes a single character to the screen. Equivalent to `putch(ch)` which is *not* a `#define`.

#### `void SetCursorPosition(int left, int top)`

#### `void SetTextColor(int back, int fore)`

#### `void SetBold(bool bold)`

If `bold` is `true`, enables the bold type bit so characters in text mode are rendered as such, clears it otherwise.

#### `void ClearScreen()`

### Drawing

All drawing library calls can be reached with the `DRAW` shorthand `#define`.

#### `void ResetPalette()`

#### `void DisplayPicture(TImageFile* picData)`

#### `void Fade(bool in, bool toWhite)`

| Effect             | `in`    | `toWhite` |
| ------------------ | ------- | --------- |
| fade in from black | `true`  | `false`   |
| fade out to black  | `false` | `false`   |
| fade in from white | `true`  | `true`    |
| fade out to white  | `false` | `true`    |

#### `void DrawString(const char* str, int x, int y, int color)`

#### `void DrawFormat(const char* format, int x, int y, int color, ...)`

#### `void DrawChar(char ch, int x, int y, int color)`

#### `void DrawLine(int x0, int y0, int x1, int y1, int color, uint8_t* dest)`

#### `void FloodFill(int x, int y, int color, uint8_t* dest)`

### Miscellaneous

All miscellaneous library calls can be reached with the `MISC` shorthand `#define`.

#### `void SetTextMode(int flags)`

#### `void SetBitmapMode16(int flags)`

#### `void SetBitmapMode256(int flags)`

#### `void EnableObjects(bool enabled)`

#### `void WaitForVBlank()`

#### `void WaitForVBlanks(int vbls)`

#### `void DmaCopy(void* dst, const void* src, size_t size, int step)`

#### `void DmaClear(void* dst, int src, size_t size, int step)`

#### `void MidiReset()`

#### `void RleUnpack(int8_t* dst, int8_t* src, size_t size)`

### Disk I/O

All disk I/O library calls can be reached with the `DISK` shorthand `#define`.

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

