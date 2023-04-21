#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define REG_MON_FLTR REG_NOTIFY_CHANGE_NAME | \
					 REG_NOTIFY_CHANGE_ATTRIBUTES | \
					 REG_NOTIFY_CHANGE_LAST_SET | \
					 REG_NOTIFY_CHANGE_SECURITY

DWORD WINAPI RegMonCallback(PVOID pParam)
{
	PHANDLE parObjs = (PHANDLE)pParam;
	HKEY	hKey = (HKEY)parObjs[0];
	HANDLE	hevNoti = parObjs[2], hevExit = parObjs[1];

	cout << "Waiting for a change in the specified key..." << endl;
	while (true)
	{
		LONG lErrCode = RegNotifyChangeKeyValue
		(
			hKey, TRUE, REG_MON_FLTR, hevNoti, TRUE
		);
		if (lErrCode != ERROR_SUCCESS)
		{
			cout << "Error occurred, code = " << lErrCode << endl;
			break;
		}

		DWORD dwWaitRet = WaitForMultipleObjects(2, parObjs + 1, FALSE, INFINITE);
		if (dwWaitRet == WAIT_FAILED || dwWaitRet == WAIT_OBJECT_0)
			break;

		cout << "Change has occurred." << endl;
	}
	SetEvent(hevExit);
	return 0;
}

void _tmain()
{
	HANDLE	 hevNoti = NULL, hevExit = NULL;
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

		hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hevExit == NULL)
			throw (LONG)GetLastError();

		HANDLE arObjs[3] = { (HANDLE)hKey, hevExit , hevNoti};
		QueueUserWorkItem(RegMonCallback, arObjs, WT_EXECUTEINPERSISTENTTHREAD);

		getchar();
		SignalObjectAndWait(hevExit, hevExit, INFINITE, FALSE);
	}
	catch (LONG ex)
	{
		cout << "Error occurred, code = " << ex << endl;
	}
	if (hKey != NULL)
		RegCloseKey(hKey);
	if (hevNoti != NULL)
		CloseHandle(hevNoti);
	if (hevExit != NULL)
		CloseHandle(hevExit);
}
