#include "..\asspull.h"

extern void AssociateFiletypes();

namespace UI
{
	namespace Options
	{
		using namespace Presentation;

		HWND hWnd = NULL;
		RECT shieldIcon;

		BOOL CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_CLOSE:
			{
				DestroyWindow(hWnd);
				Options::hWnd = NULL;
				return true;
			}
			case WM_INITDIALOG:
			{
				Options::hWnd = hWnd;

				for (int i = 10; i < 15; i++)
					SendDlgItemMessage(hWnd, i, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);

				CheckDlgButton(hWnd, IDC_FPSCAP, fpsCap);
				CheckDlgButton(hWnd, IDC_ASPECT, Video::stretch200);
				CheckDlgButton(hWnd, IDC_SHOWFPS, fpsVisible);

				for (int i = 0; i < 2; i++)
					SendDlgItemMessage(hWnd, IDC_THEME, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_THEMES + i));
				if (Presentation::Windows10::IsWin10())
					SendDlgItemMessage(hWnd, IDC_THEME, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_THEMES + 2));
				SendDlgItemMessage(hWnd, IDC_THEME, CB_SETCURSEL, ini.GetLongValue(L"misc", L"theme", 0), 0);

				int midiDevs = midiOutGetNumDevs();
				MIDIOUTCAPS caps = { 0 };
				for (int i = 0; i < midiDevs; i++)
				{
					midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS));
					SendDlgItemMessage(hWnd, IDC_MIDIDEV, CB_ADDSTRING, 0, (LPARAM)caps.szPname);
				}
				int dev = ini.GetLongValue(L"audio", L"midiDevice", 0);
				if (dev >= midiDevs) dev = 0;
				CheckDlgButton(hWnd, IDC_SOUND, ini.GetBoolValue(L"audio", L"sound", true));
				CheckDlgButton(hWnd, IDC_MUSIC, ini.GetBoolValue(L"audio", L"music", true));
				SendDlgItemMessage(hWnd, IDC_MIDIDEV, CB_SETCURSEL, dev, 0);

				CheckDlgButton(hWnd, IDC_RELOAD, UI::reloadROM);
				CheckDlgButton(hWnd, IDC_REMOUNT, UI::reloadIMG);
				SetDlgItemText(hWnd, IDC_BIOSPATH, ini.GetValue(L"media", L"bios", L""));

				GetIconPos(hWnd, IDC_LINK, &shieldIcon, -20, 0);

				Tooltips::CreateTooltips(hWnd, IDC_ASPECT, IDC_RELOAD, IDC_REMOUNT, 0);
				return true;
			}
			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				DrawWindowBk(hWnd, true, &ps, hdc);
				ImageList_Draw(Images::hIml, IML_SHIELD, hdc, shieldIcon.left, shieldIcon.top, ILD_NORMAL);
				EndPaint(hWnd, &ps);
				return true;
			}
			case WM_NOTIFY:
			{
				if (((LPNMHDR)lParam)->code == NM_CUSTOMDRAW)
				{
					auto nmc = (LPNMCUSTOMDRAW)lParam;
					int idFrom = nmc->hdr.idFrom;
					switch (idFrom)
					{
					case IDC_SHOWFPS:
					case IDC_FPSCAP:
					case IDC_ASPECT:
					case IDC_RELOAD:
					case IDC_REMOUNT:
					case IDC_SOUND:
					case IDC_MUSIC:
					{
						DrawCheckbox(hWnd, nmc);
						return true;
					}
					break;
					case IDOK:
					case IDCANCEL:
					case IDC_BIOSBROWSE:
						return DrawButton(hWnd, nmc);
					}
				}
				else if (((LPNMHDR)lParam)->code == NM_CLICK)
				{
					PNMLINK pNMLink = (PNMLINK)lParam;
					LITEM item = pNMLink->item;
					if ((((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hWnd, IDC_LINK)) && (item.iLink == 0))
						AssociateFiletypes();
					break;
				}
			}
			case WM_CTLCOLORSTATIC:
			{
				SetTextColor((HDC)wParam, rgbText);
				SetBkColor((HDC)wParam, rgbBack);
				for (int i = 10; i < 15; i++)
					if (lParam == (LPARAM)GetDlgItem(hWnd, i))
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
					case IDC_LINK:
					{
						AssociateFiletypes();
						return false;
					}
					case IDC_BIOSBROWSE:
					{
						WCHAR thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hWnd, IDC_BIOSPATH), thePath, FILENAME_MAX);
						if (ShowFileDlg(false, thePath, 256, L"A3X BIOS files (*.apb)|*.apb"))
							SetWindowText(GetDlgItem(hWnd, IDC_BIOSPATH), thePath);
						return false;
					}
					case IDOK:
					{
						WCHAR thePath[FILENAME_MAX] = { 0 };
						GetWindowText(GetDlgItem(hWnd, IDC_BIOSPATH), thePath, FILENAME_MAX);
						ini.SetValue(L"media", L"bios", thePath);
						ini.SetLongValue(L"misc", L"theme", SendDlgItemMessage(hWnd, IDC_THEME, CB_GETCURSEL, 0, 0));
						ini.SetLongValue(L"audio", L"midiDevice", SendDlgItemMessage(hWnd, IDC_MIDIDEV, CB_GETCURSEL, 0, 0));

						UI::fpsVisible = (IsDlgButtonChecked(hWnd, IDC_SHOWFPS) == 1);
						UI::fpsCap = (IsDlgButtonChecked(hWnd, IDC_FPSCAP) == 1);
						Video::stretch200 = (IsDlgButtonChecked(hWnd, IDC_ASPECT) == 1);
						UI::reloadROM = (IsDlgButtonChecked(hWnd, IDC_RELOAD) == 1);
						UI::reloadIMG = (IsDlgButtonChecked(hWnd, IDC_REMOUNT) == 1);
						auto enableSound = (IsDlgButtonChecked(hWnd, IDC_SOUND) == 1);
						auto enableMusic = (IsDlgButtonChecked(hWnd, IDC_MUSIC) == 1);

						ini.SetBoolValue(L"video", L"fpsCap", fpsCap);
						ini.SetBoolValue(L"video", L"showFps", fpsVisible);
						ini.SetBoolValue(L"video", L"stretch200", Video::stretch200);
						ini.SetBoolValue(L"audio", L"sound", enableSound);
						ini.SetBoolValue(L"audio", L"music", enableMusic);
						ini.SetBoolValue(L"misc", L"reloadRom", UI::reloadROM);
						ini.SetBoolValue(L"misc", L"reloadImg", UI::reloadIMG);
						ini.SaveFile(settingsFile, false);
						DestroyWindow(hWnd);
						Options::hWnd = NULL;

						Presentation::SetThemeColors();
						Sound::Initialize();
						return true;
					}
					case IDCANCEL:
					{
						DestroyWindow(hWnd);
						Options::hWnd = NULL;
						return true;
					}
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
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_OPTIONS), (HWND)hWndMain, (DLGPROC)WndProc);
				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}
