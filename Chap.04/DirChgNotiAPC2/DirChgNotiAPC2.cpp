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
	HANDLE	_dir;
	BYTE	_buff[BUFF_SIZE];

	DIR_BUFF(HANDLE hDir)
	{
		memset(this, 0, sizeof(*this));
		_dir = hDir;
	}
};
typedef DIR_BUFF* PDIR_BUFF;

VOID CALLBACK DirChangedProc(DWORD dwErrCode, DWORD dwTranBytes, LPOVERLAPPED pOL)
{
	PDIR_BUFF pdb = (PDIR_BUFF)pOL;
	if (dwErrCode != NO_ERROR || dwTranBytes == 0)
	{
		_tprintf(_T("Dir %d has Internal error : %d\n"), pdb->_dir, dwErrCode);
		return;
	}

	PrintDirModEntries(pdb->_buff);

	BOOL bIsOK = ReadDirectoryChangesW
	(
		pdb->_dir, pdb->_buff, BUFF_SIZE, FALSE,
		DIR_NOTI_FILTER, NULL, pOL, DirChangedProc
	);
	if (!bIsOK)
		_tprintf(_T("Dir %d RDCW error : %d\n"), pdb->_dir, GetLastError());
}

VOID CALLBACK MonStartProc(ULONG_PTR dwParam)
{
	PDIR_BUFF pdb = (PDIR_BUFF)dwParam;
	BOOL bIsOK = ReadDirectoryChangesW
	(
		pdb->_dir, pdb->_buff, BUFF_SIZE, FALSE,
		DIR_NOTI_FILTER, NULL, pdb, DirChangedProc
	);
	if (!bIsOK)
		_tprintf(_T("Dir %d RDCW error : %d\n"), pdb->_dir, GetLastError());
}

HANDLE g_hevExit;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	SetEvent(g_hevExit);
	return TRUE;
}

#define MAX_COPY_CNT  10
void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		cout << "Uasge : MultiCopyIOCP MonDir1 MonDir2 MonDir3 ..." << endl;
		return;
	}

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
	g_hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);

	PDIR_BUFF arBuff[MAX_COPY_CNT];
	memset(arBuff, 0, sizeof(PDIR_BUFF) * MAX_COPY_CNT);

	int nMonCnt = 0;
	for (int i = 1; i < argc; i++)
	{
		HANDLE hDir = CreateFile
		(
			argv[i], GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL
		);
		if (hDir == ERROR_SUCCESS)
		{
			cout << "CreateFile failed, code=" << GetLastError() << endl;
			continue;
		};

		PDIR_BUFF pdb = new DIR_BUFF(hDir);
		arBuff[i - 1] = pdb;
		nMonCnt++;
	}

	for (int i = 0; i < nMonCnt; i++)
	{
		PDIR_BUFF pdb = arBuff[i];
		QueueUserAPC(MonStartProc, GetCurrentThread(), (ULONG_PTR)pdb);
	}

	while (true)
	{
		DWORD dwWaitCode = WaitForSingleObjectEx(g_hevExit, INFINITE, TRUE);
		if (dwWaitCode == WAIT_OBJECT_0)
			break;

		if (dwWaitCode != WAIT_IO_COMPLETION)
		{
			cout << "WaitForSingleObjectEx failed, code=" << GetLastError() << endl;
			break;
		}
	}

	for (int i = 0; i < nMonCnt; i++)
	{
		PDIR_BUFF pdb = arBuff[i];
		CloseHandle(pdb->_dir);
		delete pdb;
	}
	CloseHandle(g_hevExit);
}
