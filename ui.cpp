#include "asspull.h"
#include "ini.h"

extern "C" {

#include "zfont.c"

char uiStatus[512] = { 0 };
char uiFPS[32] = { 0 };
int statusTimer = 0;

#define STATUS_TEXT 0x7FFF
#define BAR_FILL 0x000F
#define BAR_HIGHLIGHT 0x001F
#define BAR_TEXT 0x7FFF
#define PULLDOWN_BORDER 0x0008
#define PULLDOWN_FILL 0x000F
#define PULLDOWN_HIGHLIGHT 0x001F
#define PULLDOWN_TEXT 0x7FFF

typedef struct uiMenuItem
{
	char caption[256];
	int(*onSelect)(int, int, int);
} uiMenuItem;

typedef struct uiMenu
{
	int numItems;
	uiMenuItem items[16];
} uiMenu;

int _uiMainMenu(int, int, int);
int _uiLoadROM(int, int, int);
int _uiUnloadROM(int, int, int);
int _uiReset(int, int, int);
int _uiQuit(int, int, int);

const uiMenu mainMenu =
{
	2, { { "File", _uiMainMenu }, { "Device", _uiMainMenu } }
};

const uiMenu fileMenu =
{
	4,
	{
		{ "Load ROM", _uiLoadROM },
		{ "Unload ROM", _uiUnloadROM },
		{ "Reset", _uiReset },
		{ "Quit", _uiQuit }
	}
};

int pullDownLevel = 0;
uiMenu* pullDowns[4] = { 0 };
int pullDownLefts[4] = { 0 };
int pullDownTops[4] = { 0 };

extern unsigned char* pixels;
#define RenderPixel(row, column, color) \
{ \
	auto target = (((row) * 640) + (column)) * 4; \
	auto r = (color >> 0) & 0x1F; \
	auto g = (color >> 5) & 0x1F; \
	auto b = (color >> 10) & 0x1F; \
	pixels[target + 0] = (b << 3) + (b >> 2); \
	pixels[target + 1] = (g << 3) + (g >> 2); \
	pixels[target + 2] = (r << 3) + (r >> 2); \
}

void DrawCharacter(int x, int y, int color, char ch)
{
	auto glyph = &zsnesFont[(ch - ' ') * 5];
	for (int line = 0; line < 5; line++)
	{
		auto num = glyph[line];
		for (int bit = 0; bit < 8; bit++)
		{
			if (num & 1)
			{
				RenderPixel(y + line, x + 8 - bit, color);
				RenderPixel(y + line + 1, x + 8 - bit + 1, 0);
			}
			num >>= 1;
		}
	}
}

void DrawString(int x, int y, int color, char* str)
{
	while(*str)
	{
		DrawCharacter(x, y, color, *str++);
		x += 6;
	}
}

#ifdef WITH_OPENGL
int GetMouseState(int *x, int *y)
{
	int buttons = SDL_GetMouseState(x, y);

	int winWidth, winHeight;
	SDL_GetWindowSize(sdlWindow, &winWidth, &winHeight);
	int scrWidth = (winWidth / 640) * 640;
	int scrHeight = (winHeight / 480) * 480;
	scrWidth = (int)(scrHeight * 1.33334f);
	int minx = (winWidth - scrWidth) / 2;
	int miny = (winHeight - scrHeight) / 2;
	float scaleX = 640.0f / winWidth;
	float scaleY = 480.0f / winHeight;
	*x = (int)(*x * scaleX);
	*y = (int)(*y * scaleY);
	*x -= minx;
	*y -= miny;	
	return buttons;
}
#else
#define GetMouseState SDL_GetMouseState
#endif

void SetStatus(char* text)
{
	strcpy_s(uiStatus, 512, text);
	statusTimer = 100;
}

void uiHandleStatusLine(int left)
{
	if (statusTimer)
	{
		DrawString(left, 2, STATUS_TEXT, uiStatus);
		statusTimer--;
	}
	DrawString(640 - 8 - (strlen(uiFPS) * 5), 2, STATUS_TEXT, uiFPS);
}

int uiHandleMenuDrop(uiMenu* menu, int left, int top, int *openedTop)
{
	int width = 0;

	for (int i = 0; i < menu->numItems; i++)
	{
		int thisWidth = (strlen(menu->items[i].caption) * 6) + 8;
		if (thisWidth > width)
			width = thisWidth;
	}

	int focused = -1;
	int x = 0, y = 0;
	int buttons = GetMouseState(&x, &y);
	for (int i = 0; i < menu->numItems; i++)
	{
		if (x >= left && x < left + width && y >= top + (i * 8) && y < top + (i * 8) + 8)
		{
			focused = i;
			break;
		}
	}

	for (int col = 0; col < width + 2; col++)
	{
		RenderPixel(top - 1, left + col, PULLDOWN_BORDER);
		RenderPixel(top + (menu->numItems * 8), left + col, PULLDOWN_BORDER);
	}
	for (int i = 0, y = top; i < menu->numItems; i++, y += 8)
	{
		for (int line = 0; line < 8; line++)
		{
			RenderPixel(y + line, left, PULLDOWN_BORDER);
			int color = PULLDOWN_FILL;
			if (i == focused)
				color = PULLDOWN_HIGHLIGHT;
			for (int col = 0; col < width; col++)
			{
				RenderPixel(y + line, left + 1 + col, color);
			}
			RenderPixel(y + line, left + 1 + width, PULLDOWN_BORDER);
		}

		DrawString(left + 2, y + 1, PULLDOWN_TEXT, menu->items[i].caption);
	}

	if (buttons == 1 && focused != -1)
	{
		if (menu->items[focused].onSelect)
			return menu->items[focused].onSelect(focused, left + width, top + (focused * 8));
		return focused;
	}
	if (buttons == 1 && focused == -1 && y > 10)
		pullDownLevel = 0;

	return 0;
}

int uiHandleMenuBar(uiMenu* menu, int *openedLeft)
{
	int extents[16];
	for (int i = 0; i < menu->numItems; i++)
		extents[i] = (strlen(menu->items[i].caption) * 6) + 10;
	for (int i = 1; i < menu->numItems; i++)
		extents[i] += extents[i - 1];
	int width = extents[menu->numItems - 1] + 8;
	int x = 0, y = 0;
	int buttons = GetMouseState(&x, &y);

	uiHandleStatusLine(width + 8);

	int focused = -1;
	int focusL = -32, focusR = -32;
	if (x < extents[0])
	{
		focused = 0;
		focusL = 0;
		focusR = extents[0];
	}
	else if (y < 10)
	{
		for (int i = 1; i < menu->numItems; i++)
		{
			if (x >= extents[i - 1] && x < extents[i])
			{
				focused = i;
				focusL = extents[i - 1];
				focusR = extents[i];
				break;
			}
		}
	}

	for (int i = 0; i < width; i++)
	{
		int color = BAR_FILL;
		if (i >= focusL && i < focusR)
			color = BAR_HIGHLIGHT;
		for (int j = 0; j < 10; j++)
		{
			RenderPixel(j, i, color);
		}
	}
	for (int i = 0, x = 4; i < menu->numItems; i++, x = extents[i - 1] + 4)
	{
		DrawString(x, 2, BAR_TEXT, menu->items[i].caption);
	}

	if (buttons == 1 && focused != -1)
	{
		*openedLeft = focusL;
		if (menu->items[focused].onSelect)
			return menu->items[focused].onSelect(focused, focusL, 0);
		return focused;
	}

	return -2;
}

void HandleUI()
{
	int x = 0, y = 0;
	int buttons = GetMouseState(&x, &y);
	if (pullDownLevel > 0)
	{
		auto response = uiHandleMenuBar(&(uiMenu)mainMenu, &x);
		response = uiHandleMenuDrop(pullDowns[0], pullDownLefts[0], pullDownTops[0], &y);
	}
	else if (y < 10)
	{
		auto response = uiHandleMenuBar(&(uiMenu)mainMenu, &x);
	}
	else
	{
		uiHandleStatusLine(4);
	}
}

int _uiMainMenu(int item, int itemLeft, int itemTop)
{
	switch (item)
	{
	case 0: //File
		pullDownLevel = 1;
		pullDowns[0] = (uiMenu*)&fileMenu;
		pullDownLefts[0] = itemLeft + 2;
		pullDownTops[0] = 10;
		break;
	case 1: //Devices
		pullDownLevel = 0;
		break;
	}
	return 0;
}

int uiCommand, uiData;

int _uiLoadROM(int, int, int)
{
	pullDownLevel = 0;
	uiCommand = cmdLoadRom;
	return 0;
}
int _uiUnloadROM(int, int, int)
{
	pullDownLevel = 0;
	uiCommand = cmdUnloadRom;
	return 0;
}
int _uiReset(int, int, int)
{
	pullDownLevel = 0;
	uiCommand = cmdReset;
	uiData = (SDL_GetModState() & KMOD_CTRL);
	return 0;
}
int _uiQuit(int, int, int)
{
	pullDownLevel = 0;
	uiCommand = cmdQuit;
	return 0;
}

}