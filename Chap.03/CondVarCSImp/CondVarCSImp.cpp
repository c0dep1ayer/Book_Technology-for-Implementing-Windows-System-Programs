#include "stdafx.h"
#include "windows.h"
#include "list"
#include "iostream"
using namespace std;


typedef std::list<ULONG> VAR_QUEUE;
struct WORK_ENV
{
	VAR_QUEUE			_queue;
	CRITICAL_SECTION	_csQue;
	HANDLE				_hevNE;	// Not Empty
	HANDLE				_hevNF;	// Not Full

	WORK_ENV()
	{
		_hevNE = _hevNF = NULL;
	}
	~WORK_ENV()
	{
		if (_hevNE != NULL) CloseHandle(_hevNE);
		if (_hevNF != NULL) CloseHandle(_hevNF);
	}
};
typedef WORK_ENV* PWORK_ENV;

#define CONSUMER_CNT	3
#define QUEUE_SIZE		10
#define DELAY_PRODUCE	500
#define DELAY_CONSUME	2000
LONG	g_lLastItem;
bool	g_bIsExit;

DWORD WINAPI ProducerProc(PVOID pParam)
{
	PWORK_ENV pwe = (PWORK_ENV)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		Sleep(rand() % DELAY_PRODUCE);

		EnterCriticalSection(&pwe->_csQue);
		{
			LONG lNewVal = g_lLastItem++;
			while (pwe->_queue.size() == QUEUE_SIZE && !g_bIsExit)
			{
				LeaveCriticalSection(&pwe->_csQue);
				WaitForSingleObject(&pwe->_hevNF, INFINITE);
				EnterCriticalSection(&pwe->_csQue);
			}
			if (g_bIsExit)
			{
				LeaveCriticalSection(&pwe->_csQue);
				break;
			}

			pwe->_queue.push_back(lNewVal);
			printf(" <= Producer %d ENQUE: item %2d, queued size %d\n", 
					dwThrId, lNewVal, pwe->_queue.size());
		}
		LeaveCriticalSection(&pwe->_csQue);

		SetEvent(pwe->_hevNE);
	}

	printf("=== Producer Thread %d terminated....\n", dwThrId);
	return 0;
}

DWORD WINAPI ConsumerProc(PVOID pParam)
{
	PWORK_ENV pwe = (PWORK_ENV)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		EnterCriticalSection(&pwe->_csQue);
		{
			while (pwe->_queue.size() == 0 && !g_bIsExit)
			{
				LeaveCriticalSection(&pwe->_csQue);
				WaitForSingleObject(&pwe->_hevNE, INFINITE);
				EnterCriticalSection(&pwe->_csQue);
			}
			if (g_bIsExit && pwe->_queue.size() == 0)
			{
				LeaveCriticalSection(&pwe->_csQue);
				break;
			}

			VAR_QUEUE::iterator it = pwe->_queue.begin();
			LONG lPopItem = *it;
			pwe->_queue.pop_front();
			printf("    => Consumer %d DEQUE: item %2d, queued size %d\n", 
					dwThrId, lPopItem, pwe->_queue.size());
		}
		LeaveCriticalSection(&pwe->_csQue);

		SetEvent(&pwe->_hevNF);

		Sleep(rand() % DELAY_CONSUME);
	}

	printf("=== Consumer Thread %d terminated....\n", dwThrId);
	return 0;
}


void _tmain(void)
{
	WORK_ENV we;
	we._hevNE = CreateEvent(NULL, FALSE, FALSE, NULL);
	we._hevNF = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&we._csQue);

	DWORD dwThrId;
	HANDLE hThrProd = CreateThread(NULL, 0, ProducerProc, &we, 0, &dwThrId);
	HANDLE harThrCsmrs[CONSUMER_CNT];
	for (int i = 0; i < CONSUMER_CNT; i++)
	{
		harThrCsmrs[i] = CreateThread(NULL, 0, ConsumerProc, &we, 0, &dwThrId);
	}

	puts("=== Press enter to stop...");
	getchar();

	EnterCriticalSection(&we._csQue);
	g_bIsExit = true;
	LeaveCriticalSection(&we._csQue);

	SetEvent(we._hevNF);
	SetEvent(we._hevNE);

	WaitForSingleObject(hThrProd, INFINITE);
	WaitForMultipleObjects(CONSUMER_CNT, harThrCsmrs, TRUE, INFINITE);

	DeleteCriticalSection(&we._csQue);
}