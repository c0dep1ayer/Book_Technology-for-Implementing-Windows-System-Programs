#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

DWORD WINAPI ThreadProc(PVOID pParam)
{
	DWORD dwDelay = (DWORD)pParam;

	cout << ">>>>> Thread " << GetCurrentThreadId() << " enter." << endl;
	Sleep(dwDelay);
	cout << "<<<<< Thread " << GetCurrentThreadId() << " leave." << endl;

	return dwDelay;
}

void _tmain()
{
	cout << "Main thread creating sub thread..." << endl;
	DWORD dwThreadID = 0;
	HANDLE hThread = CreateThread
	(
		NULL, 
		0, 
		ThreadProc, 
		(PVOID)5000, 
		0, 
		&dwThreadID
	);
	if (hThread == NULL)
	{
		cout << "~~~ CreateThread failed, error code : " 
			 << GetLastError() << endl;
		return;
	}

	WaitForSingleObject(hThread, INFINITE);
	
	DWORD dwExitCode = 0;
	GetExitCodeThread(hThread, &dwExitCode);
	cout << "Sub thread " << dwThreadID 
		 << " terminated with ExitCode " << dwExitCode << endl;
	CloseHandle(hThread);
}
