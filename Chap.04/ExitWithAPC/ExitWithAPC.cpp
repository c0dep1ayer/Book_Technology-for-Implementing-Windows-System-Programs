#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define MAX_RES_CNT		3
BOOL	g_abUsedFlag[MAX_RES_CNT];
INT		g_anSharedRes[MAX_RES_CNT] = { 5 , 3, 7 };

DWORD WINAPI SemaThreadProc(LPVOID pParam)
{
	HANDLE hSemaphore = (HANDLE)pParam;
	DWORD  dwThreadId = GetCurrentThreadId();

	while (true)
	{
		DWORD dwWaitCode = WaitForSingleObjectEx(hSemaphore, INFINITE, TRUE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitCode == WAIT_IO_COMPLETION)
			break;

		int nSharedIdx = 0;
		for (; nSharedIdx < MAX_RES_CNT; nSharedIdx++)
		{
			if (!g_abUsedFlag[nSharedIdx])
				break;
		}

		g_abUsedFlag[nSharedIdx] = TRUE;
		cout << " ==> Thread " << dwThreadId <<
			" waits " << g_anSharedRes[nSharedIdx] << " seconds..." << endl;
		Sleep((DWORD)g_anSharedRes[nSharedIdx] * 1000);
		cout << " ==> Thread " << dwThreadId << " releases semaphore..." << endl;
		g_abUsedFlag[nSharedIdx] = FALSE;

		ReleaseSemaphore(hSemaphore, 1, NULL);
	}
	cout << " *** Thread " << dwThreadId << " exits..." << endl;

	return 0;
}

VOID WINAPI SemaThreadExitAPC(ULONG_PTR)
{
	// 아무 일도 하지 않는다.
}


void _tmain()
{
	cout << "======= Start ExitWithAPC Test ========" << endl;

	HANDLE hSemaphore = CreateSemaphore(NULL, MAX_RES_CNT, MAX_RES_CNT, NULL);

	DWORD	dwThreadID;
	HANDLE	hThreads[MAX_RES_CNT + 2];
	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		hThreads[i] = CreateThread(NULL, 0, SemaThreadProc, hSemaphore, 0, &dwThreadID);

	getchar();
	for (int i = 0; i < MAX_RES_CNT + 2; i++)
		QueueUserAPC(SemaThreadExitAPC, hThreads[i], NULL);
	WaitForMultipleObjects(MAX_RES_CNT + 2, hThreads, TRUE, INFINITE);

	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		CloseHandle(hThreads[i]);
	CloseHandle(hSemaphore);

	cout << "======= End ExitWithAPC Test ==========" << endl;
}
