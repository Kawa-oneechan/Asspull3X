#include "..\asspull.h"

namespace UI
{
	namespace Shaders
	{
		using namespace Presentation;

		HWND hWnd = NULL;

		void UpdateButtons()
		{
			EnableWindow(GetDlgItem(hWnd, IDC_ADDSHADER),
				(SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) < MAXSHADERS) &&
				(SendDlgItemMessage(hWnd, IDC_SHADERSAVAILABLE, LB_GETCURSEL, 0, 0) != LB_ERR)
			);

			auto inUseCount = SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0);
			auto inUseSel = SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCURSEL, 0, 0);

			auto allowInUse = (inUseCount > 0) && (inUseSel != LB_ERR);

			EnableWindow(GetDlgItem(hWnd, IDC_REMOVESHADER), allowInUse);
			EnableWindow(GetDlgItem(hWnd, IDC_MOVESHADERUP), allowInUse && (inUseSel > 0));
			EnableWindow(GetDlgItem(hWnd, IDC_MOVESHADERDOWN), allowInUse && (inUseSel < inUseCount - 1));
		}

		BOOL CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_CLOSE:
			{
				DestroyWindow(hWnd);
				Shaders::hWnd = NULL;
				return true;
			}
			case WM_INITDIALOG:
			{
				Shaders::hWnd = hWnd;

				WCHAR shaderPat[FILENAME_MAX];
				wcscpy_s(shaderPat, UI::startingPath);
				wcscat_s(shaderPat, L"\\*.fs");

				SendDlgItemMessage(hWnd, IDC_SHADERSAVAILABLE, LB_DIR, DDL_READWRITE, (LPARAM)shaderPat);

				auto numShaders = ini.GetLongValue(L"video", L"shaders", 0);
				for (int i = 0; i < numShaders; i++)
				{
					WCHAR key[16] = { 0 };
					wsprintf(key, L"shader%d", i + 1);
					SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_ADDSTRING, 0, (LPARAM)ini.GetValue(L"video", key, L""));
				}

				UpdateButtons();
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
					case IDC_ADDSHADER:
					case IDC_REMOVESHADER:
					case IDC_MOVESHADERUP:
					case IDC_MOVESHADERDOWN:
						return DrawButton(hWnd, nmc);
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
			case WM_COMMAND:
			{
				if (HIWORD(wParam) == LBN_SELCHANGE)
				{
					UpdateButtons();
				}
				else if (HIWORD(wParam) == LBN_DBLCLK)
				{
					//Fake clicking the Add/Remove buttons depending on which list it was
					if (LOWORD(wParam) == IDC_SHADERSAVAILABLE)
						SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_ADDSHADER, BN_CLICKED), 0);
					else if (LOWORD(wParam) == IDC_SHADERSINUSE)
						SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_REMOVESHADER, BN_CLICKED), 0);
				}
				else if (HIWORD(wParam) == BN_CLICKED)
				{
					switch (LOWORD(wParam))
					{
					case IDOK:
					{
						auto cnt = SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0);

						ini.SetLongValue(L"video", L"shaders", cnt);
						for (int i = 0; i < MAXSHADERS; i++)
						{
							WCHAR key[16] = { 0 };
							wsprintf(key, L"shader%d", i + 1);
							if (i < cnt)
							{
								WCHAR val[256];
								SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETTEXT, i, (LPARAM)val);
								ini.SetValue(L"video", key, val);
							}
							else
							{
								ini.Delete(L"video", key, true);
							}
						}
						ini.SaveFile(settingsFile, false);

						//DestroyWindow(hWnd);
						//Shaders::hWnd = NULL;

						Video::InitShaders();

						return true;
					}
					case IDCANCEL:
					{
						DestroyWindow(hWnd);
						Shaders::hWnd = NULL;
						return true;
					}
					case IDC_ADDSHADER:
					{
						if (SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) >= MAXSHADERS)
							return false;
						auto sel = SendDlgItemMessage(hWnd, IDC_SHADERSAVAILABLE, LB_GETCURSEL, 0, 0);
						if (sel == LB_ERR)
							return false;
						char selText[256];
						SendDlgItemMessage(hWnd, IDC_SHADERSAVAILABLE, LB_GETTEXT, sel, (LPARAM)selText);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_ADDSTRING, 0, (LPARAM)selText);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_SETCURSEL, SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) - 1, 0);
						UpdateButtons();
						return false;
					}
					case IDC_REMOVESHADER:
					{
						auto sel = SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCURSEL, 0, 0);
						if (sel == LB_ERR)
							return false;
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_DELETESTRING, sel, 0);
						if (sel > 0)
							SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_SETCURSEL, sel - 1, 0);
						else if (SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0) > 0)
							SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_SETCURSEL, sel, 0);
						UpdateButtons();
						return false;
					}
					case IDC_MOVESHADERUP:
					{
						auto sel = SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCURSEL, 0, 0);
						if (sel == LB_ERR || sel == 0)
							return false;
						char toMove[256];
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETTEXT, sel, (LPARAM)toMove);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_DELETESTRING, sel, 0);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_INSERTSTRING, sel - 1, (LPARAM)toMove);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_SETCURSEL, sel - 1, 0);
						UpdateButtons();
						return false;
					}
					case IDC_MOVESHADERDOWN:
					{
						auto sel = SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCURSEL, 0, 0);
						auto cnt = SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETCOUNT, 0, 0);
						if (sel == LB_ERR || sel == cnt - 1)
							return false;
						char toMove[256];
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_GETTEXT, sel, (LPARAM)toMove);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_DELETESTRING, sel, 0);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_INSERTSTRING, sel + 1, (LPARAM)toMove);
						SendDlgItemMessage(hWnd, IDC_SHADERSINUSE, LB_SETCURSEL, sel + 1, 0);
						UpdateButtons();
						return false;
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
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_SHADERS), (HWND)hWndMain, (DLGPROC)WndProc);
				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}
