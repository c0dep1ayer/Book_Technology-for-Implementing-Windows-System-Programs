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
			PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL
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

	char szIn[BUFSIZE];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		DWORD dwWrotes = 0;
		BOOL bIsOK = WriteFile(hPipe, szIn, strlen(szIn), &dwWrotes, NULL);
		if (!bIsOK)
		{
			cout << "WriteFile failed, code = " << GetLastError() << endl;
			break;
		}

		DWORD dwReads = 0;
		bIsOK = ReadFile(hPipe, szIn, BUFSIZE, &dwReads, NULL);
		if (!bIsOK)
		{
			cout << "ReadFile failed, code = " << GetLastError() << endl;
			break;
		}

		szIn[dwReads] = 0;
		cout << " ==> Server sent : " << szIn << endl;
	}
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);
}