#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

class MyRWLock
{
	HANDLE	m_hevRO;
	LONG	m_lRefs;

	HANDLE	m_hevWO;
	HANDLE	m_hmuWO;

public:
	MyRWLock()
	{
		m_hevRO = NULL; m_lRefs = 0;
		m_hevWO = m_hmuWO = NULL;
	}
	~MyRWLock()
	{
		if (m_hevRO != NULL) CloseHandle(m_hevRO);
		if (m_hevWO != NULL) CloseHandle(m_hevWO);
		if (m_hmuWO != NULL) CloseHandle(m_hmuWO);
	}

public:
	HRESULT Init()
	{
		m_hevRO = CreateEvent(NULL, TRUE, TRUE, NULL);
		m_lRefs = 0;

		m_hevWO = CreateEvent(NULL, TRUE, TRUE, NULL);
		m_hmuWO = CreateMutex(NULL, FALSE, NULL);

		return S_OK;
	}

	void SharedLock()
	{
		WaitForSingleObject(m_hevWO, INFINITE);
		ResetEvent(m_hevRO);
		InterlockedIncrement(&m_lRefs);
	}
	void SharedUnlock()
	{
		if (InterlockedDecrement(&m_lRefs) == 0)
			SetEvent(m_hevRO);
	}

	void ExclusiveLock()
	{
		ResetEvent(m_hevWO);
		WaitForSingleObject(m_hevRO, INFINITE);
		WaitForSingleObject(m_hmuWO, INFINITE);
	}

	void ExclusiveUnlock()
	{
		ReleaseMutex(m_hmuWO);
		SetEvent(m_hevWO);
	}
};


#define DELAY_WRITE	500
#define DELAY_READ	2000
LONG	g_nValue;
LONG	g_bIsExit;

DWORD WINAPI ReaderProc(PVOID pParam)
{
	MyRWLock* prwl = (MyRWLock*)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (g_bIsExit == 0)
	{
		prwl->SharedLock();
		{
			cout << "    => TH " << dwThrId << " Reads : " << g_nValue << endl;
		}
		prwl->SharedUnlock();

		Sleep(rand() % DELAY_READ);
	}

	return 0;
}

DWORD WINAPI WriterProc(PVOID pParam)
{
	MyRWLock* prwl = (MyRWLock*)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (g_bIsExit == 0)
	{
		Sleep(rand() % DELAY_WRITE);

		prwl->ExclusiveLock();
		{
			g_nValue++;
			cout << " <= TH " << dwThrId << " writes value : " << g_nValue << endl;
		}
		prwl->ExclusiveUnlock();
	}

	return 0;
}

#define MAX_READ_CNT	5
#define MAX_WRITE_CNT	2
void _tmain()
{
	MyRWLock rwl;
	rwl.Init();

	HANDLE woThrs[MAX_WRITE_CNT];
	for (int i = 0; i < MAX_WRITE_CNT; i++)
	{
		DWORD dwThrId = 0;
		woThrs[i] = CreateThread(NULL, 0, WriterProc, &rwl, 0, &dwThrId);
	}
	HANDLE roThrs[MAX_READ_CNT];
	for (int i = 0; i < MAX_READ_CNT; i++)
	{
		DWORD dwThrId = 0;
		roThrs[i] = CreateThread(NULL, 0, ReaderProc, &rwl, 0, &dwThrId);
	}

	getchar();
	g_bIsExit = TRUE;

	WaitForMultipleObjects(MAX_READ_CNT , roThrs, TRUE, INFINITE);
	WaitForMultipleObjects(MAX_WRITE_CNT, woThrs, TRUE, INFINITE);
}

