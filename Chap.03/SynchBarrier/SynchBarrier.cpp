#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define DEL_SYNC_TEST


SYNCHRONIZATION_BARRIER g_sb;

DWORD WINAPI ThreadProc(LPVOID pParam)
{
	int nWait = (int)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	SYSTEMTIME st;
	GetLocalTime(&st);
	printf(".... Thread %d started and waits %d secs : %2d:%02d:%02d+%03d\n",
		dwThrId, (nWait + 1), st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep((nWait + 1) * 1000);
	
	GetLocalTime(&st);
	printf("==>> Thread %d Enters into barrier : %2d:%02d:%02d+%03d\n",
		dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	BOOL bIsLast = EnterSynchronizationBarrier(&g_sb, 0);
	GetLocalTime(&st);
	if (bIsLast)
		printf("<<== LAST  Thread %d Exit from barrier : %2d:%02d:%02d+%03d\n", 
			dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	else
		printf("<<== OTHER Thread %d Exit from barrier : %2d:%02d:%02d+%03d\n",
			dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	return 0;
}

#ifdef DEL_SYNC_TEST
#define MAX_THR_CNT		2
#define WAIT_THR_CNT	4
#else
#define MAX_THR_CNT		4
#define WAIT_THR_CNT	2
#endif
void _tmain(void)
{
	cout << "======= Start SynchBarrier Test ========" << endl;

	InitializeSynchronizationBarrier(&g_sb, WAIT_THR_CNT, 2000);

	HANDLE arhThreads[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwTheaID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc, (PVOID)i, 0, &dwTheaID);
	}

#ifdef DEL_SYNC_TEST
	Sleep(4000);
	cout << "~~~Calling DeleteSynchronizationBarrier." << endl;
	DeleteSynchronizationBarrier(&g_sb);
#endif

	WaitForMultipleObjects(MAX_THR_CNT, arhThreads, TRUE, INFINITE);
	DeleteSynchronizationBarrier(&g_sb);

	cout << "======= End SynchBarrier Test ==========" << endl;
}

