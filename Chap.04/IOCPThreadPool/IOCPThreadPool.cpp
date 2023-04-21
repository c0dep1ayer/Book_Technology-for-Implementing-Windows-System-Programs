#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


typedef void(WINAPI *PFN_WICB)(PVOID pParma);
class IocpThreadPool
{
	static DWORD WINAPI IocpWorkerProc(PVOID pParam);

	HANDLE	m_hIocp;
	int		m_nThrCnt;
	PHANDLE	m_parhThrs;

public:
	void Init(int nMaxCnt = 0, int nConcurrentCnt = 0)
	{
		m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, (DWORD)nConcurrentCnt);

		m_nThrCnt = nMaxCnt;
		if (m_nThrCnt == 0)
		{
			if (nConcurrentCnt > 0)
				m_nThrCnt = nConcurrentCnt * 2;
			else
			{
				SYSTEM_INFO si;
				GetSystemInfo(&si);
				m_nThrCnt = (int)si.dwNumberOfProcessors * 2;
			}
		}
		m_parhThrs = new HANDLE[m_nThrCnt];
		for (int i = 0; i < m_nThrCnt; i++)
		{
			DWORD dwThreadId;
			m_parhThrs[i] = CreateThread(NULL, 0, IocpWorkerProc, this, 0, &dwThreadId);
		}
	}

	void Uninit()
	{
		for (int i = 0; i < m_nThrCnt; i++)
			PostQueuedCompletionStatus(m_hIocp, 0, NULL, NULL);

		WaitForMultipleObjects(m_nThrCnt, m_parhThrs, TRUE, INFINITE);
		for (int i = 0; i < m_nThrCnt; i++)
			CloseHandle(m_parhThrs[i]);
		delete[] m_parhThrs;

		if (m_hIocp != NULL)
		{
			CloseHandle(m_hIocp);
			m_hIocp = NULL;
		}
	}

	void Enqueue(PFN_WICB pfnCB, PVOID pParam)
	{
		PostQueuedCompletionStatus
		(
			m_hIocp, 0, (ULONG_PTR)pfnCB, (LPOVERLAPPED)pParam
		);
	}
};

DWORD WINAPI IocpThreadPool::IocpWorkerProc(PVOID pParam)
{
	IocpThreadPool*	pTP = (IocpThreadPool*)pParam;
	LPOVERLAPPED	pov = NULL;
	DWORD			dwTrBytes = 0;
	ULONG_PTR		ulKey = 0;

	while (true)
	{
		BOOL bIsOK = GetQueuedCompletionStatus
		(
			pTP->m_hIocp, &dwTrBytes, &ulKey, &pov, INFINITE
		);
		if (!bIsOK)
			break;

		if (ulKey == 0)
			break;

		PFN_WICB pfnCB  = (PFN_WICB)ulKey;
		PVOID    pParam = (PVOID)pov;
		__try
		{
			pfnCB(pParam);
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
	IocpThreadPool tp;
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
