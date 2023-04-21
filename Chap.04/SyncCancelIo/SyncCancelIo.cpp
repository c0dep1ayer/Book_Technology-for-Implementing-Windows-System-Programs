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

		if (pFNI->NextEntryOffset == 0)
			break;
		pIter += pFNI->NextEntryOffset;
	}
}

DWORD WINAPI ThreadProc(PVOID pParam)
{
	DWORD  dwErrCode = ERROR_SUCCESS;
	HANDLE hDir = INVALID_HANDLE_VALUE;
	try
	{
		hDir = CreateFile
		(
			_T("C:\\Temp"), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL
		);
		if (hDir == ERROR_SUCCESS)
			throw GetLastError();

		while (true)
		{
			DWORD dwNotiBytes = 0;
			BYTE arBuff[65536];
			BOOL bIsOK = ReadDirectoryChangesW
			(
				hDir, arBuff, sizeof(arBuff), FALSE,
				DIR_NOTI_FILTER, &dwNotiBytes, NULL, NULL
			);
			if (!bIsOK)
				throw GetLastError();

			PrintDirModEntries(arBuff);
		}
	}
	catch (DWORD ex)
	{
		dwErrCode = ex;
	}
	if (hDir != INVALID_HANDLE_VALUE)
		CloseHandle(hDir);

	return dwErrCode;
}

void _tmain()
{
	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, &dwThrID);

	getchar();
	if (!CancelSynchronousIo(hThread))
		cout << GetLastError() << endl;

	WaitForSingleObject(hThread, INFINITE);
	DWORD hr = 0;
	GetExitCodeThread(hThread, &hr);
	CloseHandle(hThread);

	if (hr != ERROR_SUCCESS)
	{
		if (hr == ERROR_OPERATION_ABORTED)
			cout << "File copy aboarted by user..." << endl;
		else
			cout << "Error occurred in file copying, code=" << hr << endl;
	}
	else
		cout << endl << "File copy successfully completed..." << endl;
}
