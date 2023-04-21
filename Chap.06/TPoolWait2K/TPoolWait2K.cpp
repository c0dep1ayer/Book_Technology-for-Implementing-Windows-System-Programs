#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

VOID CALLBACK ThreadPoolWaitProc(PVOID pParam, BOOLEAN bTW)
{
	int nEvtIdx = (int)pParam;
	cout << " => Thread " << nEvtIdx << "(" 
		<< GetCurrentThreadId() << ") Signaled..." << endl;
	Sleep((nEvtIdx + 1) * 1000);
}

void _tmain()
{
	HANDLE arEvents[10];
	HANDLE arWaits[10];
	for (int i = 0; i < 10; i++)
	{
		arEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		RegisterWaitForSingleObject(&arWaits[i], arEvents[i],
			ThreadPoolWaitProc, (PVOID)i, INFINITE, WT_EXECUTEDEFAULT);
	}
	//cout << "Main thread creating sub thread..." << endl;

	for (int i = 0; i < 10; i++)
	{
		int evtIdx = rand() % 10;
		SetEvent(arEvents[evtIdx]);
		Sleep(1000);
	}

	//cout << "Sub thread " << dwThreadID << " terminated..." << endl;
	for (int i = 0; i < 10; i++)
	{
		UnregisterWait(arWaits[i]);
		CloseHandle(arEvents[i]);
	}
}
