#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


DWORD WINAPI ThreadProc(LPVOID pParam)
{
	HANDLE hMutex = (HANDLE)pParam;

	DWORD dwWaitCode = WaitForSingleObject(hMutex, INFINITE);
	if (dwWaitCode == WAIT_FAILED)
	{
		cout << "WaitForSingleObject failed : " << GetLastError() << endl;
		return 0;
	}
	if (dwWaitCode == WAIT_ABANDONED)
		cout << "Abandoned Mutex acquired!!!" << GetLastError() << endl;

	SYSTEMTIME st;
	GetLocalTime(&st);

	cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
	cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
	cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
	cout << st.wMilliseconds << endl;
	ReleaseMutex(hMutex);

	return 0;
}

#define MAX_THR_CNT	10
void _tmain(void)
{
	cout << "======= Start Mutex Test ========" << endl;
	HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);

	HANDLE arhThreads[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwTheaID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc, hMutex, 0, &dwTheaID);
	}

	//if (ReleaseMutex(hMutex) == FALSE)
	//	printf("~~~ ReleaseMutex error, code=%d\n", GetLastError());

	WaitForMultipleObjects(MAX_THR_CNT, arhThreads, TRUE, INFINITE);
	for (int i = 0; i < MAX_THR_CNT; i++)
		CloseHandle(arhThreads[i]);
	CloseHandle(hMutex);
	cout << "======= End Mutex Test ==========" << endl;
}

