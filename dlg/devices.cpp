#include "..\asspull.h"

namespace UI
{
	namespace DeviceManager
	{
		using namespace Presentation;

		HWND hWnd = NULL;
		RECT primaryIcon, warningIcon, shrugIcon;
		HBITMAP hShrugImage;
		int numDrives;

		void UpdatePage(HWND hwndDlg)
		{
			int devNum = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
			auto device = devices[devNum];

			//Don't allow changing device #0 from disk drive
			EnableWindow(GetDlgItem(hwndDlg, IDC_DEVTYPE), (devNum > 0));

			//Hide everything regardless at first.
			int everything[] = { IDC_DEVNONE, IDC_DDFILE, IDC_DDINSERT, IDC_DDEJECT, IDC_PRIMARYDEVICE, IDC_ONLYFOURDRIVES };
			for (int i = 0; i < ARRAYSIZE(everything); i++)
				ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_HIDE);

			if (devNum == 0)
				ShowWindow(GetDlgItem(hwndDlg, IDC_PRIMARYDEVICE), SW_SHOW);

			if (device == NULL)
			{
				ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
				//ShowWindow(GetDlgItem(hwndDlg, IDC_SHRUG), SW_SHOW);
				SetDlgItemText(hwndDlg, IDC_HEADER, GetString(IDS_DEVICES2 + 0)); //"No device"
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, 0, 0);
			}
			else
			{
				switch (device->GetID())
				{
				case 0x0144:
				{
					for (int i = 1; i < 4; i++)
						ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_SHOW);
					SetDlgItemText(hwndDlg, IDC_HEADER, (((DiskDrive*)device)->GetType() == ddHardDisk) ? GetString(IDS_DEVICES2 + 2) : GetString(IDS_DEVICES2 + 1)); //"Diskette drive" "Hard drive"
					SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, (((DiskDrive*)device)->GetType() == ddHardDisk) ? 2 : 1, 0);
					WCHAR key[8] = { 0 };
					_itow(devNum, key, 10);
					auto val = ini.GetValue((((DiskDrive*)device)->GetType() == ddDiskette) ? L"devices/diskDrive" : L"devices/hardDrive", key, L"");
					SetDlgItemText(hwndDlg, IDC_DDFILE, val);
					EnableWindow(GetDlgItem(hwndDlg, IDC_DDINSERT), val[0] == 0);
					EnableWindow(GetDlgItem(hwndDlg, IDC_DDEJECT), val[0] != 0);

					ShowWindow(GetDlgItem(hwndDlg, IDC_ONLYFOURDRIVES), (numDrives > 4) ? SW_SHOW : SW_HIDE);

					break;
				}
				case 0x4C50:
				{
					ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
					//ShowWindow(GetDlgItem(hwndDlg, IDC_SHRUG), SW_SHOW);
					SetDlgItemText(hwndDlg, IDC_HEADER, GetString(IDS_DEVICES2 + 3)); //"Line printer"
					SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, 3, 0);
					break;
				}
				}
			}

			//if (numDrives > 4 || firstDev)
			InvalidateRect(hwndDlg, NULL, true);
		}

		void UpdateList(HWND hwndDlg)
		{
			int selection = 0;
			if (SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCOUNT, 0, 0))
				selection = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
			WCHAR item[64] = { 0 };
			SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_RESETCONTENT, 0, 0);
			numDrives = 0;
			for (int i = 0; i < MAXDEVS; i++)
			{
				int icon = 0;
				if (devices[i] == NULL)
				{
					wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1 + 0)); //"Nothing"
					icon = IML_CROSS;
				}
				else
				{
					switch (devices[i]->GetID())
					{
					case 0x0144:
						if (((DiskDrive*)devices[i])->GetType() == ddDiskette)
						{
							wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1 + 1)); //"Diskette drive"
							icon = IML_DISKDRIVE;
							numDrives++;
						}
						else
						{
							wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1 + 2)); //"Hard drive"
							icon = IML_HARDDRIVE;
							numDrives++;
						}
						break;
					case 0x4C50:
						wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1 + 3)); //"Line printer"
						icon = IML_PRINTER;
						break;
					}
				}
				SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_ADDSTRING, 0, (LPARAM)item);
				SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_SETITEMDATA, i, (LPARAM)icon);
			}

			SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_SETCURSEL, selection, 0);
			UpdatePage(hwndDlg);
		}

		void SwitchDevice(HWND hwndDlg)
		{
			int devNum = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
			int newType = SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_GETCURSEL, 0, 0);

			int oldType = 0;
			if (devices[devNum] != NULL)
			{
				switch (devices[devNum]->GetID())
				{
				case 0x0144:
					oldType = 1;
					if (((DiskDrive*)devices[devNum])->GetType() == ddHardDisk)
						oldType = 2;
					break;
				case 0x4C50:
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
			UpdateList(hwndDlg);
		}

		BOOL CALLBACK WndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_CLOSE:
			{
				DestroyWindow(hwndDlg);
				DeleteObject(hShrugImage);
				hWnd = NULL;
				if (!wasPaused) pauseState = 0;
				return true;
			}
			case WM_INITDIALOG:
			{
				const int deviceIcons[] = { IML_CROSS, IML_DISKDRIVE, IML_HARDDRIVE, IML_PRINTER };
				SendDlgItemMessage(hwndDlg, IDC_DEVNONE, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
				for (int i = 0; i < 4; i++)
				{
					SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_DEVICES1 + i));
					SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETITEMDATA, i, deviceIcons[i]);
				}
				GetIconPos(hwndDlg, IDC_PRIMARYDEVICE, &primaryIcon, -24, 0);
				GetIconPos(hwndDlg, IDC_ONLYFOURDRIVES, &warningIcon, -24, 0);
				GetIconPos(hwndDlg, IDC_SHRUG, &shrugIcon, 0, 0);
				hShrugImage = Images::LoadPNGResource(IDB_SHRUG);
				UpdateList(hwndDlg);
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
					case IDC_DDEJECT: return DrawDarkButton(hwndDlg, nmc);
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
				HDC hdc = BeginPaint(hwndDlg, &ps);
				FillRect(hdc, &ps.rcPaint, hbrBack);
				auto hdcMem = CreateCompatibleDC(hdc);
				if (SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0) == 0)
					ImageList_Draw(Images::hIml, IML_INFO, hdc, primaryIcon.left, primaryIcon.top, ILD_NORMAL);
				if (numDrives > 4)
					ImageList_Draw(Images::hIml, IML_WARNING, hdc, warningIcon.left, warningIcon.top, ILD_NORMAL);
				if (IsWindowVisible(GetDlgItem(hwndDlg, IDC_DEVNONE)))
				{
					auto oldBitmap = SelectObject(hdcMem, hShrugImage);
					BLENDFUNCTION ftn = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
					AlphaBlend(hdc, shrugIcon.left, shrugIcon.top, 64, 64, hdcMem, 0, 0, 64, 64, ftn);
					SelectObject(hdcMem, oldBitmap);
				}
				DeleteDC(hdcMem);
				EndPaint(hwndDlg, &ps);
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
				if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_DEVNONE))
					SetTextColor((HDC)wParam, rgbHeader);
				SetBkColor((HDC)wParam, rgbBack);
				if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDC_DDFILE))
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
					UpdatePage(hwndDlg);
					return true;
				}
				if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_DEVTYPE)
				{
					SwitchDevice(hwndDlg);
					return true;
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_DDINSERT)
				{
					int devID = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
					InsertDisk(devID);
					return true;
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_DDEJECT)
				{
					int devID = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
					EjectDisk(devID);
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK)
				{
					DestroyWindow(hwndDlg);
					hWnd = NULL;
					if (!wasPaused) pauseState = 0;
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
				if (!wasPaused) pauseState = 1;
				hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICES), (HWND)hWndMain, (DLGPROC)WndProc);
				ShowWindow(hWnd, SW_SHOW);
			}
		}
	}
}
