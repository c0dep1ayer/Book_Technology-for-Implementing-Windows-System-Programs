#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Synchronization.lib")

#define CMD_NONE	  0
#define CMD_STR		  1
#define CMD_POINT	  2
#define CMD_TIME	  3
#define CMD_EXIT	100

struct WAIT_ENV
{
	SYNCHRONIZATION_BARRIER	_sb;
	BYTE					_arBuff[256];
};

#define CONSUMER_CNT	4

DWORD WINAPI WorkerProc(LPVOID pParam)
{
	WAIT_ENV* pwe = (WAIT_ENV*)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	LONG lCmp = CMD_NONE;
	while (true)
	{
		WaitOnAddress(pwe->_arBuff, &lCmp, sizeof(LONG), INFINITE);
		LONG lCmd = *((PLONG)pwe->_arBuff);
		if (lCmd == CMD_EXIT)
			break;

		PBYTE pIter = pwe->_arBuff + sizeof(LONG);
		LONG lSize = *((PLONG)pIter); pIter += sizeof(LONG);
		PBYTE pData = new BYTE[lSize + 1];
		memcpy(pData, pIter, lSize);

		if (EnterSynchronizationBarrier(&pwe->_sb, 0))
		{
			*((PLONG)pwe->_arBuff) = CMD_NONE;
			WakeByAddressSingle(pwe->_arBuff);
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
	cout << "======= Start WaitAddrRWMulti Test ========" << endl;

	WAIT_ENV we;
	*((PLONG)we._arBuff) = CMD_NONE;
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

		WakeByAddressAll(we._arBuff);
		WaitOnAddress(we._arBuff, &lCmd, sizeof(LONG), INFINITE);
	}

	*((PLONG)we._arBuff) = CMD_EXIT;
	WakeByAddressAll(we._arBuff);
	WaitForMultipleObjects(CONSUMER_CNT, harThrCsmrs, TRUE, INFINITE);
	DeleteSynchronizationBarrier(&we._sb);

	cout << "======= End WaitAddrRWMulti Test ==========" << endl;
}
