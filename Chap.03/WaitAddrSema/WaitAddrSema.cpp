#include "stdafx.h"
#include "Windows.h"
#include "string"
#include "list"
#include "iostream"
using namespace std;

#pragma comment(lib, "Synchronization.lib")


int	 g_nSleeps = 0;

typedef std::list<std::string> TP_QUE;
struct THREAD_POOL
{
	short	m_bMutx;
	LONG	m_lSema;
	TP_QUE	m_queue;

	THREAD_POOL()
	{
		m_lSema = 0;
		m_bMutx = FALSE;
	}
};
typedef THREAD_POOL* PTHREAD_POOL;

DWORD WINAPI PoolItemProc(LPVOID pParam)
{
	PTHREAD_POOL pTQ = (PTHREAD_POOL)pParam;
	DWORD dwThId = GetCurrentThreadId();

	LONG lCmp = 0; short bMuCmp = TRUE;
	while (true)
	{
		WaitOnAddress(&pTQ->m_lSema, &lCmp, sizeof(LONG), INFINITE);
		if (pTQ->m_lSema < 0)
			break;
		InterlockedDecrement(&pTQ->m_lSema);

		std::string item;
		while (InterlockedExchange16(&pTQ->m_bMutx, TRUE) == TRUE)
			WaitOnAddress(&pTQ->m_bMutx, &bMuCmp, sizeof(short), INFINITE);
		TP_QUE::iterator it = pTQ->m_queue.begin();
		item = *it;
		pTQ->m_queue.pop_front();
		InterlockedExchange16(&pTQ->m_bMutx, FALSE);
		WakeByAddressSingle(&pTQ->m_bMutx);

		printf(" => BEGIN | Thread %d works : %s\n", dwThId, item.c_str());
		Sleep((DWORD)((g_nSleeps++ % 3) + 1) * 1000);
		printf(" <= END   | Thread %d works : %s\n", dwThId, item.c_str());
	}

	printf("......PoolItemProc Thread %d Exit!!!\n", dwThId);

	return 0;
}

void _tmain()
{
	cout << "======= Start WaitAddrSema Test ========" << endl;

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	THREAD_POOL tp;

	PHANDLE parPoolThs = new HANDLE[si.dwNumberOfProcessors];
	for (int i = 0; i < (int)si.dwNumberOfProcessors; i++)
	{
		DWORD dwThreadId;
		parPoolThs[i] = CreateThread(NULL, 0, PoolItemProc, &tp, 0, &dwThreadId);
	}
	cout << "*** MAIN -> ThreadPool Started, count=" << si.dwNumberOfProcessors << endl;

	char szIn[1024];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		short bMuCmp = TRUE;
		while (InterlockedExchange16(&tp.m_bMutx, TRUE) == TRUE)
			WaitOnAddress(&tp.m_bMutx, &bMuCmp, sizeof(short), INFINITE);
		tp.m_queue.push_back(szIn);
		InterlockedExchange16(&tp.m_bMutx, FALSE);
		WakeByAddressSingle(&tp.m_bMutx);

		InterlockedIncrement(&tp.m_lSema);
		WakeByAddressSingle(&tp.m_lSema);
	}

	InterlockedExchange(&tp.m_lSema, -1);
	WakeByAddressAll(&tp.m_lSema);

	WaitForMultipleObjects(si.dwNumberOfProcessors, parPoolThs, TRUE, INFINITE);
	for (int i = 0; i < (int)si.dwNumberOfProcessors; i++)
		CloseHandle(parPoolThs[i]);

	cout << "======= End WaitAddrSema Test ==========" << endl;
}
