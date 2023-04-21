#include "stdafx.h"
#include "windows.h"
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

void _tmain()
{
	DWORD  dwErrCode = ERROR_SUCCESS;
	HANDLE hMonDir = INVALID_HANDLE_VALUE;

	try
	{
		hMonDir = CreateFile
		(
			_T("C:\\Temp"), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL
		);
		if (hMonDir == INVALID_HANDLE_VALUE)
			throw GetLastError();

		OVERLAPPED ov = { 0, };
		DWORD dwNotiBytes = 0;
		BYTE arBuff[65536];
		BOOL bIsOK = ReadDirectoryChangesW
		(
			hMonDir, arBuff, sizeof(arBuff), FALSE,
			DIR_NOTI_FILTER, NULL, &ov, NULL
		);
		if (!bIsOK)
			throw GetLastError();

		DWORD dwMonBytes = 0;
		bool bIoCompeleted = false;
		for (int i = 0; i < 10; i++)
		{
			bIsOK = GetOverlappedResult(hMonDir, &ov, &dwMonBytes, FALSE);
			if (!bIsOK)
			{
				dwErrCode = GetLastError();
				if (dwErrCode != ERROR_IO_INCOMPLETE)
					throw dwErrCode;
				Sleep(500);
			}
			else
			{
				bIoCompeleted = true;
				break;
			}
		}
		if (!bIoCompeleted)
		{
			bIsOK = CancelIo(hMonDir);
			if (bIsOK == TRUE || GetLastError() != ERROR_NOT_FOUND)
			{
				bIsOK = GetOverlappedResult(hMonDir, &ov, &dwMonBytes, TRUE);
				if (!bIsOK)
					throw GetLastError();
			}
		}

		PrintDirModEntries(arBuff);
	}
	catch (DWORD ex)
	{
		dwErrCode = ex;
	}
	if (hMonDir != INVALID_HANDLE_VALUE)
		CloseHandle(hMonDir);

	if (dwErrCode != ERROR_SUCCESS)
	{
		if (dwErrCode == ERROR_OPERATION_ABORTED)
			cout << "DirMon aboarted by user..." << endl;
		else
			cout << "Error occurred in DirMon, code=" << dwErrCode << endl;
	}
	else
		cout << endl << "DirMon successfully completed..." << endl;
}
