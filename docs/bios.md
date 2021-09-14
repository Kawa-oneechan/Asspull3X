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
* `VBlank`: a function pointer called by the BIOS' interrupt dispatcher when not null. May be set by programs as needed.
* `reserved`: exactly that.
* `DrawChar`: a function pointer to a routine that draws characters,  Automatically set to BIOS-provided routines by the `SetBitmapMode` interface calls.
* `textLibrary`, `drawingLibrary`, `miscLibrary`, and `diskLibrary`: substructures with function pointers to various interface calls.
* `DrawCharFont`: a pointer to the font graphics data that `DrawChar` may use.
* `DrawCharHeight`: used by `DrawChar`, least significant byte holds the cell height and most significant the line height.

## Libraries

### Text

All text library calls can be reached with the `TEXT` shorthand `#define`, like in `TEXT->ClearScreen();`

#### `int Write(const char* format, ...)`

Writes formatted text to the screen. Equivalent to `printf(format, ...)`, which is in fact a `#define`.

#### `int Format(char *buffer, const char* format, ...)`

Writes formatted text to memory. Equivalent to `sprintf(buffer, format, ...)` which is *not* a `#define`.

#### `void WriteChar(char ch)`

Writes a single character to the screen. Equivalent to `putch(ch)` which is *not* a `#define`.

#### `void SetBold(int32_t bold)`

If `bold` is nonzero, enables the bold type bit so characters in text mode are rendered as such, clears it otherwise.

#### `void SetCursorPosition(int32_t left, int32_t top)`

#### `void SetTextColor(int32_t back, int32_t fore)`

#### `void ClearScreen()`



