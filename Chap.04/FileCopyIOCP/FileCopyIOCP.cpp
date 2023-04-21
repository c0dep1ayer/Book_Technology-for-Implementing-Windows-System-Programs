#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	65536

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Uasge : FileCopySync SourceFile TargetFile" << endl;
		return;
	}

	TCHAR* pszSrcFile = argv[1];
	TCHAR* pszDstFile = argv[2];

	HANDLE hSrcFile = CreateFile
	(
		pszSrcFile, GENERIC_READ, 0, NULL,
		OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
	);
	if (hSrcFile == INVALID_HANDLE_VALUE)
	{
		cout << pszSrcFile << " open failed, code : " << GetLastError() << endl;
		return;
	}

	HANDLE hDstFile = CreateFile
	(
		pszDstFile, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL
	);
	if (hDstFile == INVALID_HANDLE_VALUE)
	{
		cout << pszDstFile << " open failed, code : " << GetLastError() << endl;
		return;
	}

	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (hIocp == NULL)
	{
		cout << " ~~~ CreateIoCompletionPort failed, code : " << GetLastError() << endl;
		return;
	}

	CreateIoCompletionPort(hSrcFile, hIocp, 1, 0);
	CreateIoCompletionPort(hDstFile, hIocp, 2, 0);

	OVERLAPPED ro = { 0 };
	OVERLAPPED wo = { 0 };

	DWORD	dwErrCode = 0;
	BYTE	btBuff[BUFF_SIZE];
	while (true)
	{
		BOOL bIsOK = ReadFile
		(
			hSrcFile, btBuff, sizeof(btBuff), NULL, &ro
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

		LPOVERLAPPED pOL = NULL;
		DWORD		dwReadBytes = 0;
		ULONG_PTR	dwReadKey;
		bIsOK = GetQueuedCompletionStatus
		(
			hIocp, &dwReadBytes, &dwReadKey, &pOL, INFINITE
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (pOL != NULL)
			{
				if (dwErrCode == ERROR_HANDLE_EOF)
					dwErrCode = 0;
			}
			break;
		}
		pOL->Offset += dwReadBytes;
		printf(" => KEY-> %d, Read  bytes : %d\n", dwReadKey, pOL->Offset);

		bIsOK = WriteFile
		(
			hDstFile, btBuff, dwReadBytes, NULL, &wo
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

		DWORD		dwWroteBytes = 0;
		ULONG_PTR	dwWroteKey;
		pOL = NULL;
		bIsOK = GetQueuedCompletionStatus
		(
			hIocp, &dwWroteBytes, &dwWroteKey, &pOL, INFINITE
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			break;
		}
		pOL->Offset += dwWroteBytes;
		printf(" <= KEY-> %d, Wrote bytes : %d\n", dwWroteKey, pOL->Offset);
	}
	CloseHandle(hSrcFile);
	CloseHandle(hDstFile);
	CloseHandle(hIocp);

	if (dwErrCode != ERROR_SUCCESS)
		cout << endl << "Error occurred in file copying, code=" << dwErrCode << endl;
	else
		cout << endl << "File copy successfully completed..." << endl;
}
