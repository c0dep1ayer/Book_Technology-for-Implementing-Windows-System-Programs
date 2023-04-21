#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define CMD_NONE	  0
#define CMD_STR		  1
#define CMD_POINT	  2
#define CMD_TIME	  3
#define CMD_EXIT	100

struct WAIT_ENV
{
	HANDLE					_hevSend;
	HANDLE					_hevResp;
	SYNCHRONIZATION_BARRIER	_sb;
	BYTE					_arBuff[256];
};

#define CONSUMER_CNT	4

DWORD WINAPI WorkerProc(LPVOID pParam)
{
	WAIT_ENV* pwe = (WAIT_ENV*)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		DWORD dwWaitCode = WaitForSingleObject(pwe->_hevSend, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		PBYTE pIter = pwe->_arBuff;
		LONG lCmd = *((PLONG)pIter); pIter += sizeof(LONG);
		if (lCmd == CMD_EXIT)
			break;

		LONG lSize = *((PLONG)pIter); pIter += sizeof(LONG);
		PBYTE pData = new BYTE[lSize + 1];
		memcpy(pData, pIter, lSize);

		if (EnterSynchronizationBarrier(&pwe->_sb, 0))
		{
			ResetEvent(pwe->_hevSend);
			SetEvent(pwe->_hevResp);
		}

		switch (lCmd)
		{
			case CMD_STR:
			{
				pData[lSize] = 0;
				printf("  <== R-TH %d read STR : %s\n", dwThrId, pData);
			}
			break;

			case CMD_POINT:
			{
				PPOINT ppt = (PPOINT)pData;
				printf("  <== R-TH %d read POINT : (%d, %d)\n", dwThrId, ppt->x, ppt->y);
			}
			break;

			case CMD_TIME:
			{
				PSYSTEMTIME pst = (PSYSTEMTIME)pData;
				printf("  <== R-TH %d read TIME : %04d-%02d-%02d %02d:%02d:%02d+%03d\n",
					dwThrId, pst->wYear, pst->wMonth, pst->wDay, pst->wHour,
					pst->wMinute, pst->wSecond, pst->wMilliseconds);
			}
			break;
		}
		delete[] pData;
	}
	cout << " *** WorkerProc Thread exits..." << endl;

	return 0;
}

void _tmain()
{
	cout << "======= Start SynchBarNoti Test ========" << endl;

	WAIT_ENV we;
	we._hevSend = CreateEvent(NULL, TRUE, FALSE, NULL);
	we._hevResp = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeSynchronizationBarrier(&we._sb, CONSUMER_CNT, 2000);

	DWORD dwThrId;
	HANDLE harThrCsmrs[CONSUMER_CNT];
	for (int i = 0; i < CONSUMER_CNT; i++)
	{
		harThrCsmrs[i] = CreateThread(NULL, 0, WorkerProc, &we, 0, &dwThrId);
	}

	char szIn[512];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		LONG lCmd = CMD_NONE, lSize = 0;
		PBYTE pIter = we._arBuff + sizeof(LONG) * 2;

		if (_stricmp(szIn, "time") == 0)
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			memcpy(pIter, &st, sizeof(st));
			lCmd = CMD_TIME, lSize = sizeof(st);
		}
		else if (_stricmp(szIn, "point") == 0)
		{
			POINT pt;
			pt.x = rand() % 1000; pt.y = rand() % 1000;
			*((PPOINT)pIter) = pt;
			lCmd = CMD_POINT, lSize = sizeof(pt);
		}
		else
		{
			lSize = strlen(szIn);
			if (lSize <= 0)
				continue;
			memcpy(pIter, szIn, lSize);
			lCmd = CMD_STR;
		}
		((PLONG)we._arBuff)[0] = lCmd;
		((PLONG)we._arBuff)[1] = lSize;

		SignalObjectAndWait(we._hevSend, we._hevResp, INFINITE, FALSE);
	}

	*((PLONG)we._arBuff) = CMD_EXIT;
	SetEvent(we._hevSend);
	WaitForMultipleObjects(CONSUMER_CNT, harThrCsmrs, TRUE, INFINITE);

	CloseHandle(we._hevResp);
	CloseHandle(we._hevSend);
	DeleteSynchronizationBarrier(&we._sb);
	cout << "======= End SynchBarNoti Test ==========" << endl;
}
