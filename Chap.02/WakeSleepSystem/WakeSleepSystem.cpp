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
	lUTC.QuadPart = -600000000LL;
	if (!SetWaitableTimer(hTimer, &lUTC, 0, NULL, NULL, FALSE))
		cout << "...SetWaitableTimer error : " << GetLastError() << endl;
	GetLocalTime(&st);
	printf("Timer Reserved : %04d/%02d/%02d-%02d:%02d:%02d+%03d\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	SetSuspendState(TRUE, FALSE, FALSE);

	WaitForSingleObject(hTimer, INFINITE);
	GetLocalTime(&st);
	printf("Alarm Signaled : %04d/%02d/%02d-%02d:%02d:%02d+%03d\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	MessageBeep(MB_ICONWARNING);

	getchar();
	GetLocalTime(&st);
	printf("Alarm Signaled : %04d/%02d/%02d-%02d:%02d:%02d+%03d\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	CloseHandle(hTimer);
}

