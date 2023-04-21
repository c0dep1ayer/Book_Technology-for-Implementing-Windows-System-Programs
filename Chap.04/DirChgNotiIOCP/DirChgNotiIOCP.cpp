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


DWORD WINAPI DirMonProc(PVOID pParam)
{
	HANDLE hIocp = (HANDLE)pParam;

	while (true)
	{
		LPOVERLAPPED pov = NULL;
		ULONG_PTR	 ulKey;
		DWORD		 dwTrBytes = 0;
		BOOL bIsOK = GetQueuedCompletionStatus(hIocp, &dwTrBytes, &ulKey, &pov, INFINITE);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (pov == NULL)
			{
				if (dwErrCode != ERROR_ABANDONED_WAIT_0)
					cout << "GQCS failed: " << dwErrCode << endl;
				break;
			}
			else
			{
				cout << "Internal error: " << dwErrCode << endl;
				continue;
			}
		}

		HANDLE	  hDir = (HANDLE)ulKey;
		PDIR_BUFF pdb  = (PDIR_BUFF)pov;
		if (dwTrBytes > 0)
			PrintDirModEntries(pdb->_buff);

		bIsOK = ReadDirectoryChangesW
		(
			hDir, pdb->_buff, BUFF_SIZE, FALSE,
			DIR_NOTI_FILTER, NULL, pov, NULL
		);
		if (!bIsOK)
			cout << "ReadDirectoryChangesW failed: " << GetLastError() << endl;
	}

	return 0;
}

#define MAX_COPY_CNT  10
void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		cout << "Uasge : DirChgNotiIOCP MonDir1 MonDir2 MonDir3 ..." << endl;
		return;
	}
	if (argc > MAX_COPY_CNT + 1)
		argc = MAX_COPY_CNT + 1;

	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

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
		CreateIoCompletionPort(hDir, hIocp, (ULONG_PTR)hDir, 0);

		PDIR_BUFF pdb = new DIR_BUFF();

		BOOL bIsOK = ReadDirectoryChangesW
		(
			hDir, pdb->_buff, BUFF_SIZE, FALSE,
			DIR_NOTI_FILTER, NULL, pdb, NULL
		);
		if (!bIsOK)
		{
			cout << "ReadDirectoryChangesW failed: " << GetLastError() << endl;
			delete pdb;
			CloseHandle(hDir);
			continue;
		}

		arBuff[i - 1] = pdb;
		nMonCnt++;
	}

	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, DirMonProc, hIocp, 0, &dwThrID);

	getchar();
	CloseHandle(hIocp);
	WaitForSingleObject(hThread, INFINITE);
	for (int i = 0; i < nMonCnt; i++)
	{
		PDIR_BUFF pdb = arBuff[i];
		delete pdb;
	}
}
