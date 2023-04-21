#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;



VOID CALLBACK MyTimerCallback(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_TIMER)
{
	DWORD dwThrId = GetCurrentThreadId();
	int nVal = (int)pCtx;
	printf(" ####### TIMER Worker %d expired, value=%d\n", dwThrId, nVal);
}

VOID CALLBACK MyWorkCallback(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WORK)
{
	DWORD dwThrId = GetCurrentThreadId();
	int nVal = (int)pCtx;
	printf(" ******* WORK Worker %d called, value=%d\n", dwThrId, nVal);
	Sleep(2000);
}

VOID CALLBACK MyCGCancelCallback(PVOID pObjCtx, PVOID pCleanCtx)
{
	DWORD dwThrId = GetCurrentThreadId();
	int nObjVal   = (int)pObjCtx;
	int nCleanVal = (int)pCleanCtx;
	printf(" ===> Cancel Callback called, tid=%d, OjbVal=%d, CleanVal=%d\n", dwThrId, nObjVal, nCleanVal);
}

void _tmain()
{
	TP_CALLBACK_ENVIRON ce;
	InitializeThreadpoolEnvironment(&ce);

	PTP_POOL pPool = NULL;
	PTP_WORK ptpWork = NULL;
	PTP_TIMER ptpTimer = NULL;
	PTP_CLEANUP_GROUP pCG = NULL;

	try
	{
		pPool = CreateThreadpool(NULL);
		if (pPool == NULL)
			throw GetLastError();

		SetThreadpoolThreadMaximum(pPool, 2);
		if (!SetThreadpoolThreadMinimum(pPool, 1))
			throw GetLastError();

		pCG = CreateThreadpoolCleanupGroup();
		if (pCG == NULL)
			throw GetLastError();

		SetThreadpoolCallbackPool(&ce, pPool);

		SetThreadpoolCallbackCleanupGroup(&ce, pCG, MyCGCancelCallback);

		ptpWork = CreateThreadpoolWork(MyWorkCallback, (PVOID)32, &ce);
		if (ptpWork == NULL)
			throw GetLastError();
		SubmitThreadpoolWork(ptpWork);

		ptpTimer = CreateThreadpoolTimer(MyTimerCallback, (PVOID)64, &ce);
		if (ptpTimer == NULL)
			throw GetLastError();
		ULARGE_INTEGER ll;
		ll.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
		FILETIME ft;
		ft.dwHighDateTime = ll.HighPart;
		ft.dwLowDateTime = ll.LowPart;
		SetThreadpoolTimer(ptpTimer, &ft, 0, 0);

		Sleep(1500);

		CloseThreadpoolCleanupGroupMembers(pCG, FALSE, (PVOID)128);
	}
	catch (DWORD dwErrcode)
	{
		cout << "Error occurred, code = " << dwErrcode << endl;
	}
	if (pCG != NULL)
		CloseThreadpoolCleanupGroup(pCG);
	if (pPool != NULL)
		CloseThreadpool(pPool);
	DestroyThreadpoolEnvironment(&ce);
}