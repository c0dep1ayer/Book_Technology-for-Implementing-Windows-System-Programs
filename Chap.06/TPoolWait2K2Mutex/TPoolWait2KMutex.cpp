#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


VOID CALLBACK ThreadPoolWaitProc(PVOID pParam, BOOLEAN bTW)
{
	HANDLE hMutex = (HANDLE)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	printf(" => Thread %d enters into WAIT while 2 seconds...\n", dwThrId);
	Sleep(2000);
	printf(" => Callback %d terminated...\n", dwThrId);
	if (!ReleaseMutex(hMutex))
		cout << "ReleaseMutex error : " << GetLastError() << endl;
}

void _tmain()
{
	HANDLE hMutex = CreateMutex(NULL, TRUE, NULL);
	HANDLE hWait = NULL;
	BOOL bIsOK = RegisterWaitForSingleObject
	(
		&hWait, hMutex, 
		ThreadPoolWaitProc, hMutex,	INFINITE, 
		WT_EXECUTEDEFAULT
		//WT_EXECUTEONLYONCE
		//WT_EXECUTEINWAITTHREAD 
	);

	cout << "Enter any key to signal event..." << endl;
	getchar();
	ReleaseMutex(hMutex);
	Sleep(1000);
	WaitForSingleObject(hMutex, INFINITE);

	UnregisterWaitEx(hWait, INVALID_HANDLE_VALUE); 
	CloseHandle(hMutex);
}
