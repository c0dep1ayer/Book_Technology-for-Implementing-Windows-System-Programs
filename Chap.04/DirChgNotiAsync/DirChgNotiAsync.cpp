#include "stdafx.h"
#include "Windows.h"
#include "Iphlpapi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Iphlpapi.lib")

#define DIR_NOTI_FILTER	FILE_NOTIFY_CHANGE_FILE_NAME |	\
						FILE_NOTIFY_CHANGE_DIR_NAME |	\
						FILE_NOTIFY_CHANGE_SIZE |		\
						FILE_NOTIFY_CHANGE_LAST_WRITE

#define REG_NOTI_FILTER REG_NOTIFY_CHANGE_NAME |		\
						REG_NOTIFY_CHANGE_ATTRIBUTES |	\
						REG_NOTIFY_CHANGE_LAST_SET |	\
						REG_NOTIFY_CHANGE_SECURITY

HANDLE g_hevExit;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	SetEvent(g_hevExit);
	return TRUE;
}


void _tmain()
{
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		return;
	}

	HANDLE hDir = INVALID_HANDLE_VALUE;
	HKEY   hKey = NULL;

	HANDLE hevReg = NULL, hevIps = NULL;
	try
	{
		BYTE arBuff[65536];
		OVERLAPPED ovi = { 0, }, ovd = { 0, };

		g_hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);

		hDir = CreateFile
		(
			_T("C:\\Temp"), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL,OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL
		);
		if (hDir == INVALID_HANDLE_VALUE)
			throw HRESULT_FROM_WIN32(GetLastError());

		hevReg = CreateEvent(NULL, FALSE, FALSE, NULL);
		LONG lErrCode = RegOpenKeyEx
		(
			HKEY_CURRENT_USER, _T("Software\\00_Test"), 0, KEY_NOTIFY, &hKey
		);
		if (lErrCode != ERROR_SUCCESS)
			throw HRESULT_FROM_WIN32(lErrCode);

		hevIps = CreateEvent(NULL, FALSE, FALSE, NULL);
		ovi.hEvent = hevIps;

		int nOptIdx = -1;
		HANDLE arWaits[4] = { g_hevExit, hDir, hevReg, hevIps };
		while (true)
		{
			if (nOptIdx >= 0)
			{
				DWORD dwWait = WaitForMultipleObjects(4, arWaits, FALSE, INFINITE);
				if (dwWait == WAIT_OBJECT_0)
					break;

				nOptIdx = (int)(dwWait - WAIT_OBJECT_0);
				switch (nOptIdx)
				{
					case 1:
						printf(" ...Directory entiries changed.\n");
					break;
					case 2:
						printf(" ...Registry entiries changed.\n");
					break;
					case 3:
						printf(" ...IP addresses.\n");
					break;
				}
			}

			if (nOptIdx < 0 || nOptIdx == 1)
			{
				BOOL bIsOK = ReadDirectoryChangesW
				(
					hDir, arBuff, sizeof(arBuff), FALSE,
					DIR_NOTI_FILTER, NULL, &ovd, NULL
				);
				if (!bIsOK)
					throw HRESULT_FROM_WIN32(GetLastError());
			}
			if (nOptIdx < 0 || nOptIdx == 2)
			{
				lErrCode = RegNotifyChangeKeyValue
				(
					hKey, TRUE, REG_NOTI_FILTER, hevReg, TRUE
				);
				if (lErrCode != ERROR_SUCCESS)
					throw HRESULT_FROM_WIN32(lErrCode);
			}
			if (nOptIdx < 0 || nOptIdx == 3)
			{
				HANDLE hevIPChg = NULL;
				if (NotifyAddrChange(&hevIPChg, &ovi) != NO_ERROR)
				{
					DWORD dwErrCode = GetLastError();
					if (dwErrCode != ERROR_IO_PENDING)
						throw HRESULT_FROM_WIN32(dwErrCode);
				}
			}
			if (nOptIdx < 0)
				nOptIdx = 0;
		}
	}
	catch (HRESULT ex)
	{
		printf("Error occurred, code = 0x%08X\n", ex);
	}
	if (hDir != INVALID_HANDLE_VALUE)
		CloseHandle(hDir);
	if (hKey != NULL)
		RegCloseKey(hKey);

	if (hevReg != NULL)		CloseHandle(hevReg);
	if (hevIps != NULL)		CloseHandle(hevIps);
	if (g_hevExit != NULL)	CloseHandle(g_hevExit);
}
