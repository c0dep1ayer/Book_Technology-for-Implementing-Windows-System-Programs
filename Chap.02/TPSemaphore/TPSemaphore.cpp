#include "stdafx.h"
#include "Windows.h"
#include "string"
#include "list"
#include "iostream"
using namespace std;


HANDLE g_hExit = NULL;

typedef std::list<std::string> TP_QUE;
struct THREAD_POOL
{
	HANDLE	m_hMutx;
	HANDLE	m_hSema;
	TP_QUE	m_queue;
};
typedef THREAD_POOL* PTHREAD_POOL;

DWORD WINAPI PoolItemProc(LPVOID pParam)
{
	PTHREAD_POOL pTQ = (PTHREAD_POOL)pParam;

	DWORD dwThId = GetCurrentThreadId();
	HANDLE arhObjs[2] = { g_hExit, pTQ->m_hSema };
	while (true)
	{
		DWORD dwWaitCode = WaitForMultipleObjects(2, arhObjs, FALSE, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}
		
		if (dwWaitCode == WAIT_OBJECT_0)
			break;

		std::string item;
		WaitForSingleObject(pTQ->m_hMutx, INFINITE);
		TP_QUE::iterator it = pTQ->m_queue.begin();
		item = *it;
		pTQ->m_queue.pop_front();
		ReleaseMutex(pTQ->m_hMutx);

		printf(" => BEGIN | Thread %d works : %s\n", dwThId, item.c_str());
		Sleep(((rand() % 3) + 1) * 1000);
		printf(" <= END   | Thread %d works : %s\n", dwThId, item.c_str());
	}
	ReleaseMutex(g_hExit);

	printf("......PoolItemProc Thread %d Exit!!!\n", dwThId);

	return 0;
}

void _tmain()
{
	cout << "======= Start TPSemaphore Test ========" << endl;

	g_hExit = CreateMutex(NULL, TRUE, NULL);

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	THREAD_POOL tp;
	tp.m_hSema = CreateSemaphore(NULL, 0, INT_MAX, NULL);
	tp.m_hMutx = CreateMutex(NULL, FALSE, NULL);

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

		WaitForSingleObject(tp.m_hMutx, INFINITE);
		tp.m_queue.push_back(szIn);
		ReleaseMutex(tp.m_hMutx);

		ReleaseSemaphore(tp.m_hSema, 1, NULL);
	}

	ReleaseMutex(g_hExit);
	WaitForMultipleObjects(si.dwNumberOfProcessors, parPoolThs, TRUE, INFINITE);
	for (int i = 0; i < (int)si.dwNumberOfProcessors; i++)
		CloseHandle(parPoolThs[i]);
	delete[] parPoolThs;

	CloseHandle(tp.m_hSema);
	CloseHandle(tp.m_hMutx);
	CloseHandle(g_hExit);

	cout << "======= End TPSemaphore Test ==========" << endl;
}
