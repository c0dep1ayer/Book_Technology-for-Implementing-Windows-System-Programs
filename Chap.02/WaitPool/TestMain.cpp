#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#include "WaitPool.h"


#define TEST_MANY	0

#if (TEST_MANY == 0)
void WINAPI EventProc(PVOID pParam)
{
	int nWait = (int)pParam;
	cout << " => Thread " << "Event" << "(" << GetCurrentThreadId() << ") Signaled..." << endl;
}
void WINAPI MutexProc(PVOID pParam)
{
	HANDLE hMutex = (HANDLE)pParam;
	cout << " => Thread " << "Mutex" << "(" << GetCurrentThreadId() << ") Signaled..." << endl;
	ReleaseMutex(hMutex);
}
void WINAPI SemaphoreProc(PVOID pParam)
{
	cout << " => Thread " << "Semaphore" << "(" << GetCurrentThreadId() << ") Signaled..." << endl;
}
void WINAPI TimerProc(PVOID pParam)
{
	int nWait = (int)pParam;
	cout << " => Thread " << "Timer" << "(" << GetCurrentThreadId() << ") Signaled..." << endl;
}

void _tmain()
{
	WaitPool wq;
	wq.Start();

	HANDLE arhSyncs[4];
	arhSyncs[0] = CreateWaitableTimer(NULL, FALSE, FALSE);
	arhSyncs[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
	arhSyncs[2] = CreateMutex(NULL, TRUE, NULL);
	arhSyncs[3] = CreateSemaphore(NULL, 0, 1, NULL);

	WP_ITEM  arWaits[4] = { NULL, };
	PFN_WICB arpfncs[4] = { TimerProc, EventProc, MutexProc, SemaphoreProc };
	for (int i = 0; i < 4; i++)
		arWaits[i] = wq.Register(arhSyncs[i], arpfncs[i], arhSyncs[i]);

	char szIn[32];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		if (stricmp(szIn, "event") == 0)
			SetEvent(arhSyncs[1]);
		else if (stricmp(szIn, "mutex") == 0)
		{
			ReleaseMutex(arhSyncs[2]);
			WaitForSingleObject(arhSyncs[2], INFINITE);
		}
		else if (stricmp(szIn, "semaphore") == 0)
			ReleaseSemaphore(arhSyncs[3], 1, NULL);
		else if (stricmp(szIn, "timer") == 0)
		{
			LARGE_INTEGER lUTC;
			lUTC.QuadPart = 0;
			SetWaitableTimer(arhSyncs[0], &lUTC, 0, NULL, NULL, TRUE);
		}
	}

	for (int i = 0; i < 4; i++)
	{
		wq.Unregister(arWaits[i]);
		CloseHandle(arhSyncs[i]);
	}
	wq.Stop();
}

#else

void WINAPI EventSignalProc(PVOID pParam)
{
	int nEvtIdx = (int)pParam;
	printf(" => Event %d signaled in thread %d.....\n", nEvtIdx, GetCurrentThreadId());
	//Sleep(100);
}


#define MAX_EVENT_CNT	500
void _tmain()
{
	WaitPool wq;
	wq.Start();

	HANDLE  arhEvents[MAX_EVENT_CNT];
	WP_ITEM arwpRegs[MAX_EVENT_CNT];
	for (int i = 0; i < MAX_EVENT_CNT; i++)
	{
		arhEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		arwpRegs[i] = wq.Register(arhEvents[i], EventSignalProc, (PVOID)i);
	}

	for (int i = 0; i < 100; i++)
	{
		int idx = rand() % MAX_EVENT_CNT;
		SetEvent(arhEvents[idx]);
		Sleep(100);
	}

	for (int i = 0; i < MAX_EVENT_CNT; i++)
	{
		wq.Unregister(arwpRegs[i]);
		CloseHandle(arhEvents[i]);
	}
	wq.Stop();
}
#endif
