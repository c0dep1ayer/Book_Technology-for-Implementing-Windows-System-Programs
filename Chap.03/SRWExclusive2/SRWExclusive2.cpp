#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


DWORD WINAPI ThreadProc(LPVOID pParam)
{
	PSRWLOCK pSRW = (PSRWLOCK)pParam;

	SYSTEMTIME st;
	GetLocalTime(&st);

	AcquireSRWLockExclusive(pSRW);
	{
		cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
		cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
		cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
		cout << st.wMilliseconds << endl;
	}
	//ReleaseSRWLockExclusive(pSRW);

	return 0;
}

void _tmain(void)
{
	cout << "======= Start SRWExclusive2 Test ========" << endl;
	SRWLOCK srw;
	InitializeSRWLock(&srw);
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
		ReleaseSRWLockExclusive(&srw);
	}
	WaitForMultipleObjects(10, arhThreads, TRUE, INFINITE);

	cout << "======= End SRWExclusive2 Test ==========" << endl;
}
