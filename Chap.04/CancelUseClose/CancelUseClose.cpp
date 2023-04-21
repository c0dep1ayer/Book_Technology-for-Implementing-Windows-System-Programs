#include "stdafx.h"
#include "windows.h"
#include "NTSecApi.h"
#include "iostream"
using namespace std;

PCTSTR c_pszActions[] =
{
	_T("ADDED"),
	_T("REMOVED"),
	_T("MODIFIED"),
	_T("RENAMED_OLD_NAME"),
	_T("RENAMED_NEW_NAME")
};
#define DIR_NOTI_FILTER	FILE_NOTIFY_CHANGE_FILE_NAME |	\
						FILE_NOTIFY_CHANGE_DIR_NAME |	\
						FILE_NOTIFY_CHANGE_SIZE |		\
						FILE_NOTIFY_CHANGE_LAST_WRITE

void PrintDirModEntries(PBYTE pIter)
{
	while (true)
	{
		PFILE_NOTIFY_INFORMATION pFNI = (PFILE_NOTIFY_INFORMATION)pIter;
		TCHAR szFileName[MAX_PATH];
		memcpy(szFileName, pFNI->FileName, pFNI->FileNameLength * sizeof(TCHAR));
		szFileName[pFNI->FileNameLength / sizeof(TCHAR)] = 0;
		_tprintf(_T("File %s %s\n"), szFileName, c_pszActions[pFNI->Action - 1]);
		pIter += pFNI->NextEntryOffset;

		if (pFNI->NextEntryOffset == 0)
			break;
	}
}

#define BUFF_SIZE	65536

DWORD WINAPI DirMonProc(PVOID pParam)
{
	HANDLE hDir = (HANDLE)pParam;
	OVERLAPPED ov = { 0, };
	BYTE arBuff[65536];

	while (true)
	{
		BOOL bIsOK = ReadDirectoryChangesW
		(
			hDir, arBuff, BUFF_SIZE, FALSE,
			DIR_NOTI_FILTER, NULL, &ov, NULL
		);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
			{
				cout << " ~~~ ReadDirectoryChangesW failed with code " << dwErrCode << endl;
				break;
			}
		}

		DWORD dwWaitCode = WaitForSingleObject(hDir, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			DWORD dwErrCode = GetLastError();
			cout << " ~~~WaitForSingleObject FAILED : " << dwErrCode << endl;
			break;
		}

		if (ov.InternalHigh == 0 || ov.Internal != 0)
		{
			DWORD dwErrCode = LsaNtStatusToWinError(ov.Internal);
			if (dwErrCode == ERROR_NOTIFY_CLEANUP)
				cout << " <=== Directory Monitoring stopping...." << endl;
			else
				cout << " ~~~ GetOverlappedResult failed with code " << dwErrCode << endl;
			break;
		}

		PrintDirModEntries(arBuff);
	}

	return 0;
}


void _tmain()
{
	HANDLE hMonDir = CreateFile
	(
		_T("C:\\Temp"), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL
	);
	if (hMonDir == INVALID_HANDLE_VALUE)
	{
		cout << " --- CreatFile Error, code=" << GetLastError() << endl;
		return;
	}

	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, DirMonProc, hMonDir, 0, &dwThrID);

	getchar();
	CloseHandle(hMonDir);
	WaitForSingleObject(hThread, INFINITE);
}
