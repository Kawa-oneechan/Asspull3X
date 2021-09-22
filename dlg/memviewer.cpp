#include "..\ui.h"

#define BYTES 16
#define LINES 16
#define PAGE (BYTES * LINES)
#define MAXRANGE (VRAM_ADDR + VRAM_SIZE - PAGE)

unsigned long memViewerOffset;
extern "C" unsigned int m68k_read_memory_8(unsigned int address);

extern unsigned char* ramVideo;

void MemViewerDraw(DRAWITEMSTRUCT* dis)
{
	RECT rect;
	GetClientRect(dis->hwndItem, &rect);
	int w = 584; //rect.right - rect.left;
	int h = 262; //rect.bottom - rect.top;

	HDC hdc = CreateCompatibleDC(dis->hDC);
	auto bmp = CreateCompatibleBitmap(dis->hDC, w, h);
	auto oldBmp = SelectObject(hdc, bmp);

	FillRect(hdc, &dis->rcItem, hbrList);
	auto oldFont = SelectObject(hdc, monoFont);
	SIZE fontSize;
	GetTextExtentPoint(hdc, L"0", 1, &fontSize);
	SetTextColor(hdc, rgbList);
	SetBkColor(hdc, rgbListBk);

	RECT r;
	r.top = 3;
	r.left = 3;
	r.bottom = r.top + fontSize.cy;
	r.right = rect.right - 3;

	WCHAR buf[256] = { 0 };

	const WCHAR hex[] = L"0123456789ABCDEF";
	unsigned int offset = memViewerOffset;
	for (int row = 0; row < LINES; row++)
	{
		r.left = 3;
		wsprintf(buf, L"%08X", offset);
		DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);

		for (int col = 0; col < BYTES; col++)
		{
			auto here = m68k_read_memory_8(offset++);
			r.left = 3 + 80 + (col * 22);
			wsprintf(buf, L"%c%c", hex[(here & 0xF0) >> 4], hex[here & 0x0F]);
			DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
			r.left = 3 + 442 + (col * fontSize.cx);
			//sprintf(buf, "%c", here);
			//DrawText(hdc, buf, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);

			unsigned char* glyph = (ramVideo + FONT_ADDR + 0x1000) + (here * 16);
			for (auto line = 0; line < 16; line++)
			{
				unsigned char scan = glyph[line];
				for (auto bit = 0; bit < 8; bit++)
				{
					SetPixel(hdc, r.left + bit, r.top + line, ((scan >> bit) & 1) == 1 ? rgbList : rgbListBk);
				}
			}
		}

		r.top += fontSize.cy;
		r.bottom += fontSize.cy;
	}
	SelectObject(hdc, oldFont);

	//BitBlt(dis->hDC, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
	StretchBlt(dis->hDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hdc, 0, 0, w, h, SRCCOPY);
	SelectObject(hdc, oldBmp);
	DeleteDC(hdc);
	DeleteObject(bmp);
}

void CALLBACK MemViewerAutoUpdate(HWND a, UINT b, UINT_PTR c, DWORD d)
{
	a, b, c, d;
	InvalidateRect(GetDlgItem(hWndMemViewer, IDC_MEMVIEWERGRID), NULL, true);
}

void SetMemViewer(HWND hwndDlg, int to)
{
	if (to < 0) to = 0;
	if (to > MAXRANGE) to = MAXRANGE;
	memViewerOffset = to;
	WCHAR asText[64] = { 0 };
	wsprintf(asText, L"%08X", memViewerOffset);
	SetDlgItemText(hwndDlg, IDC_MEMVIEWEROFFSET, asText);
	InvalidateRect(GetDlgItem(hwndDlg, IDC_MEMVIEWERGRID), NULL, true);
	SetScrollPos(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, to / BYTES, true);
}

void MemViewerScroll(HWND hwndDlg, int message, int position)
{
	position;
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
	uint32_t areas[] = { BIOS_ADDR, CART_ADDR, WRAM_ADDR + 0x1000, DEVS_ADDR, REGS_ADDR, VRAM_ADDR };
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
					WCHAR thing[16] = { 0 };
					GetWindowText(wnd, thing, 16);
					SetMemViewer(hWndMemViewer, wcstol(thing, NULL, 16));
					return 0;
			}
		default:
			return CallWindowProc(oldTextProc, wnd, msg, wParam, lParam);
	}
}

BOOL CALLBACK MemViewerWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
		{
			DestroyWindow(hwndDlg);
			KillTimer(hwndDlg, 1);
			hWndMemViewer = NULL;
			return true;
		}
		case WM_INITDIALOG:
		{
			//SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, WM_SETFONT, (WPARAM)monoFont, false);
			for (int i = 0; i < 6; i++)
				SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_REGIONS + i));
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_SETCURSEL, 1, 0);
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, EM_SETLIMITTEXT, 8, 0);
			oldTextProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_MEMVIEWEROFFSET), GWLP_WNDPROC, (LONG_PTR)MemViewerEditProc);
			SetScrollRange(GetDlgItem(hwndDlg, IDC_MEMVIEWERSCROLL), SB_CTL, 0, ((VRAM_ADDR + VRAM_SIZE) / BYTES) - LINES, false);
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
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_REFRESH)
			{
				InvalidateRect(GetDlgItem(hWndMemViewer, IDC_MEMVIEWERGRID), NULL, true);
				return true;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_AUTOUPDATE)
			{
				if (IsDlgButtonChecked(hwndDlg, IDC_AUTOUPDATE))
					SetTimer(hwndDlg, 1, 100, MemViewerAutoUpdate);
				else
					KillTimer(hwndDlg, 1);
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
			else if (wParam == IDC_MEMVIEWERDROP)
				return DrawComboBox((LPDRAWITEMSTRUCT)lParam);
			return false;
		}
		case WM_PAINT:
		{
			DrawWindowBk(hwndDlg, false);
			return true;
		}
		case WM_NOTIFY:
		{
			if(((LPNMHDR)lParam)->code == NM_CUSTOMDRAW)
			{
				auto nmc = (LPNMCUSTOMDRAW)lParam;
				if (nmc->hdr.idFrom == IDC_AUTOUPDATE)
				{
					DrawCheckbox(hwndDlg, nmc);
					return true;
				}
				else if (nmc->hdr.idFrom == IDC_REFRESH)
					return DrawDarkButton(hwndDlg, nmc);
			}
		}
		case WM_CTLCOLORSTATIC:
		{
			SetTextColor((HDC)wParam, rgbText);
			if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_MEMVIEWERGRID))
				return (INT_PTR)hbrList;
			return (INT_PTR)hbrBack;
		}
		case WM_CTLCOLOREDIT:
		{	
			SetTextColor((HDC)wParam, rgbList);
			SetBkColor((HDC)wParam, rgbListBk);
			return (INT_PTR)hbrList;
		}
		case WM_CTLCOLORBTN:
		{
			return (INT_PTR)hbrBack;
		}
		case WM_VSCROLL:
		{
			MemViewerScroll(hwndDlg, LOWORD(wParam), HIWORD(wParam));
			return true;
		}
		case WM_MOUSEWHEEL:
		{
			auto delta = GET_WHEEL_DELTA_WPARAM(wParam);
			auto inc = PAGE;
			if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)
				inc *= 8;
			if (delta > 0)
				inc = -inc;
			SetMemViewer(hwndDlg, memViewerOffset + inc);
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
