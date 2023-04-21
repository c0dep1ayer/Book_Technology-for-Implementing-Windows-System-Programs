#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


DWORD WINAPI ThreadProc(PVOID pParam)
{
	DWORD dwDelay = (DWORD)pParam;

	cout << ">>>>> Thread " << GetCurrentThreadId() << " enter." << endl;
	Sleep(5000);
	cout << "<<<<< Thread " << GetCurrentThreadId() << " leave." << endl;

	return dwDelay;
}

void _tmain()
{
	cout << "Main thread creating sub thread..." << endl;

	DWORD dwThreadID = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, (PVOID)5000, 0, &dwThreadID);

	Sleep(10000);

	DWORD dwExitCode;
	GetExitCodeThread(hThread, &dwExitCode);
	cout << "Sub thread " << dwThreadID << " terminated. ";
	cout << "Exit code=" << dwExitCode << endl;
	CloseHandle(hThread);
}

