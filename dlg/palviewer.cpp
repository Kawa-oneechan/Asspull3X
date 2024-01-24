#include "..\asspull.h"

extern unsigned char* ramVideo;

namespace UI
{
	namespace PalViewer
	{
		using namespace Presentation;

#define GRIDWIDTH 256
#define GRIDHEIGHT 512
#define GRIDCOLS 16
#define GRIDROWS 32
#define GRIDBORDER 2

		HWND hWnd = NULL;
		int currentIndex = 0;

		BITMAPINFO bmpInfo;
		unsigned char *bmpData = NULL;

		HBITMAP gridBitmap = NULL;

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
			bmpInfo.bmiHeader.biWidth = GRIDWIDTH;
			bmpInfo.bmiHeader.biHeight = -GRIDHEIGHT;
			auto hdc = CreateCompatibleDC(dis->hDC);
			if (gridBitmap == NULL)
				gridBitmap = CreateCompatibleBitmap(dis->hDC, GRIDWIDTH, GRIDHEIGHT);

			int c = 0;
			int skip = GRIDWIDTH * 4 - 16 * 4;
			for (int row = 0; row < GRIDCOLS; row++)
			{
				for (int col = 0; col < GRIDCOLS; col++)
				{
					unsigned char *start = bmpData + row * 16 * GRIDWIDTH * 4 + col * 16 * 4;

					auto snes = (ramVideo[PAL_ADDR + (c * 2) + 0] << 8) + ramVideo[PAL_ADDR + (c * 2) + 1];
					auto r = (snes >> 0) & 0x1F; r = (r << 3) + (r >> 2);
					auto g = (snes >> 5) & 0x1F; g = (g << 3) + (g >> 2);
					auto b = (snes >> 10) & 0x1F; b = (b << 3) + (b >> 2);

					for (int i = 0; i < 16; i++)
					{
						for (int j = 0; j < 16; j++)
						{
							*start++ = b;
							*start++ = g;
							*start++ = r;
							*start++ = 0;
						}
						start += skip;
					}
					c++;
				}
			}

			auto oldBitmap = SelectObject(hdc, gridBitmap);
			SetDIBitsToDevice(hdc, 0, 0, GRIDWIDTH, GRIDHEIGHT, 0, 0, 0, GRIDHEIGHT, bmpData, &bmpInfo, DIB_RGB_COLORS);
			BitBlt(dis->hDC, 0, 0, GRIDWIDTH, GRIDHEIGHT, hdc, 0, 0, SRCCOPY);

			int row = currentIndex / GRIDCOLS;
			int col = currentIndex % GRIDCOLS;
			RECT selection = { col * 16, row * 16, col * 16 + 16, row * 16 + 16 };
			InvertRect(dis->hDC, &selection);
			InflateRect(&selection, -1, -1);
			InvertRect(dis->hDC, &selection);

			SelectObject(hdc, oldBitmap);
			DeleteDC(hdc);
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
				DeleteObject(gridBitmap);
				gridBitmap = NULL;
				DestroyWindow(hWnd);
				KillTimer(hWnd, 1);
				PalViewer::hWnd = NULL;
				delete[] bmpData;
				bmpData = NULL;
				return true;
			}
			case WM_SIZE:
			{
				HWND hwndGrid = GetDlgItem(hWnd, IDC_MEMVIEWERGRID);
				SetWindowPos(hwndGrid, 0, 0, 0, GRIDWIDTH + GRIDBORDER, GRIDHEIGHT + GRIDBORDER, SWP_NOMOVE | SWP_NOZORDER);
				HWND hwndDetails = GetDlgItem(hWnd, IDC_DETAILS);
				SetWindowPos(hwndDetails, 0, GRIDWIDTH + GRIDBORDER + 32, 48, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
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
					int y = py / (h / 32);
					if (x > 15 || y > 31)
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
						return DrawButton(hWnd, nmc);
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

				memset(&bmpInfo, 0, sizeof(bmpInfo));

				bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
				bmpInfo.bmiHeader.biWidth = GRIDWIDTH;
				bmpInfo.bmiHeader.biHeight = GRIDHEIGHT;
				bmpInfo.bmiHeader.biPlanes = 1;
				bmpInfo.bmiHeader.biBitCount = 32;
				bmpInfo.bmiHeader.biCompression = BI_RGB;
				if (bmpData == NULL)
					bmpData = new unsigned char[4 * 32 * 64 * 64]();

				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}
