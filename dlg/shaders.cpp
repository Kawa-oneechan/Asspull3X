#include "..\ui.h"

BOOL CALLBACK ShadersWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
		{
			DestroyWindow(hwndDlg);
			hWndShaders = NULL;
			if (!wasPaused) pauseState = 0;
			return true;
		}
		case WM_INITDIALOG:
		{
			SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_DIR, DDL_READWRITE, (LPARAM)"*.fs");

			//TEST: try moving the fourth item up
			char fourth[256];
			SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_GETTEXT, 4, (LPARAM)fourth);
			SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_DELETESTRING, 4, 0);
			SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_INSERTSTRING, 3, (LPARAM)fourth);

			return true;
		}
		case WM_NOTIFY:
		{
			switch (((LPNMHDR)lParam)->code)
			{
				case NM_CUSTOMDRAW:
				{
					auto nmc = (LPNMCUSTOMDRAW)lParam;
					int idFrom = nmc->hdr.idFrom;
					switch(idFrom)
					{
						case IDOK:
						case IDCANCEL:
						case IDC_ADDSHADER:
						case IDC_REMOVESHADER:
						case IDC_MOVESHADERUP:
						case IDC_MOVESHADERDOWN:
							return DrawDarkButton(hwndDlg, nmc);
						break;
					}
				}
			}
		}
		case WM_PAINT:
		{
			DrawWindowBk(hwndDlg, true);
			return true;
		}
		case WM_CTLCOLORLISTBOX:
		{
			SetTextColor((HDC)wParam, rgbList);
			SetBkColor((HDC)wParam, rgbListBk);
			return (INT_PTR)hbrList;
		}
		case WM_CTLCOLORSTATIC:
		{
			SetTextColor((HDC)wParam, rgbText);
			SetBkColor((HDC)wParam, rgbBack);
			return (INT_PTR)hbrBack;
		}
		case WM_CTLCOLORBTN:
		{
			return (INT_PTR)hbrBack;
		}
		case WM_COMMAND:
		{
			if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK)
			{
				DestroyWindow(hwndDlg);
				hWndDevices = NULL;
				if (!wasPaused) pauseState = 0;
				return true;
			}
		}
	}
	return false;
}

void ShowShaders()
{
	if (!IsWindow(hWndShaders))
	{
		wasPaused = pauseState > 0;
		if (!wasPaused) pauseState = 1;
		hWndShaders = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_SHADERS), (HWND)hWnd, (DLGPROC)ShadersWndProc);
		ShowWindow(hWndShaders, SW_SHOW);
	}
}
