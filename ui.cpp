#include "asspull.h"
#include "ini.h"

extern "C" {

#define LETITSNOW

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
int currentTopMenu = -1;

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

#define DarkenPixel(row, column) \
{ \
	auto target = (((row) * 640) + (column)) * 4; \
	pixels[target + 0] = pixels[target + 0] / 2; \
	pixels[target + 1] = pixels[target + 1] / 2; \
	pixels[target + 2] = pixels[target + 2] / 2; \
}

int justClicked = 0;
int mouseTimer = 0, lastMouseTimer = 0;
#ifdef WITH_OPENGL
int GetMouseState(int *x, int *y)
{
	int buttons = SDL_GetMouseState(x, y);
	if (mouseTimer == lastMouseTimer)
		return (justClicked == 2) ? 1 : 0;
	lastMouseTimer = mouseTimer;
	if (justClicked == 0 && buttons == 1)
		justClicked = 1;
	else if (justClicked == 1 && buttons == 0)
		justClicked = 2;
	else if (justClicked == 2)
		justClicked = 0;

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
	return (justClicked == 2) ? 1 : 0;
}
#else
int GetMouseState(int *x, int *y)
{
	int buttons = SDL_GetMouseState(x, y);
	if (justClicked == 0 && buttons == 1)
		justClicked = 1;
	else if (justClicked == 1 && buttons == 0)
		justClicked = 2;
	else if (justClicked == 2)
		justClicked = 0;
	return justClicked == 1;
}
#endif

static unsigned short cursor[] =
{
	0x739C,0x77BD,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x2108,0x8000,
	0x6F7B,0x739C,0x77BD,0x77BD,0x7FFF,0x2108,0x2108,0x8000,
	0x6739,0x6F7B,0x739C,0x77BD,0x2108,0x2108,0x8000,0x8000,
	0x6739,0x6F7B,0x6F7B,0x739C,0x77BD,0x8000,0x8000,0x0000,
	0x6739,0x6739,0x2108,0x6F7B,0x739C,0x77BD,0x8000,0x0000,
	0x6739,0x2108,0x2108,0x8000,0x6F7B,0x739C,0x2108,0x8000,
	0x2108,0x2108,0x8000,0x8000,0x8000,0x2108,0x2108,0x8000,
	0x8000,0x8000,0x8000,0x0000,0x0000,0x8000,0x8000,0x8000,
};

int oldX, oldY, cursorTimer = 1000;
void DrawCursor()
{
	int x, y;
	GetMouseState(&x, &y);
	if (oldX != x || oldY != y)
		cursorTimer = 100;
	oldX = x;
	oldY = y;
	
	if (cursorTimer == 0)
		return;

	cursorTimer--;

	unsigned short pix = 0;
	for (int row = 0; row < 8; row++)
	{
		if (y + row >= 480)
			break;
		for (int col = 0; col < 8; col++)
		{
			pix = cursor[(row * 8) + col];
			if (x + col >= 640)
				break;
			if (pix == 0x0000)
				continue;
			if (pix == 0x8000)
			{
				DarkenPixel(y + row, x + col);
				continue;
			}
			RenderPixel(y + row, x + col, pix);
		}
	}
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

#ifdef LETITSNOW
#define MAXSNOW 256
int snowData[MAXSNOW * 2];
int snowTimer = -1;
void LetItSnow()
{
	if (snowTimer == -1)
	{
		//Prepare
		srand(0xC001FACE);
		for (int i = 0; i < MAXSNOW; i++)
		{
			snowData[(i * 2) + 0] = rand() % 640;
			snowData[(i * 2) + 1] = rand() % 480;
		}
		snowTimer = 1;
	}

	snowTimer--;
	if (snowTimer == 0)
	{
		snowTimer = 4;
		for (int i = 0; i < MAXSNOW; i++)
		{
			snowData[(i * 2) + 0] += -1 + (rand() % 3);
			snowData[(i * 2) + 1] += rand() % 2;
			if (snowData[(i * 2) + 1] >= 480)
			{
				snowData[(i * 2) + 0] = rand() % 640;
				snowData[(i * 2) + 1] = -10;
			}
		}
	}

	for (int i = 0; i < MAXSNOW; i++)
	{
		int x = snowData[(i * 2) + 0];
		int y = snowData[(i * 2) + 1];
		if (x < 0 || y < 0 || x >= 640 || y >= 480)
			continue;
		RenderPixel(y, x, 0x7FFF);
	}
}
#else
#define LetItSnow()
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
	for (int y = top; y <= top + (menu->numItems * 8); y++)
	{
		DarkenPixel(y + 2, left + 2 + width);
		DarkenPixel(y + 2, left + 3 + width);
	}
	for (int col = 0; col < width; col++)
	{
		DarkenPixel(top + (menu->numItems * 8) + 1, col + 4);
		DarkenPixel(top + (menu->numItems * 8) + 2, col + 4);
	}

	if (buttons == 1 && focused != -1)
	{
		currentTopMenu = -1;
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
	int otherFocusL = -32, otherFocusR = -32;
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
	if (currentTopMenu != -1)
	{
		if (currentTopMenu == 0)
			otherFocusL = 0;
		else
			otherFocusL = extents[currentTopMenu - 1];
		otherFocusR = extents[currentTopMenu];
	}

	for (int i = 0; i < width; i++)
	{
		int color = BAR_FILL;
		if ((i >= focusL && i < focusR) || (i >= otherFocusL && i < otherFocusR))
			color = BAR_HIGHLIGHT;
		for (int j = 0; j < 10; j++)
		{
			RenderPixel(j, i, color);
		}
		DarkenPixel(10, i);
		DarkenPixel(11, i);
	}
	for (int j = 0; j < 12; j++)
	{
		DarkenPixel(j, width + 0);
		DarkenPixel(j, width + 1);
	}

	for (int i = 0, x = 4; i < menu->numItems; i++, x = extents[i - 1] + 4)
	{
		DrawString(x, 2, BAR_TEXT, menu->items[i].caption);
	}

	if (buttons == 1 && focused != -1)
	{
		currentTopMenu = focused;
		*openedLeft = focusL;
		if (menu->items[focused].onSelect)
			return menu->items[focused].onSelect(focused, focusL, 0);
		return focused;
	}

	return -2;
}

extern int pauseState;
void HandleUI()
{
	int x = 0, y = 0;
	mouseTimer++;
	int buttons = GetMouseState(&x, &y);
	if (pauseState == 2)
		LetItSnow();
	if (pullDownLevel > 0)
	{
		auto response = uiHandleMenuBar(&(uiMenu)mainMenu, &x);
		response = uiHandleMenuDrop(pullDowns[0], pullDownLefts[0], pullDownTops[0], &y);
		cursorTimer = 100;
	}
	else if (y < 10 || pauseState == 2)
	{
		auto response = uiHandleMenuBar(&(uiMenu)mainMenu, &x);
		cursorTimer = 100;
	}
	else
	{
		uiHandleStatusLine(4);
	}
	DrawCursor();
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