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
			for (int i = 10; i < 14; i++)
				SendDlgItemMessage(hwndDlg, i, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);

			CheckDlgButton(hwndDlg, IDC_FPSCAP, fpsCap);
			CheckDlgButton(hwndDlg, IDC_ASPECT, stretch200);
			CheckDlgButton(hwndDlg, IDC_SHOWFPS, fpsVisible);
			
			LPCSTR themes[] = { "Light", "Dark" };
			for (int i = 0; i < 3; i++)
				SendDlgItemMessage(hwndDlg, IDC_THEME, CB_ADDSTRING, 0, (LPARAM)themes[i]);
			if (IsWin10())
				SendDlgItemMessage(hwndDlg, IDC_THEME, CB_ADDSTRING, 0, (LPARAM)"Match");
			SendDlgItemMessage(hwndDlg, IDC_THEME, CB_SETCURSEL, ini.GetLongValue("media", "theme", 0), 0);

			int midiDevs = midiOutGetNumDevs();
			MIDIOUTCAPS caps = { 0 };
			for (int i = 0; i < midiDevs; i++)
			{
				midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS));
				SendDlgItemMessage(hwndDlg, IDC_MIDIDEV, CB_ADDSTRING, 0, (LPARAM)caps.szPname);
			}
			int dev = ini.GetLongValue("audio", "midiDevice", 0);
			if (dev >= midiDevs) dev = 0;
			CheckDlgButton(hwndDlg, IDC_SOUND, ini.GetBoolValue("audio", "sound", true));
			CheckDlgButton(hwndDlg, IDC_MUSIC, ini.GetBoolValue("audio", "music", true));
			SendDlgItemMessage(hwndDlg, IDC_MIDIDEV, CB_SETCURSEL, dev, 0);

			CheckDlgButton(hwndDlg, IDC_RELOAD, reloadROM);
			CheckDlgButton(hwndDlg, IDC_REMOUNT, reloadIMG);
			SetDlgItemText(hwndDlg, IDC_BIOSPATH, ini.GetValue("media", "bios", ""));
			return true;
		}
		case WM_PAINT:
		{
			DrawWindowBk(hwndDlg, true);
			return true;
		}
		case WM_NOTIFY:
		{
			if(((LPNMHDR)lParam)->code == NM_CUSTOMDRAW)
			{
				auto nmc = (LPNMCUSTOMDRAW)lParam;
				int idFrom = nmc->hdr.idFrom;
				switch(idFrom)
				{
					case IDC_SHOWFPS:
					case IDC_FPSCAP:
					case IDC_ASPECT:
					case IDC_RELOAD:
					case IDC_REMOUNT:
					case IDC_SOUND:
					case IDC_MUSIC:
					{
						DrawCheckbox(hwndDlg, nmc);
						return true;
					}
					break;
					case IDOK:
					case IDCANCEL:
					case IDC_BIOSBROWSE:
						return DrawDarkButton(hwndDlg, nmc);
				}
			}
		}
		case WM_CTLCOLORSTATIC:
		{
			SetTextColor((HDC)wParam, rgbText);
			SetBkColor((HDC)wParam, rgbBack);
			for (int i = 10; i < 14; i++)
				if (lParam == (LPARAM)GetDlgItem(hwndDlg, i))
					SetTextColor((HDC)wParam, rgbHeader);
			return (INT_PTR)hbrBack;
		}
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
		{
			SetTextColor((HDC)wParam, rgbList);
			SetBkColor((HDC)wParam, rgbListBk);
			return (INT_PTR)hbrList;
		}
		case WM_CTLCOLORBTN:
		{
			return (INT_PTR)hbrBack;
		}
		case WM_COMMAND:
		{
			ResetPath();
			if (HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDC_BIOSBROWSE:
					{
						char thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath, FILENAME_MAX);
						if (ShowFileDlg(false, thePath, 256, "A3X BIOS files (*.apb)|*.apb"))
							SetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath);
						return false;
					}
					case IDOK:
					{
						char thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath, FILENAME_MAX);
						ini.SetValue("media", "bios", thePath);
						ini.SetLongValue("media", "theme", SendDlgItemMessage(hwndDlg, IDC_THEME, CB_GETCURSEL, 0, 0));
						ini.SetLongValue("audio", "midiDevice", SendDlgItemMessage(hwndDlg, IDC_MIDIDEV, CB_GETCURSEL, 0, 0));

						fpsVisible = (IsDlgButtonChecked(hwndDlg, IDC_SHOWFPS) == 1);
						fpsCap = (IsDlgButtonChecked(hwndDlg, IDC_FPSCAP) == 1);
						stretch200 = (IsDlgButtonChecked(hwndDlg, IDC_ASPECT) == 1);
						reloadROM = (IsDlgButtonChecked(hwndDlg, IDC_RELOAD) == 1);
						reloadIMG = (IsDlgButtonChecked(hwndDlg, IDC_REMOUNT) == 1);
						auto enableSound = (IsDlgButtonChecked(hwndDlg, IDC_SOUND) == 1);
						auto enableMusic = (IsDlgButtonChecked(hwndDlg, IDC_MUSIC) == 1);

						ini.SetBoolValue("video", "fpsCap", fpsCap);
						ini.SetBoolValue("video", "showFps", fpsVisible);
						ini.SetBoolValue("video", "stretch200", stretch200);
						ini.SetBoolValue("audio", "sound", enableSound);
						ini.SetBoolValue("audio", "music", enableMusic);
						ini.SetBoolValue("media", "reloadRom", reloadROM);
						ini.SetBoolValue("media", "reloadImg", reloadIMG);
						ini.SaveFile("settings.ini");
						DestroyWindow(hwndDlg);
						hWndOptions = NULL;

						SetThemeColors();
						InitSound();
						return true;
					}
					case IDCANCEL:
					{
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
