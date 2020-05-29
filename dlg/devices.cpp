#include "..\ui.h"


void UpdateDevicePage(HWND hwndDlg)
{
	int devNum = SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_GETCURSEL, 0, 0);
	auto device = devices[devNum];

	//Don't allow changing device #0 from disk drive
	EnableWindow(GetDlgItem(hwndDlg, IDC_DEVTYPE), (devNum > 0));

	//Hide everything regardless at first.
	int everything[] = { IDC_DEVNONE, IDC_DDFILE, IDC_DDINSERT, IDC_DDEJECT, IDC_PRIMARYDEVICE };
	for (int i = 0; i < 5; i++)
		ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_HIDE);

	if (devNum == 0)
		ShowWindow(GetDlgItem(hwndDlg, IDC_PRIMARYDEVICE), SW_SHOW);

	if (device == NULL)
	{
		ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
		SetDlgItemText(hwndDlg, IDC_HEADER, "No device");
		SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hwndDlg, IDC_DECO, STM_SETICON, (WPARAM)LoadImage(hInstance, MAKEINTRESOURCE(IDI_BLANK), IMAGE_ICON, 0, 0, 0), 0);
	}
	else
	{
		switch (device->GetID())
		{
			case 0x0144:
			{
				for (int i = 1; i < 4; i++)
					ShowWindow(GetDlgItem(hwndDlg, everything[i]), SW_SHOW);
				SetDlgItemText(hwndDlg, IDC_HEADER, (((DiskDrive*)device)->GetType() == ddHardDisk) ? "Hard drive" : "Diskette drive");
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, (((DiskDrive*)device)->GetType() == ddHardDisk) ? 2 : 1, 0);
				char key[8] = { 0 };
				SDL_itoa(devNum, key, 10);
				auto val = ini.GetValue("devices/diskDrive", key, "");
				SetDlgItemText(hwndDlg, IDC_DDFILE, val);
				EnableWindow(GetDlgItem(hwndDlg, IDC_DDINSERT), val[0] == 0);
				EnableWindow(GetDlgItem(hwndDlg, IDC_DDEJECT), val[0] != 0);
				SendDlgItemMessage(hwndDlg, IDC_DECO, STM_SETICON, (WPARAM)LoadImage(hInstance, MAKEINTRESOURCE((((DiskDrive*)device)->GetType() == ddHardDisk) ? IDI_HARDDRIVE :IDI_DISKDRIVE), IMAGE_ICON, 0, 0, 0), 0);
				break;
			}
			case 0x4C50:
			{
				ShowWindow(GetDlgItem(hwndDlg, IDC_DEVNONE), SW_SHOW);
				SetDlgItemText(hwndDlg, IDC_HEADER, "Line printer");
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_SETCURSEL, 3, 0);
				SendDlgItemMessage(hwndDlg, IDC_DECO, STM_SETICON, (WPARAM)LoadImage(hInstance, MAKEINTRESOURCE(IDI_PRINTER), IMAGE_ICON, 0, 0, 0), 0);
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
	char item[64] = { 0 };
	SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_RESETCONTENT, 0, 0);
	for (int i = 0; i < MAXDEVS; i++)
	{
		if (devices[i] == NULL)
		{
			sprintf_s(item, 64, "%d. Nothing", i + 1);
		}
		else
		{
			switch (devices[i]->GetID())
			{
			case 0x0144:
				if (((DiskDrive*)devices[i])->GetType() == ddDiskette)
					sprintf_s(item, 64, "%d. Diskette drive", i + 1);
				else
					sprintf_s(item, 64, "%d. Hard drive", i + 1);
				break;
			case 0x4C50:
				sprintf_s(item, 64, "%d. Line printer", i + 1);
				break;
			}
		}
		SendDlgItemMessage(hwndDlg, IDC_DEVLIST, LB_ADDSTRING, 0, (LPARAM)item);
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
				oldType = 2;
				break;
		}
	}

	if (newType == oldType)
		return;

	char key[8] = { 0 };
	SDL_itoa(devNum, key, 10);

	if (devices[devNum] != NULL) delete devices[devNum];
	switch (newType)
	{
		case 0:
			devices[devNum] = NULL;
			//ini.SetValue("devices", key, "");
			ini.Delete("devices", key, true);
			break;
		case 1:
			devices[devNum] = (Device*)(new DiskDrive(0));
			ini.SetValue("devices", key, "diskDrive");
			break;
		case 2:
			devices[devNum] = (Device*)(new DiskDrive(1));
			ini.SetValue("devices", key, "hardDrive");
			break;
		case 3:
			devices[devNum] = (Device*)(new LinePrinter());
			ini.SetValue("devices", key, "linePrinter");
			break;
	}
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
			SendDlgItemMessage(hwndDlg, IDC_HEADER, WM_SETFONT, (WPARAM)headerFont, (LPARAM)true);
			LPCSTR devices[] = { "Nothing", "Diskette drive", "Hard drive", "Line printer" };
			for (int i = 0; i < 4; i++)
				SendDlgItemMessage(hwndDlg, IDC_DEVTYPE, CB_ADDSTRING, 0, (LPARAM)devices[i]);
			UpdateDeviceList(hwndDlg);
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
				EjectDisk();
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
