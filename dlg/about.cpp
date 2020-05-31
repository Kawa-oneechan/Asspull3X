#include "..\ui.h"

BOOL CALLBACK AboutWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			SendDlgItemMessage(hwndDlg, IDC_HEADER, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
			return true;
		}
		case WM_PAINT:
		{
			DrawWindowBk(hwndDlg, true);
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
