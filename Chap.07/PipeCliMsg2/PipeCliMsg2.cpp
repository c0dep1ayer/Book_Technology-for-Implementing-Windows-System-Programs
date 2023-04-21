#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define BUFSIZE 512
#define PIPE_NAME	_T("\\\\.\\pipe\\PIPE_SYNC_TEST")

void _tmain()
{
	char szIn[BUFSIZE];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		DWORD	dwErrCode = NO_ERROR;
		char	szRead[256];
		DWORD	dwReads = 0;
		BOOL bIsOK = CallNamedPipe
		(
			PIPE_NAME, szIn, strlen(szIn), szRead, sizeof(szRead), &dwReads, 2000
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_MORE_DATA)
			{
				cout << "CallNamedPipe failed, code = " << dwErrCode << endl;
				break;
			}
			cout << "...Extra data in message was lost." << endl;
		}
		szRead[dwReads] = 0;
		cout << " ==> Server sent : " << szIn << endl;
	}
}
