#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;

//FILE_NOTIFY_CHANGE_FILE_NAME    0x00000001   
//FILE_NOTIFY_CHANGE_DIR_NAME     0x00000002   
//FILE_NOTIFY_CHANGE_ATTRIBUTES   0x00000004   
//FILE_NOTIFY_CHANGE_SIZE         0x00000008   
//FILE_NOTIFY_CHANGE_LAST_WRITE   0x00000010   
//FILE_NOTIFY_CHANGE_LAST_ACCESS  0x00000020   
//FILE_NOTIFY_CHANGE_CREATION     0x00000040  
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

#include "NTSecApi.h"

#define BUFF_SIZE	65536
struct MON_ENV
{
	HANDLE	_monDir;
	DWORD	_filter;
	BOOL	_monSub;
};
typedef MON_ENV* PMON_ENV;

DWORD WINAPI DirMonProc(PVOID pParam)
{
	PMON_ENV pme = (PMON_ENV)pParam;
	OVERLAPPED ov = { 0, };
	BYTE arBuff[65536];

	while (true)
	{
		BOOL bIsOK = ReadDirectoryChangesW
		(
			pme->_monDir, arBuff, BUFF_SIZE, pme->_monSub,
			pme->_filter, NULL, &ov, NULL
		);
		if (!bIsOK)
		{
			cout << " ~~~RDCW error: " << GetLastError() << endl;
			break;
		}

		DWORD dwWaitCode = WaitForSingleObject(pme->_monDir, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			DWORD dwErrCode = GetLastError();
			cout << " ~~~WaitForSingleObject FAILED : " << dwErrCode << endl;
			break;
		}

		if (ov.InternalHigh == 0 || ov.Internal != 0)
		{
			DWORD dwErrCode = LsaNtStatusToWinError(ov.Internal);
			if (dwErrCode == ERROR_OPERATION_ABORTED)
			{
				cout << "==> DirMon canceled by user." << endl;
				continue;
			}
			if (dwErrCode == ERROR_NOTIFY_CLEANUP)
				cout << "==> DirMon stoped by user." << endl;
			else
				cout << " ~~~Internal error: " << dwErrCode << endl;
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

	MON_ENV me;
	me._monDir = hMonDir;
	me._filter = DIR_NOTI_FILTER;
	me._monSub = FALSE;

	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, DirMonProc, &me, 0, &dwThrID);

	char szIn[256];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		bool modified = false;
		long filter = strtol(szIn, NULL, 16);
		if (filter > 0 && filter < 0x80)
		{
			if (me._filter != (DWORD)filter)
			{
				me._filter = (DWORD)filter;
				modified = true;
			}
		}
		else if (stricmp(szIn, "true") == 0)
		{
			if (!me._monSub)
			{
				me._monSub = true;
				modified = true;
			}
		}
		else if (stricmp(szIn, "false") == 0)
		{
			if (me._monSub)
			{
				me._monSub = false;
				modified = true;
			}
		}

		if (modified)
			CancelIoEx(me._monDir, NULL);
	}
	CloseHandle(me._monDir);
	WaitForSingleObject(hThread, INFINITE);
}
