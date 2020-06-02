#include "..\ui.h"

HANDLE aboutAnim = NULL;
int aboutFrame = 0;
#define DANCE_X 550
#define DANCE_Y 80
#define DANCE_W 37
#define DANCE_H 56
#define DANCE_F 8
RECT aboutRect = { DANCE_X, DANCE_Y, DANCE_X + DANCE_W, DANCE_Y + DANCE_H };

void AnimateAbout()
{
	static int lastTick = 0;

	int ticksNow = GetTickCount();
	if (ticksNow - lastTick < 100)
		return;
	lastTick = ticksNow;

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
			if (aboutAnim == NULL)
				aboutAnim = LoadImage(hInstance, MAKEINTRESOURCE(IDB_DANCE), IMAGE_BITMAP, 0, 0, LR_LOADTRANSPARENT);
			SendDlgItemMessage(hwndDlg, IDC_HEADER, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
			return true;
		}
		case WM_PAINT:
		{
			//DrawWindowBk(hwndDlg, true);
			//gotta repeat most of that function so we can do the dance
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwndDlg, &ps);
			RECT rect = { 0 };
			rect.bottom = 7 + 14 + 7; //margin, button, padding
			MapDialogRect(hwndDlg, &rect);
			auto h = rect.bottom;
			GetClientRect(hwndDlg, &rect);
			rect.bottom -= h;
			FillRect(hdc, &ps.rcPaint, hbrStripe);
			FillRect(hdc, &rect, hbrBack);

			auto hdcMem = CreateCompatibleDC(hdc);
			auto oldBitmap = SelectObject(hdcMem, aboutAnim);
			BitBlt(hdc, DANCE_X, DANCE_Y, DANCE_W, DANCE_H, hdcMem, DANCE_W * aboutFrame, DANCE_H, SRCAND);
			BitBlt(hdc, DANCE_X, DANCE_Y, DANCE_W, DANCE_H, hdcMem, DANCE_W * aboutFrame, 0, SRCPAINT);
			SelectObject(hdcMem, oldBitmap);
			DeleteDC(hdcMem);
			EndPaint(hwndDlg, &ps);
			return true;
		}
		case WM_CTLCOLORSTATIC:
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
		case WM_CLOSE:
		case WM_COMMAND:
		{
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
