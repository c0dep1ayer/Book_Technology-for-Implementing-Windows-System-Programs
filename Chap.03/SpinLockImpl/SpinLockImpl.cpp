#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


struct SpinWait
{
	struct _SYSINFO : public SYSTEM_INFO
	{
		_SYSINFO() { GetSystemInfo(this); }
	};
	static _SYSINFO s_si;

	const int YIELD_THRESHOLD = 25;
	const int SLEEP_0_TIMES = 2;
	const int SLEEP_1_TIMES = 10;
	const int MAX_SPIN_INTERVAL = 32;

	const int STEP = 5;
	const int MAX_TIME = 50;

	int m_count;

	SpinWait()
	{
		m_count = 0;
	}

#define NextSpinWillYield	(s_si.dwNumberOfProcessors == 1 || m_count >= YIELD_THRESHOLD)

	void SpinOnce()
	{
		if (NextSpinWillYield)
		{
			int yieldSoFar = (m_count >= YIELD_THRESHOLD ? m_count - YIELD_THRESHOLD : m_count);
			if ((yieldSoFar % SLEEP_1_TIMES) == SLEEP_0_TIMES - 1)
				Sleep(0);
			else if ((yieldSoFar % SLEEP_1_TIMES) == SLEEP_1_TIMES - 1)
				Sleep(1);
			else
				SwitchToThread();
		}
		else
		{
			int nLoopCnt = (int)(m_count * ((float)MAX_SPIN_INTERVAL / YIELD_THRESHOLD)) + 1;
			while (nLoopCnt-- > 0)
				YieldProcessor();
		}
		m_count = ((m_count == INT_MAX) ? YIELD_THRESHOLD : m_count + 1);
	}

	void Reset() { m_count = 0; }
	int Count() { return m_count; }
};
SpinWait::_SYSINFO SpinWait::s_si;


class SpinLock
{
	volatile DWORD m_state;
	const LONG LOCK_AVAIL = 0;

public:
	SpinLock()
	{
		m_state = LOCK_AVAIL;
	}

public:
	void Acquire()
	{
		DWORD dwCurThrId = GetCurrentThreadId();
		if (dwCurThrId == m_state)
			throw HRESULT_FROM_WIN32(ERROR_INVALID_OWNER);

		if (InterlockedCompareExchange(&m_state, dwCurThrId, LOCK_AVAIL) != LOCK_AVAIL)
		{
			SpinWait sw;
			do
			{
				do
				{
					sw.SpinOnce();
				}
				while (m_state != 0);
			}
			while (InterlockedCompareExchange(&m_state, dwCurThrId, LOCK_AVAIL) != LOCK_AVAIL);
		}
	}

	void Release()
	{
		DWORD dwCurThrId = GetCurrentThreadId();
		if (dwCurThrId != m_state)
			throw HRESULT_FROM_WIN32(ERROR_INVALID_OWNER);

		InterlockedExchange(&m_state, LOCK_AVAIL);
	}
};


LONGLONG  g_llSharedValue = 0;
#define MAX_LOOP_CNT	10000

DWORD WINAPI SpinLockProc(LPVOID pParam)
{
	SpinLock* pLock = (SpinLock*)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	try
	{
		for (int i = 0; i < MAX_LOOP_CNT; i++)
		{
			pLock->Acquire();
			g_llSharedValue++;
			cout << " SubT_After \t" << dwThrId << "\t: " << g_llSharedValue << endl;
			pLock->Release();
		}
	}
	catch (HRESULT hr)
	{
		printf("!!!! Thread %d stopped by error : %08X\n", hr);
	}

	return 0;
}

#define MAX_THR_CNT	50
void _tmain()
{
	SpinLock lock;
	SRWLOCK srw;
	InitializeSRWLock(&srw);

	HANDLE arhThrs[MAX_THR_CNT];
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwTheaID = 0;
		arhThrs[i] = CreateThread(NULL, 0, SpinLockProc, &lock, 0, &dwTheaID);
	}

	getchar();

	for (int i = 0; i < MAX_THR_CNT; i++)
		CloseHandle(arhThrs[i]);

	cout << "======= Simulation terminated ========" << endl;
}
