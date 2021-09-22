#include "..\ui.h"

extern int invertButtons;

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
			for (int i = 10; i < 15; i++)
				SendDlgItemMessage(hwndDlg, i, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);

			CheckDlgButton(hwndDlg, IDC_FPSCAP, fpsCap);
			CheckDlgButton(hwndDlg, IDC_ASPECT, stretch200);
			CheckDlgButton(hwndDlg, IDC_SHOWFPS, fpsVisible);
			
			for (int i = 0; i < 2; i++)
				SendDlgItemMessage(hwndDlg, IDC_THEME, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_THEMES + i));
			if (IsWin10())
				SendDlgItemMessage(hwndDlg, IDC_THEME, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_THEMES + 2)); 
			SendDlgItemMessage(hwndDlg, IDC_THEME, CB_SETCURSEL, ini.GetLongValue(L"media", L"theme", 0), 0);

			int midiDevs = midiOutGetNumDevs();
			MIDIOUTCAPS caps = { 0 };
			for (int i = 0; i < midiDevs; i++)
			{
				midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS));
				SendDlgItemMessage(hwndDlg, IDC_MIDIDEV, CB_ADDSTRING, 0, (LPARAM)caps.szPname);
			}
			int dev = ini.GetLongValue(L"audio", L"midiDevice", 0);
			if (dev >= midiDevs) dev = 0;
			CheckDlgButton(hwndDlg, IDC_SOUND, ini.GetBoolValue(L"audio", L"sound", true));
			CheckDlgButton(hwndDlg, IDC_MUSIC, ini.GetBoolValue(L"audio", L"music", true));
			SendDlgItemMessage(hwndDlg, IDC_MIDIDEV, CB_SETCURSEL, dev, 0);

			CheckDlgButton(hwndDlg, IDC_INVERTBUTTONS, ini.GetBoolValue(L"media", L"invertButtons", false));

			CheckDlgButton(hwndDlg, IDC_RELOAD, reloadROM);
			CheckDlgButton(hwndDlg, IDC_REMOUNT, reloadIMG);
			SetDlgItemText(hwndDlg, IDC_BIOSPATH, ini.GetValue(L"media", L"bios", L""));
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
					case IDC_INVERTBUTTONS:
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
		case WM_DRAWITEM:
		{
			if (wParam == IDC_THEME || wParam == IDC_MIDIDEV)
				return DrawComboBox((LPDRAWITEMSTRUCT)lParam);
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
						WCHAR thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath, FILENAME_MAX);
						if (ShowFileDlg(false, thePath, 256, L"A3X BIOS files (*.apb)|*.apb"))
							SetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath);
						return false;
					}
					case IDOK:
					{
						WCHAR thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hwndDlg, IDC_BIOSPATH), thePath, FILENAME_MAX);
						ini.SetValue(L"media", L"bios", thePath);
						ini.SetLongValue(L"media", L"theme", SendDlgItemMessage(hwndDlg, IDC_THEME, CB_GETCURSEL, 0, 0));
						ini.SetLongValue(L"audio", L"midiDevice", SendDlgItemMessage(hwndDlg, IDC_MIDIDEV, CB_GETCURSEL, 0, 0));

						fpsVisible = (IsDlgButtonChecked(hwndDlg, IDC_SHOWFPS) == 1);
						fpsCap = (IsDlgButtonChecked(hwndDlg, IDC_FPSCAP) == 1);
						stretch200 = (IsDlgButtonChecked(hwndDlg, IDC_ASPECT) == 1);
						reloadROM = (IsDlgButtonChecked(hwndDlg, IDC_RELOAD) == 1);
						reloadIMG = (IsDlgButtonChecked(hwndDlg, IDC_REMOUNT) == 1);
						invertButtons = (IsDlgButtonChecked(hwndDlg, IDC_INVERTBUTTONS) == 1) ? 1 : 0;
						auto enableSound = (IsDlgButtonChecked(hwndDlg, IDC_SOUND) == 1);
						auto enableMusic = (IsDlgButtonChecked(hwndDlg, IDC_MUSIC) == 1);

						ini.SetBoolValue(L"video", L"fpsCap", fpsCap);
						ini.SetBoolValue(L"video", L"showFps", fpsVisible);
						ini.SetBoolValue(L"video", L"stretch200", stretch200);
						ini.SetBoolValue(L"audio", L"sound", enableSound);
						ini.SetBoolValue(L"audio", L"music", enableMusic);
						ini.SetBoolValue(L"media", L"reloadRom", reloadROM);
						ini.SetBoolValue(L"media", L"reloadImg", reloadIMG);
						ini.SetBoolValue(L"media", L"invertButtons", invertButtons == 1);
						ini.SaveFile(settingsFile, false);
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
