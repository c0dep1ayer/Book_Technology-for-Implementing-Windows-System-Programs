#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

SRWLOCK	g_srw;
int		g_nValue;
LONG	g_bIsExit;

DWORD WINAPI ThreadProc(PVOID pParam)
{
	DWORD dwToVal = *((DWORD*)pParam);

	while (g_bIsExit == 0)
	{
		AcquireSRWLockShared(&g_srw);
		cout << "....TH " << dwToVal << " Read : " << g_nValue << endl;
		ReleaseSRWLockShared(&g_srw);
		Sleep(dwToVal * 1000);
	}

	return 0;
}

#define MAX_THR_CNT	5
void _tmain()
{
	InitializeSRWLock(&g_srw);

	DWORD ardwWaits[] = { 1, 2, 3, 4, 5 };
	HANDLE roThrs[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwTheaID = 0;
		roThrs[i] = CreateThread(NULL, 0, ThreadProc, &ardwWaits[i], 0, &dwTheaID);
	}

	char szCmd[512];
	while (true)
	{
		cin >> szCmd;
		if (strcmpi(szCmd, "quit") == 0)
		{
			InterlockedExchange(&g_bIsExit, TRUE);
			break;
		}

		int val = atoi(szCmd);
		if (val >= 0)
		{
			AcquireSRWLockExclusive(&g_srw);
			g_nValue = val;
			cout << "MAIN TH Change value : " << g_nValue << endl;
			Sleep(2000);
			ReleaseSRWLockExclusive(&g_srw);
		}
	}

	WaitForMultipleObjects(MAX_THR_CNT, roThrs, TRUE, INFINITE);
}
