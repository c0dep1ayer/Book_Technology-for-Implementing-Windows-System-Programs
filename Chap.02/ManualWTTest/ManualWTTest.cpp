#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

HANDLE	g_hTimer  = NULL;
HANDLE	g_hExit   = NULL;
HANDLE	g_hWait	  = NULL;

#define MAX_THR_CNT  4


DWORD WINAPI ThreadProc(PVOID pParam)
{
	HANDLE arHandles[2] = { g_hExit, g_hTimer };

	while (true)
	{
		DWORD dwWaitCode = WaitForMultipleObjects(2, arHandles, FALSE, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitCode == WAIT_OBJECT_0)
			break;

		SYSTEMTIME st;
		GetLocalTime(&st);
		printf("%6d Timer Signaled : %5d/%02d/%02d-%02d:%02d:%02d+%03d\n",
			GetCurrentThreadId(), 
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

		DWORD dwDelay = (rand() % 4) + 1;
		printf("\n==> Timer updated... Next signal : %d seconds\n", dwDelay);

		LARGE_INTEGER lUTC;
		lUTC.QuadPart = -((__int64)dwDelay * 10000000LL);
		SetWaitableTimer(g_hTimer, &lUTC, 0, NULL, NULL, TRUE);
	}

	return 0;
}


void _tmain()
{
	g_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hWait = CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hTimer = CreateWaitableTimer(NULL, TRUE, NULL);

	HANDLE arhThrs[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwThrId = 0;
		arhThrs[i] = CreateThread(NULL, 0, ThreadProc, NULL, 0, &dwThrId);
	}

	DWORD dwDelay = (rand() % 4) + 1;
	printf("==> Timer started... Next signal : %d seconds\n", dwDelay);

	LARGE_INTEGER lUTC;
	lUTC.QuadPart = -((__int64)dwDelay * 10000000LL);
	SetWaitableTimer(g_hTimer, &lUTC, 0, NULL, NULL, TRUE);

	getchar();
	SetEvent(g_hExit);

	WaitForMultipleObjects(MAX_THR_CNT, arhThrs, TRUE, INFINITE);
	for (int i = 0; i < MAX_THR_CNT; i++)
		CloseHandle(arhThrs[i]);
	CloseHandle(g_hTimer);
	CloseHandle(g_hWait);
	CloseHandle(g_hExit);
}
