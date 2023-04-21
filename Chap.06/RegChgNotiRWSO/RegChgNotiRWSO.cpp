#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define REG_MON_FLTR REG_NOTIFY_CHANGE_NAME | \
					 REG_NOTIFY_CHANGE_ATTRIBUTES | \
					 REG_NOTIFY_CHANGE_LAST_SET | \
					 REG_NOTIFY_CHANGE_SECURITY

VOID WINAPI RegMonCallback(PVOID pParam, BOOLEAN bTW)
{
	PHANDLE parObjs = (PHANDLE)pParam;
	HKEY	hKey	= (HKEY)parObjs[0];
	HANDLE	hevNoti = parObjs[1];

	cout << "Change has occurred." << endl;

	LONG lErrCode = RegNotifyChangeKeyValue
	(
		hKey, TRUE, 
		REG_MON_FLTR | REG_NOTIFY_THREAD_AGNOSTIC,
		hevNoti, TRUE
	);
	if (lErrCode != ERROR_SUCCESS)
		cout << "Error occurred, code = " << lErrCode << endl;
}

void _tmain()
{
	HANDLE	 hevNoti = NULL, hRWSO = NULL;
	HKEY	 hKey = NULL;
	try
	{
		LONG lErrCode = RegOpenKeyEx(HKEY_CURRENT_USER,
			_T("Software\\00_Test"), 0, KEY_NOTIFY, &hKey);
		if (lErrCode != ERROR_SUCCESS)
			throw lErrCode;

		hevNoti = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hevNoti == NULL)
			throw (LONG)GetLastError();

		HANDLE arMons[2] = { (HANDLE)hKey, hevNoti };
		if (!RegisterWaitForSingleObject(&hRWSO, hevNoti,
			RegMonCallback, arMons, INFINITE,  WT_EXECUTEDEFAULT))
			throw (LONG)GetLastError();

		lErrCode = RegNotifyChangeKeyValue
		(
			hKey, TRUE, REG_MON_FLTR, hevNoti, TRUE
		);
		if (lErrCode != ERROR_SUCCESS)
			throw (LONG)GetLastError();
		cout << "Waiting for a change in the specified key..." << endl;

		getchar();
	}
	catch (LONG ex)
	{
		cout << "Error occurred, code = " << ex << endl;
	}
	if (hRWSO != NULL)
		UnregisterWait(hRWSO);
	if (hevNoti != NULL)
		CloseHandle(hevNoti);
	if (hKey != NULL)
		RegCloseKey(hKey);
}
