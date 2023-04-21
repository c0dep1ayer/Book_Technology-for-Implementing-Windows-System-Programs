#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

void PrintCSInfo(PCRITICAL_SECTION pcs, PCSTR pszMsg, int nIndex = -1, int nOpt = 0)
{
	DWORD	dwCurThId = GetCurrentThreadId();
	LONG	lLockCnt	= pcs->LockCount;
	HANDLE	hLockEvt	= pcs->LockSemaphore;
	DWORD	dwOwnThId	= (DWORD)pcs->OwningThread;
	LONG	lRecuCnt	= pcs->RecursionCount;

	char locked, cswork;
	if (lLockCnt == 0 && hLockEvt == 0 && dwOwnThId == 0 && lRecuCnt == 0)
		locked = cswork = ' ';
	else
	{
		locked = (lLockCnt & 1) ? 'U' : 'L';
		cswork = (lLockCnt & 2) ? 'N' : 'W';
		if (lLockCnt != -1)
			lLockCnt = ~(lLockCnt >> 2);
	}

	if (nIndex < 0)
		printf("   %d\t%s\t%d(%c:%c)\t%d\t%d\t%d\n", 
			dwCurThId, pszMsg, lLockCnt, locked, cswork, dwOwnThId, lRecuCnt, hLockEvt);
	else
	{
		char szMsg[16] = { 0 };
		if (nOpt > 0)
		{
			if (nOpt == 1)
			{
				for (int j = 0; j < lRecuCnt - 1; j++)
					szMsg[j] = ' ';
			}
			else
			{
				for (int j = lRecuCnt - 1; j >= 0; j--)
					szMsg[j] = ' ';
			}
			szMsg[lRecuCnt] = 0;
			strcat(szMsg, pszMsg);
		}
		else
			strcpy(szMsg, pszMsg);
		printf("%d) %d\t %s\t%d(%c:%c)\t%d\t%d\t%d\n",
			nIndex, dwCurThId, szMsg, lLockCnt, locked, cswork, dwOwnThId, lRecuCnt, hLockEvt);
	}
}

HANDLE g_hevGo;
CRITICAL_SECTION g_cs;

DWORD WINAPI ThreadProc(LPVOID pParam)
{
	LONG lPrms = (LONG)pParam;
	int nRecuCnt = (int)HIWORD(lPrms);
	int nThrIdx  = (int)LOWORD(lPrms);
	char szMsg[16] = { 0 };

	PrintCSInfo(&g_cs, "BEFORE_CS", nThrIdx);
	
	for (int i = 0; i < nRecuCnt; i++)
	{
		EnterCriticalSection(&g_cs);
		PrintCSInfo(&g_cs, "ENTER_CS", nThrIdx, 1);
	}

	WaitForSingleObject(g_hevGo, INFINITE);

	for (int i = 0; i < nRecuCnt; i++)
	{
		LeaveCriticalSection(&g_cs);
		PrintCSInfo(&g_cs, "LEAVE_CS", nThrIdx, 2);
	}

	return 0;
}

#define MAX_THR_CNT 3
void _tmain(void)
{
	cout << "======= Start CS_Internal Test ========" << endl;
	printf("CT\tMSG\t\tLC\tOT\tRC\tLS\n");

	g_hevGo = CreateEvent(NULL, FALSE, FALSE, NULL);

	InitializeCriticalSection(&g_cs);
	PrintCSInfo(&g_cs, "MAIN_NEW");
	Sleep(200);

	HANDLE	harThrs[MAX_THR_CNT];
	int		narRecs[MAX_THR_CNT] = { 1, 3, 5 }; // , 3, 1};
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwTheaID = 0;
		LONG  lPrms = MAKELONG((WORD)i, (WORD)narRecs[i]);
		harThrs[i] = CreateThread(NULL, 0, ThreadProc, (PVOID)lPrms, 0, &dwTheaID);
	}

	Sleep(200);
	PrintCSInfo(&g_cs, "MAIN_MID");

	////////////////////////////////////////////////////////////////////////////////////////////////
	// FOR LockSemaphore SIGNAL_TEST :
	//g_cs.LockCount = -1;
	//g_cs.RecursionCount = 0;
	//g_cs.OwningThread = 0;
	//SetEvent(g_cs.LockSemaphore);
	//PrintCSInfo(&g_cs, "MAIN_MID");

	//WaitForMultipleObjects(MAX_THR_CNT, harThrs, TRUE, INFINITE);
	//DeleteCriticalSection(&g_cs);
	//PrintCSInfo(&g_cs, "MAIN_MID");
	//cout << "======= End CS_Internal Test ==========" << endl;
	//return;
	////////////////////////////////////////////////////////////////////////////////////////////////

	SetEvent(g_hevGo);
	for (int i = 0; i < MAX_THR_CNT; i++)
	{
		DWORD dwWaitCode = WaitForMultipleObjects(MAX_THR_CNT - i, harThrs, FALSE, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
			break;

		int idx = (int)(dwWaitCode - WAIT_OBJECT_0);
		CloseHandle(harThrs[idx]);
		for (int j = idx; j < MAX_THR_CNT - 1; j++)
			harThrs[j] = harThrs[j + 1];
		SetEvent(g_hevGo);
	}

	Sleep(200);
	DeleteCriticalSection(&g_cs);
	PrintCSInfo(&g_cs, "MAIN_DEL");
	cout << "======= End CS_Internal Test ==========" << endl;
}

