#include "..\ui.h"

HBITMAP hPromoImage = NULL;
#define PROMO_W 280
#define PROMO_H 160
HIMAGELIST hDanceIml = NULL;
int aboutFrame = 0;
#define DANCE_X 550
#define DANCE_Y 80
#define DANCE_W 37
#define DANCE_H 56
#define DANCE_F 8
RECT aboutRect = { DANCE_X, DANCE_Y, DANCE_X + DANCE_W, DANCE_Y + DANCE_H };

void CALLBACK AnimateAbout(HWND a, UINT b, UINT_PTR c, DWORD d)
{
	a, b, c, d;
	aboutFrame++;
	if (aboutFrame == DANCE_F)
		aboutFrame = 0;

	InvalidateRect(hWndAbout, &aboutRect, false);
}

BOOL CALLBACK AboutWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			hPromoImage = LoadImageFromPNG(IDB_ABOUT);
			hDanceIml = ImageList_Create(DANCE_W, DANCE_H, ILC_COLOR32, DANCE_F, 0);
			ImageList_Add(hDanceIml, LoadImageFromPNG(IDB_DANCE), NULL);
			SendDlgItemMessage(hwndDlg, IDC_HEADER, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
			SetTimer(hwndDlg, 1, 100, AnimateAbout);
			return true;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwndDlg, &ps);
			if (ps.rcPaint.left != aboutRect.left)
			{
				DrawWindowBk(hwndDlg, true, &ps, hdc);
				auto hdcMem = CreateCompatibleDC(hdc);
				auto oldBitmap = SelectObject(hdcMem, hPromoImage);
				BitBlt(hdc, 0, 0, PROMO_W, PROMO_H, hdcMem, 0, 0, SRCCOPY);
				SelectObject(hdcMem, oldBitmap);
				DeleteDC(hdcMem);
			}
			else
				FillRect(hdc, &ps.rcPaint, hbrBack);

			ImageList_Draw(hDanceIml, aboutFrame, hdc, DANCE_X, DANCE_Y, ILD_NORMAL);
			EndPaint(hwndDlg, &ps);
			return true;
		}
		case WM_CTLCOLORSTATIC:
		{
			SetBkColor((HDC)wParam, rgbBack);
			SetTextColor((HDC)wParam, rgbText);
			if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_LINK))
			{
				SetBkColor((HDC)wParam, rgbStripe);
				return (INT_PTR)hbrStripe;
			}
			else if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_HEADER))
				SetTextColor((HDC)wParam, rgbHeader);
			return (INT_PTR)hbrBack;
		}
		case WM_CTLCOLORBTN:
		{
			return (INT_PTR)hbrBack;
		}
		case WM_CLOSE:
		case WM_COMMAND:
		{
			DeleteObject(hPromoImage);
			ImageList_Destroy(hDanceIml);
			KillTimer(hwndDlg, 1);
			DestroyWindow(hwndDlg);
			hWndAbout = NULL;
			return true;
		}
		case WM_NOTIFY:
			switch (((LPNMHDR)lParam)->code)
			{
				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK pNMLink = (PNMLINK)lParam;
					LITEM item = pNMLink->item;
					if ((((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hwndDlg, IDC_LINK)) && (item.iLink == 0))
						ShellExecuteA(NULL, "open", "https://github.com/Kawa-oneechan/Asspull3X", NULL, NULL, SW_SHOW);
					break;
				}
				case NM_CUSTOMDRAW:
				{
					auto nmc = (LPNMCUSTOMDRAW)lParam;
					int idFrom = nmc->hdr.idFrom;
					switch(idFrom)
					{
						case IDOK: return DrawDarkButton(hwndDlg, nmc);
						break;
					}
				}
			}
	}
	return false;
}

void ShowAbout()
{
	if (!IsWindow(hWndAbout))
	{
		hWndAbout = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_ABOUT), (HWND)hWnd, (DLGPROC)AboutWndProc);
		ShowWindow(hWndAbout, SW_SHOW);
	}
}
