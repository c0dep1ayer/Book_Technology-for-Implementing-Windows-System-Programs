#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;


typedef void(WINAPI *PFN_WICB)(PVOID pParma);
class WaitableQueue
{
	static DWORD WINAPI QueueWorkerProc(PVOID pParam);
	struct _WQUE_ITEM
	{
		PVOID	 _pParam;
		PFN_WICB _pfnCB;
	};

	typedef std::list<_WQUE_ITEM> ITEM_QUE;
	ITEM_QUE m_queue;
	HANDLE	 m_hSema;
	HANDLE	 m_hMutx;
	HANDLE	 m_hThread;

public:
	void Start(int nMaxCnt = INT_MAX)
	{
		m_hSema = CreateSemaphore(NULL, 0, nMaxCnt, NULL);
		m_hMutx = CreateMutex(NULL, FALSE, NULL);

		DWORD dwThreadId;
		m_hThread = CreateThread(NULL, 0, QueueWorkerProc, this, 0, &dwThreadId);
	}

	void Stop()
	{
		Enque(NULL, NULL);
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		CloseHandle(m_hMutx);
		CloseHandle(m_hSema);
	}

	void Enque(PFN_WICB pfnCB, PVOID pParam)
	{
		_WQUE_ITEM wqi;
		wqi._pfnCB = pfnCB;
		wqi._pParam = pParam;

		WaitForSingleObject(m_hMutx, INFINITE);
		m_queue.push_back(wqi);
		ReleaseMutex(m_hMutx);

		ReleaseSemaphore(m_hSema, 1, NULL);
	}
};

DWORD WINAPI WaitableQueue::QueueWorkerProc(PVOID pParam)
{
	WaitableQueue* pWQ = (WaitableQueue*)pParam;

	while (true)
	{
		DWORD dwWaitCode = WaitForSingleObject(pWQ->m_hSema, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			DWORD dwErrCode = GetLastError();
			break;
		}

		_WQUE_ITEM wqi;
		WaitForSingleObject(pWQ->m_hMutx, INFINITE);
		ITEM_QUE::iterator it = pWQ->m_queue.begin();
		wqi = *it;
		pWQ->m_queue.pop_front();
		ReleaseMutex(pWQ->m_hMutx);

		if (wqi._pfnCB == NULL)
			break;
		wqi._pfnCB(wqi._pParam);
	}
	return 0;
}


void WINAPI QueueItemWorkProc(PVOID pParam)
{
	int nWait = (int)pParam;
	cout << " => Thread " << nWait << "(" << GetCurrentThreadId() << ") Signaled..." << endl;
	Sleep(nWait * 1000);
}

void _tmain()
{
	WaitableQueue wq;
	wq.Start();

	char szIn[64];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		int nWait = atoi(szIn);
		if (nWait <= 0 || nWait > 10)
		{
			cout << "Invalid wait time seconds..." << endl;
			continue;
		}

		wq.Enque(QueueItemWorkProc, (PVOID)nWait);
	}

	wq.Stop();
}
