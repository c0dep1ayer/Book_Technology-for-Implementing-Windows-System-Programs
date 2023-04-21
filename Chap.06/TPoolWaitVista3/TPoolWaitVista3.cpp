#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

VOID CALLBACK ThreadPoolWaitProc(PTP_CALLBACK_INSTANCE, PVOID pParam, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	int nWorkId = (int)pParam;
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("=> WorkItem %d(ID : %d) started at : %02d:%02d:%02d.%03d\n",
		nWorkId, GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep(nWorkId * 1000);

	GetLocalTime(&st);
	printf("...WorkItem %d(ID : %d) ended at : %02d:%02d:%02d.%03d\n",
		nWorkId, GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void _tmain()
{
	printf("===== MainThread(ID : %d) started...\n\n", GetCurrentThreadId());

	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	PTP_WAIT ptpWait = CreateThreadpoolWait(ThreadPoolWaitProc, (PVOID)4, NULL);
	SetThreadpoolWait(ptpWait, hEvent, NULL);
	SetEvent(hEvent);
	SetThreadpoolWait(ptpWait, hEvent, NULL);
	SetEvent(hEvent);

	WaitForThreadpoolWaitCallbacks(ptpWait, FALSE);
	CloseThreadpoolWait(ptpWait);

	printf("\n===== MainThread(ID : %d) terminated...\n", GetCurrentThreadId());
}
