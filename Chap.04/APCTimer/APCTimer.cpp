#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;



VOID CALLBACK TimerAPCProc
(
	PVOID pArgs, DWORD dwTimerLowValue, DWORD dwTimerHighValue
)
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	FILETIME uft, ft;
	uft.dwHighDateTime = dwTimerHighValue;
	uft.dwLowDateTime  = dwTimerLowValue;
	FileTimeToLocalFileTime(&uft, &ft);
	FileTimeToSystemTime(&ft, &st);

	char szTimeS[64];
	sprintf(szTimeS, "%04d/%02d/%02d-%02d:%02d:%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	int* pnSnoozeCnt = (int*)pArgs;
	if (*pnSnoozeCnt == 0)
		cout << "Alarm Started : ";
	else
		cout << *pnSnoozeCnt << " Snooze Time : ";
	cout << szTimeS << endl;
}


void _tmain()
{
	WORD  wHour, wMin;
	DWORD dwPeriod;

	cout << "Enter Hour Minute Period : ";
	cin >> wHour >> wMin >> dwPeriod;

	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	SYSTEMTIME st;
	GetLocalTime(&st);
	st.wHour = wHour;
	st.wMinute = wMin;
	st.wSecond = st.wMilliseconds = 0;

	FILETIME ftLocal, ftUTC;
	SystemTimeToFileTime(&st, &ftLocal);
	LocalFileTimeToFileTime(&ftLocal, &ftUTC);
	LARGE_INTEGER lUTC;
	lUTC.LowPart = ftUTC.dwLowDateTime;
	lUTC.HighPart = ftUTC.dwHighDateTime;

	int nSnoozeCnt = 0;
	SetWaitableTimer(hTimer, &lUTC, dwPeriod * 1000, TimerAPCProc, &nSnoozeCnt, TRUE);

	char szTimeS[64];
	sprintf(szTimeS, "%04d/%02d/%02d-%02d:%02d:%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	cout << "Alarm Reserved : " << szTimeS << endl << endl;

	while (nSnoozeCnt < 5)
	{
		SleepEx(INFINITE, TRUE);
		nSnoozeCnt++;
	}
	CloseHandle(hTimer);
}

