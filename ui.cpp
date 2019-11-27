#include "asspull.h"
#include "ini.h"
#include <vector>

#define LETITSNOW
#define PROPER_ARROW
//#define BLUE_BALLS

#include "nokia.c"

char uiStatus[512] = { 0 };
char uiFPS[32] = { 0 };
int statusTimer = 0;

#define FAIZ(r, g, b) (((b) >> 3) << 10) | (((g) >> 3) << 5) | ((r) >> 3)
#define WITH_SHADOW | 0x8000

//TODO: inactive window border/caption

#define STATUS_TEXT			FAIZ(255, 255, 255) WITH_SHADOW
#define WINDOW_BORDER_L		FAIZ(0, 0, 0)
#define WINDOW_BORDER_T		WINDOW_BORDER_L
#define WINDOW_BORDER_R		WINDOW_BORDER_L
#define WINDOW_BORDER_B		WINDOW_BORDER_L
#ifndef BLUE_BALLS
#define WINDOW_FILL			FAIZ(170, 0, 0)
#define WINDOW_BORDERFOC_L	FAIZ(128, 0, 0)
#else
#define WINDOW_FILL			FAIZ(0, 0, 170)
#define WINDOW_BORDERFOC_L	FAIZ(0, 0, 128)
#endif
#define WINDOW_BORDERFOC_T	WINDOW_BORDERFOC_L
#define WINDOW_BORDERFOC_R	WINDOW_BORDERFOC_L
#define WINDOW_BORDERFOC_B	WINDOW_BORDERFOC_L
#define WINDOW_TEXT			FAIZ(255, 255, 255)
#ifndef BLUE_BALLS
#define WINDOW_CAPTION		FAIZ(255, 0, 4 )
#else
#define WINDOW_CAPTION		FAIZ(0, 4, 255)
#endif
#define WINDOW_CAPTEXT		FAIZ(255, 255, 255) WITH_SHADOW
#define WINDOW_CAPTEXTUNF	FAIZ(142, 142, 142) WITH_SHADOW
#define WINDOW_CAPLINE		WINDOW_CAPTION
#define BUTTON_BORDER_L		FAIZ(142, 142, 142)
#define BUTTON_BORDER_T		BUTTON_BORDER_L
#define BUTTON_BORDER_R		FAIZ(85, 85, 85)
#define BUTTON_BORDER_B		BUTTON_BORDER_R
#define BUTTON_FILL			FAIZ(113, 113, 113)
#define BUTTON_TEXT			WINDOW_TEXT
#define BUTTON_HIGHLIGHT	FAIZ(133, 133, 133)
#define BUTTON_HIGHTEXT		BUTTON_TEXT
#ifndef BLUE_BALLS
#define MENUBAR_FILL		FAIZ(125, 0, 0)
#else
#define MENUBAR_FILL		FAIZ(0, 0, 125)
#endif
#define MENUBAR_TEXT		WINDOW_CAPTEXT
#define MENUBAR_HIGHLIGHT	FAIZ(0, 170, 0)
#define MENUBAR_HIGHTEXT	FAIZ(85, 255, 85) WITH_SHADOW
#define PULLDOWN_BORDER_L	WINDOW_BORDER_L
#define PULLDOWN_BORDER_T	WINDOW_BORDER_T
#define PULLDOWN_BORDER_R	WINDOW_BORDER_R
#define PULLDOWN_BORDER_B	WINDOW_BORDER_B
#define PULLDOWN_FILL		MENUBAR_FILL
#define PULLDOWN_TEXT		MENUBAR_TEXT
#define PULLDOWN_HIGHLIGHT	MENUBAR_HIGHLIGHT
#define PULLDOWN_HIGHTEXT	MENUBAR_HIGHTEXT
#define LISTBOX_FILL		FAIZ(0, 0, 0)
#define LISTBOX_TEXT		WINDOW_TEXT
#define LISTBOX_HIGHLIGHT	MENUBAR_HIGHLIGHT
#define LISTBOX_HIGHTEXT	(MENUBAR_HIGHTEXT) & ~0x8000
#define LISTBOX_TRACK		BUTTON_BORDER_R

inline void RenderRawPixel(int row, int column, int color);
inline void DarkenPixel(int row, int column);
int GetMouseState(int *x, int *y);
void DrawCursor();
void DrawCharacter(int x, int y, int color, unsigned short ch);
void DrawString(int x, int y, int color, const char* str);
int MeasureString(const char* str);
void DrawImage(int x, int y, unsigned short* pixmap, int width, int height);
void LetItSnow();

int uiCommand, uiData;
void* draggingWindow = NULL;
void* focusedWindow = NULL;
int dragStartX, dragStartY;
int dragLeft, dragTop, dragWidth, dragHeight, dragStartLeft, dragStartTop;

class Control
{
public:
	int left, top, width, height;
	int absLeft, absTop;
	int marginLeft, marginTop;
	bool enabled, visible;
	std::string text;
	Control* parent;
	std::vector<std::unique_ptr<Control>> children;
	virtual void Draw() {}
	virtual void Handle() {}
	void AddChild(Control* child)
	{
		child->absLeft = absLeft + child->left + marginLeft;
		child->absTop = absTop + child->top + marginTop;
		child->parent = this;
		children.push_back(std::unique_ptr<Control>(child));
	}
	void Propagate()
	{
		for (auto child = children.begin(); child != children.end(); child++)
		{
			(*child)->absLeft = absLeft + (*child)->left + marginLeft;
			(*child)->absTop = absTop + (*child)->top + marginTop;
			(*child)->Propagate();
		}
	}
	bool WasInside()
	{
		if (!visible) return false;
		if (draggingWindow != NULL)
			return false;
		if (parent != 0 && (parent != focusedWindow && parent->parent != focusedWindow))
			return false;
		int x = 0, y = 0;
		GetMouseState(&x, &y);
		if (x > absLeft && y > absTop && x < absLeft + width && y < absTop + height)
			return true;
		return false;
	}
	bool WasClicked()
	{
		if (!visible) return false;
		if (draggingWindow != NULL)
			return false;
		if (parent != 0 && (parent != focusedWindow && parent->parent != focusedWindow))
			return false;
		int x = 0, y = 0;
		int buttons = GetMouseState(&x, &y);
		if (x > absLeft && y > absTop && x < absLeft + width && y < absTop + height && buttons)
			return true;
		return false;
	}
};

std::vector<std::unique_ptr<Control>> topLevelControls;
void BringWindowToFront(Control* window);

class Label : public Control
{
public:
	int color;
	Label(const char* caption, int left, int top, int color, int width)
	{
		this->text = caption;
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
		this->color = color;
		this->width = (width == 0) ? MeasureString(caption) : width;
	}
	void Draw()
	{
		if (!visible) return;
		DrawString(absLeft, absTop, color, text.c_str());
	}
};

class Image : public Control
{
public:
	unsigned short* pixels;
	Image(const unsigned short* pixels, int left, int top, int width, int height)
	{
		this->pixels = (unsigned short*)pixels;
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
		this->width = width;
		this->height = height;
	}
	void Draw()
	{
		if (!visible) return;
		DrawImage(absLeft, absTop, pixels, width, height);
	}
};

class Window : public Control
{
private:
	bool WasInsideCloseBox()
	{
		if (draggingWindow != NULL)
			return false;
		auto closeButtonLeft = absLeft + width - 12;
		auto closeButtonTop = absTop + 2;
		int x = 0, y = 0;
		GetMouseState(&x, &y);
		if (x > closeButtonLeft && y > closeButtonTop && x < closeButtonLeft + 10 && y < closeButtonTop + 10)
			return true;
		return false;
	}
	bool WasCloseBoxClicked()
	{
		if (draggingWindow != NULL)
			return false;
		auto closeButtonLeft = absLeft + width - 12;
		auto closeButtonTop = absTop + 2;
		int x = 0, y = 0;
		int buttons = GetMouseState(0, 0);
		if (WasInsideCloseBox() && buttons)
			return true;
		return false;
	}
	void DrawCloseBox()
	{
		auto fillColor = BUTTON_FILL;
		auto textColor = (enabled ? BUTTON_TEXT : BUTTON_BORDER_B);
		auto closeButtonLeft = absLeft + width - 12;
		auto closeButtonTop = absTop + 2;
		if (WasInsideCloseBox())
		{
			fillColor = BUTTON_HIGHLIGHT;
			textColor = BUTTON_HIGHTEXT;
		}
		for (auto col = closeButtonLeft; col < closeButtonLeft + 10; col++)
		{
			RenderRawPixel(closeButtonTop, col, BUTTON_BORDER_T);
			RenderRawPixel(closeButtonTop + 9, col, BUTTON_BORDER_B);
		}
		for (auto row = closeButtonTop + 1; row < closeButtonTop + 9; row++)
		{
			RenderRawPixel(row, closeButtonLeft, BUTTON_BORDER_L);
			for (auto col = closeButtonLeft + 1; col < closeButtonLeft + 9; col++)
				RenderRawPixel(row, col, fillColor);
			RenderRawPixel(row, closeButtonLeft + 9, BUTTON_BORDER_R);
		}
		DrawCharacter(closeButtonLeft + 1, closeButtonTop + 1, textColor, 256 + 0);
	}
public:
	Window(const char* caption, int left, int top, int width, int height)
	{
		this->text = caption;
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
		this->width = width;
		this->height = height;
		this->marginLeft = 0;
		this->marginTop = 13;
		this->enabled = true;
		this->visible = true;
		this->parent = 0;
	}
	void Show()
	{
		visible = true;
		BringWindowToFront(this);
	}
	void Hide()
	{
		visible = false;
		if (topLevelControls.size() > 1)
		{
			auto next = topLevelControls.end() - 2;
			BringWindowToFront(next->get());
		}
	}
	void Draw()
	{
		if (!visible) return;
		for (auto col = left; col < left + width; col++)
		{
			RenderRawPixel(top, col, (focusedWindow == this) ? WINDOW_BORDERFOC_T : WINDOW_BORDER_T);
			RenderRawPixel(top + height - 1, col, (focusedWindow == this) ? WINDOW_BORDERFOC_B : WINDOW_BORDER_B);
			DarkenPixel(top + height + 0, col + 2);
			DarkenPixel(top + height + 1, col + 2);
		}
		auto color = WINDOW_CAPTION;
		for (auto row = top + 1; row < top + height - 1; row++)
		{
			RenderRawPixel(row, left, (focusedWindow == this) ? WINDOW_BORDERFOC_L : WINDOW_BORDER_L);
			if (row == top + 12) color = WINDOW_CAPLINE;
			if (row == top + 13) color = WINDOW_FILL;
			for (auto col = left + 1; col < left + width - 1; col++)
				RenderRawPixel(row, col, color);
			RenderRawPixel(row, left + width - 1, (focusedWindow == this) ? WINDOW_BORDERFOC_R : WINDOW_BORDER_R);
			DarkenPixel(row + 1, left + width + 0);
			DarkenPixel(row + 1, left + width + 1);
		}
		DrawString(left + 3, top + 3, (focusedWindow == this) ? WINDOW_CAPTEXT : WINDOW_CAPTEXTUNF, text.c_str());

		DrawCloseBox();
		if (WasCloseBoxClicked())
		{
			Hide();
			return;
		}

		for (auto child = children.begin(); child != children.end(); child++)
			(*child)->Draw();

		int x = 0, y = 0;
		GetMouseState(&x, &y);
		int buttons = SDL_GetMouseState(0, 0); //need actual button state to drag!
		if (buttons == 1 && x > absLeft && y > absTop && x < absLeft + width && y < absTop + 12 && !WasInsideCloseBox())
		{
			if (draggingWindow == NULL)
			{
				draggingWindow = this;
				dragStartX = x - absLeft;
				dragStartY = y - absTop;
				dragLeft = dragStartLeft =  absLeft;
				dragTop = dragStartTop = absTop;
				dragWidth = width;
				dragHeight = height;
			}
		}
		else if (buttons == 1 && draggingWindow == this)
		{
			dragLeft = x - dragStartX;
			dragTop = y - dragStartY;
		}
		else if (buttons == 0 && draggingWindow == this)
		{
			left = absLeft = dragLeft;
			top = absTop = dragTop;
			draggingWindow = NULL;
			Propagate();
		}
		return;
	}
	void Handle()
	{
		if (!visible) return;
		for (auto child = children.begin(); child != children.end(); child++)
			(*child)->Handle();
	}
};

class Button : public Control
{
public:
	void(*onClick)(Control* me);
	Button(const char* caption, int left, int top, int width, void(*click)(Control*))
	{
		this->text = caption;
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
		this->width = width;
		this->height = 13;
		this->onClick = click;
		this->enabled = true;
	}
	void Draw()
	{
		if (!visible) return;
		auto fillColor = BUTTON_FILL;
		auto textColor = (enabled ? BUTTON_TEXT : BUTTON_BORDER_B);
		height = 13;
		if (WasInside() && enabled)
		{
			fillColor = BUTTON_HIGHLIGHT;
			textColor = BUTTON_HIGHTEXT;
		}
		for (auto col = absLeft; col < absLeft + width; col++)
		{
			RenderRawPixel(absTop, col, BUTTON_BORDER_T);
			RenderRawPixel(absTop + height, col, BUTTON_BORDER_B);
		}
		for (auto row = absTop + 1; row < absTop + 13; row++)
		{
			RenderRawPixel(row, absLeft, BUTTON_BORDER_L);
			for (auto col = absLeft + 1; col < absLeft + width - 1; col++)
				RenderRawPixel(row, col, fillColor);
			RenderRawPixel(row, absLeft + width - 1, BUTTON_BORDER_R);
		}
		auto capLeft = absLeft + (width / 2) - (MeasureString(text.c_str()) / 2);
		DrawString(capLeft, absTop + 3, textColor, text.c_str());
	}
	void Handle()
	{
		if (WasClicked() && onClick != NULL)
			onClick(this);
	}
};

class CheckBox : public Control
{
public:
	bool checked;
	void(*onClick)(Control* me);
	CheckBox(const char* caption, int left, int top, int width, bool checked, void(*click)(Control*))
	{
		this->text = caption;
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
		this->width = width;
		this->height = 13;
		this->onClick = click;
		this->enabled = true;
		this->checked = checked;
	}
	void Draw()
	{
		if (!visible) return;
		auto fillColor = BUTTON_FILL;
		auto textColor = (enabled ? BUTTON_TEXT : BUTTON_BORDER_B);
		height = 10;
		if (WasInside() && enabled)
		{
			fillColor = BUTTON_HIGHLIGHT;
			textColor = BUTTON_HIGHTEXT;
		}
		for (auto col = absLeft; col < absLeft + width; col++)
		{
			RenderRawPixel(absTop, col, BUTTON_BORDER_T);
			RenderRawPixel(absTop + height, col, BUTTON_BORDER_B);
		}
		for (auto row = absTop + 1; row < absTop + 9; row++)
		{
			RenderRawPixel(row, absLeft, BUTTON_BORDER_L);
			for (auto col = absLeft + 1; col < absLeft + width - 1; col++)
				RenderRawPixel(row, col, fillColor);
			RenderRawPixel(row, absLeft + width - 1, BUTTON_BORDER_R);
		}
		if (checked)
			DrawCharacter(absLeft + 1, absTop + 1, textColor, 256 + 14);
		DrawString(absLeft + 14, absTop + 3, textColor, text.c_str());
	}
	void Handle()
	{
		if (WasClicked() && onClick != NULL)
			onClick(this);
	}
};

class IconButton : public Control
{
public:
	int icon;
	void(*onClick)(Control*);
	IconButton(int icon, int left, int top, void(*click)(Control*))
	{
		this->icon = icon;
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
		this->width = 10;
		this->height = 10;
		this->onClick = click;
		this->enabled = true;
	}
	void Draw()
	{
		if (!visible) return;
		auto fillColor = BUTTON_FILL;
		auto textColor = (enabled ? BUTTON_TEXT : BUTTON_BORDER_B);
		width = 10;
		height = 10;
		if (WasInside() && enabled)
		{
			fillColor = BUTTON_HIGHLIGHT;
			textColor = BUTTON_HIGHTEXT;
		}
		for (auto col = absLeft; col < absLeft + width; col++)
		{
			RenderRawPixel(absTop, col, BUTTON_BORDER_T);
			RenderRawPixel(absTop + height - 1, col, BUTTON_BORDER_B);
		}
		for (auto row = absTop + 1; row < absTop + 9; row++)
		{
			RenderRawPixel(row, absLeft, BUTTON_BORDER_L);
			for (auto col = absLeft + 1; col < absLeft + width - 1; col++)
				RenderRawPixel(row, col, fillColor);
			RenderRawPixel(row, absLeft + width - 1, BUTTON_BORDER_R);
		}
		DrawCharacter(absLeft + 1, absTop + 1, textColor, 256 + icon);
	}
	void Handle()
	{
		if (WasClicked() && onClick != NULL)
			onClick(this);
	}
};

void doListButton(Control* me);
class ListBox : public Control
{
public:
	int selection;
	int scroll;
	int visible;
	std::vector<std::string> items;
	void(*onChange)(Control*, int);
	ListBox(int left, int top, int width, int height, void(*change)(Control*, int))
	{
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
		this->width = width;
		this->height = height;
		this->onChange = change;
		this->enabled = true;
		this->selection = 0;
		this->scroll = 0;
		this->visible = (height - 2) / 9;
		this->marginLeft = this->marginTop = 0;
		this->AddChild(new IconButton(4, width - 11, 1, doListButton));
		this->AddChild(new IconButton(5, width - 11, (visible * 9) - 9, doListButton));
	}
	void Draw()
	{
		if (!visible) return;
		for (int col = 0; col < width; col++)
		{
			RenderRawPixel(absTop, absLeft + col, WINDOW_BORDERFOC_B);
			RenderRawPixel(absTop + 1 + (visible * 9), absLeft + col, WINDOW_BORDERFOC_T);
		}
		for (int i = 0; i < visible && (i + scroll) < (signed)items.size(); i++)
		{
			int fillColor = (i + scroll == selection) ? LISTBOX_HIGHLIGHT : LISTBOX_FILL;
			int textColor = (i + scroll == selection) ? LISTBOX_HIGHTEXT : LISTBOX_TEXT;
			for (int row = 0; row < 9; row++)
			{
				RenderRawPixel(absTop + 1 + row + (i * 9), absLeft, WINDOW_BORDERFOC_R);
				for (int col = 1; col < width - 11; col++)
				{
					RenderRawPixel(absTop + 1 + row + (i * 9), absLeft + col, fillColor);
				}
				for (int col = width - 11; col < width - 1; col++)
				{
					RenderRawPixel(absTop + 1 + row + (i * 9), absLeft + col, LISTBOX_TRACK);
				}
				RenderRawPixel(absTop + 1 + row + (i * 9), absLeft + width - 1, WINDOW_BORDERFOC_L);
			}
			DrawString(absLeft + 3, absTop + 2 + (i * 9), textColor, items[i + scroll].c_str());
		}
		for (auto child = children.begin(); child != children.end(); child++)
			(*child)->Draw();
	}
	void Handle()
	{
		if (!visible) return;
		if (draggingWindow != NULL)
			return;
		if (parent != 0 && (parent != focusedWindow && parent->parent != focusedWindow))
			return;
		int x = 0, y = 0;
		int buttons = GetMouseState(&x, &y);
		if (x > absLeft && y > absTop && x < absLeft + width - 10 && y < absTop + height && buttons)
		{
			y -= absTop;
			y /= 9;
			selection = y + scroll;
			if (onChange)
				onChange(this, selection);
			return;
		}

		for (auto child = children.begin(); child != children.end(); child++)
			(*child)->Handle();
	}
};
void doListButton(Control* me)
{
	auto what = ((IconButton*)me)->icon;
	auto who = (ListBox*)me->parent;
	if (what == 4 && who->scroll > 0)
		who->scroll--;
	else if (what == 5 && who->scroll < (signed)who->items.size() - who->visible)
		who->scroll++;
}

void* currentMenu = NULL;
int currentMenuTop, currentMenuLeft, currentMenuWidth, currentMenuHeight;

class MenuItem : public Control
{
public:
	char hotkey;
	void(*onClick)(Control*);
	int command;
	MenuItem(const char* caption, char hotkey, void(*click)(Control*))
	{
		this->text = caption;
		this->width = MeasureString(caption) + 10;
		this->hotkey = hotkey;
		this->height = 12;
		this->onClick = click;
		this->command = 0;
		this->enabled = true;
	}
	MenuItem(const char* caption, char hotkey, int uiCommand)
	{
		this->text = caption;
		this->width = MeasureString(caption) + 10;
		this->hotkey = hotkey;
		this->height = 12;
		this->onClick = NULL;
		this->command = uiCommand;
		this->enabled = true;

		if (caption[0] == '-')
		{
			this->enabled = false;
			this->height = 3;
		}
	}
	void Draw()
	{
		if (this->height == 3) //separator
		{
			for (int line = 0; line < 3; line++)
			{
				int lineColor = (line == 1) ? PULLDOWN_BORDER_B : PULLDOWN_FILL;
				RenderRawPixel(absTop + line, absLeft, PULLDOWN_BORDER_L);
				for (int col = 1; col < width; col++)
				{
					RenderRawPixel(absTop + line, absLeft + col, lineColor);
				}
				RenderRawPixel(absTop + line, absLeft + width, PULLDOWN_BORDER_R);
				DarkenPixel(absTop + line + 1, absLeft + width + 1);
				DarkenPixel(absTop + line + 1, absLeft + width + 2);
			}
			return;
		}

		int fillColor = WasInside() ? PULLDOWN_HIGHLIGHT : PULLDOWN_FILL;
		int textColor = WasInside() ? PULLDOWN_HIGHTEXT : PULLDOWN_TEXT;
		for (int line = 0; line < 12; line++)
		{
			RenderRawPixel(absTop + line, absLeft, PULLDOWN_BORDER_L);
			for (int col = 1; col < width; col++)
			{
				RenderRawPixel(absTop + line, absLeft + col, fillColor);
			}
			RenderRawPixel(absTop + line, absLeft + width, PULLDOWN_BORDER_R);
			DarkenPixel(absTop + line + 1, absLeft + width + 1);
			DarkenPixel(absTop + line + 1, absLeft + width + 2);
		}

		DrawString(absLeft + 4, absTop + 2, textColor, text.c_str());
		if (hotkey)
			DrawCharacter(absLeft + width - 8, absTop + 2, textColor, hotkey);
	}
	void Handle()
	{
		if (height == 3) return;
		if (WasClicked())
		{
			currentMenu = NULL;
			if (onClick != NULL && command == 0)
				onClick(this);
			else if (command)
				uiCommand = command;
		}
	}
};

class Menu : public Control
{
private:
	bool sizedTheChildren;
public:
	Menu(const char* caption)
	{
		this->text = caption;
		this->width = MeasureString(caption) + 10;
		this->height = 13;
		this->absTop = 0;
		this->enabled = true;
		this->sizedTheChildren = false;
	}
	void AddChild(Control* child)
	{
		child->absLeft = absLeft + child->left + marginLeft;
		child->absTop = absTop + child->top + marginTop;
		child->parent = 0;
		children.push_back(std::unique_ptr<Control>(child));
	}
	void Draw()
	{
		int fillColor = (WasInside() || currentMenu == this ) ? MENUBAR_HIGHLIGHT : MENUBAR_FILL;
		int textColor = (WasInside() || currentMenu == this ) ? MENUBAR_HIGHTEXT : MENUBAR_TEXT;
		for (int i = absLeft; i < absLeft + width; i++)
		{
			for (int j = 0; j < 12; j++)
			{
				RenderRawPixel(j, i, fillColor);
			}
			DarkenPixel(12, i + 2);
			DarkenPixel(13, i + 2);
		}
		for (int j = 2; j < 12; j++)
		{
			DarkenPixel(j, absLeft + width + 0);
			DarkenPixel(j, absLeft + width + 1);
		}
		DrawString(absLeft + 4, 2, textColor, text.c_str());

		if (currentMenu == this)
		{
			currentMenuLeft = absLeft + 2;
			currentMenuTop = absTop + height + 2;

			int t = currentMenuTop;
			for (auto child = children.begin(); child != children.end(); child++)
			{
				(*child)->absLeft = currentMenuLeft;
				(*child)->absTop = t;
				(*child)->Draw();
				t += (*child)->height;
			}

			currentMenuHeight = t - currentMenuTop;

			if (!sizedTheChildren)
			{
				int w = 1;
				bool hotkeys = false;
				for (auto child = children.begin(); child != children.end(); child++)
				{
					if ((*child)->width > w)
						w = (*child)->width;
					auto mc = (MenuItem*)child->get();
					if (mc->hotkey)
						hotkeys = true;
				}
				if (hotkeys)
					w += 24;
				for (auto child = children.begin(); child != children.end(); child++)
				{
					(*child)->width = w;
				}
				sizedTheChildren = true;
			}

			currentMenuWidth = children.begin()->get()->width;

			for (int col = 0; col <= currentMenuWidth; col++)
			{
				RenderRawPixel(currentMenuTop - 1, currentMenuLeft + col, PULLDOWN_BORDER_T);
				RenderRawPixel(currentMenuTop + currentMenuHeight, currentMenuLeft + col, PULLDOWN_BORDER_B);
				DarkenPixel(currentMenuTop + currentMenuHeight + 1, currentMenuLeft + col + 2);
				DarkenPixel(currentMenuTop + currentMenuHeight + 2, currentMenuLeft + col + 2);
			}
		}
	}
	void Handle()
	{
		if (WasClicked())
		{
			if (children.empty())
				currentMenu = NULL;
			else
				currentMenu = this;
		}

		if (currentMenu == this)
		{
			for (auto child = children.begin(); child != children.end(); child++)
				(*child)->Handle();
		}
	}
};

class MenuBar : public Control
{
public:
	MenuBar()
	{
		this->parent = 0;
	}
	void AddChild(Control* child)
	{
		child->absLeft = absLeft + child->left + marginLeft;
		child->absTop = absTop + child->top + marginTop;
		child->parent = 0;
		children.push_back(std::unique_ptr<Control>(child));
	}
	void Draw()
	{
		int w = 0;
		for (auto child = children.begin(); child != children.end(); child++)
		{
			(*child)->absLeft = w;
			(*child)->absTop = 0;
			(*child)->Draw();
			w += (*child)->width;
		}
	}
	void Handle()
	{
		for (auto child = children.begin(); child != children.end(); child++)
			(*child)->Handle();
	}
};

extern int pauseState;
int mouseTimer = 0, cursorTimer = 0;
MenuBar* menuBar = NULL;

Window* aboutWindow;
Window* memoryWindow;
Window* deviceWindow;
Window* BuildAboutWindow();
Window* BuildMemoryWindow();
Window* BuildDeviceWindow();

void _showAboutDialog(Control*) { aboutWindow->Show(); }
void _showMemoryViewer(Control*) { memoryWindow->Show(); }
void _showDevices(Control*) { deviceWindow->Show(); }

void InitializeUI()
{
	menuBar = new MenuBar();

	auto fileMenu = new Menu("File");
	fileMenu->AddChild(new MenuItem("Load ROM", 'L', cmdLoadRom));
	fileMenu->AddChild(new MenuItem("Unload ROM", 'U', cmdUnloadRom));
	fileMenu->AddChild(new MenuItem("Reset", 'R', cmdReset));
	fileMenu->AddChild(new MenuItem("Quit", 0, cmdQuit));
	menuBar->AddChild(fileMenu);
	auto toolsMenu = new Menu("Tools");
	toolsMenu->AddChild(new MenuItem("Options", 0, 0));
	toolsMenu->AddChild(new MenuItem("-", 0, 0));
	toolsMenu->AddChild(new MenuItem("Devices", 0, _showDevices));
	toolsMenu->AddChild(new MenuItem("Screenshot", 'S', cmdScreenshot));
	toolsMenu->AddChild(new MenuItem("Dump RAM", 'D', cmdDump));
	toolsMenu->AddChild(new MenuItem("Memory viewer", 0, _showMemoryViewer));
	menuBar->AddChild(toolsMenu);

	auto helpMenu = new Menu("Help");
	helpMenu->AddChild(new MenuItem("About", 0, _showAboutDialog));
	menuBar->AddChild(helpMenu);

	aboutWindow = BuildAboutWindow();
	memoryWindow = BuildMemoryWindow();
	deviceWindow = BuildDeviceWindow();
	focusedWindow = aboutWindow;
}

void BringWindowToFront(Control* window)
{
	focusedWindow = window;
	if (topLevelControls.size() < 2)
		return;
	for (auto child = topLevelControls.begin(); child != topLevelControls.end(); child++)
	{
		if (child->get() == window)
		{
			std::iter_swap(child, topLevelControls.end() - 1);
			break;
		}
	}
}

bool CheckForWindowPops()
{
	if (topLevelControls.empty() || draggingWindow != NULL)
		return false;
	int x = 0, y = 0;
	GetMouseState(&x, &y);
	int buttons = SDL_GetMouseState(0, 0);
	if (buttons == 0)
		return 0;
	for (auto child = topLevelControls.end() - 1; ; child--)
	{
		auto win = child->get();
		if (x > win->absLeft && y > win->absTop && x < win->absLeft + win->width && y < win->absTop + win->height)
		{
			BringWindowToFront(win);
			return true;
		}
		if (child == topLevelControls.begin())
			break;
	}
	return false;
}

bool initialized = false;

void HandleUI()
{	
	int x = 0, y = 0;
	mouseTimer++;
	int buttons = GetMouseState(&x, &y);
	if (pauseState == 2)
		LetItSnow();

	if (!initialized)
	{
		InitializeUI();
		initialized = true;
	}

	if (!topLevelControls.empty())
	{
		CheckForWindowPops();

		for (auto child = topLevelControls.begin(); child != topLevelControls.end(); child++)
		{
			(*child)->Draw();
			(*child)->Handle();
		}
	}

	if (draggingWindow != NULL && (dragStartLeft != dragLeft || dragStartTop != dragTop))
	{
		for (int i = dragLeft; i < dragLeft + dragWidth; i += 2)
		{
			RenderRawPixel(dragTop, i, 0x7FFF);
			RenderRawPixel(dragTop + dragHeight, i, 0x7FFF);
		}
		for (int i = dragTop; i < dragTop + dragHeight; i += 2)
		{
			RenderRawPixel(i, dragLeft, 0x7FFF);
			RenderRawPixel(i, dragLeft + dragWidth, 0x7FFF);
		}
	}

	if (currentMenu != NULL && buttons)
	{
		if (x < currentMenuLeft || y < currentMenuTop || x > currentMenuLeft + currentMenuWidth || y > currentMenuTop + currentMenuHeight)
			currentMenu = NULL;
	}

	if (focusedWindow != NULL)
		cursorTimer = 100;

	if (y < 10 || currentMenu != NULL || pauseState == 2)
	{
		menuBar->Draw();
		menuBar->Handle();
		cursorTimer = 100;
	}

	DrawCursor();
}

extern unsigned char* pixels;
inline void RenderRawPixel(int row, int column, int color)
{
	if (row < 0 || row >= 480 || column < 0 || column >= 640) return;
	auto target = (((row) * 640) + (column)) * 4;
	auto r = (color >> 0) & 0x1F;
	auto g = (color >> 5) & 0x1F;
	auto b = (color >> 10) & 0x1F;
	pixels[target + 0] = (b << 3) + (b >> 2);
	pixels[target + 1] = (g << 3) + (g >> 2);
	pixels[target + 2] = (r << 3) + (r >> 2);
}
inline void DarkenPixel(int row, int column)
{
	if (row < 0 || row >= 480 || column < 0 || column >= 640) return;
	auto target = (((row) * 640) + (column)) * 4;
	pixels[target + 0] = pixels[target + 0] / 2;
	pixels[target + 1] = pixels[target + 1] / 2;
	pixels[target + 2] = pixels[target + 2] / 2;
}

int justClicked = 0;
int lastMouseTimer = 0;

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

static unsigned short cursor[] =
{
#ifdef PROPER_ARROW
	0x0001,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0001,0x0001,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0001,0x7FFF,0x0001,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0001,0x7FFF,0x7FFF,0x0001,0x0000,0x0000,0x0000,0x0000,
	0x0001,0x7FFF,0x7FFF,0x7FFF,0x0001,0x0000,0x0000,0x0000,
	0x0001,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x0001,0x0000,0x0000,
	0x0001,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x0001,0x0000,
	0x0001,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x0001,0x8000,0x8000,
	0x0001,0x7FFF,0x0001,0x7FFF,0x7FFF,0x7FFF,0x0001,0x0000,
	0x0001,0x0001,0x8000,0x0001,0x7FFF,0x7FFF,0x0001,0x8000,
	0x0000,0x8000,0x8000,0x8000,0x0001,0x0001,0x8000,0x8000,
	0x0000,0x0000,0x0000,0x0000,0x8000,0x8000,0x8000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
#else
	0x739C,0x77BD,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x2108,0x8000,
	0x6F7B,0x739C,0x77BD,0x77BD,0x7FFF,0x2108,0x2108,0x8000,
	0x6739,0x6F7B,0x739C,0x77BD,0x2108,0x2108,0x8000,0x8000,
	0x6739,0x6F7B,0x6F7B,0x739C,0x77BD,0x8000,0x8000,0x0000,
	0x6739,0x6739,0x2108,0x6F7B,0x739C,0x77BD,0x8000,0x0000,
	0x6739,0x2108,0x2108,0x8000,0x6F7B,0x739C,0x2108,0x8000,
	0x2108,0x2108,0x8000,0x8000,0x8000,0x2108,0x2108,0x8000,
	0x8000,0x8000,0x8000,0x0000,0x0000,0x8000,0x8000,0x8000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
#endif
};

int oldX, oldY;
extern bool customMouse;
void DrawCursor()
{
	int x, y;
	GetMouseState(&x, &y);
	if (oldX != x || oldY != y)
	{
		cursorTimer = 100;
		if (!customMouse)
			SDL_ShowCursor(1);
	}
	oldX = x;
	oldY = y;

	if (cursorTimer == 0)
	{
		if (!customMouse)
			SDL_ShowCursor(0);
		return;
	}

	cursorTimer--;
	if (!customMouse)
		return;

	unsigned short pix = 0;
	for (int row = 0; row < 16; row++)
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
			RenderRawPixel(y + row, x + col, pix);
		}
	}
}

void DrawCharacter(int x, int y, int color, unsigned short ch)
{
	auto glyph = &nokiaFont[ch * 8];
	for (int line = 0; line < 8; line++)
	{
		auto num = glyph[line];
		for (int bit = 0; bit < 8; bit++)
		{
			if (num & 1)
			{
				RenderRawPixel(y + line, x + bit, color & 0x7FFF);
				if (color & 0x8000) DarkenPixel(y + line + 1, x + bit + 1);
			}
			num >>= 1;
		}
	}
}

void DrawImage(int x, int y, unsigned short* pixmap, int width, int height)
{
	for (int row = 0; row < height; row++)
		for (int col = 0; col < width; col++)
			RenderRawPixel(y + row, x + col, pixmap[(row * width) + col]);
}

#define TABWIDTH 48

void DrawString(int x, int y, int color, const char* str)
{
	int sx = x;
	while(*str)
	{
		if (*str == '\t')
			x = ((x / TABWIDTH) * TABWIDTH) + TABWIDTH;
		else if (*str == '\n')
		{
			x = sx;
			y += 10;
		}
		else
		{
			DrawCharacter(x, y, color, *str);
			x += nokiaFontWidth[*str];
		}
		str++;
	}
}

int MeasureString(const char* str)
{
	int width = 0;
	while(*str)
	{
		if (*str == '\t')
			width = ((width / TABWIDTH) * TABWIDTH) + TABWIDTH;
		else
			width += nokiaFontWidth[*str];
		str++;
	}
	return width;
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
		RenderRawPixel(y, x, 0x7FFF);
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

/*
void uiHandleStatusLine(int left)
{
	if (statusTimer)
	{
		DrawString(left, 2, STATUS_TEXT, uiStatus);
		statusTimer--;
	}
	DrawString(640 - 8 - (strlen(uiFPS) * 5), 2, STATUS_TEXT, uiFPS);
}
*/

void _closeWindow(Control* me)
{
	((Window*)me->parent)->Hide();
}

#include "aboutpic.c"
Window* BuildAboutWindow()
{
	auto win = new Window("E Clunibus Tractum", 32, 32, 227, 78);
	win->AddChild(new Image(aboutPic, 1, 0, 144, 64));
	win->AddChild(new Label("Asspull IIIx", 150, 4, 0x07FF, 0));
	win->AddChild(new Label("System design\nand emulator\nby Kawa", 150, 15, WINDOW_TEXT, 0));
	win->AddChild(new Button("Cool", 190, 48, 34, _closeWindow));
	topLevelControls.push_back(std::unique_ptr<Control>(win));
	win->visible = false;
	return win;
}

extern "C" unsigned int m68k_read_memory_8(unsigned int address);
signed long memViewerOffset = MAP1_ADDR;
#define MAXVIEWEROFFSET (0x10000000 - 0x100)

class MemoryViewer : public Control
{
public:
	MemoryViewer(int left, int top)
	{
		this->left = this->absLeft = left;
		this->top = this->absTop = top;
	}
	void Draw()
	{
		if (!visible) return;
		const char hex[] = "0123456789ABCDEF";
		char offsetStr[16] = { 0 };
		unsigned int offset = memViewerOffset;
		for (int row = 0; row < 32; row++)
		{
			sprintf_s(offsetStr, 16, "%08X", offset);
			DrawString(absLeft, absTop + (row * 9), WINDOW_TEXT, offsetStr);
			for (int col = 0; col < 8; col++)
			{
				auto here = m68k_read_memory_8(offset++);
				DrawCharacter(absLeft + 54 + (col * 16), absTop + (row * 9), WINDOW_TEXT, *(hex + ((here & 0xF0) >> 4)));
				DrawCharacter(absLeft + 60 + (col * 16), absTop + (row * 9), WINDOW_TEXT, *(hex + (here & 0x0F)));
				DrawCharacter(absLeft + 184 + (col * 8), absTop + (row * 9), WINDOW_TEXT, here);
			}
		}
	}
};

void _memViewerScroller(Control *me)
{
	auto mods = SDL_GetModState();
	auto bigStep = (mods & KMOD_CTRL) ? 0x01000000 : ((mods & KMOD_SHIFT) ? 0x8000 : 0x1000);
	auto what = ((IconButton*)me)->icon;
	switch (what)
	{
	case 4:
		memViewerOffset -= 0x100;
		break;
	case 5:
		memViewerOffset += 0x100;
		break;
	case 6:
		memViewerOffset -= bigStep;
		break;
	case 7:
		memViewerOffset += bigStep;
		break;
	}
	if (memViewerOffset < 0)
		memViewerOffset = 0;
	else if (memViewerOffset > MAXVIEWEROFFSET)
		memViewerOffset = MAXVIEWEROFFSET;
}

Window* BuildMemoryWindow()
{
	auto win = new Window("Memory Viewer", 128, 128, 265, 324);
	win->AddChild(new MemoryViewer(3, 2));
	win->AddChild(new IconButton(6, 252, 2, _memViewerScroller));
	win->AddChild(new IconButton(4, 252, 12, _memViewerScroller));
	win->AddChild(new IconButton(5, 252, 270, _memViewerScroller));
	win->AddChild(new IconButton(7, 252, 280, _memViewerScroller));
	win->AddChild(new Label("(TODO: add a text field and dropdown hereabouts)", 12, 294, WINDOW_TEXT, 0));
	topLevelControls.push_back(std::unique_ptr<Control>(win));
	win->visible = false;
	return win;
}

ListBox* devManList;
Label* devManHeader;
Label* devManNoOptions;
Label* devManDiskette;
Button* devManEject;
Button* devManInsert;

void _devUpdateDiskette(int devId)
{
	if (devId != devManList->selection)
		return;
	char key[8] = { 0 };
	SDL_itoa(devId, key, 10);
	auto thing = ini->Get("devices/diskDrive", key, "");
	if (thing[0] != 0)
	{
		auto lastSlash = strrchr(thing, '\\');
		if (lastSlash != NULL)
			thing = lastSlash + 1;
		devManDiskette->text = thing;
		devManInsert->enabled = false;
		devManEject->enabled = true;
	}
	else
	{
		devManDiskette->text = "No disk";
		devManInsert->enabled = true;
		devManEject->enabled = false;
	}
}

void _devDiskette(Control* me)
{
	uiData = devManList->selection;
	if (me == devManInsert)
		uiCommand = cmdInsertDisk;
	else if (me == devManEject)
		uiCommand = cmdEjectDisk;
}

void _devSelect(Control* me, int selection)
{
	devManNoOptions->visible = false;
	devManDiskette->visible = false;
	devManEject->visible = false;
	devManInsert->visible = false;
	int devType = 0;
	if (devices[selection] != 0)
	{
		switch (devices[selection]->GetID())
		{
			case 0x0144: devType = 1; break;
			case 0x4C50: devType = 2; break;
		}
	}
	switch (devType)
	{
	case 0:
	case 2:
		devManHeader->text = ((devType == 0) ? "Nothingness" : "Line printer");
		devManNoOptions->visible = true;
		break;
	case 1:
		devManHeader->text = "Disk drive";
		_devUpdateDiskette(selection);
		devManDiskette->visible = true;
		devManEject->visible = true;
		devManInsert->visible = true;
		break;
	}
}

void UpdateDevManList()
{
	char entry[32] = { 0 };
	for (int i = 0; i < MAXDEVS; i++)
	{
		if (devices[i] == 0)
		{
			sprintf_s(entry, 32, "%d. Nothing", i + 1);
		}
		else
		{
			switch (devices[i]->GetID())
			{
			case 0x0144:
				sprintf_s(entry, 32, "%d. Disk drive", i + 1);
				break;
			case 0x4C50:
				sprintf_s(entry, 32, "%d. Line printer", i + 1);
				break;
			}
		}
		devManList->items[i] = entry;
	}
}

Window* BuildDeviceWindow()
{
	auto win = new Window("Devices", 64, 128, 320, 110);
	win->AddChild(devManList = new ListBox(2, 1, 95, 94, _devSelect));
	win->AddChild(devManHeader = new Label("...", 100, 2, 0x07FF, 0));
	win->AddChild(devManNoOptions = new Label("A swirling void\nhowls before you.", 172, 20, WINDOW_TEXT, 0));
	win->AddChild(devManDiskette = new Label("...", 175, 19, WINDOW_TEXT, 0)); //TODO: replace with something with a border
	win->AddChild(devManInsert = new Button("Insert", 232, 31, 39, _devDiskette));
	win->AddChild(devManEject = new Button("Eject", 274, 31, 39, _devDiskette));
	win->AddChild(new Button("Okay", 278, 80, 39, _closeWindow));
	win->Propagate();
	for (int i = 0; i < MAXDEVS; i++)
		devManList->items.push_back("..."); 
	UpdateDevManList();
	_devSelect(devManList, 0);
	topLevelControls.push_back(std::unique_ptr<Control>(win));
	return win;
}

/*
extern bool stretch200, fpsCap;
int _uiOptions(int);
uiWindow optionsWindow = { 0x6969, 64, 64, 256, 128, "Options", _uiOptions };
int _uiOptions(int me)
{
	auto win = &windows[me];
	optionsWindow.left = win->left;
	optionsWindow.top = win->top;
	DrawString(win->left + 4, win->top + 48, WINDOW_TEXT, "Work in obvious progress.");
	if (uiHandleIconButton(win->left + 244, win->top + 2, 0))
	{
		CloseWindow(0x6969);
		return -1;
	}
	else if (uiHandleCheckbox(win->left + 4, win->top + 16, "FPS cap", fpsCap))
	{
		fpsCap = !fpsCap;
		ini->Set("video", "fpscap", (char*)(fpsCap ? "true" : "false"));
	}
	else if (uiHandleCheckbox(win->left + 4, win->top + 32, "Aspect correction", stretch200))
	{
		stretch200 = !stretch200;
		ini->Set("video", "stretch200", (char*)(stretch200 ? "true" : "false"));
	}
	return 0;
}
*/
