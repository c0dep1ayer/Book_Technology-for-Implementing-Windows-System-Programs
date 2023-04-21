#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#include "PowrProf.h"
#pragma comment(lib, "PowrProf.lib")

void _tmain()
{
	SYSTEMTIME st;
	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	LARGE_INTEGER lUTC;
	lUTC.QuadPart = -300000000LL;

	REASON_CONTEXT rc;
	rc.Version = POWER_REQUEST_CONTEXT_VERSION;
	rc.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
	rc.Reason.SimpleReasonString = L"Continuous measurement";

	SetWaitableTimerEx(hTimer, &lUTC, 30000, NULL, NULL, &rc, 1000);
	GetLocalTime(&st);
	printf("Timer Set Call : %04d/%02d/%02d-%02d:%02d:%02d+%03d\n",
		st.wYear, st.wMonth, st.wDay, 
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	SetSuspendState(TRUE, FALSE, FALSE);
	
	int nSnoozeCnt = 0;
	while (nSnoozeCnt < 5)
	{
		WaitForSingleObject(hTimer, INFINITE);

		GetLocalTime(&st);
		printf("Timer Signaled : %04d/%02d/%02d-%02d:%02d:%02d+%03d\n",
			st.wYear, st.wMonth, st.wDay, 
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		if (nSnoozeCnt == 0)
			MessageBeep(MB_ICONWARNING);
		nSnoozeCnt++;
	}

	getchar();
	CloseHandle(hTimer);
}

