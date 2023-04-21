#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define REG_MON_FLTR REG_NOTIFY_CHANGE_NAME | \
					 REG_NOTIFY_CHANGE_ATTRIBUTES | \
					 REG_NOTIFY_CHANGE_LAST_SET | \
					 REG_NOTIFY_CHANGE_SECURITY

VOID WINAPI RegMonCallback(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx)
{
	PHANDLE parObjs = (PHANDLE)pCtx;
	HKEY	hKey = (HKEY)parObjs[0];
	HANDLE	hevNoti = parObjs[2], hevExit = parObjs[1];

	cout << "Waiting for a change in the specified key..." << endl;
	while (true)
	{
		LONG lErrCode = RegNotifyChangeKeyValue
		(
			hKey, TRUE,
			REG_MON_FLTR, // | REG_NOTIFY_THREAD_AGNOSTIC,
			hevNoti, TRUE
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
	SetEventWhenCallbackReturns(pInst, hevExit);
}

void _tmain()
{
	TP_CALLBACK_ENVIRON ce;
	InitializeThreadpoolEnvironment(&ce);

	HANDLE	 hevNoti = NULL, hevExit = NULL;
	HKEY	 hKey = NULL;
	PTP_POOL pPool = NULL;
	try
	{
		pPool = CreateThreadpool(NULL);
		if (!pPool)
			throw (LONG)GetLastError();

		SetThreadpoolThreadMaximum(pPool, 1);
		if (!SetThreadpoolThreadMinimum(pPool, 1))
			throw (LONG)GetLastError();
		SetThreadpoolCallbackPool(&ce, pPool);

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
		if (!TrySubmitThreadpoolCallback(RegMonCallback, arObjs, &ce))
			throw (LONG)GetLastError();

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
	if (pPool != NULL)
		CloseThreadpool(pPool);

	DestroyThreadpoolEnvironment(&ce);
}
