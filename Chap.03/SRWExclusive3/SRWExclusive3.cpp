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
	cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";

	AcquireSRWLockExclusive(pSRW);
	cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';

	AcquireSRWLockExclusive(pSRW);
	cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
	cout << st.wMilliseconds << endl;

	ReleaseSRWLockExclusive(pSRW);
	ReleaseSRWLockExclusive(pSRW);
	ReleaseSRWLockExclusive(pSRW);

	return 0;
}

void _tmain(void)
{
	cout << "======= Start SRWExclusive2 Test ========" << endl;
	SRWLOCK srw;
	InitializeSRWLock(&srw);

	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, &srw, 0, &dwThrID);

	getchar();
	ReleaseSRWLockExclusive(&srw);
	getchar();
	ReleaseSRWLockExclusive(&srw);
	getchar();

	WaitForSingleObject(hThread, INFINITE);

	cout << "======= End SRWExclusive2 Test ==========" << endl;
}
