#include "..\asspull.h"

//TODO: there's a bug in removing the last shader. Needs study.

extern SDL_GameControllerButton buttonMap[16];
extern const WCHAR* buttonNames[];

namespace UI
{
	namespace ButtonMaps
	{
		using namespace Presentation;

		HWND hWnd = NULL;
		SDL_GameControllerButton newMap[16];

		void UpdateList()
		{
			WCHAR work[256] = { 0 };
			SendDlgItemMessage(hWnd, IDC_COMBO1, LB_RESETCONTENT, 0, 0);
			for (int i = 0; i < 12; i++)
			{
				wcscpy(work, GetString(IDS_BUTTONS + i));
				wcscat(work, L"\t");
				wcscat(work, GetString(IDS_BUTTONSFROM + newMap[i]));
				SendDlgItemMessage(hWnd, IDC_COMBO1, LB_ADDSTRING, 0, (LPARAM)work);
			}
		}

		BOOL CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_CLOSE:
			{
				DestroyWindow(hWnd);
				ButtonMaps::hWnd = NULL;
				return true;
			}
			case WM_INITDIALOG:
			{
				ButtonMaps::hWnd = hWnd;
				for (int i = 0; i < 16; i++)
					newMap[i] = buttonMap[i];

				const int tabs[] = { 64 };
				SendDlgItemMessage(hWnd, IDC_COMBO1, LB_SETTABSTOPS, 1, (LPARAM)&tabs);
				for (int i = 0; i < 14; i++)
					SendDlgItemMessage(hWnd, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_BUTTONSFROM + i));
				UpdateList();
				SendDlgItemMessage(hWnd, IDC_COMBO1, LB_SETCURSEL, 0, 0);
				SendDlgItemMessage(hWnd, IDC_COMBO2, CB_SETCURSEL, buttonMap[0], 0);
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
					switch (idFrom)
					{
					case IDOK:
					case IDCANCEL:
						return DrawDarkButton(hWnd, nmc);
						break;
					}
				}
				}
			}
			case WM_PAINT:
			{
				DrawWindowBk(hWnd, true);
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
			case WM_DRAWITEM:
			{
				if (wParam == IDC_COMBO2)
					return DrawComboBox((LPDRAWITEMSTRUCT)lParam);
			}
			case WM_COMMAND:
			{
				if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == IDC_COMBO1)
				{
					int choice = SendDlgItemMessage(hWnd, IDC_COMBO1, LB_GETCURSEL, 0, 0);
					SendDlgItemMessage(hWnd, IDC_COMBO2, CB_SETCURSEL, buttonMap[choice], 0);
					return true;
				}
				if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_COMBO2)
				{
					int button = SendDlgItemMessage(hWnd, IDC_COMBO1, LB_GETCURSEL, 0, 0);
					int mapto = SendDlgItemMessage(hWnd, IDC_COMBO2, CB_GETCURSEL, 0, 0);
					newMap[button] = (SDL_GameControllerButton)mapto;
					UpdateList();
					SendDlgItemMessage(hWnd, IDC_COMBO1, LB_SETCURSEL, button, 0);
					return true;
				}
				if (HIWORD(wParam) == BN_CLICKED)
				{
					switch (LOWORD(wParam))
					{
					case IDOK:
					{
						for (int i = 0; i < 12; i++)
						{
							buttonMap[i] = newMap[i];
							ini.SetValue(L"input", buttonNames[i], SDL_GameControllerGetStringForButtonW(newMap[i]));
						}
						ini.SaveFile(settingsFile, false);
						DestroyWindow(hWnd);
						ButtonMaps::hWnd = NULL;
						return true;
					}
					case IDCANCEL:
					{
						DestroyWindow(hWnd);
						ButtonMaps::hWnd = NULL;
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
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_BUTTONMAPS), (HWND)hWndMain, (DLGPROC)WndProc);
				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}