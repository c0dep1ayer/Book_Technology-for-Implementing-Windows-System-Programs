#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


VOID CALLBACK ThreadPoolWaitProc(PVOID pParam, BOOLEAN bTW)
{
	DWORD dwDelay = (DWORD)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	printf(" => Thread %d enters into WAIT while %d seconds...\n", dwThrId, dwDelay);
	Sleep(dwDelay * 1000);
	printf(" => Callback %d terminated...\n", dwThrId);
}

void _tmain()
{
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	HANDLE hWait = NULL;
	BOOL bIsOK = RegisterWaitForSingleObject
	(
		&hWait, hEvent,
		ThreadPoolWaitProc, (PVOID)50,
		INFINITE, WT_EXECUTEDEFAULT
	);

	cout << "Enter any key to signal event..." << endl;
	getchar();
	SetEvent(hEvent);

	cout << "Enter a key to call UnregisterWaitEx..." << endl;
	int ch = getchar();
	if (ch == '1')
		UnregisterWaitEx(hWait, INVALID_HANDLE_VALUE); 
	else if (ch == '0')
	{
		if (!UnregisterWaitEx(hWait, NULL))
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode == ERROR_IO_PENDING)
				cout << "Pending callback exists..." << endl;
			else
				cout << "UnregisterWaitEx failed : " << dwErrCode << endl;
		}
	}
	else
	{
		HANDLE hevComp = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!UnregisterWaitEx(hWait, hevComp))
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
			{
				cout << "UnregisterWaitEx failed : " << dwErrCode << endl;
				return;
			}
		}
		cout << " ...Waits for UnregisterWaitEx completion..." << endl;
		WaitForSingleObject(hevComp, INFINITE);
	}
	cout << "Main thread : UnregisterWaitEx completed..." << endl;
	CloseHandle(hEvent);
}
