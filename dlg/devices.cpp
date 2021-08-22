#include "..\ui.h"

extern WCHAR* GetString(int);

extern HIMAGELIST hIml;

void UpdateDevicePage(HWND hwndDlg)
{
	int devNum = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
	auto device = devices[devNum];

	//Don't allow changing device #0 from disk drive
	EnableWindow(GetDlgItem(hwndDlg, IDC_DEVTYPE), (devNum > 0));

	//Hide everything regardless at first.
	int everything[] = { IDC_DEVNONE, IDC_SHRUG, IDC_DDFILE, IDC_DDINSERT, IDC_DDEJECT, IDC_PRIMARYDEVICE };
	for (int i = 0; i < ARRAYSIZE(everything); i++)
		ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_HIDE);

	if (devNum == 0)
		ShowWindow(GetDlgItem(hwndDlg, IDC_PRIMARYDEVICE), SW_SHOW);

	if (device == NULL)
	{
		ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
		ShowWindow(GetDlgItem(hwndDlg, IDC_SHRUG), SW_SHOW);
		SetDlgItemText(hwndDlg, IDC_HEADER, GetString(IDS_DEVICES2+0)); //"No device"
		SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, 0, 0);
	}
	else
	{
		switch (device->GetID())
		{
			case 0x0144:
			{
				for (int i = 2; i < 5; i++)
					ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_SHOW);
				SetDlgItemText(hwndDlg, IDC_HEADER, (((DiskDrive*)device)->GetType() == ddHardDisk) ? GetString(IDS_DEVICES2+2) : GetString(IDS_DEVICES2+1) ); //"Diskette drive" "Hard drive"
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, (((DiskDrive*)device)->GetType() == ddHardDisk) ? 2 : 1, 0);
				WCHAR key[8] = { 0 };
				_itow_s(devNum, key, 8, 10);
				auto val = ini.GetValue(L"devices/diskDrive", key, L"");
				SetDlgItemText(hwndDlg, IDC_DDFILE, val);
				EnableWindow(GetDlgItem(hwndDlg, IDC_DDINSERT), val[0] == 0);
				EnableWindow(GetDlgItem(hwndDlg, IDC_DDEJECT), val[0] != 0);
				break;
			}
			case 0x4C50:
			{
				ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
				ShowWindow(GetDlgItem(hwndDlg, IDC_SHRUG), SW_SHOW);
				SetDlgItemText(hwndDlg, IDC_HEADER, GetString(IDS_DEVICES2+3)); //"Line printer"
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, 3, 0);
				break;
			}
		}
	}
}

void UpdateDeviceList(HWND hwndDlg)
{
	int selection = 0;
	if (SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCOUNT, 0, 0))
		selection = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
	WCHAR item[64] = { 0 };
	SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_RESETCONTENT, 0, 0);
	for (int i = 0; i < MAXDEVS; i++)
	{
		int icon = 0;
		if (devices[i] == NULL)
		{
			wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1+0)); //"Nothing"
			icon = 0;
		}
		else
		{
			switch (devices[i]->GetID())
			{
			case 0x0144:
				if (((DiskDrive*)devices[i])->GetType() == ddDiskette)
				{
					wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1+1)); //"Diskette drive"
					icon = 1;
				}
				else
				{
					wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1+2)); //"Hard drive"
					icon = 2;
				}
				break;
			case 0x4C50:
				wsprintf(item, L"%d. %s", i + 1, GetString(IDS_DEVICES1+3)); //"Line printer"
				icon = 3;
				break;
			}
		}
		SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_ADDSTRING, 0, (LPARAM)item);
		SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_SETITEMDATA, i, (LPARAM)icon);
	}

	SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_SETCURSEL, selection, 0);
	UpdateDevicePage(hwndDlg);
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
	_itow_s(devNum, key, 8, 10);

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
	ini.SaveFile(L"settings.ini", false);
	UpdateDeviceList(hwndDlg);
}

BOOL CALLBACK DevicesWndProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
		{
			DestroyWindow(hwndDlg);
			hWndDevices = NULL;
			if (!wasPaused) pauseState = 0;
			return true;
		}
		case WM_INITDIALOG:
		{
			SendDlgItemMessage(hwndDlg, IDC_DEVNONE, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
			for (int i = 0; i < 4; i++)
			{
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_ADDSTRING, 0, (LPARAM)GetString(IDS_DEVICES1 + i));
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETITEMDATA, i, i);
			}
			UpdateDeviceList(hwndDlg);
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
					switch(idFrom)
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
		{
			DrawWindowBk(hwndDlg, false);
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
			int x = LOWORD(GetDialogBaseUnits()) / 4;

			WCHAR text[256];

			if (dis->CtlID == IDC_DEVTYPE)
				SendMessage(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)text);
			else if (dis->CtlID == IDC_DEVLIST)
				SendMessage(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);

			ExtTextOut(dis->hDC, 24, y, ETO_CLIPPED | ETO_OPAQUE, &dis->rcItem, text, (UINT)wcslen(text), NULL);
			ImageList_Draw(hIml, dis->itemData, dis->hDC, dis->rcItem.left + 1, dis->rcItem.top + 1, ILD_NORMAL);
			return true;
		}
		case WM_COMMAND:
		{
			if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == IDC_DEVLIST)
			{
				UpdateDevicePage(hwndDlg);
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
				hWndDevices = NULL;
				if (!wasPaused) pauseState = 0;
				return true;
			}
		}
	}
	return false;
}

void ShowDevices()
{
	if (!IsWindow(hWndDevices))
	{
		wasPaused = pauseState > 0;
		if (!wasPaused) pauseState = 1;
		hWndDevices = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICES), (HWND)hWnd, (DLGPROC)DevicesWndProc);
		ShowWindow(hWndDevices, SW_SHOW);
	}
}
