#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;

typedef void(WINAPI *PFN_WICB)(PVOID pParma);
class SemaThreadPool
{
	static DWORD WINAPI SemaWorkerProc(PVOID pParam);
	struct _WORK_ITEM
	{
		PVOID	 _pParam;
		PFN_WICB _pfnCB;

		_WORK_ITEM()
		{
			_pParam = NULL, _pfnCB = NULL;
		}
		_WORK_ITEM(PFN_WICB pfnCB, PVOID pParam)
		{
			_pParam = pParam, _pfnCB = pfnCB;
		}
	};
	typedef std::list<_WORK_ITEM> TP_QUE;

	HANDLE	m_hMutx;
	HANDLE	m_hSema;
	TP_QUE	m_queue;

	int		m_nThrCnt;
	PHANDLE	m_parhThrs;

	_WORK_ITEM Dequeue()
	{
		_WORK_ITEM wi;

		WaitForSingleObject(m_hMutx, INFINITE);
		TP_QUE::iterator it = m_queue.begin();
		wi = *it;
		m_queue.pop_front();
		ReleaseMutex(m_hMutx);

		return wi;
	}

public:
	void Init(int nWorkerCnt = 0)
	{
		m_hSema = CreateSemaphore(NULL, 0, INT_MAX, NULL);
		m_hMutx = CreateMutex(NULL, FALSE, NULL);

		m_nThrCnt = nWorkerCnt;
		if (m_nThrCnt == 0)
		{
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			m_nThrCnt = (int)si.dwNumberOfProcessors;
		}

		m_parhThrs = new HANDLE[m_nThrCnt];
		for (int i = 0; i < m_nThrCnt; i++)
		{
			DWORD dwThreadId;
			m_parhThrs[i] = CreateThread(NULL, 0, SemaWorkerProc, this, 0, &dwThreadId);
		}
	}

	void Uninit()
	{
		for (int i = 0; i < m_nThrCnt; i++)
			Enqueue(NULL, NULL);
		WaitForMultipleObjects(m_nThrCnt, m_parhThrs, TRUE, INFINITE);
		for (int i = 0; i < m_nThrCnt; i++)
			CloseHandle(m_parhThrs[i]);
		delete[] m_parhThrs;

		if (m_hMutx != NULL)
		{
			CloseHandle(m_hMutx);
			m_hMutx = NULL;
		}
		if (m_hSema != NULL)
		{
			CloseHandle(m_hSema);
			m_hSema = NULL;
		}
	}

	void Enqueue(PFN_WICB pfnCB, PVOID pParam)
	{
		_WORK_ITEM wi(pfnCB, pParam);

		WaitForSingleObject(m_hMutx, INFINITE);
		m_queue.push_back(wi);
		ReleaseMutex(m_hMutx);

		ReleaseSemaphore(m_hSema, 1, NULL);
	}
};

DWORD WINAPI SemaThreadPool::SemaWorkerProc(PVOID pParam)
{
	SemaThreadPool*	pTP = (SemaThreadPool*)pParam;

	while (true)
	{
		DWORD dwWaitCode = WaitForSingleObject(pTP->m_hSema, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		_WORK_ITEM wi = pTP->Dequeue();
		if (wi._pfnCB == NULL)
			break;

		__try
		{
			wi._pfnCB(wi._pParam);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			cout << "Unknown exception occurred : " << GetExceptionCode() << endl;
		}
	}
	return 0;
}



void WINAPI MyWorkerCallback(PVOID pParam)
{
	PCHAR pszStr = (PCHAR)pParam;
	DWORD dwThrId = GetCurrentThreadId();
	printf(" ==> Thread %d working : %s\n", dwThrId, pszStr);
	delete pszStr;
}

void _tmain()
{
	SemaThreadPool tp;
	tp.Init();

	char szIn[512];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		PCHAR pszStr = new char[strlen(szIn) + 1];
		strcpy(pszStr, szIn);
		tp.Enqueue(MyWorkerCallback, pszStr);
	}

	tp.Uninit();
}
