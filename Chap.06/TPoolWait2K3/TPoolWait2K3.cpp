#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


VOID CALLBACK ThreadPoolWaitProc(PVOID pParam, BOOLEAN bTW)
{
	HANDLE phReg = (HANDLE)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	printf(" => Thread %d signaled...\n", dwThrId);
	CloseHandle(phReg);
	Sleep(10000);
	//UnregisterWaitEx(*phReg, INVALID_HANDLE_VALUE);
	printf(" => Callback %d terminated...\n", dwThrId);
}

void _tmain()
{
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	HANDLE hWait = NULL;
	BOOL bIsOK = RegisterWaitForSingleObject
	(
		&hWait, hEvent,
		ThreadPoolWaitProc, hEvent,
		INFINITE, WT_EXECUTEDEFAULT
	);

	cout << "Enter any key to signal event..." << endl;
	getchar();
	SetEvent(hEvent);

	cout << "Enter a key to call UnregisterWaitEx..." << endl;
	getchar();
	UnregisterWaitEx(hWait, INVALID_HANDLE_VALUE);
	INT K = 0;
}
