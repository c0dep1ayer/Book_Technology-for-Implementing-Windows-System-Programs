#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;


#define TM_CMD_EXIT	WM_USER + 1
#define TM_CMD_DATA	WM_USER + 2


DWORD WINAPI QueueWorkerProc(LPVOID pParam)
{
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == TM_CMD_EXIT)
			break;

		char* pszData = (char*)msg.lParam;
		printf(" => queued data : %s\n", pszData);
		delete[] pszData;
	}
	printf("......QueueWorkerProc Thread Exit!!!\n");

	return 0;
}

void _tmain()
{
	cout << "======= Start MsgQueue Test ========" << endl;

	DWORD dwThreadId;
	HANDLE hThread = CreateThread(NULL, 0, QueueWorkerProc, NULL, 0, &dwThreadId);

	char szIn[1024];
	while (true)
	{
		cin >> szIn;

		if (_stricmp(szIn, "quit") == 0)
		{
			PostThreadMessage(dwThreadId, TM_CMD_EXIT, 0, 0);
			break;
		}

		char* pszData = new char[strlen(szIn) + 1];
		strcpy(pszData, szIn);
		PostThreadMessage(dwThreadId, TM_CMD_DATA, 0, (LPARAM)pszData);
	}

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);

	cout << "======= End MsgQueue Test ==========" << endl;
}
