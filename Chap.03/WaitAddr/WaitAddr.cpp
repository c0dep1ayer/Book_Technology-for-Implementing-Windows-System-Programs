#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Synchronization.lib")


ULONG g_ulState = 0;

DWORD WINAPI ThreadProc(LPVOID)
{
	ULONG ulUndesire = 0;
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		WaitOnAddress(&g_ulState, &ulUndesire, sizeof(ULONG), INFINITE);
		if (g_ulState == 0)
			break;

		cout << "Th " << dwThrId << " => State : " << g_ulState << ", Undesire : " << ulUndesire << endl;
		if (g_ulState == 1)
			ulUndesire = g_ulState;
		else
			Sleep(1000);
	}

	return 0;
}

void _tmain()
{
	cout << "======= Start WaitAddr Test ========" << endl;

	HANDLE arhThreads[10];
	for (int i = 0; i < 10; i++)
	{
		DWORD dwTheaID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc, (PVOID)i, 0, &dwTheaID);
	}

	while (true)
	{
		cin >> g_ulState;
		if (g_ulState == 0)
		{
			WakeByAddressAll(&g_ulState);
			break;
		}
		WakeByAddressSingle(&g_ulState);
	}
	WaitForMultipleObjects(10, arhThreads, TRUE, INFINITE);

	cout << "======= End WaitAddr Test ==========" << endl;
}

