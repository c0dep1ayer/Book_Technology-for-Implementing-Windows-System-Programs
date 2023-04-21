#include "stdafx.h"
#include "Windows.h"
#include "time.h"
#include "iostream"
using namespace std;

VOID CALLBACK ThreadPoolTimerProc(PTP_CALLBACK_INSTANCE ptpCbInst, PVOID pParam, PTP_TIMER ptpWait)
{
	int nWorkId = (int)pParam;
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("=> Timer %d(TID : %d) started at : %02d:%02d:%02d.%03d\n",
		nWorkId, GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep(nWorkId * 1000);

	GetLocalTime(&st);
	printf("...Timer %d(TID : %d) ended at : %02d:%02d:%02d.%03d\n",
		nWorkId, GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}


void _tmain()
{
	PTP_TIMER arptpTimers[5];
	for (int i = 0; i < 5; i++)
		arptpTimers[i] = CreateThreadpoolTimer(ThreadPoolTimerProc, (PVOID)i, NULL);

	for (int i = 0; i < 5; i++)
	{
		ULARGE_INTEGER ll;
		ll.QuadPart = -((__int64)i * 10000000);
		FILETIME ft;
		ft.dwHighDateTime = ll.HighPart;
		ft.dwLowDateTime  = ll.LowPart;

		SetThreadpoolTimer(arptpTimers[i], &ft, (5 - i) * 1000, 0);
	}

	Sleep(10000);

	for (int i = 0; i < 5; i++)
		WaitForThreadpoolTimerCallbacks(arptpTimers[i], TRUE);

	for (int i = 0; i < 5; i++)
		CloseThreadpoolTimer(arptpTimers[i]);
}
