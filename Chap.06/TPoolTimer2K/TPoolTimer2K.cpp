#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

VOID CALLBACK ThreadPoolTimerProc(PVOID pParam, BOOLEAN bTW)
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
	HANDLE hTmrQue = CreateTimerQueue();
	HANDLE arTimers[5];
	for (int i = 0; i < 5; i++)
	{
		LARGE_INTEGER ll;
		ll.QuadPart = -((__int64)i * 10000000);
		CreateTimerQueueTimer
		(
			&arTimers[i], hTmrQue,
			ThreadPoolTimerProc, (PVOID)i, 
			i * 1000, (10 - i) * 1000, WT_EXECUTEDEFAULT
		);
	}
	//cout << "Main thread creating sub thread..." << endl;
	getchar();

	//cout << "Sub thread " << dwThreadID << " terminated..." << endl;
	for (int i = 0; i < 5; i++)
		DeleteTimerQueueTimer(hTmrQue, arTimers[i], NULL);
	DeleteTimerQueueEx(hTmrQue, NULL);
}
