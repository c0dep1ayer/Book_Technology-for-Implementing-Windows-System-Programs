#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define MAX_PROC_CNT 4

#define SYNC_EVT_EXIT	_T("Global\\SYNC_EVT_EXIT")
#define SYNC_EVT_SEND	_T("Global\\SYNC_EVT_SEND")
#define SYNC_EVT_RESP	_T("Global\\SYNC_EVT_RESP")
#define SYNC_SEM_REFS	_T("Global\\SYNC_SEM_REFS")
#define SHARE_FILE		_T("Global\\MMF_SHARE_FILE")


VOID CALLBACK ChildTPollProc(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	HANDLE hevSend	= ((PHANDLE)pCtx)[0];
	HANDLE hevResp	= ((PHANDLE)pCtx)[1];
	HANDLE hseRefs	= ((PHANDLE)pCtx)[2];
	PBYTE  pView	= (PBYTE)((PHANDLE)pCtx)[3];

	int nSize = *((PINT)pView);
	PSTR pszOut = new char[nSize + 1];
	memcpy(pszOut, pView + sizeof(INT), nSize);
	pszOut[nSize] = 0;
	cout << pszOut << endl;
	delete[] pszOut;

	LONG lPreCnt = 0;
	ReleaseSemaphore(hseRefs, 1, &lPreCnt);
	if (lPreCnt >= MAX_PROC_CNT - 1)
	{
		SetEventWhenCallbackReturns(pInst, hevResp);
		ResetEvent(hevSend);
	}
	else
		WaitForSingleObject(hevResp, INFINITE);
	SetThreadpoolWait(ptpWait, hevSend, NULL);
}

void _tmain()
{
	HANDLE hevExit = NULL, hevSend = NULL, hevResp = NULL, hseRefs = NULL;
	HANDLE hmfShared = NULL;
	PBYTE  pView = NULL;

	try
	{
		hmfShared = OpenFileMapping(PAGE_READWRITE, FALSE, SHARE_FILE);
		if (hmfShared == NULL)
			throw GetLastError();
		pView = (LPBYTE)MapViewOfFile(hmfShared, FILE_MAP_READ, 0, 0, 0);
		if (pView == NULL)
			throw GetLastError();

		hevExit = OpenEvent(EVENT_ALL_ACCESS, FALSE, SYNC_EVT_EXIT);
		if (hevExit == NULL)
			throw GetLastError();

		hevSend = OpenEvent(EVENT_ALL_ACCESS, FALSE, SYNC_EVT_SEND);
		if (hevSend == NULL)
			throw GetLastError();
		hevResp = OpenEvent(EVENT_ALL_ACCESS, FALSE, SYNC_EVT_RESP);
		if (hevResp == NULL)
			throw GetLastError();
		hseRefs = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SYNC_SEM_REFS);
		if (hevResp == NULL)
			throw GetLastError();

		cout << "======= START Child Process ========" << endl;
		HANDLE arSyncs[4] = { hevSend, hevResp, hseRefs, (HANDLE)pView };
		PTP_WAIT ptpWait = CreateThreadpoolWait(ChildTPollProc, arSyncs, NULL);
		if (ptpWait != NULL)
		{
			SetThreadpoolWait(ptpWait, hevSend, NULL);
			WaitForSingleObject(hevExit, INFINITE);

			WaitForThreadpoolWaitCallbacks(ptpWait, TRUE);
			CloseThreadpoolWait(ptpWait);
		}
		cout << "======= END Child Process ========" << endl;
	}
	catch (DWORD hr)
	{
		cout << "======= Error occurred, code=" << hr << endl;
	}
	if (hseRefs != NULL)
		CloseHandle(hseRefs);
	if (hevSend != NULL)
		CloseHandle(hevSend);
	if (hevResp != NULL)
		CloseHandle(hevResp);
	if (hevExit != NULL)
		CloseHandle(hevExit);
	if (pView != NULL)
		UnmapViewOfFile(pView);
	if (hmfShared != NULL)
		CloseHandle(hmfShared);
}
