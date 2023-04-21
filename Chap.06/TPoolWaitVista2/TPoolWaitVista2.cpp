#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define SAMPLE_NO	0

VOID CALLBACK ThreadPoolWaitProc(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	HANDLE hSync = (HANDLE)pCtx;
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("=> WorkItem %d started at : %02d:%02d:%02d.%03d\n",
		GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep(1000);

	GetLocalTime(&st);
	printf("...WorkItem %d ended at : %02d:%02d:%02d.%03d\n",
		GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	SetThreadpoolWait(ptpWait, hSync, NULL);
}

void _tmain()
{
	printf("===== MainThread(ID : %d) started...\n\n", GetCurrentThreadId());

#if (SAMPLE_NO == 0)
	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
	PTP_WAIT ptpWait = CreateThreadpoolWait(ThreadPoolWaitProc, hTimer, NULL);

	SetThreadpoolWait(ptpWait, hTimer, NULL);

	LARGE_INTEGER ll; ll.QuadPart = -40000000LL;
	SetWaitableTimer(hTimer, &ll, 2000, NULL, NULL, FALSE);

	getchar();

	WaitForThreadpoolWaitCallbacks(ptpWait, FALSE);
	CloseThreadpoolWait(ptpWait);
#elif (SAMPLE_NO == 1) 
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	PTP_WAIT ptpWait = CreateThreadpoolWait(ThreadPoolWaitProc, hEvent, NULL);

	SetThreadpoolWait(ptpWait, hEvent, NULL);
	while (true)
	{
		char ch = getchar();
		if (ch == 'q')
			break;

		SetEvent(hEvent);
	}
	WaitForThreadpoolWaitCallbacks(ptpWait, FALSE);
	CloseThreadpoolWait(ptpWait);
#endif

	printf("\n===== MainThread(ID : %d) terminated...\n", GetCurrentThreadId());
}
