#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


class MyLock
{
	CRITICAL_SECTION m_cs;

public:
	MyLock()
	{
		InitializeCriticalSection(&m_cs);
	}
	~MyLock()
	{
		DeleteCriticalSection(&m_cs);
	}

	void Lock()
	{
		EnterCriticalSection(&m_cs);
	}
	void Unlock()
	{
		LeaveCriticalSection(&m_cs);
	}
};

class MyAutoLock
{
	PCRITICAL_SECTION m_pcs;

protected:
	MyAutoLock() { m_pcs = NULL; }

public:
	static void Init(PCRITICAL_SECTION pcs)
	{
		InitializeCriticalSection(pcs);
	}
	static void Delete(PCRITICAL_SECTION pcs)
	{
		DeleteCriticalSection(pcs);
	}

public:
	MyAutoLock(PCRITICAL_SECTION pcs)
	{
		EnterCriticalSection(pcs);
		m_pcs = pcs;
	}
	~MyAutoLock()
	{
		LeaveCriticalSection(m_pcs);
	}
};


void PrintDateTime(PCRITICAL_SECTION pCS, PSYSTEMTIME pst)
{
	MyAutoLock lock(pCS);

	cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
	cout << pst->wYear << '/' << pst->wMonth << '/' << pst->wDay << ' ';
	cout << pst->wHour << ':' << pst->wMinute << ':' << pst->wSecond << '+';
	cout << pst->wMilliseconds << endl;
}

DWORD WINAPI ThreadProc3(LPVOID pParam)
{
	PCRITICAL_SECTION pCS = (PCRITICAL_SECTION)pParam;

	SYSTEMTIME st;
	GetLocalTime(&st);

	{
		MyAutoLock lock(pCS);
		cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
		cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
		if (st.wMilliseconds > 500)
		{
			cout << endl;
			return 0;
		}
		cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
		cout << st.wMilliseconds << endl;
	}

	return 0;
}

DWORD WINAPI ThreadProc2(LPVOID pParam)
{
	PCRITICAL_SECTION pCS = (PCRITICAL_SECTION)pParam;

	SYSTEMTIME st;
	GetLocalTime(&st);

	PrintDateTime(pCS, &st);

	return 0;
}

DWORD WINAPI ThreadProc1(LPVOID pParam)
{
	MyLock* pLock = (MyLock*)pParam;

	SYSTEMTIME st;
	GetLocalTime(&st);

	pLock->Lock();
	{
		cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
		cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
		cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
		cout << st.wMilliseconds << endl;
	}
	pLock->Unlock();

	return 0;
}

void _tmain(void)
{
	cout << "======= Start MyCSLock Test ========" << endl;
	HANDLE arhThreads[10];

	MyLock lock;
	for (int i = 0; i < 10; i++)
	{
		DWORD dwThrID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc1, &lock, 0, &dwThrID);
	}
	WaitForMultipleObjects(10, arhThreads, TRUE, INFINITE);

	int stop;
	cin >> stop;

	CRITICAL_SECTION cs;
	MyAutoLock::Init(&cs);
	for (int i = 0; i < 10; i++)
	{
		DWORD dwThrID = 0;
		arhThreads[i] = CreateThread(NULL, 0, ThreadProc3, &cs, 0, &dwThrID);
	}
	WaitForMultipleObjects(10, arhThreads, TRUE, INFINITE);
	MyAutoLock::Delete(&cs);

	cout << "======= End MyCSLock Test ==========" << endl;
}

