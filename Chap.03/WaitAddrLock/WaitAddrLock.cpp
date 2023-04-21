#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Synchronization.lib")

#define PERF_TEST1

#ifndef PERF_TEST

DWORD WINAPI ThreadProc(PVOID pParam)
{
	PLONG pbIsLocked = (PLONG)pParam;

	LONG lCmpVal = TRUE;
	while (InterlockedExchange(pbIsLocked, TRUE) == TRUE)
		WaitOnAddress(pbIsLocked, &lCmpVal, sizeof(LONG), INFINITE);
	{
		SYSTEMTIME st;
		GetLocalTime(&st);

		cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
		cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
		cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
		cout << st.wMilliseconds << endl;
	}
	InterlockedExchange(pbIsLocked, FALSE);
	WakeByAddressSingle(pbIsLocked);

	return 0;
}

#define MAX_THR_CNT	20
void _tmain()
{
	cout << "======= Start WaitAddrLock Test ========" << endl;
	LONG bIsLocked = FALSE;

	HANDLE arhThrs[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwTheaID = 0;
		arhThrs[i] = CreateThread(NULL, 0, ThreadProc, &bIsLocked, 0, &dwTheaID);
	}
	WaitForMultipleObjects(MAX_THR_CNT, arhThrs, TRUE, INFINITE);

	for (int i = 0; i < MAX_THR_CNT; i++)
		CloseHandle(arhThrs[i]);
	cout << "======= End WaitAddrLock Test ==========" << endl;
}

#else
struct IMyLock
{
	virtual PCSTR Name() = 0;
	virtual void Acquire() = 0;
	virtual void Release() = 0;

};
typedef IMyLock* PIMyLock;

class WOALock : public IMyLock
{
	LONG m_state;

public:
	WOALock()
	{
		m_state = TRUE;
	}

public:
	PCSTR Name()
	{
		return "WOA";
	}

	void Acquire()
	{
		LONG cmpVal = FALSE;
		while (InterlockedExchange(&m_state, FALSE) == FALSE)
			WaitOnAddress(&m_state, &cmpVal, sizeof(LONG), 0);
	}

	void Release()
	{
		InterlockedExchange(&m_state, TRUE);
	}
};

class WOALock2 : public IMyLock
{
	LONG m_state;

public:
	WOALock2()
	{
		m_state = TRUE;
	}

public:
	PCSTR Name()
	{
		return "WOA2";
	}

	void Acquire()
	{
		LONG cmpVal = FALSE;
		while (InterlockedExchange(&m_state, FALSE) == FALSE)
			WaitOnAddress(&m_state, &cmpVal, sizeof(LONG), 1);
	}

	void Release()
	{
		InterlockedExchange(&m_state, TRUE);
	}
};

class WOALock3 : public IMyLock
{
	LONG m_state;

public:
	WOALock3()
	{
		m_state = TRUE;
	}

public:
	PCSTR Name()
	{
		return "WOA3";
	}

	void Acquire()
	{
		LONG cmpVal = FALSE;
		while (InterlockedExchange(&m_state, FALSE) == FALSE)
			WaitOnAddress(&m_state, &cmpVal, sizeof(LONG), INFINITE);
	}

	void Release()
	{
		m_state = TRUE;
		WakeByAddressSingle(&m_state);
	}
};

enum SPIN_OP
{
	YIELD, SWITCH, SLEEP
};
class SpinLock2 : public IMyLock
{
	const LONG LOCK_AVAILABLE = 0;
	SPIN_OP m_spOp;
	volatile LONG m_state;

public:
	SpinLock2(SPIN_OP so)
	{
		m_spOp = so;
		m_state = LOCK_AVAILABLE;
	}

public:
	PCSTR Name()
	{
		switch (m_spOp)
		{
		case SPIN_OP::SLEEP: return "SLEEP";
		case SPIN_OP::SWITCH: return "SWITCH";
		case SPIN_OP::YIELD: return "YIELD";
		}
		return "SPLOCK";
	}

	void Acquire()
	{
		DWORD dwCurThrId = GetCurrentThreadId();
		if (dwCurThrId == m_state)
			throw HRESULT_FROM_WIN32(ERROR_INVALID_OWNER);

		while (InterlockedCompareExchange(&m_state, dwCurThrId, LOCK_AVAILABLE) != LOCK_AVAILABLE)
		{
			LONG lCmp = m_state;
			switch (m_spOp)
			{
			case SPIN_OP::SLEEP: Sleep(0); break;
			case SPIN_OP::SWITCH: SwitchToThread(); break;
			case SPIN_OP::YIELD: YieldProcessor(); break;
			}
		}
	}

	void Release()
	{
		DWORD dwCurThrId = GetCurrentThreadId();
		if (dwCurThrId != m_state)
			throw HRESULT_FROM_WIN32(ERROR_INVALID_OWNER);

		InterlockedExchange(&m_state, LOCK_AVAILABLE);
	}
};

class SpinLock3 : public IMyLock
{
	const LONG LOCK_AVAILABLE = 0;
	const int YIELD_THRESHOLD = 25;
	volatile LONG m_state;

public:
	SpinLock3()
	{
		m_state = LOCK_AVAILABLE;
	}

public:
	PCSTR Name()
	{
		return "SP_LOCK";
	}

	void Acquire()
	{
		DWORD dwCurThrId = GetCurrentThreadId();
		if (dwCurThrId == m_state)
			throw HRESULT_FROM_WIN32(ERROR_INVALID_OWNER);

		int count = 0;
		while (InterlockedCompareExchange(&m_state, dwCurThrId, LOCK_AVAILABLE) != LOCK_AVAILABLE)
		{
			if (count < YIELD_THRESHOLD)
			{
				YieldProcessor();
				count++;
			}
			else
			{
				Sleep(0);
				count = 0;
			}
		}
	}

	void Release()
	{
		DWORD dwCurThrId = GetCurrentThreadId();
		if (dwCurThrId != m_state)
			throw HRESULT_FROM_WIN32(ERROR_INVALID_OWNER);

		InterlockedExchange(&m_state, LOCK_AVAILABLE);
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
	try
	{
		for (int i = 0; i < MAX_LOOP_CNT; i++)
		{
			pLock->Acquire();
			g_llSharedValue++;
			pLock->Release();
		}
	}
	catch (HRESULT hr)
	{
		printf("!!!! Thread %d stopped by error : %08X\n", hr);
		SetEvent(g_hevCompleted);
		return 0;
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
		/*new SpinLock3(), */new SpinLock2(SLEEP), /*new WOALock(), */new WOALock2(), new WOALock3(),
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
			if (k < MAX_LOCK_CNT - 1)
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

#endif