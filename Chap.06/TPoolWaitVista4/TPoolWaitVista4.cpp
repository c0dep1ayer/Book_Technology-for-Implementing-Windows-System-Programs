#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


VOID CALLBACK TPWaitTimer(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	HANDLE hTimer = (HANDLE)pCtx;
	DWORD dwThrId = GetCurrentThreadId();

	SYSTEMTIME st;
	GetLocalTime(&st);
	GetLocalTime(&st);
	printf("=> Timer(ThrId : %d) expired at : %02d:%02d:%02d.%03d\n",
		dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	SetThreadpoolWait(ptpWait, hTimer, NULL);
}

VOID CALLBACK TPWaitEvent(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	DWORD dwDelay = (DWORD)pCtx;
	DWORD dwThrId = GetCurrentThreadId();

	SYSTEMTIME st;
	GetLocalTime(&st);
	GetLocalTime(&st);
	printf("=> Event(ThrId : %d) signaled at : %02d:%02d:%02d.%03d\n",
		dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep(dwDelay * 100);
}

#define WAIT_UINT_CNT	50
void _tmain()
{
	printf("===== MainThread(ID : %d) started...\n\n", GetCurrentThreadId());

	HANDLE arSyncs[WAIT_UINT_CNT * 2];
	PTP_WAIT ptpWaits[WAIT_UINT_CNT * 2];

	int i = 0;
	for (; i < WAIT_UINT_CNT; i++)
	{
		arSyncs[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		ptpWaits[i] = CreateThreadpoolWait(TPWaitEvent, (PVOID)i, NULL);
	}
	for (; i < WAIT_UINT_CNT * 2; i++)
	{
		arSyncs[i] = CreateWaitableTimer(NULL, FALSE, NULL);
		ptpWaits[i] = CreateThreadpoolWait(TPWaitTimer, arSyncs[i], NULL);
	}

	for (i = WAIT_UINT_CNT; i < WAIT_UINT_CNT * 2; i++)
	{
		SetThreadpoolWait(ptpWaits[i], arSyncs[i], NULL);
		LARGE_INTEGER ll; ll.QuadPart = -30000000LL;
		SetWaitableTimer(arSyncs[i], &ll, 2000, NULL, NULL, FALSE);
	}

	for (int i = 0; i < 50; i++)
	{
		int evtIdx = rand() % (WAIT_UINT_CNT / 2);
		
		SetThreadpoolWait(ptpWaits[evtIdx], arSyncs[evtIdx], NULL);
		SetEvent(arSyncs[evtIdx]);
		Sleep(2000);
	}

	Sleep(10);
	for (int i = 0; i < WAIT_UINT_CNT * 2; i++)
	{
		WaitForThreadpoolWaitCallbacks(ptpWaits[i], TRUE);
		CloseThreadpoolWait(ptpWaits[i]);
		CloseHandle(arSyncs[i]);
	}

	printf("\n===== MainThread(ID : %d) terminated...\n", GetCurrentThreadId());
}
