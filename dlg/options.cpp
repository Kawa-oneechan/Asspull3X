#include "..\ui.h"

BOOL CALLBACK OptionsWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
		{
			DestroyWindow(hwndDlg);
			hWndOptions = NULL;
			return true;
		}
		case WM_INITDIALOG:
		{
			CheckDlgButton(hwndDlg, IDC_FPSCAP, fpsCap);
			CheckDlgButton(hwndDlg, IDC_ASPECT, stretch200);
			CheckDlgButton(hwndDlg, IDC_SHOWFPS, fpsVisible);
			CheckDlgButton(hwndDlg, IDC_RELOAD, reloadROM);
			CheckDlgButton(hwndDlg, IDC_REMOUNT, reloadIMG);
			return true;
		}
		case WM_PAINT:
		{
			DrawWin7Thing(hwndDlg);
			return true;
		}
		case WM_CTLCOLORSTATIC:
			return (COLOR_WINDOW+1);
		case WM_COMMAND:
		{
			ResetPath();
			if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SHOWFPS)
			{
				fpsVisible = (IsDlgButtonChecked(hwndDlg, IDC_SHOWFPS) == 1);
				ini.SetBoolValue("video", "showFps", fpsVisible);
				ini.SaveFile("settings.ini");
				return true;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_FPSCAP)
			{
				fpsCap = (IsDlgButtonChecked(hwndDlg, IDC_FPSCAP) == 1);
				ini.SetBoolValue("video", "fpsCap", fpsCap);
				ini.SaveFile("settings.ini");
				return true;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_ASPECT)
			{
				stretch200 = (IsDlgButtonChecked(hwndDlg, IDC_ASPECT) == 1);
				ini.SetBoolValue("video", "stretch200", stretch200);
				ini.SaveFile("settings.ini");
				return true;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_RELOAD)
			{
				reloadROM = (IsDlgButtonChecked(hwndDlg, IDC_RELOAD) == 1);
				ini.SetBoolValue("media", "reloadRom", reloadROM);
				ini.SaveFile("settings.ini");
				return true;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_REMOUNT)
			{
				reloadIMG = (IsDlgButtonChecked(hwndDlg, IDC_REMOUNT) == 1);
				ini.SetBoolValue("media", "reloadImg", reloadIMG);
				ini.SaveFile("settings.ini");
				return true;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK)
			{
				DestroyWindow(hwndDlg);
				hWndOptions = NULL;
				return true;
			}
		}
	}
	return false;
}

void ShowOptions()
{
	if (!IsWindow(hWndOptions))
	{
		hWndOptions = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_OPTIONS), (HWND)hWnd, (DLGPROC)OptionsWndProc);
		ShowWindow(hWndOptions, SW_SHOW);
	}
}
