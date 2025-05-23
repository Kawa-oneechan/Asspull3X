#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "asspull.h"
#include "SDL_syswm.h"
#include <commdlg.h>
#include <Uxtheme.h>
#include <vsstyle.h>
#include <time.h>

#include <vector>
extern int decodePNG(std::vector<unsigned char>& out_image, unsigned long& image_width, unsigned long& image_height, const unsigned char* in_png, size_t in_size, bool convert_to_rgba32 = true);

extern int langID;

namespace UI
{
	WCHAR settingsFile[FILENAME_MAX];
	WCHAR startingPath[FILENAME_MAX];
	WCHAR lastPath[FILENAME_MAX];

	HWND hWndMain;
	HINSTANCE hInstance;
	HWND hWndStatusBar;
	HMENU menuBar;
	int statusBarHeight = 0;
	bool hideUI = false;
	bool startFullscreen = false;

	int uiCommand;
	WCHAR uiString[512];

	bool mouseLocked = false;
	int statusTimer = 0;
	WCHAR uiStatus[512];
	bool fpsVisible = false, fpsCap = false, reloadROM, reloadIMG;
	int oldPause = pauseTurnedOff;
	int theme = 0;
	int diskIconTimer = 0, hddIconTimer = 0;

	bool ShowFileDlg(bool toSave, WCHAR* target, size_t max, const WCHAR* filter);

	namespace Presentation
	{
		HFONT headerFont = NULL, monoFont = NULL, statusFont = NULL;
		HBRUSH hbrBack = NULL, hbrStripe = NULL, hbrList = NULL;
		HPEN hpnStripe = NULL;
		COLORREF rgbBack = NULL, rgbStripe = NULL, rgbText = NULL, rgbTextD = NULL;
		COLORREF rgbHeader = NULL, rgbList = NULL, rgbListBk = NULL;

		void DrawWindowBk(HWND hwndDlg, bool stripe)
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwndDlg, &ps);
			DrawWindowBk(hwndDlg, stripe, &ps, hdc);
			EndPaint(hwndDlg, &ps);
		}

		void DrawWindowBk(HWND hwndDlg, bool stripe, PAINTSTRUCT* ps, HDC hdc)
		{
			if (stripe)
			{
				RECT rect = { 0 };
				rect.bottom = 7 + 14 + 7; //margin, button, padding
				MapDialogRect(hwndDlg, &rect);
				auto h = rect.bottom;
				GetClientRect(hwndDlg, &rect);
				rect.bottom -= h;
				FillRect(hdc, &ps->rcPaint, hbrStripe);
				FillRect(hdc, &rect, hbrBack);
			}
			else
			{
				FillRect(hdc, &ps->rcPaint, hbrBack);
			}
			return;
		}

		void DrawCheckbox(HWND hwndDlg, LPNMCUSTOMDRAW nmc)
		{
			int idFrom = nmc->hdr.idFrom;
			switch (nmc->dwDrawStage)
			{
			case CDDS_PREERASE:
			{
				SetBkColor(nmc->hdc, rgbBack);
				SetTextColor(nmc->hdc, nmc->uItemState & CDIS_DISABLED ? rgbTextD : rgbText);

				HTHEME hTheme = OpenThemeData(nmc->hdr.hwndFrom, L"BUTTON");
				if (!hTheme)
				{
					CloseThemeData(hTheme);
					SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LONG_PTR)CDRF_DODEFAULT);
					return;
				}

				auto checked = IsDlgButtonChecked(hwndDlg, idFrom);
				int stateID = checked ? CBS_CHECKEDNORMAL : CBS_UNCHECKEDNORMAL;
				if (nmc->uItemState & CDIS_DISABLED)
					stateID = checked ? CBS_CHECKEDDISABLED : CBS_UNCHECKEDDISABLED;
				else if (nmc->uItemState & CDIS_SELECTED)
					stateID = checked ? CBS_CHECKEDPRESSED : CBS_UNCHECKEDPRESSED;
				else if (nmc->uItemState & CDIS_HOT)
					stateID = checked ? CBS_CHECKEDHOT : CBS_UNCHECKEDHOT;

				RECT r;
				SIZE s;

				GetThemePartSize(hTheme, nmc->hdc, BP_CHECKBOX, stateID, NULL, TS_TRUE, &s);

				r.left = nmc->rc.left;
				r.top = nmc->rc.top + 3;
				r.right = r.left + s.cx;
				r.bottom = r.top + s.cy;

				DrawThemeBackground(hTheme, nmc->hdc, BP_CHECKBOX, stateID, &r, NULL);

				if (theme == 1) //dark
				{
					InflateRect(&r, -1, -1);
					InvertRect(nmc->hdc, &r);
					InflateRect(&r, 1, 1);
				}

				nmc->rc.left += 3 + s.cx;

				WCHAR text[256];
				GetDlgItemText(hwndDlg, idFrom, text, 255);
				DrawText(nmc->hdc, text, -1, &nmc->rc, DT_SINGLELINE | DT_VCENTER);

				CloseThemeData(hTheme);
				SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LONG_PTR)CDRF_SKIPDEFAULT);
				return;
			}
			}
		}

		bool DrawButton(HWND hwndDlg, LPNMCUSTOMDRAW nmc)
		{
			if (theme == 0)
				return false; //don't draw jack
			int idFrom = nmc->hdr.idFrom;
			switch (nmc->dwDrawStage)
			{
			case CDDS_PREPAINT:
			{
				SetBkColor(nmc->hdc, rgbBack);
				SetTextColor(nmc->hdc, nmc->uItemState & CDIS_DISABLED ? rgbTextD : rgbText);

				HBRUSH border = CreateSolidBrush(rgbText);

				FillRect(nmc->hdc, &nmc->rc, border);
				InflateRect(&nmc->rc, -1, -1);
				if (nmc->uItemState & CDIS_DEFAULT)
					InflateRect(&nmc->rc, -1, -1);

				FillRect(nmc->hdc, &nmc->rc, (nmc->uItemState & CDIS_SELECTED) ? hbrList : hbrBack);

				if (nmc->uItemState & CDIS_FOCUS)
				{
					InflateRect(&nmc->rc, -1, -1);
					DrawFocusRect(nmc->hdc, &nmc->rc);
				}

				DeleteObject(border);

				SetBkColor(nmc->hdc, (nmc->uItemState & CDIS_SELECTED) ? rgbListBk : rgbBack);
				WCHAR text[256];
				GetDlgItemText(hwndDlg, idFrom, text, 255);
				DrawText(nmc->hdc, text, -1, &nmc->rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
				SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LONG_PTR)CDRF_SKIPDEFAULT);
			}
			}
			return true;
		}

		bool DrawComboBox(LPDRAWITEMSTRUCT dis)
		{
			if (dis->itemID == -1)
				return false;

			SetTextColor(dis->hDC, dis->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHTTEXT) : rgbList);
			SetBkColor(dis->hDC, dis->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHT) : rgbListBk);

			TEXTMETRIC tm;
			GetTextMetrics(dis->hDC, &tm);
			int y = (dis->rcItem.bottom + dis->rcItem.top - tm.tmHeight) / 2;
			int x = LOWORD(GetDialogBaseUnits()) / 4;
			if (dis->itemState & ODS_COMBOBOXEDIT)
				x += 2;

			WCHAR text[256] = { 0 };
			SendMessage(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)text);
			ExtTextOut(dis->hDC, x, y, ETO_CLIPPED | ETO_OPAQUE, &dis->rcItem, text, (UINT)wcslen(text), NULL);
			return true;
		}

		void GetIconPos(HWND hwndDlg, int ctlID, RECT* iconRect, int leftOffset, int topOffset, int size)
		{
			POINT t;
			GetWindowRect(GetDlgItem(hwndDlg, ctlID), iconRect);
			t.x = iconRect->left;
			t.y = iconRect->top;
			ScreenToClient(hwndDlg, &t);
			iconRect->left = t.x + leftOffset;
			iconRect->top = t.y + topOffset;
			iconRect->right = iconRect->left + size;
			iconRect->bottom = iconRect->top + size;
		}

		namespace Windows10
		{
			int MatchTheme()
			{
				DWORD nResult;
				HKEY key;
				auto result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &key);
				if (result != ERROR_SUCCESS)
					return -1;
				DWORD dwBufferSize = sizeof(DWORD);
				result = RegQueryValueEx(key, L"AppsUseLightTheme", 0, NULL, (LPBYTE)&nResult, &dwBufferSize);
				RegCloseKey(key);
				if (result != ERROR_SUCCESS)
					return -1;
				//AppsUseLightTheme == 0 means light, and that's the inverse of our list.
				return !nResult;
			}

			bool IsWin10()
			{
				return MatchTheme() != -1;
			}

			COLORREF GetAccent(COLORREF not10)
			{
				DWORD nResult;
				HKEY key;
				if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, KEY_READ, &key))
					return not10;
				DWORD dwBufferSize = sizeof(DWORD);
				if (RegQueryValueEx(key, L"AccentColor", 0, NULL, (LPBYTE)&nResult, &dwBufferSize))
					nResult = not10;
				RegCloseKey(key);
				nResult &= 0xFFFFFF;
				
				int r = (unsigned char)nResult;
				int g = (unsigned char)(nResult >> 8);
				int b = (unsigned char)(nResult >> 16);
				//the one helpful thing from MS Docs on darkmode :3
				int brightness = (5 * g) + (2 * r) + b;
				if (brightness < (8 * 128))
					nResult = not10; //fall back
				return nResult;
			}

			namespace DarkMenuBar
			{

#pragma region Bar
				bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr);

#define WM_UAHDESTROYWINDOW    0x0090
#define WM_UAHDRAWMENU         0x0091
#define WM_UAHDRAWMENUITEM     0x0092
#define WM_UAHINITMENU         0x0093
#define WM_UAHMEASUREMENUITEM  0x0094
#define WM_UAHNCPAINTMENUPOPUP 0x0095

				typedef union tagUAHMENUITEMMETRICS
				{
					struct {
						DWORD cx;
						DWORD cy;
					} rgsizeBar[2];
					struct {
						DWORD cx;
						DWORD cy;
					} rgsizePopup[4];
				} UAHMENUITEMMETRICS;
				
				typedef struct tagUAHMENUPOPUPMETRICS
				{
					DWORD rgcx[4];
					DWORD fUpdateMaxWidths : 2;
				} UAHMENUPOPUPMETRICS;

				typedef struct tagUAHMENU
				{
					HMENU hmenu;
					HDC hdc;
					DWORD dwFlags;
				} UAHMENU;

				typedef struct tagUAHMENUITEM
				{
					int iPosition;
					UAHMENUITEMMETRICS umim;
					UAHMENUPOPUPMETRICS umpm;
				} UAHMENUITEM;

				typedef struct UAHDRAWMENUITEM
				{
					DRAWITEMSTRUCT dis; // itemID looks uninitialized
					UAHMENU um;
					UAHMENUITEM umi;
				} UAHDRAWMENUITEM;

				typedef struct tagUAHMEASUREMENUITEM
				{
					MEASUREITEMSTRUCT mis;
					UAHMENU um;
					UAHMENUITEM umi;
				} UAHMEASUREMENUITEM;

				static HTHEME g_menuTheme = nullptr;

				void UAHDrawMenuNCBottomLine(HWND hWnd)
				{
					MENUBARINFO mbi = { sizeof(mbi) };
					if (!GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi))
					{
						return;
					}

					RECT rcClient = { 0 };
					GetClientRect(hWnd, &rcClient);
					MapWindowPoints(hWnd, nullptr, (POINT*)&rcClient, 2);

					RECT rcWindow = { 0 };
					GetWindowRect(hWnd, &rcWindow);

					OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

					// the rcBar is offset by the window rect
					RECT rcAnnoyingLine = rcClient;
					rcAnnoyingLine.bottom = rcAnnoyingLine.top;
					rcAnnoyingLine.top--;


					HDC hdc = GetWindowDC(hWnd);
					FillRect(hdc, &rcAnnoyingLine, hbrStripe);
					ReleaseDC(hWnd, hdc);
				}

				bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr)
				{
					switch (message)
					{
					case WM_UAHDRAWMENU:
					{
						UAHMENU* pUDM = (UAHMENU*)lParam;
						RECT rc = { 0 };
						{
							MENUBARINFO mbi = { sizeof(mbi) };
							GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

							RECT rcWindow;
							GetWindowRect(hWnd, &rcWindow);

							rc = mbi.rcBar;
							OffsetRect(&rc, -rcWindow.left, -rcWindow.top);
						}

						FillRect(pUDM->hdc, &rc, hbrBack);

						return true;
					}
					case WM_UAHDRAWMENUITEM:
					{
						UAHDRAWMENUITEM* pUDMI = (UAHDRAWMENUITEM*)lParam;

						HBRUSH* pbrBackground = &hbrBack;

						wchar_t menuString[256] = { 0 };
						MENUITEMINFO mii = { sizeof(mii), MIIM_STRING };
						{
							mii.dwTypeData = menuString;
							mii.cch = (sizeof(menuString) / 2) - 1;

							GetMenuItemInfo(pUDMI->um.hmenu, pUDMI->umi.iPosition, TRUE, &mii);
						}

						DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;

						int iTextStateID = 0;
						int iBackgroundStateID = 0;
						{
							if ((pUDMI->dis.itemState & ODS_INACTIVE) | (pUDMI->dis.itemState & ODS_DEFAULT))
							{
								iTextStateID = MPI_NORMAL;
								iBackgroundStateID = MPI_NORMAL;
							}
							if (pUDMI->dis.itemState & ODS_HOTLIGHT)
							{
								iTextStateID = MPI_HOT;
								iBackgroundStateID = MPI_HOT;

								pbrBackground = &hbrStripe;
							}
							if (pUDMI->dis.itemState & ODS_SELECTED)
							{
								iTextStateID = MPI_HOT;
								iBackgroundStateID = MPI_HOT;

								pbrBackground = &hbrStripe;
							}
							if ((pUDMI->dis.itemState & ODS_GRAYED) || (pUDMI->dis.itemState & ODS_DISABLED))
							{
								iTextStateID = MPI_DISABLED;
								iBackgroundStateID = MPI_DISABLED;
							}
							if (pUDMI->dis.itemState & ODS_NOACCEL)
							{
								dwFlags |= DT_HIDEPREFIX;
							}
						}

						if (!g_menuTheme)
							g_menuTheme = OpenThemeData(hWnd, L"Menu");

						DTTOPTS opts = { sizeof(opts), DTT_TEXTCOLOR, iTextStateID != MPI_DISABLED ? rgbText : rgbTextD };

						FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, *pbrBackground);
						DrawThemeTextEx(g_menuTheme, pUDMI->um.hdc, MENU_BARITEM, MBI_NORMAL, menuString, mii.cch, dwFlags, &pUDMI->dis.rcItem, &opts);

						return true;
					}
					case WM_UAHMEASUREMENUITEM:
					{
						UAHMEASUREMENUITEM* pMmi = (UAHMEASUREMENUITEM*)lParam;
						*lr = DefWindowProc(hWnd, message, wParam, lParam);
						//pMmi->mis.itemWidth = (pMmi->mis.itemWidth * 4) / 3;
						return true;
					}
					case WM_THEMECHANGED:
					{
						if (g_menuTheme)
						{
							CloseThemeData(g_menuTheme);
							g_menuTheme = nullptr;
						}
						return false;
					}
					case WM_NCPAINT:
					case WM_NCACTIVATE:
						*lr = DefWindowProc(hWnd, message, wParam, lParam);
						UAHDrawMenuNCBottomLine(hWnd);
						return true;
						break;
					default:
						return false;
					}
				}
#pragma endregion
#pragma region Dropdown
				enum PreferredAppMode
				{
					Default,
					AllowDark,
					ForceDark,
					ForceLight,
					Max
				};
				using fnAllowDarkModeForWindow = bool (WINAPI*)(HWND hWnd, bool allow); // ordinal 133
				using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode); // ordinal 135, in 1903
				using fnFlushMenuThemes = void (WINAPI*)(void); //ordinal 136
#pragma endregion

				void HandleMenu()
				{
					if (!IsWin10())
						return;

					auto hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
					if (hUxtheme == nullptr)
						return;

					auto AllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133));
					auto SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
					auto FlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
					AllowDarkModeForWindow(hWndMain, true);
					SetPreferredAppMode(theme ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
					FlushMenuThemes();
					SendMessageW(hWndMain, WM_THEMECHANGED, 0, 0);
					FreeLibrary(hUxtheme);
				}
			}
		}

		void SetThemeColors()
		{
			theme = ini.GetLongValue(L"misc", L"theme", 0);

			if (theme == 2) //match
				theme = Windows10::MatchTheme();

			switch (theme)
			{
			case 0: //light
			{
				rgbBack = RGB(255, 255, 255);
				rgbStripe = RGB(240, 240, 240);
				rgbText = RGB(0, 0, 0);
				rgbTextD = RGB(131, 131, 131);
				rgbList = RGB(0, 0, 0);
				rgbHeader = Windows10::GetAccent(RGB(0x00, 0x33, 0x99));
				rgbListBk = RGB(255, 255, 255);
				break;
			}
			case 1: //dark
			{
				rgbBack = RGB(43, 43, 43);
				rgbStripe = RGB(65, 65, 65);
				rgbText = RGB(255, 255, 255);
				rgbTextD = RGB(109, 109, 109);
				rgbList = RGB(255, 255, 255);
				rgbHeader = Windows10::GetAccent(RGB(0x8E, 0xCA, 0xF8));
				rgbListBk = RGB(65, 65, 65);
			}
			//this space for rent
			}
			if (hbrBack != NULL) DeleteObject(hbrBack);
			if (hbrStripe != NULL) DeleteObject(hbrStripe);
			if (hbrList != NULL) DeleteObject(hbrList);
			if (hpnStripe != NULL) DeleteObject(hpnStripe);
			hbrBack = CreateSolidBrush(rgbBack);
			hbrStripe = CreateSolidBrush(rgbStripe);
			hbrList = CreateSolidBrush(rgbListBk);
			hpnStripe = CreatePen(PS_SOLID, 1, rgbStripe);

			Windows10::DarkMenuBar::HandleMenu();

			RedrawWindow(hWndMain, NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
			if (About::hWnd != NULL) RedrawWindow(About::hWnd, NULL, NULL, RDW_INVALIDATE);
			if (DeviceManager::hWnd != NULL) RedrawWindow(DeviceManager::hWnd, NULL, NULL, RDW_INVALIDATE);
			if (MemoryViewer::hWnd != NULL) RedrawWindow(MemoryViewer::hWnd, NULL, NULL, RDW_INVALIDATE);
			if (Options::hWnd != NULL) RedrawWindow(Options::hWnd, NULL, NULL, RDW_INVALIDATE);
			if (ButtonMaps::hWnd != NULL) RedrawWindow(ButtonMaps::hWnd, NULL, NULL, RDW_INVALIDATE);
		}
	}

	namespace Images
	{
		HIMAGELIST hIml = NULL;

		HBITMAP GetImageListImage(int index)
		{
			//Step one, prepare a canvas.
			auto scrDC = GetDC(0);
			BITMAPINFO bmi = { 0 };
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biWidth = bmi.bmiHeader.biHeight = 16;
			void* throwAway; //only here to satisfy code analysis :shrug:
			auto hbm = CreateDIBSection(scrDC, &bmi, DIB_RGB_COLORS, &throwAway, NULL, 0);
			ReleaseDC(0, scrDC);
			if (hbm == NULL) return NULL;
			//Step two, draw on it.
			auto dc = CreateCompatibleDC(0);
			auto oldBm = SelectObject(dc, hbm);
			ImageList_Draw(hIml, index, dc, 0, 0, ILD_NORMAL);
			SelectObject(dc, oldBm);
			ReleaseDC(0, dc);

			return hbm;
		}

		std::vector<unsigned char> LoadPNGResource(int id, unsigned long *width, unsigned long *height)
		{
			unsigned long size;

			auto find = FindResourceEx(NULL, L"PNG", MAKEINTRESOURCE(id), langID);
			if (find == NULL) return { 0 };
			auto res = LoadResource(NULL, find);
			if (res == NULL) return { 0 };
			auto lock = LockResource(res);
			size = SizeofResource(NULL, find);

			std::vector<unsigned char> image;
			decodePNG(image, *width, *height, (const unsigned char*)lock, size);

			for (unsigned long i = 0; i < (*width * *height * 4); i += 4)
			{
				auto r = image[i + 0];
				auto b = image[i + 2];
				image[i + 0] = b;
				image[i + 2] = r;

				//clean up messy alpha channels
				if (image[i + 3] == 0)
				{
					image[i + 0] = 0;
					image[i + 1] = 0;
					image[i + 2] = 0;
				}
			}
			return image;
		}

		HBITMAP LoadPNGResource(int id)
		{
			unsigned long width, height;
			auto image = LoadPNGResource(id, &width, &height);
			auto bitmap = CreateBitmap(width, height, 1, 32, &image[0]);
			return bitmap;
		}
	}

	namespace Tooltips
	{
		HWND hWndTooltips;

		void Initialize()
		{
			hWndTooltips = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX /* | TTS_BALLOON */, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWndMain, NULL, hInstance, NULL);
			SetWindowPos(hWndTooltips, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		}

		void CreateTooltips(HWND hWndClient, ...)
		{
			va_list args;
			va_start(args, hWndClient);

			int dlgItem = va_arg(args, int);
			while (dlgItem)
			{
				TOOLINFO ti =
				{
					sizeof(TOOLINFO), TTF_IDISHWND | TTF_SUBCLASS, hWndClient,
					(UINT_PTR)GetDlgItem(hWndClient, dlgItem),
					{ 0, 0, 0, 0 }, hInstance,
					MAKEINTRESOURCE(IDS_TOOLTIPS + dlgItem)
				};
				SendMessage(hWndTooltips, TTM_ADDTOOL, 0, (LPARAM)&ti);
				dlgItem = va_arg(args, int);
			}

			va_end(args);
		}

		void DestroyTooltips()
		{
			DestroyWindow(hWndTooltips);
		}
	}

	WNDPROC SDLWinProc = NULL;
	LRESULT WndProc(HWND hWnd, unsigned int message, WPARAM wParam, LPARAM lParam)
	{
		//We get to do a lotta fancy fuckery now that this is a Win32 wndproc and not
		//SDL's watered-down one.

		LRESULT lr = 0;
		if (theme == 1 && Presentation::Windows10::DarkMenuBar::UAHWndProc(hWnd, message, wParam, lParam, &lr)) {
			return lr;
		}

		switch (message)
		{
		case WM_COMMAND:
		{
			if (wParam > 1000 && wParam < 2000)
			{
				uiCommand = (int)(wParam - 1000);
				if (uiCommand >= cmdMemViewer && uiCommand <= cmdButtonMapper)
				{
					void(*commands[])(void) = {
						MemoryViewer::Show, PalViewer::Show, About::Show,
						Options::Show, Shaders::Show, DeviceManager::Show,
						TileViewer::Show, nullptr, nullptr, nullptr, nullptr,
						nullptr, UI::ButtonMaps::Show,
					};
					if (commands[uiCommand - cmdMemViewer] != nullptr)
					{
						commands[uiCommand - cmdMemViewer]();
						uiCommand = 0;
					}
				}
				return 0;
			}
		}
		case WM_SETTINGCHANGE:
		{
			if (!strcmp((const char*)lParam, "ImmersiveColorSet"))
			{
				if (ini.GetLongValue(L"misc", L"theme", 0) == 2)
				{
					//Lightly throttle the five or six identical WM.
					static __time64_t lastColorChange = 0;
					__time64_t now;
					_time64(&now);
					if (now >= lastColorChange + 1)
					{
						lastColorChange = now;
						Presentation::SetThemeColors();
					}
				}
			}
		}
		default:
			return CallWindowProc(SDLWinProc, hWnd, message, wParam, lParam);
		}
	}

	WCHAR statusFPS[16], statusText[512];

	HDC statusRealDC = NULL;
	HDC statusDC = NULL;
	HBITMAP statusBmp = NULL;
	void DrawStatusBar();

	void ResizeStatusBar()
	{
		RECT sbRect;
		GetClientRect(hWndMain, &sbRect);
		SetWindowPos(hWndStatusBar, HWND_NOTOPMOST, 0, sbRect.bottom - statusBarHeight, sbRect.right, statusBarHeight, SWP_NOZORDER);

		if (statusRealDC) ReleaseDC(hWndMain, statusRealDC);
		if (statusDC) DeleteDC(statusDC);
		if (statusBmp) DeleteObject(statusBmp);
		statusRealDC = GetDC(hWndStatusBar);
		statusDC = CreateCompatibleDC(statusRealDC);
		statusBmp = CreateCompatibleBitmap(statusRealDC, sbRect.right, sbRect.bottom);

		SelectObject(statusDC, GetStockObject(DC_PEN));
		SelectObject(statusDC, Presentation::statusFont);
		SelectObject(statusDC, statusBmp);

		DrawStatusBar();
	}

	void DrawStatusBar()
	{
		RECT sbRect;
		GetClientRect(hWndMain, &sbRect);
		sbRect.bottom = statusBarHeight;

		if (hideUI)
		{
			//FillRect(statusDC, &sbRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
			BitBlt(statusRealDC, 0, 0, sbRect.right, sbRect.bottom, NULL, 0, 0, BLACKNESS);
			return;
		}

		auto hdc = statusDC;

		FillRect(hdc, &sbRect, Presentation::hbrBack);

		SetDCPenColor(hdc, Presentation::rgbStripe);
		MoveToEx(hdc, 0, 0, NULL);
		LineTo(hdc, sbRect.right, 0);
		MoveToEx(hdc, 40, 3, NULL);
		LineTo(hdc, 40, sbRect.bottom - 3);

		SetBkColor(hdc, Presentation::rgbBack);
		SetTextColor(hdc, Presentation::rgbText);
		RECT sbText = { 4, 4, 32, sbRect.bottom - 4 };
		DrawText(hdc, statusFPS, wcslen(statusFPS), &sbText, DT_RIGHT);
		sbText.left = 48;
		sbText.right = sbRect.right - 48;
		DrawText(hdc, statusText, wcslen(statusText), &sbText, DT_END_ELLIPSIS);

		int x = sbRect.right - 24;
		ImageList_Draw(Images::hIml, IML_PAUSESTATUS + pauseState, hdc, x, 3, ILD_NORMAL);
		x -= 20;
		if (::key2joy)
		{
			ImageList_Draw(Images::hIml, IML_GAMEPAD, hdc, x, 3, ILD_NORMAL);
			x -= 20;
		}
		if (UI::mouseLocked)
		{
			ImageList_Draw(Images::hIml, IML_MOUSE, hdc, x, 3, ILD_NORMAL);
			x -= 20;
		}
		if (hddIconTimer)
		{
			ImageList_Draw(Images::hIml, IML_HARDDRIVE, hdc, x, 3, ILD_NORMAL);
			x -= 20;
		}
		if (diskIconTimer)
		{
			ImageList_Draw(Images::hIml, IML_DISKETTE, hdc, x, 3, ILD_NORMAL);
			x -= 20;
		}

		BitBlt(statusRealDC, 0, 0, sbRect.right, sbRect.bottom, hdc, 0, 0, SRCCOPY);
	}

	void Update()
	{
		if (statusTimer)
			statusTimer--;
		else
			statusText[0] = 0;

		if (diskIconTimer)
			diskIconTimer--;
		if (hddIconTimer)
			hddIconTimer--;

		if (!hideUI)
			DrawStatusBar();
	}

	void Initialize()
	{
		//SetProcessDPIAware();

		//GetCurrentDirectory(FILENAME_MAX, startingPath);
		GetModuleFileName(NULL, startingPath, 256);
		WCHAR* lastSlash = wcsrchr(startingPath, L'\\');
		*lastSlash = 0;
		if (!wcsncmp(lastSlash - 5, L"Debug",6))
		{
			lastSlash = wcsrchr(startingPath, L'\\');
			*lastSlash = 0;
		}

		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if (SDL_GetWindowWMInfo(Video::sdlWindow, &info))
		{
			if (info.subsystem != SDL_SYSWM_WINDOWS)
				return;

			hWndMain = info.info.win.window;
			hInstance = info.info.win.hinstance;

			SDLWinProc = (WNDPROC)GetWindowLongPtr(hWndMain, GWLP_WNDPROC);
			SetWindowLongPtr(hWndMain, GWLP_WNDPROC, (LONG_PTR)WndProc);

			InitCommonControls();

			Images::hIml = ImageList_Create(16, 16, ILC_COLOR32, 32, 0);
			ImageList_Add(Images::hIml, Images::LoadPNGResource(IDB_ICONS), NULL);

			menuBar = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAINMENU));

			bool drunk = false;
			{
				using fnWineGetVersion = const char *(CDECL*)(void);
				HMODULE hntdll = GetModuleHandle(L"ntdll.dll");
				if (hntdll)
				{
					auto wineGetVersion = (fnWineGetVersion)GetProcAddress(hntdll, "wine_get_version");
					if (wineGetVersion)
						drunk = true;
				}
			}

			if (!drunk)
			{
				MENUINFO mainInfo =
				{
					sizeof(MENUINFO),
					MIM_APPLYTOSUBMENUS | MIM_STYLE,
					/* MNS_MODELESS | */ MNS_AUTODISMISS,
					0,
					NULL,
					NULL,
					0
				};
				SetMenuInfo(menuBar, &mainInfo);

				MENUITEMINFO miInfo = { sizeof(MENUITEMINFO), MIIM_BITMAP };
				for (int i = 0; i < 24; i++)
				{
					//auto hBmp = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(1000 + i), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION);
					auto hBmp = Images::GetImageListImage(IML_MENUSTART + i);
					if (hBmp == NULL)
						continue;
					miInfo.hbmpItem = hBmp;
					SetMenuItemInfo(menuBar, 1000 + i, FALSE, &miInfo);
				}
			}

			SetMenu(hWndMain, menuBar);

			hWndStatusBar = CreateWindowEx(0, L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 0, 0, 0, hWndMain, 0, hInstance, NULL);
			statusBarHeight = 23;
			wcscpy_s(statusFPS, 16, L"     ");

			auto scrDC = GetDC(0);
			auto dpiY = GetDeviceCaps(scrDC, LOGPIXELSY);
			auto headerSize = MulDiv(13, dpiY, 72);
			ReleaseDC(0, scrDC);

			Presentation::headerFont = CreateFont(headerSize, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
			Presentation::monoFont = CreateFont(16, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Courier New");
			Presentation::statusFont = CreateFont(14, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

			Presentation::SetThemeColors();
			Tooltips::Initialize();

			SetTitle(NULL);

			if (startFullscreen)
				HideUI(true);
		}
		return;
	}

	bool ShowFileDlg(bool toSave, WCHAR* target, size_t max, const WCHAR* filter)
	{
		WCHAR sFilter[512];
		wcscpy_s(sFilter, 512, filter);
		WCHAR* f = sFilter;
		while (*f)
		{
			if (*f == L'|')
				*f = 0;
			f++;
		}
		*f++ = 0;
		*f++ = 0;

		OPENFILENAME ofn = {
			sizeof(OPENFILENAME),
			hWndMain, hInstance,
			sFilter, NULL, 0, 1,
			target, max,
			NULL, 0,
			NULL,
			NULL,
			OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
		};
		ofn.lpstrFile[0] = '\0';

		WCHAR cwd[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, cwd);

		if ((!toSave && (GetOpenFileName(&ofn) != FALSE)) || (toSave && (GetSaveFileName(&ofn) != FALSE)))
		{
			SetCurrentDirectory(cwd);
			wcscpy_s(target, max, ofn.lpstrFile);
			return true;
		}

		SetCurrentDirectory(cwd);
		return false;
	}

	void SetStatus(const WCHAR* text)
	{
		wcscpy_s(statusText, 512, text);
		statusTimer = 4 * wcslen(text);
	}

	void SetStatus(int stab)
	{
		WCHAR b[256];
		if (LoadString(hInstance, stab, b, 256) > 0)
			SetStatus(b);
	}

	void SetFPS(int fps)
	{
		if (fpsVisible)
			_itow_s(fps, statusFPS, 16, 10);
		else
			wcscpy_s(statusFPS, 16, L"     ");
	}

	static WCHAR getStringBuffer[512];
	WCHAR* GetString(int stab)
	{
		if (LoadString(hInstance, stab, getStringBuffer, 512) > 0)
			return getStringBuffer;
		return L"";
	}

#define MAXSNOW 256
	int snowData[MAXSNOW * 2];
	int snowTimer = -1;

	void LetItSnow()
	{
		if (snowTimer == -1)
		{
			//Prepare
			srand(0xC001FACE); //-V1057 this is entirely on purpose actually lol
			for (int i = 0; i < MAXSNOW; i++)
			{
				snowData[(i * 2) + 0] = rand() % 639;
				snowData[(i * 2) + 1] = rand() % 479;
			}
			snowTimer = 1;
		}

		snowTimer--;
		if (snowTimer == 0)
		{
			snowTimer = 4;
			for (int i = 0; i < MAXSNOW; i++)
			{
				snowData[(i * 2) + 0] += -1 + (rand() % 4);
				snowData[(i * 2) + 1] += rand() % 2;
				if (snowData[(i * 2) + 0] >= 639)
					snowData[(i * 2) + 0] = 0;
				if (snowData[(i * 2) + 1] >= 479)
				{
					snowData[(i * 2) + 0] = rand() % 639;
					snowData[(i * 2) + 1] = -10;
				}
			}
		}

		for (int i = 0; i < MAXSNOW; i++)
		{
			int x = snowData[(i * 2) + 0];
			int y = snowData[(i * 2) + 1];
			if (x < 0 || y < 0 || x >= 639 || y >= 479)
				continue;
			auto target = ((y * 640) + x) * 4;
			const auto nl = 640 * 4;
			Video::pixels[target + 0] = Video::pixels[target + 1] = Video::pixels[target + 2] = 255;
			//chunky bois
			Video::pixels[target + nl + 0] = Video::pixels[target + nl + 1] = Video::pixels[target + nl + 2] = 255;
			target += 4;
			Video::pixels[target + 0] = Video::pixels[target + 1] = Video::pixels[target + 2] = 255;
			Video::pixels[target + nl + 0] = Video::pixels[target + nl + 1] = Video::pixels[target + nl + 2] = 255;
		}
	}

	void ResetPath()
	{
		SetCurrentDirectory(startingPath);
	}

	bool ReportLoadingFail(int messageId, int err, int device, const WCHAR* fileName, bool offerToForget)
	{
		WCHAR b[1024] = { 0 };
		WCHAR e[1024] = { 0 };
		WCHAR f[1024] = { 0 };
		WCHAR g[1024] = { 0 };
		_wsplitpath_s(fileName, NULL, 0, NULL, 0, f, ARRAYSIZE(f), e, ARRAYSIZE(e));
		wcscat(f, e);
		_wcserror_s(e, ARRAYSIZE(e), err);
		wsprintf(b, UI::GetString(messageId), f, device);
		wsprintf(f, L"%s", UI::GetString(IDS_SHORTTITLE));

		TASKDIALOGCONFIG tdc = { 0 };
		tdc.cbSize = sizeof(TASKDIALOGCONFIG);
		tdc.pszWindowTitle = f;
		tdc.pszMainInstruction = b;
		tdc.pszContent = e;
		tdc.dwCommonButtons = TDCBF_OK_BUTTON;
		tdc.pszMainIcon = TD_WARNING_ICON;

		if (offerToForget)
		{
			wsprintf(g, L"%s", UI::GetString(IDS_FORGETABOUTDISK));
			tdc.pszVerificationText = g;
		}
		int forget;
		TaskDialogIndirect(&tdc, NULL, NULL, &forget);
		return forget > 0;
		//TaskDialog(NULL, NULL, UI::GetString(IDS_SHORTTITLE), b, e, TDCBF_OK_BUTTON, TD_WARNING_ICON, NULL);
	}

	void ShowOpenFileDialog(int command, const WCHAR* pattern)
	{
		WCHAR thing[FILENAME_MAX] = { 0 };
		wcscpy_s(thing, FILENAME_MAX, (WCHAR*)ini.GetValue(L"media", L"rom", L""));
		if (thing[0] == 0)
		{
			wcscpy_s(thing, FILENAME_MAX, lastPath);
			if (thing[0] == 0)
				wcscpy_s(thing, FILENAME_MAX, startingPath);
			if (thing[wcslen(thing)] != L'\\')
				wcscat_s(thing, FILENAME_MAX, L"\\");
		}
		if (thing[0] != 0)
		{
			auto lastSlash = (WCHAR*)wcsrchr(thing, L'\\');
			if (lastSlash)
				*(lastSlash + 1) = 0;
		}

		uiCommand = 0;

		WCHAR thePath[FILENAME_MAX] = { 0 };
		wcscpy_s(thePath, FILENAME_MAX, thing);
		if (ShowFileDlg(false, thePath, FILENAME_MAX, pattern))
		{
			wcscpy_s(uiString, FILENAME_MAX, thePath);
			uiCommand = command;
		}

		//if (pauseState == 0)
		//	pauseState = 1;
	}

	void InsertDisk(int devId)
	{
		ShowOpenFileDialog(cmdInsertDisk, (((DiskDrive*)devices[devId])->GetType() == ddDiskette ? GetString(IDS_DDFILTER) : GetString(IDS_HDFILTER)));
		if (uiCommand == 0)
			return;
		WCHAR key[16];
		_itow_s(devId, key, 16, 10);
		auto ret = ((DiskDrive*)devices[devId])->Mount(uiString);
		if (ret == -1)
			SetStatus(IDS_EJECTFIRST); //"Eject the diskette first, with RCtrl+Shift+U."
		else if (ret != 0)
		{
			//Log(L"Error %d trying to open disk image.", ret);
			ReportLoadingFail(IDS_DISKIMAGEERROR, ret, devId, uiString);
			uiCommand = 0;
		}
		else
		{
			if (((DiskDrive*)devices[devId])->GetType() == ddDiskette)
				ini.SetValue(L"devices/diskDrive", key, uiString);
			else
				ini.SetValue(L"devices/hardDrive", key, uiString);
			SaveINI();
			if (DeviceManager::hWnd != NULL) DeviceManager::UpdatePage(true);
		}
	}

	void EjectDisk(int devId)
	{
		((DiskDrive*)devices[devId])->Unmount();
		WCHAR key[16];
		_itow_s(devId, key, 16, 10);
		if (((DiskDrive*)devices[devId])->GetType() == ddDiskette)
			ini.SetValue(L"devices/diskDrive", key, L"");
		else
			ini.SetValue(L"devices/hardDrive", key, L"");
		ResetPath();
		ini.SaveFile(settingsFile, false);
		SetStatus(IDS_DISKEJECTED); //"Disk ejected."
		if (DeviceManager::hWnd != NULL) DeviceManager::UpdatePage(true);
	}

	void SetTitle(const WCHAR* subtitle)
	{
		WCHAR title[512] = { 0 };
		if (subtitle == NULL)
			wcscpy_s(title, 512, GetString(IDS_FULLTITLE));
		else
			wsprintf(title, GetString(IDS_TEMPLATETITLE), subtitle);
		SetWindowText(hWndMain, title);
	}

	void Complain(int message)
	{
		WCHAR msg[512];
		wcscpy(msg, UI::GetString(message));

		Log(msg);

		TASKDIALOGCONFIG td = {
			sizeof(TASKDIALOGCONFIG), UI::hWndMain, NULL,
			TDF_EXPAND_FOOTER_AREA, TDCBF_OK_BUTTON,
			UI::GetString(IDS_SHORTTITLE),
			NULL,
			NULL, msg,
			0, NULL, IDOK,
			0, NULL, 0,
			NULL,
			NULL
		};
		td.pszMainIcon = TD_WARNING_ICON;
		TaskDialogIndirect(&td, NULL, NULL, NULL);
	}

	void SaveINI()
	{
		ResetPath();
		ini.SaveFile(settingsFile, false);
	}

	void HideUI(bool newHideState)
	{
		static RECT r;
		if (!hideUI && newHideState)
		{
			GetWindowRect(hWndMain, &r);
			SDL_SetWindowFullscreen(Video::sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
			SetMenu(hWndMain, NULL);
		}
		else
		{
			SDL_SetWindowFullscreen(Video::sdlWindow, 0);
			SetWindowPos(hWndMain, 0, r.left, r.top, r.right - r.left, r.bottom - r.top, 0);
			SetMenu(hWndMain, menuBar);
		}
		hideUI = newHideState;
	}
}
