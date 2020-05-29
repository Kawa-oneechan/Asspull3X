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
			DrawWin7Thing(hwndDlg);
			return true;
		}
		case WM_CTLCOLORSTATIC:
			if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_LINK))
				return false;
			else if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_HEADER))
				SetTextColor((HDC)wParam, RGB(0x00, 0x33, 0x99));
			return (COLOR_WINDOW + 1);
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
