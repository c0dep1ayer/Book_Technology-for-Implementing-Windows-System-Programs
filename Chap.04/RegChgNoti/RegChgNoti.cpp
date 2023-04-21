#include "stdafx.h"
#include "Windows.h"
#include "time.h"
#include "iostream"
using namespace std;

void _tmain()
{
	DWORD  dwFilter = REG_NOTIFY_CHANGE_NAME |
					  REG_NOTIFY_CHANGE_ATTRIBUTES |
					  REG_NOTIFY_CHANGE_LAST_SET |
					  REG_NOTIFY_CHANGE_SECURITY;
	HANDLE hEvent = NULL;
	HKEY   hKey = NULL;

	try
	{
		LONG lErrCode = RegOpenKeyEx(HKEY_CURRENT_USER,
			_T("Software\\00_Test"), 0, KEY_NOTIFY, &hKey);
		if (lErrCode != ERROR_SUCCESS)
			throw HRESULT_FROM_WIN32(lErrCode);

		hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (hEvent == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());

		cout << "Waiting for a change in the specified key..." << endl;
		while (true)
		{
			lErrCode = RegNotifyChangeKeyValue
			(
				hKey,
				TRUE,
				dwFilter,
				hEvent,
				TRUE
			);
			if (lErrCode != ERROR_SUCCESS)
				throw HRESULT_FROM_WIN32(lErrCode);

			DWORD dwWaitRet = WaitForSingleObject(hEvent, INFINITE);
			if (dwWaitRet == WAIT_OBJECT_0)
				cout << "Change has occurred." << endl;
			else
			{
				cout << "Error in WaitForSingleObject." << endl;
				break;
			}
		}
	}
	catch (LONG ex)
	{
		cout << "Error occurred, code = " << ex << endl;
	}
	if (hKey != NULL)
		RegCloseKey(hKey);
	if (hEvent != NULL)
		CloseHandle(hEvent);
}
