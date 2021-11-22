#include "..\asspull.h"

#define PROMO_W 280
#define PROMO_H 160
#define DANCE_X 550
#define DANCE_Y 80
#define DANCE_W 37
#define DANCE_H 56
#define DANCE_F 8

namespace UI
{
	namespace About
	{
		using namespace Presentation;

		HWND hWnd = NULL;
		HBITMAP hPromoImage = NULL;
		HIMAGELIST hDanceIml = NULL;
		int aboutFrame = 0;
		RECT aboutRect = { DANCE_X, DANCE_Y, DANCE_X + DANCE_W, DANCE_Y + DANCE_H };

		void CALLBACK Animate(HWND a, UINT b, UINT_PTR c, DWORD d)
		{
			a, b, c, d;
			aboutFrame++;
			if (aboutFrame == DANCE_F)
				aboutFrame = 0;

			InvalidateRect(hWnd, &aboutRect, false);
		}

		BOOL CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_INITDIALOG:
			{
				hPromoImage = Images::LoadPNGResource(IDB_ABOUT);
				hDanceIml = ImageList_Create(DANCE_W, DANCE_H, ILC_COLOR32, DANCE_F, 0);
				ImageList_Add(hDanceIml, Images::LoadPNGResource(IDB_DANCE), NULL);
				SendDlgItemMessage(hWnd, IDC_HEADER, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
				SetTimer(hWnd, 1, 100, Animate);
				return true;
			}
			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				if (ps.rcPaint.left != aboutRect.left)
				{
					DrawWindowBk(hWnd, true, &ps, hdc);
					auto hdcMem = CreateCompatibleDC(hdc);
					auto oldBitmap = SelectObject(hdcMem, hPromoImage);
					BitBlt(hdc, 0, 0, PROMO_W, PROMO_H, hdcMem, 0, 0, SRCCOPY);
					SelectObject(hdcMem, oldBitmap);
					DeleteDC(hdcMem);
				}
				else
					FillRect(hdc, &ps.rcPaint, hbrBack);

				ImageList_Draw(hDanceIml, aboutFrame, hdc, DANCE_X, DANCE_Y, ILD_NORMAL);
				EndPaint(hWnd, &ps);
				return true;
			}
			case WM_CTLCOLORSTATIC:
			{
				SetBkColor((HDC)wParam, rgbBack);
				SetTextColor((HDC)wParam, rgbText);
				if (lParam == (LPARAM)GetDlgItem(hWnd, IDC_LINK))
				{
					SetBkColor((HDC)wParam, rgbStripe);
					return (INT_PTR)hbrStripe;
				}
				else if (lParam == (LPARAM)GetDlgItem(hWnd, IDC_HEADER))
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
				KillTimer(hWnd, 1);
				DestroyWindow(hWnd);
				About::hWnd = NULL;
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
					if ((((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd, IDC_LINK)) && (item.iLink == 0))
						ShellExecuteA(NULL, "open", "https://github.com/Kawa-oneechan/Asspull3X", NULL, NULL, SW_SHOW);
					break;
				}
				case NM_CUSTOMDRAW:
				{
					auto nmc = (LPNMCUSTOMDRAW)lParam;
					int idFrom = nmc->hdr.idFrom;
					switch (idFrom)
					{
					case IDOK: return Presentation::DrawDarkButton(hWnd, nmc);
						break;
					}
				}
				}
			}
			return false;
		}

		void Show()
		{
			if (!IsWindow(hWnd))
			{
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_ABOUT), (HWND)hWndMain, (DLGPROC)WndProc);
				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}
