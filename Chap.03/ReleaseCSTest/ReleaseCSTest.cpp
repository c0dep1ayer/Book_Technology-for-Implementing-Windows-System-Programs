#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


DWORD WINAPI ThreadProc(LPVOID pParam)
{
	PCRITICAL_SECTION pCS = (PCRITICAL_SECTION)pParam;

	EnterCriticalSection(pCS);
	{
		SYSTEMTIME st;
		GetLocalTime(&st);

		cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
		cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
		cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
		cout << st.wMilliseconds << endl;
	}
	//LeaveCriticalSection(pSRW);

	return 0;
}

void _tmain(void)
{
	cout << "======= Start ReleaseCSTest Test ========" << endl;
	CRITICAL_SECTION cs;
	InitializeCriticalSection(&cs);

	HANDLE arhThreads[10];
	for (int i = 0; i < 10; i++)
	{
		DWORD dwTheaID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc, &cs/*&srw*/, 0, &dwTheaID);
	}

	for (int i = 0; i < 10; i++)
	{
		getchar();
		LeaveCriticalSection(&cs);
	}
	WaitForMultipleObjects(10, arhThreads, TRUE, INFINITE);
	DeleteCriticalSection(&cs);
	cout << "======= End ReleaseCSTest Test ==========" << endl;
}
