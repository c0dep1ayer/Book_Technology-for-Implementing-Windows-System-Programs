#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


struct IMyLock
{
	virtual PCSTR Name() = 0;
	virtual void Acquire() = 0;
	virtual void Release() = 0;

};
typedef IMyLock* PIMyLock;

class MyMutex : public IMyLock
{
	HANDLE m_hMutex;

public:
	MyMutex()
	{
		m_hMutex = CreateMutex(NULL, FALSE, NULL);
	}
	~MyMutex()
	{
		CloseHandle(m_hMutex);
	}

public:
	PCSTR Name() { return "MUTEX"; }

	void Acquire()
	{
		WaitForSingleObject(m_hMutex, INFINITE);
	}

	void Release()
	{
		ReleaseMutex(m_hMutex);
	}

};

class MyCS : public IMyLock
{
	CRITICAL_SECTION m_cs;

public:
	MyCS()
	{
		InitializeCriticalSection(&m_cs);
	}
	~MyCS()
	{
		DeleteCriticalSection(&m_cs);
	}

public:
	PCSTR Name() { return "CS"; }

	void Acquire()
	{
		EnterCriticalSection(&m_cs);
	}

	void Release()
	{
		LeaveCriticalSection(&m_cs);
	}

};


class MySRW : public IMyLock
{
	SRWLOCK m_srw;

public:
	MySRW()
	{
		InitializeSRWLock(&m_srw);
	}
	~MySRW()
	{
	}

public:
	PCSTR Name() { return "SRW"; }

	void Acquire()
	{
		AcquireSRWLockExclusive(&m_srw);
	}

	void Release()
	{
		ReleaseSRWLockExclusive(&m_srw);
	}

};

LONGLONG  g_llSharedValue = 0;
HANDLE	  g_hevStart;
HANDLE	  g_hevCompleted;
LONG	  g_nThreadCnt;

#define MAX_LOOP_CNT	10000
DWORD WINAPI PerfTestProc(LPVOID pParam)
{
	PIMyLock pLock = (PIMyLock)pParam;

	WaitForSingleObject(g_hevStart, INFINITE);
	for (int i = 0; i < MAX_LOOP_CNT; i++)
	{
		pLock->Acquire();
		g_llSharedValue++;
		pLock->Release();
	}
	if (InterlockedDecrement(&g_nThreadCnt) == 0)
		SetEvent(g_hevCompleted);

	return 0;
}

#define MAX_LOCK_CNT	10
void _tmain()
{
	g_hevStart = CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hevCompleted = CreateEvent(NULL, FALSE, FALSE, NULL);

	PIMyLock pLocks[] =
	{
		 new MyMutex(), new MyCS(), new MySRW()
	};

	LARGE_INTEGER st, et, freq;
	char szLogs[4096];
	int nLogLen = 0;

	cout << "press any key to start : ";
	getchar();

	int nEnvCnt = sizeof(pLocks) / sizeof(PIMyLock);
	for (int j = 0; j < nEnvCnt; j++)
	{
		nLogLen += sprintf(szLogs + nLogLen, "%s\t", pLocks[j]->Name());
		for (int k = 0; k < MAX_LOCK_CNT; k++)
		{
			ResetEvent(g_hevStart);
			g_llSharedValue = 0;
			int nThreadCnt = g_nThreadCnt = (2 << k);

			PHANDLE parThreads = new HANDLE[nThreadCnt];
			for (int i = 0; i < nThreadCnt; i++)
			{
				DWORD dwTheaID = 0;
				parThreads[i] = CreateThread(NULL, 0, PerfTestProc, pLocks[j], 0, &dwTheaID);
			}

			Sleep(500);
			cout << "==> Start Synch Test : " << nThreadCnt << ", " << pLocks[j]->Name() << endl;

			QueryPerformanceFrequency(&freq);
			QueryPerformanceCounter(&st);

			SignalObjectAndWait(g_hevStart, g_hevCompleted, INFINITE, FALSE);

			QueryPerformanceCounter(&et);

			__int64 elapsed = et.QuadPart - st.QuadPart;
			elapsed *= 1000000;
			elapsed /= freq.QuadPart;

			double ms = (double)elapsed / 1000.;
			printf("   Result : %I64d, Elapsed : %.3f\n\n", g_llSharedValue, ms);
			if (k < 8)
				nLogLen += sprintf(szLogs + nLogLen, "%.3f\t", ms);
			else
				nLogLen += sprintf(szLogs + nLogLen, "%.3f\n", ms);

			for (int i = 0; i < nThreadCnt; i++)
				CloseHandle(parThreads[i]);
			delete[] parThreads;
		}
	}
	for (int j = 0; j < nEnvCnt; j++)
		delete pLocks[j];

	cout << "===> TOTAL Result : " << endl;
	cout << szLogs << endl;

	cout << "======= Simulation terminated ========" << endl;
}
