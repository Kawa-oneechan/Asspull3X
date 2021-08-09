#include "..\ui.h"

static void EnableShaderButtons(HWND hwndDlg)
{
	EnableWindow(GetDlgItem(hwndDlg, IDC_ADDSHADER),
		(SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) < MAXSHADERS) &&
		(SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_GETCURSEL, 0, 0) != LB_ERR)
	);

	auto inUseCount = SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0);
	auto inUseSel = SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_GETCURSEL, 0, 0);

	auto allowInUse = (inUseCount > 0) && (inUseSel != LB_ERR);

	EnableWindow(GetDlgItem(hwndDlg, IDC_REMOVESHADER), allowInUse);
	EnableWindow(GetDlgItem(hwndDlg, IDC_MOVESHADERUP), allowInUse);
	EnableWindow(GetDlgItem(hwndDlg, IDC_MOVESHADERDOWN), allowInUse);
}

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

			/*
			//TEST: try moving the fourth item up
			char fourth[256];
			SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_GETTEXT, 4, (LPARAM)fourth);
			SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_DELETESTRING, 4, 0);
			SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_INSERTSTRING, 3, (LPARAM)fourth);
			*/

			auto numShaders = ini.GetLongValue("video", "shaders", -1);
			for (int i = 0; i < numShaders; i++)
			{
				char key[16] = { 0 };
				sprintf(key, "shader%d", i + 1);
				SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_ADDSTRING, 0, (LPARAM)ini.GetValue("video", key, ""));
			}

			EnableShaderButtons(hwndDlg);
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
			if (HIWORD(wParam) == LBN_SELCHANGE)
			{
				EnableShaderButtons(hwndDlg);
			}
			else if (HIWORD(wParam) == LBN_DBLCLK)
			{
				//Fake clicking the Add/Remove buttons depending on which list it was
				if (LOWORD(wParam) == IDC_SHADERSAVAILABLE)
					SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDC_ADDSHADER, BN_CLICKED), 0);
				else if (LOWORD(wParam) == IDC_SHADERSINUSE)
					SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDC_REMOVESHADER, BN_CLICKED), 0);
			}
			else if (HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
				case IDOK:
					{
						DestroyWindow(hwndDlg);
						hWndDevices = NULL;
						if (!wasPaused) pauseState = 0;
						return true;
					}
				case IDC_ADDSHADER:
					{
						if (SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) >= MAXSHADERS)
							return false;
						auto sel = SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_GETCURSEL, 0, 0);
						if (sel == LB_ERR)
							return false;
						char selText[256];
						SendDlgItemMessage(hwndDlg, IDC_SHADERSAVAILABLE, LB_GETTEXT, sel, (LPARAM)selText);
						SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_ADDSTRING, 0, (LPARAM)selText);
						SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_SETCURSEL, SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) - 1, 0);
						EnableShaderButtons(hwndDlg);
						return false;
					}
				case IDC_REMOVESHADER:
					{
						auto sel = SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_GETCURSEL, 0, 0);
						if (sel == LB_ERR)
							return false;
						SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_DELETESTRING, sel, 0);
						if (sel > 0)
							SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_SETCURSEL, sel - 1, 0);
						else if (SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) > 0)
							SendDlgItemMessage(hwndDlg, IDC_SHADERSINUSE, LB_SETCURSEL, sel, 0);
						EnableShaderButtons(hwndDlg);
						return false;
					}

				}
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
