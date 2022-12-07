#include "..\asspull.h"

namespace UI
{
	namespace DeviceManager
	{
		using namespace Presentation;

		HWND hWnd = NULL;
		RECT shrugIcon;
		HBITMAP hShrugImage;
		int numDrives;

		int currentPage = -1;
		
		void GetDevListItem(int i, int* icon, WCHAR* text)
		{
			if (devices[i] == NULL)
			{
				wsprintf(text, L"%d. %s", i, GetString(IDS_DEVICES1 + 0)); //"Nothing"
				*icon = IML_CROSS;
			}
			else
			{
				switch (devices[i]->GetID())
				{
				case DEVID_DISKDRIVE:
					if (((DiskDrive*)devices[i])->GetType() == ddDiskette)
					{
						wsprintf(text, L"%d. %s", i, GetString(IDS_DEVICES1 + 1)); //"Diskette drive"
						*icon = IML_DISKDRIVE;
					}
					else
					{
						wsprintf(text, L"%d. %s", i, GetString(IDS_DEVICES1 + 2)); //"Hard drive"
						*icon = IML_HARDDRIVE;
					}
					break;
				case DEVID_LINEPRINTER:
					wsprintf(text, L"%d. %s", i, GetString(IDS_DEVICES1 + 3)); //"Line printer"
					*icon = IML_PRINTER;
					break;
				}
			}
		}

		void UpdatePage()
		{
			int devNum = SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_GETCURSEL, 0, 0) + 1;
			auto device = devices[devNum];

			if (devNum == currentPage)
				return;
			currentPage = devNum;

			//Don't allow changing device #0 from disk drive
			//EnableWindow(GetDlgItem(hWnd, IDC_DEVTYPE), (devNum > 0));

			//Hide everything regardless at first.
			int everything[] = { IDC_DEVNONE, IDC_DDFILE, IDC_DDINSERT, IDC_DDEJECT };
			for (int i = 0; i < ARRAYSIZE(everything); i++)
				ShowWindow(GetDlgItem(hWnd, everything[i]), SW_HIDE);

			if (device == NULL)
			{
				ShowWindow(GetDlgItem(hWnd, IDC_DEVNONE), SW_SHOW);
				//ShowWindow(GetDlgItem(hWnd, IDC_SHRUG), SW_SHOW);
				SetDlgItemText(hWnd, IDC_HEADER, GetString(IDS_DEVICES2 + 0)); //"No device"
				SendDlgItemMessage(hWnd, IDC_DEVTYPE, CB_SETCURSEL, 0, 0);
			}
			else
			{
				switch (device->GetID())
				{
				case DEVID_DISKDRIVE:
				{
					for (int i = 1; i < 4; i++)
						ShowWindow(GetDlgItem(hWnd, everything[i]), SW_SHOW);
					SetDlgItemText(hWnd, IDC_HEADER, (((DiskDrive*)device)->GetType() == ddHardDisk) ? GetString(IDS_DEVICES2 + 2) : GetString(IDS_DEVICES2 + 1)); //"Diskette drive" "Hard drive"
					SendDlgItemMessage(hWnd, IDC_DEVTYPE, CB_SETCURSEL, (((DiskDrive*)device)->GetType() == ddHardDisk) ? 2 : 1, 0);
					WCHAR key[8] = { 0 };
					_itow(devNum, key, 10);
					auto val = ini.GetValue((((DiskDrive*)device)->GetType() == ddDiskette) ? L"devices/diskDrive" : L"devices/hardDrive", key, L"");
					SetDlgItemText(hWnd, IDC_DDFILE, val);
					EnableWindow(GetDlgItem(hWnd, IDC_DDINSERT), val[0] == 0);
					EnableWindow(GetDlgItem(hWnd, IDC_DDEJECT), val[0] != 0);
					break;
				}
				case DEVID_LINEPRINTER:
				{
					ShowWindow(GetDlgItem(hWnd, IDC_DEVNONE), SW_SHOW);
					//ShowWindow(GetDlgItem(hWnd, IDC_SHRUG), SW_SHOW);
					SetDlgItemText(hWnd, IDC_HEADER, GetString(IDS_DEVICES2 + 3)); //"Line printer"
					SendDlgItemMessage(hWnd, IDC_DEVTYPE, CB_SETCURSEL, 3, 0);
					break;
				}
				}
			}

			//if (numDrives > 4 || firstDev)
			InvalidateRect(hWnd, NULL, true);
		}

		void UpdateList()
		{
			WCHAR item[64] = { 0 };
			int icon = 0;
			int selection = 0;

			if (SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_GETCOUNT, 0, 0))
			{
				selection = SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
				GetDevListItem(selection + 1, &icon, item);
				SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_INSERTSTRING, selection, (LPARAM)item);
				SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_DELETESTRING, selection + 1, 0);
				SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_SETITEMDATA, selection, (LPARAM)icon);
				SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_SETCURSEL, selection, 0);
				UpdatePage();
				return;
			}

			SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_RESETCONTENT, 0, 0);
			for (int i = 1; i < MAXDEVS; i++)
			{
				GetDevListItem(i, &icon, item);
				SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_ADDSTRING, 0, (LPARAM)item);
				SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_SETITEMDATA, i - 1, (LPARAM)icon);
			}

			SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_SETCURSEL, selection, 0);
			UpdatePage();
		}

		void SwitchDevice()
		{
			int devNum = SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_GETCURSEL, 0, 0) + 1;
			int newType = SendDlgItemMessage(hWnd, IDC_DEVTYPE, CB_GETCURSEL, 0, 0);

			int oldType = 0;
			if (devices[devNum] != NULL)
			{
				switch (devices[devNum]->GetID())
				{
				case DEVID_DISKDRIVE:
					oldType = 1;
					if (((DiskDrive*)devices[devNum])->GetType() == ddHardDisk)
						oldType = 2;
					break;
				case DEVID_LINEPRINTER:
					oldType = 3;
					break;
				}
			}

			if (newType == oldType)
				return;

			WCHAR key[8] = { 0 };
			_itow(devNum, key, 10);

			if (devices[devNum] != NULL) delete devices[devNum];
			switch (newType)
			{
			case 0:
				devices[devNum] = NULL;
				//ini.SetValue("devices", key, "");
				ini.Delete(L"devices", key, true);
				break;
			case 1:
				devices[devNum] = (Device*)(new DiskDrive(0));
				ini.SetValue(L"devices", key, L"diskDrive");
				break;
			case 2:
				devices[devNum] = (Device*)(new DiskDrive(1));
				ini.SetValue(L"devices", key, L"hardDrive");
				break;
			case 3:
				devices[devNum] = (Device*)(new LinePrinter());
				ini.SetValue(L"devices", key, L"linePrinter");
				break;
			}
			ini.SaveFile(settingsFile, false);
			UpdateList();
		}

		BOOL CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_CLOSE:
			{
				DestroyWindow(hWnd);
				DeleteObject(hShrugImage);
				DeviceManager::hWnd = NULL;
				if (!wasPaused) pauseState = pauseNot;
				return true;
			}
			case WM_INITDIALOG:
			{
				DeviceManager::hWnd = hWnd;
				const int deviceIcons[] = { IML_CROSS, IML_DISKDRIVE, IML_HARDDRIVE, IML_PRINTER };
				SendDlgItemMessage(hWnd, IDC_DEVNONE, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
				for (int i = 0; i < 4; i++)
				{
					SendDlgItemMessage(hWnd, IDC_DEVTYPE, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_DEVICES1 + i));
					SendDlgItemMessage(hWnd, IDC_DEVTYPE, CB_SETITEMDATA, i, deviceIcons[i]);
				}
				GetIconPos(hWnd, IDC_SHRUG, &shrugIcon, 0, 0);
				hShrugImage = Images::LoadPNGResource(IDB_SHRUG);
				UpdateList();
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
					case IDC_DDINSERT:
					case IDC_DDEJECT: return DrawButton(hWnd, nmc);
						break;
					}
				}
				}
			}
			case WM_ERASEBKGND:
				return false;
			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				FillRect(hdc, &ps.rcPaint, hbrBack);
				auto hdcMem = CreateCompatibleDC(hdc);
				if (IsWindowVisible(GetDlgItem(hWnd, IDC_DEVNONE)))
				{
					auto oldBitmap = SelectObject(hdcMem, hShrugImage);
					BLENDFUNCTION ftn = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
					AlphaBlend(hdc, shrugIcon.left, shrugIcon.top, 64, 64, hdcMem, 0, 0, 64, 64, ftn);
					SelectObject(hdcMem, oldBitmap);
				}
				DeleteDC(hdcMem);
				EndPaint(hWnd, &ps);
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
				if (lParam == (LPARAM)GetDlgItem(hWnd, IDC_DEVNONE))
					SetTextColor((HDC)wParam, rgbHeader);
				SetBkColor((HDC)wParam, rgbBack);
				if (lParam == (LPARAM)GetDlgItem(hWnd, IDC_DDFILE))
				{
					SetTextColor((HDC)wParam, rgbList);
					SetBkColor((HDC)wParam, rgbListBk);
					return (INT_PTR)hbrList;
				}
				return (INT_PTR)hbrBack;
			}
			case WM_CTLCOLORBTN:
			{
				return (INT_PTR)hbrBack;
			}
			case WM_MEASUREITEM:
			{
				auto mis = (LPMEASUREITEMSTRUCT)lParam;
				if (mis->itemHeight < 18)
					mis->itemHeight = 18;
				break;
			}
			case WM_DRAWITEM:
			{
				auto dis = (LPDRAWITEMSTRUCT)lParam;
				if (dis->itemID == -1)
					break;
				SetTextColor(dis->hDC, dis->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHTTEXT) : rgbList);
				SetBkColor(dis->hDC, dis->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHT) : rgbListBk);

				TEXTMETRIC tm;
				GetTextMetrics(dis->hDC, &tm);
				int y = (dis->rcItem.bottom + dis->rcItem.top - tm.tmHeight) / 2;
				//int x = LOWORD(GetDialogBaseUnits()) / 4;

				WCHAR text[256] = { 0 };

				if (dis->CtlID == IDC_DEVTYPE)
					SendMessage(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)text);
				else if (dis->CtlID == IDC_DEVLIST)
					SendMessage(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);

				ExtTextOut(dis->hDC, 24, y, ETO_CLIPPED | ETO_OPAQUE, &dis->rcItem, text, (UINT)wcslen(text), NULL);
				ImageList_Draw(Images::hIml, dis->itemData, dis->hDC, dis->rcItem.left + 1, dis->rcItem.top + 1, ILD_NORMAL);
				return true;
			}
			case WM_COMMAND:
			{
				if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == IDC_DEVLIST)
				{
					UpdatePage();
					return true;
				}
				if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_DEVTYPE)
				{
					SwitchDevice();
					return true;
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_DDINSERT)
				{
					int devID = SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_GETCURSEL, 0, 0) + 1;
					InsertDisk(devID);
					return true;
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_DDEJECT)
				{
					int devID = SendDlgItemMessage(hWnd, IDC_DEVLIST, LB_GETCURSEL, 0, 0) + 1;
					EjectDisk(devID);
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK)
				{
					DestroyWindow(hWnd);
					hWnd = NULL;
					if (!wasPaused) pauseState = pauseNot;
					return true;
				}
			}
			}
			return false;
		}

		void Show()
		{
			if (!IsWindow(hWnd))
			{
				wasPaused = pauseState > 0;
				if (!wasPaused) pauseState = pauseEntering;
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICES), (HWND)hWndMain, (DLGPROC)WndProc);
				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}
