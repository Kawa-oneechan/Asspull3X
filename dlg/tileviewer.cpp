#include "..\asspull.h"

#define MAXRANGE 31
extern unsigned char* ramVideo;

namespace UI
{
	namespace TileViewer
	{
		using namespace Presentation;

#define GRIDWIDTH 256
#define GRIDHEIGHT 512
#define GRIDCOLS 32
#define GRIDROWS 64
#define GRIDBORDER 2
#define ZOOMSIZE 64

		HWND hWnd = NULL;
		int currentIndex = 0;
		int currentPal = 0;

		BITMAPINFO bmpInfo, zoomInfo;
		unsigned char *bmpData = NULL;

		HBITMAP gridBitmap = NULL;

		void UpdateDetails()
		{
			WCHAR buffer[256] = { 0 };
			wsprintf(buffer, GetString(IDS_TILEDETAILS), currentIndex, currentIndex, currentPal);
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
			int skip = GRIDWIDTH * 4 - 8 * 4;
			int offset = TILES_ADDR;
			int palStart = PAL_ADDR + (currentPal * 32);
			for (int row = 0; row < GRIDCOLS; row++)
			{
				for (int col = 0; col < GRIDCOLS; col++)
				{
					unsigned char *start = bmpData + row * 8 * GRIDWIDTH * 4 + col * 8 * 4;
					
					for (int i = 0; i < 8; i++)
					{
						for (int j = 0; j < 4; j++)
						{
							unsigned char two = ramVideo[offset++];
							auto p = (two >> 0) & 0x0F;
							auto snes = (ramVideo[palStart + (p * 2) + 0] << 8) + ramVideo[palStart + (p * 2) + 1];
							auto r = (snes >> 0) & 0x1F; r = (r << 3) + (r >> 2);
							auto g = (snes >> 5) & 0x1F; g = (g << 3) + (g >> 2);
							auto b = (snes >> 10) & 0x1F; b = (b << 3) + (b >> 2);

							*start++ = b;
							*start++ = g;
							*start++ = r;
							*start++ = 0;

							p = (two >> 4) & 0x0F;
							snes = (ramVideo[palStart + (p * 2) + 0] << 8) + ramVideo[palStart + (p * 2) + 1];
							r = (snes >> 0) & 0x1F; r = (r << 3) + (r >> 2);
							g = (snes >> 5) & 0x1F; g = (g << 3) + (g >> 2);
							b = (snes >> 10) & 0x1F; b = (b << 3) + (b >> 2);

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
			RECT selection = { col * 8, row * 8, col * 8 + 8, row * 8 + 8 };
			InvertRect(dis->hDC, &selection);
			InflateRect(&selection, -1, -1);
			InvertRect(dis->hDC, &selection);

			SelectObject(hdc, oldBitmap);
			DeleteDC(hdc);

			UpdateDetails();
		}

		void DrawZoom(DRAWITEMSTRUCT* dis)
		{
			int row = currentIndex / GRIDCOLS;
			int col = currentIndex % GRIDCOLS;

			auto hdc = CreateCompatibleDC(dis->hDC); 
			auto oldBitmap = SelectObject(hdc, gridBitmap);
			SetDIBitsToDevice(hdc, 0, 0, GRIDWIDTH, GRIDHEIGHT, 0, 0, 0, GRIDHEIGHT, bmpData, &bmpInfo, DIB_RGB_COLORS);

			StretchBlt(dis->hDC, 0, 0, ZOOMSIZE, ZOOMSIZE, hdc, col * 8, row * 8, 8, 8, SRCCOPY);

			SelectObject(hdc, oldBitmap);
			DeleteDC(hdc);
		}

		void CALLBACK AutoUpdate(HWND a, UINT b, UINT_PTR c, DWORD d)
		{
			a, b, c, d;
			InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
		}

		void ScrollTo(int to)
		{
			if (to < 0) to = 0;
			if (to > MAXRANGE) to = MAXRANGE;
			currentPal = to;
			InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
			InvalidateRect(GetDlgItem(hWnd, IDC_ZOOMIN), NULL, true);
			SetScrollPos(GetDlgItem(hWnd, IDC_MEMVIEWERSCROLL), SB_CTL, to, true);
		}

		void ScrollMsg(int message, int position)
		{
			position;
			switch (message)
			{
			case SB_BOTTOM:
				ScrollTo(MAXRANGE);
				return;
			case SB_TOP:
				ScrollTo(0);
				return;
			case SB_LINEDOWN:
				ScrollTo(currentPal + 1);
				return;
			case SB_LINEUP:
				ScrollTo(currentPal - 1);
				return;
			case SB_PAGEDOWN:
				ScrollTo(currentPal + 8);
				return;
			case SB_PAGEUP:
				ScrollTo(currentPal - 8);
				return;
			case SB_THUMBPOSITION:
			{
				SCROLLINFO si;
				ZeroMemory(&si, sizeof(si));
				si.cbSize = sizeof(si);
				si.fMask = SIF_TRACKPOS;
				GetScrollInfo(GetDlgItem(hWnd, IDC_MEMVIEWERSCROLL), SB_CTL, &si);
				ScrollTo(si.nTrackPos);
			}
			}
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
				HWND hwndZoom = GetDlgItem(hWnd, IDC_ZOOMIN);
				SetWindowPos(hwndZoom, 0, GRIDWIDTH + GRIDBORDER + 32, 96, ZOOMSIZE + GRIDBORDER, ZOOMSIZE + GRIDBORDER, SWP_NOZORDER);
				HWND hwndScroll = GetDlgItem(hWnd, IDC_MEMVIEWERSCROLL);
				SetWindowPos(hwndScroll, 0, GRIDWIDTH + GRIDBORDER + 32, 96 + ZOOMSIZE + GRIDBORDER + 16, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
				return true;
			}
			case WM_SHOWWINDOW:
			{
				if (wParam)
				{
					InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
					InvalidateRect(GetDlgItem(hWnd, IDC_ZOOMIN), NULL, true);
				}
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
					int x = px / (w / 32);
					int y = py / (h / 64);
					if (x > 31 || y > 63)
						return true;
					currentIndex = (y * 32) + x;
					InvalidateRect(GetDlgItem(hWnd, IDC_MEMVIEWERGRID), NULL, true);
					InvalidateRect(GetDlgItem(hWnd, IDC_ZOOMIN), NULL, true);

				}
				return true;
			}
			case WM_HSCROLL:
			{
				ScrollMsg(LOWORD(wParam), HIWORD(wParam));
				return true;
			}
			case WM_MOUSEWHEEL:
			{
				auto delta = GET_WHEEL_DELTA_WPARAM(wParam);
				auto inc = 1;
				if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)
					inc *= 8;
				if (delta > 0)
					inc = -inc;
				ScrollTo(currentPal + inc);
			}
			case WM_DRAWITEM:
			{
				if (wParam == IDC_MEMVIEWERGRID)
				{
					DrawGrid((DRAWITEMSTRUCT*)lParam);
					return true;
				}
				else if (wParam == IDC_ZOOMIN)
				{
					DrawZoom((DRAWITEMSTRUCT*)lParam);
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
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_TILEVIEWER), (HWND)hWndMain, (DLGPROC)WndProc);
				SetScrollRange(GetDlgItem(hWnd, IDC_MEMVIEWERSCROLL), SB_CTL, 0, MAXRANGE, false);

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
