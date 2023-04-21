#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

struct ERWLOCK
{
	HANDLE	_hevRO, _hevWO;
	LONG	_nRoRefs;

	ERWLOCK()
	{
		_hevRO = _hevWO = NULL;
		_nRoRefs = 0;
	}
	~ERWLOCK()
	{
		if (_hevRO != NULL) CloseHandle(_hevRO);
		if (_hevWO != NULL) CloseHandle(_hevWO);
	}
};

ERWLOCK g_rwl;
int		g_nValue;
LONG	g_bIsExit;

DWORD WINAPI ReaderProc(PVOID pParam)
{
	DWORD dwToVal = *((DWORD*)pParam);

	while (g_bIsExit == 0)
	{
		WaitForSingleObject(g_rwl._hevWO, INFINITE);
		{
			ResetEvent(g_rwl._hevRO);
			InterlockedIncrement(&g_rwl._nRoRefs);

			cout << "....TH " << dwToVal << " Read : " << g_nValue << endl;
			
			if (InterlockedDecrement(&g_rwl._nRoRefs) == 0)
				SetEvent(g_rwl._hevRO);
		}
		Sleep(dwToVal * 200);
	}

	return 0;
}

#define MAX_THR_CNT	5
void _tmain()
{
	g_rwl._hevRO = CreateEvent(NULL, TRUE, TRUE, NULL);
	g_rwl._hevWO = CreateEvent(NULL, TRUE, TRUE, NULL);

	DWORD ardwWaits[] = { 1, 2, 3, 4, 5 };
	HANDLE roThrs[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwThrId = 0;
		roThrs[i] = CreateThread(NULL, 0, ReaderProc, &ardwWaits[i], 0, &dwThrId);
	}

	char szCmd[512];
	while (true)
	{
		cin >> szCmd;
		if (_stricmp(szCmd, "quit") == 0)
		{
			g_bIsExit = TRUE;
			break;
		}

		int val = atoi(szCmd);
		if (val >= 0)
		{
			ResetEvent(g_rwl._hevWO);
			WaitForSingleObject(g_rwl._hevRO, INFINITE);
			{
				g_nValue = val;
				cout << "MAIN TH Change value : " << g_nValue << endl;
			}
			SetEvent(g_rwl._hevWO);
		}
	}

	WaitForMultipleObjects(MAX_THR_CNT, roThrs, TRUE, INFINITE);
}

