#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define MAX_RES_CNT		3

bool	g_bExit = false;
BOOL	g_abUsedFlag [MAX_RES_CNT];
INT		g_anSharedRes[MAX_RES_CNT] = { 5 , 3, 7 };
HANDLE	g_hShareLock;

DWORD WINAPI SemaphoreProc(LPVOID pParam)
{
	HANDLE hSemaphore = (HANDLE)pParam;
	DWORD  dwThreadId = GetCurrentThreadId();

	while (!g_bExit)
	{
		DWORD dwWaitCode = WaitForSingleObject(hSemaphore, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		int nSharedIdx = 0;
		WaitForSingleObject(g_hShareLock, INFINITE);
		for (; nSharedIdx < MAX_RES_CNT; nSharedIdx++)
		{
			if (!g_abUsedFlag[nSharedIdx])
				break;
		}
		g_abUsedFlag[nSharedIdx] = TRUE;
		ReleaseMutex(g_hShareLock);

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


void _tmain()
{
	cout << "======= Start Semaphore Test ========" << endl;
	g_hShareLock = CreateMutex(NULL, FALSE, NULL);

	HANDLE hSemaphore = CreateSemaphore(NULL, MAX_RES_CNT, MAX_RES_CNT, NULL);

	DWORD	dwThrId;
	HANDLE	hThreads[MAX_RES_CNT + 2];
	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		hThreads[i] = CreateThread(NULL, 0, SemaphoreProc, hSemaphore, 0, &dwThrId);

	getchar();
	g_bExit = true;

	WaitForMultipleObjects(MAX_RES_CNT + 2, hThreads, TRUE, INFINITE);

	for (int i = 0; i<MAX_RES_CNT + 2; i++)
		CloseHandle(hThreads[i]);
	CloseHandle(hSemaphore);

	CloseHandle(g_hShareLock);
	cout << "======= End Semaphore Test ==========" << endl;
}
