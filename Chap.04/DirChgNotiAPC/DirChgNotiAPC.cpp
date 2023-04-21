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


#define BUFF_SIZE	65536
struct DIR_BUFF : OVERLAPPED
{
	BYTE _buff[BUFF_SIZE];
	DIR_BUFF()
	{
		memset(this, 0, sizeof(*this));
	}
};
typedef DIR_BUFF* PDIR_BUFF;

VOID CALLBACK DirChangedCallback(DWORD dwErrCode, DWORD dwTranBytes, LPOVERLAPPED pOL)
{
	if (dwErrCode != NO_ERROR || dwTranBytes == 0)
		return;

	PDIR_BUFF pDB = (PDIR_BUFF)pOL;
	PrintDirModEntries(pDB->_buff);
}


HANDLE g_hevExit;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	SetEvent(g_hevExit);
	return TRUE;
}

void _tmain()
{
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
	g_hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE hDir = INVALID_HANDLE_VALUE;
	try
	{
		hDir = CreateFile
		(
			_T("C:\\Temp"), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL
		);
		if (hDir == ERROR_SUCCESS)
			throw HRESULT_FROM_WIN32(GetLastError());

		DIR_BUFF db;
		memset(&db, 0, sizeof(db));
		while (true)
		{
			BOOL bIsOK = ReadDirectoryChangesW
			(
				hDir, db._buff, BUFF_SIZE, FALSE, DIR_NOTI_FILTER, NULL,
				&db, DirChangedCallback
			);
			if (!bIsOK)
				throw HRESULT_FROM_WIN32(GetLastError());

			if (WaitForSingleObjectEx(g_hevExit, INFINITE, TRUE) != WAIT_IO_COMPLETION)
				break;
		}
	}
	catch (HRESULT ex)
	{
		printf("Error occurred, code = 0x%08X\n", ex);
	}
	if (hDir != INVALID_HANDLE_VALUE)
		CloseHandle(hDir);
	CloseHandle(g_hevExit);
}