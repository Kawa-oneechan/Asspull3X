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
			for (int i = 10; i < 13; i++)
				SendDlgItemMessage(hwndDlg, i, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
			LPCSTR themes[] = { "Light", "Dark", "Match system" };
			for (int i = 0; i < 3; i++)
				SendDlgItemMessage(hwndDlg, IDC_THEME, CB_ADDSTRING, 0, (LPARAM)themes[i]);
			SendDlgItemMessage(hwndDlg, IDC_THEME, CB_SETCURSEL, 0, 0);
			SetDlgItemText(hwndDlg, IDC_BIOSPATH, ini.GetValue("media", "bios", ""));
			return true;
		}
		case WM_PAINT:
		{
			DrawWin7Thing(hwndDlg);
			return true;
		}
		case WM_CTLCOLORSTATIC:
			for (int i = 10; i < 13; i++)
				if (lParam == (LPARAM)GetDlgItem(hwndDlg, i))
					SetTextColor((HDC)wParam, RGB(0x00, 0x33, 0x99));
			return (COLOR_WINDOW+1);
		case WM_COMMAND:
		{
			ResetPath();
			if (HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDC_SHOWFPS:
					{
						fpsVisible = (IsDlgButtonChecked(hwndDlg, IDC_SHOWFPS) == 1);
						ini.SetBoolValue("video", "showFps", fpsVisible);
						ini.SaveFile("settings.ini");
						return true;
					}
					case IDC_FPSCAP:
					{
						fpsCap = (IsDlgButtonChecked(hwndDlg, IDC_FPSCAP) == 1);
						ini.SetBoolValue("video", "fpsCap", fpsCap);
						ini.SaveFile("settings.ini");
						return true;
					}
					case IDC_ASPECT:
					{
						stretch200 = (IsDlgButtonChecked(hwndDlg, IDC_ASPECT) == 1);
						ini.SetBoolValue("video", "stretch200", stretch200);
						ini.SaveFile("settings.ini");
						return true;
					}
					case IDC_RELOAD:
					{
						reloadROM = (IsDlgButtonChecked(hwndDlg, IDC_RELOAD) == 1);
						ini.SetBoolValue("media", "reloadRom", reloadROM);
						ini.SaveFile("settings.ini");
						return true;
					}
					case IDC_REMOUNT:
					{
						reloadIMG = (IsDlgButtonChecked(hwndDlg, IDC_REMOUNT) == 1);
						ini.SetBoolValue("media", "reloadImg", reloadIMG);
						ini.SaveFile("settings.ini");
						return true;
					}
					case IDC_BIOSBROWSE:
					{
						char thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath, FILENAME_MAX);
						if (ShowFileDlg(false, thePath, 256, "A3X BIOS files (*.apb)|*.apb"))
							SetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath);
					}
					case IDOK:
					{
						char thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath, FILENAME_MAX);
						ini.SetValue("media", "bios", thePath);
						DestroyWindow(hwndDlg);
						hWndOptions = NULL;
						return true;
					}
				}
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
