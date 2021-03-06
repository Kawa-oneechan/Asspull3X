#include "..\ui.h"

extern unsigned char* ramVideo;

void PalViewerDraw(DRAWITEMSTRUCT* dis)
{
	RECT rect;
	GetClientRect(dis->hwndItem, &rect);
	int w = 16 * 8;

	HDC hdc = CreateCompatibleDC(dis->hDC);
	auto bmp = CreateCompatibleBitmap(dis->hDC, w, w);
	auto oldBmp = SelectObject(hdc, bmp);

	FillRect(hdc, &dis->rcItem, hbrList);

	RECT r;
	r.top = 0;
	r.bottom = 8;
	HBRUSH hbr;
	int c = 0;

	for (auto row = 0; row < 16; row++)
	{
		r.left = 0;
		r.right = 8;
		for (auto row = 0; row < 16; row++)
		{
			auto snes = (ramVideo[PAL_ADDR + (c * 2) + 0] << 8) + ramVideo[PAL_ADDR + (c * 2) + 1];	
			auto red = (snes >> 0) & 0x1F; red = (red << 3) + (red >> 2);
			auto grn = (snes >> 5) & 0x1F; grn = (grn << 3) + (grn >> 2);
			auto blu = (snes >> 10) & 0x1F; blu = (blu << 3) + (blu >> 2);
			hbr = CreateSolidBrush(RGB(red, grn, blu));
			FillRect(hdc, &r, hbr);
			DeleteObject(hbr);
			r.left += 8;
			r.right += 8;
			c++;
		}
		r.top += 8;
		r.bottom += 8;
	}

	//BitBlt(dis->hDC, 0, 0, w, w, hdc, 0, 0, SRCCOPY);
	StretchBlt(dis->hDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hdc, 0, 0, w, w, SRCCOPY);
	SelectObject(hdc, oldBmp);
	DeleteDC(hdc);
	DeleteObject(bmp);
}

BOOL CALLBACK PalViewerWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
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
			//SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, WM_SETFONT, (WPARAM)monoFont, false);
			LPCSTR areas[] = { "BIOS", "Cart", "WRAM", "Devices", "Registers", "VRAM" };
			for (int i = 0; i < 6; i++)
				SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_ADDSTRING, 0, (LPARAM)areas[i]);
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWERDROP, CB_SETCURSEL, 1, 0);
			SendDlgItemMessage(hwndDlg, IDC_MEMVIEWEROFFSET, EM_SETLIMITTEXT, 8, 0);
			CheckDlgButton(hwndDlg, IDC_AUTOUPDATE, autoUpdatePalViewer);
			return true;
		}
		case WM_SHOWWINDOW:
		{
			if (wParam)
				InvalidateRect(GetDlgItem(hWndPalViewer, IDC_MEMVIEWERGRID), NULL, true);
		}
		case WM_COMMAND:
		{
			if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_REFRESH)
			{
				InvalidateRect(GetDlgItem(hWndPalViewer, IDC_MEMVIEWERGRID), NULL, true);
				return true;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_AUTOUPDATE)
			{
				autoUpdatePalViewer = (IsDlgButtonChecked(hwndDlg, IDC_AUTOUPDATE) == 1);
				return true;
			}
		}
		case WM_DRAWITEM:
		{
			if (wParam == IDC_MEMVIEWERGRID)
			{
				PalViewerDraw((DRAWITEMSTRUCT*)lParam);
				return true;
			}
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
		case WM_CTLCOLORBTN:
		{
			return (INT_PTR)hbrBack;
		}
	}
	return false;
}

void ShowPalViewer()
{
	if (!IsWindow(hWndPalViewer))
	{
		hWndPalViewer = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_PALVIEWER), (HWND)hWnd, (DLGPROC)PalViewerWndProc);
		ShowWindow(hWndPalViewer, SW_SHOW);
	}
}
