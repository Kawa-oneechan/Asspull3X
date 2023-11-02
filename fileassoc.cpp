#include "asspull.h"

bool IsAdmin()
{
	BOOL res = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	PSID pAdministratorsGroup = NULL;

	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup))
	{
		dwError = GetLastError();
		goto cleanup;
	}

	if (!CheckTokenMembership(NULL, pAdministratorsGroup, &res))
	{
		dwError = GetLastError();
		goto cleanup;
	}

cleanup:
	if (pAdministratorsGroup)
	{
		FreeSid(pAdministratorsGroup);
		pAdministratorsGroup = NULL;
	}

	if (dwError != ERROR_SUCCESS)
		return false;

	return res != FALSE;
}

void AssociateFiletypes()
{
	WCHAR appPath[2048];

	if (!IsAdmin())
	{
		GetModuleFileName(NULL, appPath, 2048);
		SHELLEXECUTEINFO sei = { sizeof(sei) };
		sei.lpVerb = L"runas";
		sei.lpFile = appPath;
		sei.lpParameters = L"--associate";
		sei.hwnd = NULL;
		sei.nShow = SW_NORMAL;
		if (!ShellExecuteEx(&sei))
		{
			DWORD dwError = GetLastError();
			if (dwError == ERROR_CANCELLED)
				return;
		}
		return;
	}

	DWORD disp = 0;
	HKEY key;

	auto res = RegCreateKeyEx(HKEY_CLASSES_ROOT, L".ap3", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp);
	if (res == ERROR_SUCCESS)
	{
		res = RegSetValueEx(key, L"", 0, REG_SZ, (const UCHAR *)L"clunibus", 9 * 2);
		RegCloseKey(key);
	}
	else
		return;
	res = RegCreateKeyEx(HKEY_CLASSES_ROOT, L".a3z", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp);
	if (res == ERROR_SUCCESS)
	{
		res = RegSetValueEx(key, L"", 0, REG_SZ, (const UCHAR *)L"clunibusz", 10 * 2);
		RegCloseKey(key);
	}
	else
		return;

	GetModuleFileName(NULL, appPath, 2048);
	WCHAR commandPath[2048];
	wsprintf(commandPath, L"\"%s\" \"%%1\"", appPath);

	res = RegCreateKeyEx(HKEY_CLASSES_ROOT, L"clunibus", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp);
	if (res == ERROR_SUCCESS)
	{
		HKEY key2;
		res = RegSetValueEx(key, L"", 0, REG_SZ, (const UCHAR *)L"Clunibus ROM", 13 * 2);
		res = RegCreateKeyEx(key, L"Shell\\Open\\Command", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key2, &disp);
		if (res == ERROR_SUCCESS)
		{
			res = RegSetValueEx(key2, L"", 0, REG_SZ, (const UCHAR *)commandPath, wcslen(commandPath) * 2);
			RegCloseKey(key2);
		}
		else
			return;
		res = RegCreateKeyEx(key, L"DefaultIcon", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key2, &disp);
		if (res == ERROR_SUCCESS)
		{
			wsprintf(commandPath, L"%s,1", appPath);
			res = RegSetValueEx(key2, L"", 0, REG_SZ, (const UCHAR *)commandPath, wcslen(commandPath) * 2);
			RegCloseKey(key2);
		}
		else
			return;

		RegCloseKey(key);
	}
	else
		return;

	wsprintf(commandPath, L"\"%s\" \"%%1\"", appPath);
	res = RegCreateKeyEx(HKEY_CLASSES_ROOT, L"clunibusz", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp);
	if (res == ERROR_SUCCESS)
	{
		HKEY key2;
		res = RegSetValueEx(key, L"", 0, REG_SZ, (const UCHAR *)L"Clunibus zipped ROM", 13 * 2);
		res = RegCreateKeyEx(key, L"Shell\\Open\\Command", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key2, &disp);
		if (res == ERROR_SUCCESS)
		{
			res = RegSetValueEx(key2, L"", 0, REG_SZ, (const UCHAR *)commandPath, wcslen(commandPath) * 2);
			RegCloseKey(key2);
		}
		else
			return;
		res = RegCreateKeyEx(key, L"DefaultIcon", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key2, &disp);
		if (res == ERROR_SUCCESS)
		{
			wsprintf(commandPath, L"%s,2", appPath);
			res = RegSetValueEx(key2, L"", 0, REG_SZ, (const UCHAR *)commandPath, wcslen(commandPath) * 2);
			RegCloseKey(key2);
		}
		else
			return;

		RegCloseKey(key);
	}
	else
		return;

	MessageBox(NULL, L"Associations set.", L"Clunibus", MB_OK);
}
