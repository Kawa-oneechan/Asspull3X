#include "..\ui.h"

#define BYTES 16
#define LINES 16
#define PAGE (BYTES * LINES)
#define MAXRANGE (VRAM_ADDR + VRAM_SIZE - PAGE)

unsigned long memViewerOffset;
extern "C" unsigned int m68k_read_memory_8(unsigned int address);

void MemViewerDraw(DRAWITEMSTRUCT* dis)
{
	//auto hdc = dis->hDC;
	RECT rect;
	GetClientRect(dis->hwndItem, &rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	HDC hdc = CreateCompatibleDC(NULL);
	auto bmp = CreateCompatibleBitmap(hdc, w, h);
	auto oldBmp = SelectObject(hdc, bmp);

	FillRect(hdc, &dis->rcItem, (HBRUSH)GetStockObject(WHITE_BRUSH));
	auto oldFont = SelectObject(hdc, monoFont);
	SIZE fontSize;
	GetTextExtentPoint(hdc, "0", 1, &fontSize);
	SetTextColor(hdc, RGB(0,0,0));
	SetBkColor(hdc, RGB(255,255,255));
	RECT r;
	r.top = 3;
	r.left = 3;
	r.bottom = r.top + fontSize.cy;
	r.right = rect.right - 3;

	char buf[256] = { 0 };

	const char hex[] = "0123456789ABCDEF";
	unsigned int offset = memViewerOffset;
	for (int row = 0; row < LINES; row++)
	{
		r.left = 3;
		sprintf(buf, "%08X", offset);
		DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);

		for (int col = 0; col < BYTES; col++)
		{
			auto here = m68k_read_memory_8(offset++);
			r.left = 3 + 80 + (col * 22);
			sprintf(buf, "%c%c", hex[(here & 0xF0) >> 4], hex[here & 0x0F]);
			DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
			r.left = 3 + 442 + (col * fontSize.cx);
			sprintf(buf, "%c", here);
			DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
		}

		r.top += fontSize.cy;
		r.bottom += fontSize.cy;
	}

	SelectObject(hdc, oldFont);

	BitBlt(dis->hDC, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
	SelectObject(hdc, oldBmp);
	DeleteDC(hdc);
	DeleteObject(bmp);
}

void SetMemViewer(HWND hwndDlg, int to)
{
	if (to < 0) to = 0;
	if (to > MAXRANGE) to = MAXRANGE;
	memViewerOffset = to;
	char asText[64] = { 0 };
	sprintf_s(asText, 64, "%08X", memViewerOffset);
	SetDlgItemText(hwndDlg, IDC_MEMVIEWEROFFSET, asText);
	InvalidateRect(GetDlgItem(hwndDlg, IDC_MEMVIEWERGRID), NULL, true);
	SetScrollPos(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, to / BYTES, true);
}

void MemViewerScroll(HWND hwndDlg, int message, int position)
{
	switch (message)
	{
		case SB_BOTTOM:
			SetMemViewer(hwndDlg, MAXRANGE);
			return;
		case SB_TOP:
			SetMemViewer(hwndDlg, 0);
			return;
		case SB_LINEDOWN:
			SetMemViewer(hwndDlg, memViewerOffset + BYTES);
			return;
		case SB_LINEUP:
			SetMemViewer(hwndDlg, memViewerOffset - BYTES);
			return;
		case SB_PAGEDOWN:
			SetMemViewer(hwndDlg, memViewerOffset + PAGE);
			return;
		case SB_PAGEUP:
			SetMemViewer(hwndDlg, memViewerOffset - PAGE);
			return;
		case SB_THUMBPOSITION:
		{
			SCROLLINFO si;
			ZeroMemory(&si, sizeof(si));
			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, &si);
			auto newTo = si.nTrackPos;
			SetMemViewer(hwndDlg, newTo * BYTES);
		}
	}
}

void MemViewerComboProc(HWND hwndDlg)
{
	int index = SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_GETCURSEL, 0, 0);
	uint32_t areas[] = { BIOS_ADDR, CART_ADDR, WRAM_ADDR, DEVS_ADDR, REGS_ADDR, VRAM_ADDR };
	SetMemViewer(hwndDlg, areas[index]);
}

WNDPROC oldTextProc;
BOOL CALLBACK MemViewerEditProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYDOWN:
			switch (wParam)
			{
				case VK_RETURN:
					char thing[16] = { 0 };
					GetWindowText(wnd, thing, 16);
					SetMemViewer(hWndMemViewer, strtol(thing, NULL, 16));
					return 0;
			}
		default:
			return CallWindowProc(oldTextProc, wnd, msg, wParam, lParam);
	}
	return 0;
}

BOOL CALLBACK MemViewerWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
		{
			DestroyWindow(hwndDlg);
			hWndMemViewer = NULL;
			return true;
		}
		case WM_INITDIALOG:
		{
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, WM_SETFONT, (WPARAM)monoFont, false);
			LPCSTR areas[] = { "BIOS", "Cart", "WRAM", "Devices", "Registers", "VRAM" };
			for (int i = 0; i < 6; i++)
				SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_ADDSTRING, 0, (LPARAM)areas[i]);
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_SETCURSEL, 1, 0);
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, EM_SETLIMITTEXT, 8, 0);
			oldTextProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_MEMVIEWEROFFSET), GWLP_WNDPROC, (LONG_PTR)MemViewerEditProc);
			SetScrollRange(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, 0, ((VRAM_ADDR + VRAM_SIZE) / BYTES) - LINES, false);
			CheckDlgButton(hwndDlg, IDC_AUTOUPDATE, autoUpdate);
			MemViewerComboProc(hwndDlg); //force update
			return true;
		}
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDC_MEMVIEWERDROP)
			{
				if (HIWORD(wParam) == CBN_SELCHANGE)
				{
					MemViewerComboProc(hwndDlg);
					return true;
				}
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_AUTOUPDATE)
			{
				autoUpdate = (IsDlgButtonChecked(hwndDlg, IDC_AUTOUPDATE) == 1);
				return true;
			}
		}
		case WM_DRAWITEM:
		{
			if (wParam == IDC_MEMVIEWERGRID)
			{
				MemViewerDraw((DRAWITEMSTRUCT*)lParam);
				return true;
			}
			return false;
		}
		case WM_CTLCOLORSTATIC:
		{	
			if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_MEMVIEWERGRID))
				return (COLOR_WINDOW+1);
			else
				return false;
		}
		case WM_VSCROLL:
		{
			MemViewerScroll(hwndDlg, LOWORD(wParam), HIWORD(wParam));
			return true;
		}
	}
	return false;
}

void ShowMemViewer()
{
	if (!IsWindow(hWndMemViewer))
	{
		hWndMemViewer = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MEMVIEWER), (HWND)hWnd, (DLGPROC)MemViewerWndProc);
		ShowWindow(hWndMemViewer, SW_SHOW);
	}
}
