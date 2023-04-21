#include "stdafx.h"
#include "Windows.h"
#include "time.h"
#include "iostream"
using namespace std;

#define SAMPLE_NO	0


VOID CALLBACK ThreadPoolWaitProc(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	DWORD dwDelay = (DWORD)pCtx;
	DWORD dwThrId = GetCurrentThreadId();
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("=> Work(ThrId : %d) started at : %02d:%02d:%02d.%03d\n",
		dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep(dwDelay * 1000);

	GetLocalTime(&st);
	printf("<= Work(ThrId : %d) ended at   : %02d:%02d:%02d.%03d\n",
		dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void _tmain()
{
	printf("===== MainThread(ID : %d) started...\n\n", GetCurrentThreadId());

#if (SAMPLE_NO == 0)
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	PTP_WAIT ptpWait = CreateThreadpoolWait(ThreadPoolWaitProc, (PVOID)3, NULL);

	SetThreadpoolWait(ptpWait, hEvent, NULL);

	getchar();
	SetEvent(hEvent);

	WaitForThreadpoolWaitCallbacks(ptpWait, TRUE);
	CloseThreadpoolWait(ptpWait);
#elif (SAMPLE_NO == 1) 
	PTP_WAIT ptpWait = CreateThreadpoolWait(ThreadPoolWaitProc, (PVOID)3, NULL);
	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	SetThreadpoolWait(ptpWait, hTimer, NULL);

	getchar();
	LARGE_INTEGER ll; ll.QuadPart = -30000000LL;
	SetWaitableTimer(hTimer, &ll, 2000, NULL, NULL, FALSE);

	WaitForThreadpoolWaitCallbacks(ptpWait, FALSE);
	CloseThreadpoolWait(ptpWait);
#endif

	printf("\n===== MainThread(ID : %d) terminated...\n", GetCurrentThreadId());
}
