#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;

#include "NTSecApi.h"

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
struct DIR_BUFF : OVERLAPPED
{
	HANDLE	_hDir;
	BYTE	_buff[BUFF_SIZE];

	DIR_BUFF(HANDLE hDir)
	{
		memset(this, 0, sizeof(*this));
		_hDir = hDir;
	}
};
typedef DIR_BUFF* PDIR_BUFF;


DWORD WINAPI DirChangedCallback(PVOID pParam)
{
	HANDLE hIocp = (HANDLE)pParam;

	while (true)
	{
		DWORD		dwErrCode = ERROR_SUCCESS;
		PDIR_BUFF	pdb = NULL;
		ULONG_PTR	ulKey;
		DWORD		dwTrBytes = 0;
		BOOL bIsOK = GetQueuedCompletionStatus
		(
			hIocp, &dwTrBytes, &ulKey, (LPOVERLAPPED*)&pdb, INFINITE
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (pdb != NULL)
			{
				if (dwErrCode == ERROR_OPERATION_ABORTED)
					cout << "========>> MonDir CANCELED by user." << endl;
				else
				{
					cout << "~~~ GQCS Error: " << dwErrCode << endl;
					CloseHandle(pdb->_hDir);
					pdb->_hDir = INVALID_HANDLE_VALUE;
					continue;
				}
			}
			else
			{
				if (dwErrCode != ERROR_ABANDONED_WAIT_0)
					cout << "~~~ GQCS Error: " << dwErrCode << endl;
				break;
			}
		}

		if (dwErrCode == ERROR_SUCCESS)
		{
			if (pdb->Internal != 0)
			{
				DWORD dwErrCode = LsaNtStatusToWinError(pdb->Internal);
				if (dwErrCode == ERROR_NOTIFY_CLEANUP)
					cout << "********>> MonDir CLOSED by user." << endl;
				else
				{
					cout << "~~~ Internal Error: " << dwErrCode << endl;
					CloseHandle(pdb->_hDir);
					pdb->_hDir = INVALID_HANDLE_VALUE;
				}
				continue;
			}

			if (dwTrBytes > 0)
				PrintDirModEntries(pdb->_buff);
		}

		bIsOK = ReadDirectoryChangesW
		(
			pdb->_hDir, pdb->_buff, BUFF_SIZE, FALSE,
			DIR_NOTI_FILTER, NULL, pdb, NULL
		);
		if (!bIsOK)
		{
			cout << "~~~ RDCW Error: " << GetLastError() << endl;
			CloseHandle(pdb->_hDir);
			pdb->_hDir = INVALID_HANDLE_VALUE;
		}
	}

	return 0;
}

#define MAX_COPY_CNT  10
void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		cout << "Uasge : MultiCopyIOCP MonDir1 MonDir2 MonDir3 ..." << endl;
		return;
	}

	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, DirChangedCallback, hIocp, 0, &dwThrID);

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
		if (hDir == INVALID_HANDLE_VALUE)
		{
			cout << "CreateFile failed, code=" << GetLastError() << endl;
			continue;
		};
		CreateIoCompletionPort(hDir, hIocp, 0, 0);

		PDIR_BUFF pdb = new DIR_BUFF(hDir);
		PostQueuedCompletionStatus(hIocp, 0, 0, pdb);
		arBuff[i - 1] = pdb;
		nMonCnt++;
	}

	char szIn[256];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		bool bCancel = false;
		char ch = 0;
		if (_strnicmp(szIn, "cancel:", 7) == 0)
		{
			if (strlen(szIn) < 8)
				continue;
			ch = szIn[7];
			bCancel = true;
		}
		else if (_strnicmp(szIn, "close:", 6) == 0)
		{
			if (strlen(szIn) < 7)
				continue;
			ch = szIn[6];
			bCancel = false;
		}
		if (ch < '0' || ch > '9')
			continue;

		int nIdx = ch - '0';
		if (nIdx >= nMonCnt)
			continue;

		PDIR_BUFF pdb = arBuff[nIdx];
		if (pdb->_hDir != INVALID_HANDLE_VALUE)
		{
			if (bCancel)
				CancelIoEx(pdb->_hDir, NULL);
			else
			{
				CloseHandle(pdb->_hDir);
				pdb->_hDir = INVALID_HANDLE_VALUE;
			}
		}
	}

	CloseHandle(hIocp);
	WaitForSingleObject(hThread, INFINITE);
	for (int i = 0; i < nMonCnt; i++)
	{
		PDIR_BUFF pdb = arBuff[i];
		if (pdb->_hDir != INVALID_HANDLE_VALUE)
			CloseHandle(pdb->_hDir);
		delete pdb;
	}
}
