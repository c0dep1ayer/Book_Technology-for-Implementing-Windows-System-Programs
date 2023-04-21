#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


void _tmain()
{
	WORD  wHour, wMin;
	DWORD dwPeriod;

	cout << "Enter Hour Minute Period : ";
	cin >> wHour >> wMin >> dwPeriod;

	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	SYSTEMTIME st;
	GetLocalTime(&st);
	st.wHour   = wHour;
	st.wMinute = wMin;
	st.wSecond = st.wMilliseconds = 0;

	FILETIME ftLocal, ftUTC;
	SystemTimeToFileTime(&st, &ftLocal);
	LocalFileTimeToFileTime(&ftLocal, &ftUTC);
	LARGE_INTEGER lUTC;
	lUTC.LowPart  = ftUTC.dwLowDateTime;
	lUTC.HighPart = ftUTC.dwHighDateTime;

	SetWaitableTimer(hTimer, &lUTC, dwPeriod * 1000, NULL, NULL, TRUE);

	char szTimeS[64];
	sprintf(szTimeS, "%04d/%02d/%02d-%02d:%02d:%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	cout << "Alarm Reserved : " << szTimeS << endl << endl;

	int nSnoozeCnt = 0;
	while (nSnoozeCnt < 5)
	{
		WaitForSingleObject(hTimer, INFINITE);
		GetLocalTime(&st);

		sprintf(szTimeS, "%04d/%02d/%02d-%02d:%02d:%02d",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		if (nSnoozeCnt == 0)
			cout << "Alarm Started : ";
		else
			cout << nSnoozeCnt << " Snooze Time : ";
		cout << szTimeS << endl;
		nSnoozeCnt++;
	}
	CloseHandle(hTimer);
}

