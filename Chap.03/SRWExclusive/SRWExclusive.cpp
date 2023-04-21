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
	ReleaseSRWLockExclusive(pSRW);

	return 0;
}

void _tmain(void)
{
	cout << "======= Start CriticalSection Test ========" << endl;
	SRWLOCK srw;
	InitializeSRWLock(&srw);

	HANDLE arhThreads[10];
	for (int i = 0; i < 10; i++)
	{
		DWORD dwTheaID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc, &srw, 0, &dwTheaID);
	}
	WaitForMultipleObjects(10, arhThreads, TRUE, INFINITE);

	cout << "======= End CriticalSection Test ==========" << endl;
}
