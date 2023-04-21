#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	4096
#define PIPE_NAME	_T("\\\\.\\pipe\\PIPE_SYNC_TEST")

DWORD WINAPI PipeInstProc(PVOID pParam)
{
	HANDLE hPipe = (HANDLE)pParam;
	char szBuff[BUFF_SIZE];

	while (true)
	{
		DWORD dwReads = 0;
		BOOL bIsOK = ReadFile(hPipe, szBuff, BUFF_SIZE, &dwReads, NULL);
		if (!bIsOK || dwReads == 0)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode == ERROR_BROKEN_PIPE)
				cout << "Client disconnected...." << endl;
			else
				cout << "ReadFile failed, code = " << dwErrCode << endl;
			break;
		}

		szBuff[dwReads] = 0;
		cout << " ==> Client sent : " << szBuff << endl;

		DWORD dwWrotes = 0;
		bIsOK = WriteFile(hPipe, szBuff, dwReads, &dwWrotes, NULL);
		if (!bIsOK || dwReads != dwWrotes) 
			break;
	}
	CloseHandle(hPipe);

	return 0;
}

void _tmain()
{
	while (true)
	{
		HANDLE hPipe = CreateNamedPipe
		(
			PIPE_NAME, PIPE_ACCESS_DUPLEX, 
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES, BUFF_SIZE, BUFF_SIZE,
			NMPWAIT_USE_DEFAULT_WAIT, NULL
		);
		if (hPipe == INVALID_HANDLE_VALUE)
		{
			cout << "CreateNamedPipe failed, code = " << GetLastError() << endl;
			break;
		}

		BOOL bIsOK = ConnectNamedPipe(hPipe, NULL);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_PIPE_CONNECTED)
			{
				cout << "ConnectNamedPipe failed, code = " << dwErrCode << endl;
				CloseHandle(hPipe);
				break;
			}
		}

		DWORD dwThrId = 0;
		HANDLE hThread = CreateThread(NULL, 0, PipeInstProc, hPipe, 0, &dwThrId);
		if (hThread == NULL)
		{
			cout << "CreateThread failed, code = " << GetLastError() << endl;
			CloseHandle(hPipe);
			break;
		}
		CloseHandle(hThread);
	}
}
