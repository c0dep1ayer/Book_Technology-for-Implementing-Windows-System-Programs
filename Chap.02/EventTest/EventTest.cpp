#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


DWORD WINAPI ThreadProc(LPVOID pParam)
{
	HANDLE hEvent = (HANDLE)pParam;

	DWORD dwWaitCode = WaitForSingleObject(hEvent, INFINITE);
	if (dwWaitCode == WAIT_FAILED)
	{
		cout << "WaitForSingleObject failed : " << GetLastError() << endl;
		return 0;
	}

	SYSTEMTIME st;
	GetLocalTime(&st);

	cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
	cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
	cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
	cout << st.wMilliseconds << endl;
	SetEvent(hEvent);

	return 0;
}

void _tmain(void)
{
	cout << "======= Start Event Test ========" << endl;
	HANDLE hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	HANDLE arhThreads[10];
	for (int i = 0; i < 10; i++)
	{
		DWORD dwTheaID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc, hEvent, 0, &dwTheaID);
	}
	WaitForMultipleObjects(10, arhThreads, TRUE, INFINITE);
	CloseHandle(hEvent);
	cout << "======= End Event Test ==========" << endl;
}

