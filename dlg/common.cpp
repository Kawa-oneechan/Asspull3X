#include "..\ui.h"

HWND hWndAbout = NULL, hWndMemViewer = NULL, hWndOptions = NULL, hWndDevices = NULL;
HFONT headerFont = NULL, monoFont = NULL;

bool ShowFileDlg(bool toSave, char* target, size_t max, const char* filter);

void DrawWin7Thing(HWND hwndDlg)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwndDlg, &ps);
	RECT rect = { 0 };
	rect.bottom = 7 + 14 + 7; //margin, button, padding
	MapDialogRect(hwndDlg, &rect);
	auto h = rect.bottom;
	GetClientRect(hwndDlg, &rect);
	rect.bottom -= h;
	FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
	EndPaint(hwndDlg, &ps);
	return;
}
