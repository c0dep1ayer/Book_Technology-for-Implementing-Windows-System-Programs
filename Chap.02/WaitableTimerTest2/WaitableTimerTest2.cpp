#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

HANDLE g_hTimer = NULL;
HANDLE g_hExit  = NULL;

DWORD WINAPI ThreadProc(PVOID pParam)
{
	HANDLE arHandles[2] = { g_hExit, g_hTimer };

	while (true)
	{
		DWORD dwWaitCode = WaitForMultipleObjects(2, arHandles, FALSE, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitCode == WAIT_OBJECT_0)
			break;

		SYSTEMTIME st;
		GetLocalTime(&st);
		printf("Timer Signaled : %04d/%02d/%02d-%02d:%02d:%02d\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	}

	return 0;
}

void _tmain()
{
	g_hExit  = CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	DWORD dwThreadID = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, &dwThreadID);

	DWORD dwPeriod = 0;
	cout << "Enter period : ";
	cin >> dwPeriod;
	LARGE_INTEGER lUTC;
	lUTC.QuadPart = -((__int64)dwPeriod * 10000000LL);
	SetWaitableTimer(g_hTimer, &lUTC, dwPeriod * 1000, NULL, NULL, TRUE);

	char szIn[64];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
		{
			SetEvent(g_hExit);
			break;
		}
		if (stricmp(szIn, "stop") == 0)
		{
			CancelWaitableTimer(g_hTimer);
			//lUTC.QuadPart = MINLONGLONG;
			//SetWaitableTimer(g_hTimer, &lUTC, 0, NULL, NULL, TRUE);
			cout << "Timer canceled!!!" << endl;
		}
		else if (stricmp(szIn, "start") == 0)
		{
			SetWaitableTimer(g_hTimer, &lUTC, dwPeriod * 1000, NULL, NULL, TRUE);
			cout << "Timer restarted!!!" << endl;
		}
	}
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(g_hTimer);
	CloseHandle(g_hExit);
	CloseHandle(hThread);
}
