#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


DWORD WINAPI ThreadProc(PVOID pParam)
{
	DWORD dwDelay = (DWORD)pParam * 1000;

	cout << ">>>>> Thread " << GetCurrentThreadId() << " enter." << endl;
	Sleep(dwDelay);
	cout << "<<<<< Thread " << GetCurrentThreadId() << " leave." << endl;

	return 0;
}

#define MAX_THR_CNT	 5
void _tmain()
{
	cout << "Main thread creating sub thread..." << endl;

	HANDLE arhThreads[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwThreadID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc, (PVOID)(i + 3), 0, &dwThreadID);
	}

	WaitForMultipleObjects(MAX_THR_CNT, arhThreads, TRUE, INFINITE);

	cout << "All sub threads terminated......" << endl;
	for (int i = 0; i < MAX_THR_CNT; i++)
		CloseHandle(arhThreads[i]);
}

