#include "stdafx.h"
#include "windows.h"
#include "list"
#include "iostream"
using namespace std;


typedef std::list<ULONG> VAR_QUEUE;
struct WORK_ENV
{
	VAR_QUEUE			_queue;
	SRWLOCK				_srwQue;
	LONG				_readCnt;
	CONDITION_VARIABLE	_cvNE;	// Not Empty
	CONDITION_VARIABLE	_cvNF;	// Not Full

};
typedef WORK_ENV* PWORK_ENV;

#define CONSUMER_CNT	10
#define PRODUCER_CNT	3
#define QUEUE_SIZE		10
#define DELAY_PRODUCE	500
#define DELAY_CONSUME	1000
LONG	g_lLastItem;
bool	g_bIsExit;

DWORD WINAPI ProducerProc(PVOID pParam)
{
	PWORK_ENV pwe = (PWORK_ENV)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		Sleep(rand() % DELAY_PRODUCE);

		AcquireSRWLockExclusive(&pwe->_srwQue);
		{
			LONG lNewVal = g_lLastItem++;
			while (pwe->_queue.size() == QUEUE_SIZE && !g_bIsExit)
			{
				SleepConditionVariableSRW(&pwe->_cvNF, &pwe->_srwQue, INFINITE, 0);
			}

			if (g_bIsExit)
			{
				ReleaseSRWLockExclusive(&pwe->_srwQue);
				break;
			}

			pwe->_queue.push_back(lNewVal);
			printf(" <= Producer %d ENQUE: item %2d, queued size %d\n",
				dwThrId, lNewVal, pwe->_queue.size());
		}
		ReleaseSRWLockExclusive(&pwe->_srwQue);

		WakeConditionVariable(&pwe->_cvNE);
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
		AcquireSRWLockShared(&pwe->_srwQue);
		{
			while (pwe->_queue.size() == 0 && !g_bIsExit)
			{
				SleepConditionVariableSRW(&pwe->_cvNE, &pwe->_srwQue,
					INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
			}

			if (g_bIsExit && pwe->_queue.size() == 0)
			{
				ReleaseSRWLockShared(&pwe->_srwQue);
				break;
			}

			VAR_QUEUE::iterator it = pwe->_queue.begin();
			LONG lPopItem = *it;
			printf("    => Consumer %d DEQUE: item %2d, queued size %d\n",
				dwThrId, lPopItem, pwe->_queue.size());
		}
		ReleaseSRWLockShared(&pwe->_srwQue);

		if (InterlockedIncrement(&pwe->_readCnt) == CONSUMER_CNT)
		{
			pwe->_readCnt = 0;
			pwe->_queue.pop_front();
			WakeConditionVariable(&pwe->_cvNF);
		}

		Sleep(rand() % DELAY_CONSUME);
	}

	printf("=== Consumer Thread %d terminated....\n", dwThrId);
	return 0;
}


void _tmain(void)
{
	WORK_ENV we;
	InitializeConditionVariable(&we._cvNE);
	InitializeConditionVariable(&we._cvNF);
	InitializeSRWLock(&we._srwQue);
	we._readCnt = 0;

	DWORD dwThrId;
	HANDLE harThrProds[PRODUCER_CNT];
	for (int i = 0; i < PRODUCER_CNT; i++)
	{
		harThrProds[i] = CreateThread(NULL, 0, ProducerProc, &we, 0, &dwThrId);
	}
	HANDLE harThrCsmrs[CONSUMER_CNT];
	for (int i = 0; i < CONSUMER_CNT; i++)
	{
		harThrCsmrs[i] = CreateThread(NULL, 0, ConsumerProc, &we, 0, &dwThrId);
	}

	puts("=== Press enter to stop...");
	getchar();

	AcquireSRWLockExclusive(&we._srwQue);
	g_bIsExit = true;
	ReleaseSRWLockExclusive(&we._srwQue);

	WakeAllConditionVariable(&we._cvNF);
	WakeAllConditionVariable(&we._cvNE);

	WaitForMultipleObjects(PRODUCER_CNT, harThrProds, TRUE, INFINITE);
	WaitForMultipleObjects(CONSUMER_CNT, harThrCsmrs, TRUE, INFINITE);
}
