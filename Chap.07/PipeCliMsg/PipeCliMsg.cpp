#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define BUFSIZE 512
#define PIPE_NAME	_T("\\\\.\\pipe\\PIPE_SYNC_TEST")

void _tmain()
{
	HANDLE hPipe = INVALID_HANDLE_VALUE;
	while (true)
	{
		hPipe = CreateFile
			(
			PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
			);
		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		DWORD dwErrCode = GetLastError();
		if (dwErrCode != ERROR_PIPE_BUSY)
		{
			cout << "Could not open pipe, code = " << dwErrCode << endl;
			return;
		}

		if (!WaitNamedPipe(PIPE_NAME, 2000))
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_SEM_TIMEOUT)
			{
				cout << "WaitNamedPipe failed, code = " << GetLastError() << endl;
				return;
			}
		}
	}

	DWORD dwMode = PIPE_READMODE_MESSAGE;
	BOOL bIsOK = SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL);
	if (!bIsOK)
	{
		cout << "SetNamedPipeHandleState failed, code = " << GetLastError() << endl;
		CloseHandle(hPipe);
		return;
	}

	char szIn[BUFSIZE];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		char	szRead[BUFSIZE];
		DWORD	dwReads = 0;
		bIsOK = TransactNamedPipe
			(
			hPipe, szIn, strlen(szIn),
			szRead, 8, &dwReads, NULL
			);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_MORE_DATA)
			{
				cout << "CallNamedPipe failed, code = " << dwErrCode << endl;
				break;
			}
		}

		bool bError = false;
		while (!bIsOK)
		{
			DWORD dwMores = 0;
			bIsOK = ReadFile(hPipe, szRead + dwReads, 8, &dwMores, NULL);
			if (!bIsOK)
			{
				DWORD dwErrCode = GetLastError();
				if (dwErrCode != ERROR_MORE_DATA)
				{
					cout << "ReadFile failed, code = " << dwErrCode << endl;
					bError = true;
					break;
				}
			}
			dwReads += dwMores;
		}
		if (bError)
			break;
		szIn[dwReads] = 0;
		cout << " ==> Server sent : " << szIn << endl;
	}
	CloseHandle(hPipe);
}
