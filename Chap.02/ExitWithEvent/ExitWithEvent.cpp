#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define AUTO_RESET 1


#define MAX_RES_CNT		3

HANDLE	g_hExit = NULL;
BOOL	g_abUsedFlag[MAX_RES_CNT];
INT		g_anSharedRes[MAX_RES_CNT] = { 5 , 3, 7 };

DWORD WINAPI SemaphoreThreadProc(LPVOID pParam)
{
	HANDLE hSemaphore = (HANDLE)pParam;
	DWORD  dwThreadId = GetCurrentThreadId();

	HANDLE arhWaits[2] = { g_hExit, hSemaphore };
	while (true)
	{
		DWORD dwWaitCode = WaitForMultipleObjects(2, arhWaits, FALSE, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitCode == WAIT_OBJECT_0)
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


#if AUTO_RESET == 0
void _tmain()
{
	cout << "======= Start ExitWithEvent Test ========" << endl;
	g_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);

	HANDLE hSemaphore = CreateSemaphore(NULL, MAX_RES_CNT, MAX_RES_CNT, NULL);

	DWORD	dwThreadID;
	HANDLE	hThreads[MAX_RES_CNT + 2];
	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		hThreads[i] = CreateThread(NULL, 0, SemaphoreThreadProc, hSemaphore, 0, &dwThreadID);

	getchar();
	WaitForMultipleObjects(MAX_RES_CNT + 2, hThreads, TRUE, INFINITE);

	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		CloseHandle(hThreads[i]);
	CloseHandle(hSemaphore);
	CloseHandle(g_hExit);
	cout << "======= End ExitWithEvent Test ==========" << endl;
}
#else
void _tmain()
{
	cout << "======= Start ExitWithEvent Test ========" << endl;
	g_hExit = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE hSemaphore = CreateSemaphore(NULL, MAX_RES_CNT, MAX_RES_CNT, NULL);

	DWORD	dwThreadID;
	HANDLE	hThreads[MAX_RES_CNT + 2];
	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		hThreads[i] = CreateThread(NULL, 0, SemaphoreThreadProc, hSemaphore, 0, &dwThreadID);

	for (int i = 0; i < MAX_RES_CNT + 2; i++)
	{
		getchar();
		SetEvent(g_hExit);
	}
	WaitForMultipleObjects(MAX_RES_CNT + 2, hThreads, TRUE, INFINITE);

	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		CloseHandle(hThreads[i]);
	CloseHandle(hSemaphore);
	CloseHandle(g_hExit);
	cout << "======= End ExitWithEvent Test ==========" << endl;
}
#endif