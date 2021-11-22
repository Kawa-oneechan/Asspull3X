#include "..\asspull.h"

extern unsigned char* ramVideo;

namespace UI
{
	namespace PalViewer
	{
		using namespace Presentation;

		HWND hWnd = NULL;
		int currentIndex = 0;

		void UpdateDetails()
		{
			auto snes = (ramVideo[PAL_ADDR + (currentIndex * 2) + 0] << 8) + ramVideo[PAL_ADDR + (currentIndex * 2) + 1];
			auto r = (snes >> 0) & 0x1F;
			auto g = (snes >> 5) & 0x1F;
			auto b = (snes >> 10) & 0x1F;
			auto red = r; red = (red << 3) + (red >> 2);
			auto grn = g; grn = (grn << 3) + (grn >> 2);
			auto blu = b; blu = (blu << 3) + (blu >> 2);
			WCHAR buffer[256] = { 0 };
			wsprintf(buffer, GetString(IDS_PALDETAILS), currentIndex, snes, r, red, g, grn, b, blu);
			SetDlgItemText(hWnd, IDC_DETAILS, buffer);
		}

		void DrawGrid(DRAWITEMSTRUCT* dis)
		{
			RECT rect;
			GetClientRect(dis->hwndItem, &rect);
			int w = rect.right - rect.left;
			int h = rect.bottom - rect.top;
			w += 2;
			h += 2;
			int cellW = w / 16;
			int cellH = h / 16;

			HDC hdc = dis->hDC;
			HBRUSH hbr;
			RECT r;
			r.top = 0;
			r.bottom = cellH;
			int c = 0;
			for (int row = 0; row < 16; row++)
			{
				r.left = 0;
				r.right = cellW;
				for (int col = 0; col < 16; col++)
				{
					auto snes = (ramVideo[PAL_ADDR + (c * 2) + 0] << 8) + ramVideo[PAL_ADDR + (c * 2) + 1];
					auto red = (snes >> 0) & 0x1F; red = (red << 3) + (red >> 2);
					auto grn = (snes >> 5) & 0x1F; grn = (grn << 3) + (grn >> 2);
					auto blu = (snes >> 10) & 0x1F; blu = (blu << 3) + (blu >> 2);
					hbr = CreateSolidBrush(RGB(red, grn, blu));
					FillRect(hdc, &r, hbr);
					if (c == currentIndex)
					{
						InvertRect(hdc, &r);
						InflateRect(&r, -1, -1);
						InvertRect(hdc, &r);
						InflateRect(&r, 1, 1);
					}
					DeleteObject(hbr);
					r.left += cellW;
					r.right += cellW;
					c++;
				}
				r.top += cellH;
				r.bottom += cellH;
			}

			UpdateDetails();
		}

		void CALLBACK AutoUpdate(HWND a, UINT b, UINT_PTR c, DWORD d)
		{
			a, b, c, d;
			InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
		}

		BOOL CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_CLOSE:
			{
				DestroyWindow(hWnd);
				KillTimer(hWnd, 1);
				PalViewer::hWnd = NULL;
				return true;
			}
			case WM_SIZE:
			{
				RECT rctGrid;
				HWND hwndGrid = GetDlgItem(hWnd, IDC_MEMVIEWERGRID);
				GetWindowRect(hwndGrid, &rctGrid);
				int w = rctGrid.right - rctGrid.left;
				int h = rctGrid.bottom - rctGrid.top;
				int cellW = w / 16;
				int cellH = h / 16;
				w = cellW * 16;
				h = cellH * 16;
				w += 2;
				h += 2;
				SetWindowPos(hwndGrid, 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
				return true;
			}
			case WM_SHOWWINDOW:
			{
				if (wParam)
					InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
			}
			case WM_COMMAND:
			{
				if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_REFRESH)
				{
					InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
					return true;
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_AUTOUPDATE)
				{
					if (IsDlgButtonChecked(hWnd, IDC_AUTOUPDATE))
						SetTimer(hWnd, 1, 100, AutoUpdate);
					else
						KillTimer(hWnd, 1);
					return true;
				}
			}
			case WM_LBUTTONDOWN:
			{
				POINTS ptlHit = MAKEPOINTS(lParam);
				RECT rctGrid;
				GetWindowRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), &rctGrid);
				POINT tl = { rctGrid.left, rctGrid.top };
				ScreenToClient(hWnd, &tl);
				int w = rctGrid.right - rctGrid.left;
				int h = rctGrid.bottom - rctGrid.top;
				if (ptlHit.x > tl.x && ptlHit.y > tl.y && ptlHit.x < tl.x + w && ptlHit.y < tl.y + h)
				{
					int px = (long)ptlHit.x - tl.x;
					int py = (long)ptlHit.y - tl.y;
					int x = px / (w / 16);
					int y = py / (h / 16);
					if (x > 15 || y > 15)
						return true;
					currentIndex = (y * 16) + x;
					InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
				}
				return true;
			}
			case WM_DRAWITEM:
			{
				if (wParam == IDC_MEMVIEWERGRID)
				{
					DrawGrid((DRAWITEMSTRUCT*)lParam);
					return true;
				}
				return false;
			}
			case WM_PAINT:
			{
				DrawWindowBk(hWnd, false);
				return true;
			}
			case WM_NOTIFY:
			{
				if (((LPNMHDR)lParam)->code == NM_CUSTOMDRAW)
				{
					auto nmc = (LPNMCUSTOMDRAW)lParam;
					if (nmc->hdr.idFrom == IDC_AUTOUPDATE)
					{
						DrawCheckbox(hWnd, nmc);
						return true;
					}
					else if (nmc->hdr.idFrom == IDC_REFRESH)
						return DrawDarkButton(hWnd, nmc);
				}
			}
			case WM_CTLCOLORSTATIC:
			{
				SetBkColor((HDC)wParam, rgbBack);
				SetTextColor((HDC)wParam, rgbText);
				if (lParam == (LPARAM)GetDlgItem(hWnd, IDC_MEMVIEWERGRID))
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

		void Show()
		{
			if (!IsWindow(hWnd))
			{
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_PALVIEWER), (HWND)hWndMain, (DLGPROC)WndProc);
				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}
