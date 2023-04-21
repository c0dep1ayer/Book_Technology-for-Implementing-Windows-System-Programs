#include "stdafx.h"
#include "Windows.h"
#include "time.h"
#include "iostream"
using namespace std;

VOID CALLBACK ThreadPoolTimerProc(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_TIMER ptpTimer)
{
	DWORD dwDelay = (DWORD)pCtx;
	DWORD dwThrId = GetCurrentThreadId();
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("=> Timer(ThrId : %d) started at : %02d:%02d:%02d.%03d\n",
		dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep(dwDelay * 1000);

	GetLocalTime(&st);
	printf("...Timer(ThrId : %d) ended at   : %02d:%02d:%02d.%03d\n",
		dwThrId, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}


void _tmain()
{
	printf("===== MainThread(ID : %d) started...\n\n", GetCurrentThreadId());

	PTP_TIMER ptpTimer = CreateThreadpoolTimer(ThreadPoolTimerProc, (PVOID)4, NULL);

	ULARGE_INTEGER ll;
	ll.QuadPart = -30000000LL;
	FILETIME ft;
	ft.dwHighDateTime = ll.HighPart;
	ft.dwLowDateTime = ll.LowPart;
	SetThreadpoolTimer(ptpTimer, &ft, 2000, 0);

	getchar();

	WaitForThreadpoolTimerCallbacks(ptpTimer, TRUE);
	//SetThreadpoolTimer(ptpTimer, NULL, 0, 0);
	//WaitForThreadpoolTimerCallbacks(ptpTimer, FALSE);
	CloseThreadpoolTimer(ptpTimer);

	printf("\n===== MainThread(ID : %d) terminated...\n", GetCurrentThreadId());
}
