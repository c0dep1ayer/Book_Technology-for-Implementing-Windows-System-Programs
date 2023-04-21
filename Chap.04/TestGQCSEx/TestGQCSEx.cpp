#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;


#define TM_NONE		  0
#define TM_STR		100
#define TM_POINT	101
#define TM_TIME		102
#define TM_EXIT		200

PCSTR APC_STRS[] = 
{
	"INDEX 0 >>> QueueUserAPC 00000", 
	"INDEX 1 >>> QueueUserAPC 11111"
};

DWORD WINAPI IOCPWorkerProc(PVOID pParam)
{
	HANDLE hIocp = (HANDLE)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		OVERLAPPED_ENTRY oves[4];
		memset(oves, 0, sizeof(OVERLAPPED_ENTRY) * 4);
		ULONG uRemCnt = 0;
		BOOL bIsOK = GetQueuedCompletionStatusEx
		(
			hIocp, 
			oves, 4, &uRemCnt,
			INFINITE,
			TRUE
		);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode == WAIT_IO_COMPLETION)
			{
				printf("  ======> %d APC completed by Thread %d\n", uRemCnt, dwThrId);
				continue;
			}
			break;
		}

		bool bIsExit = false;
		for (ULONG i = 0; i < uRemCnt; i++)
		{
			LPOVERLAPPED_ENTRY poe = &oves[i];
			long  lSize = (long)poe->dwNumberOfBytesTransferred;
			PBYTE pData = (PBYTE)poe->lpOverlapped;
			UINT msg	= (UINT)poe->lpCompletionKey;

			if (msg == TM_EXIT)
			{
				bIsExit = true;
				continue;
			}

			switch (msg)
			{
				case TM_STR:
				{
					PSTR pstr = (PSTR)pData;
					printf("  <== R-TH %d read STR : %s\n", dwThrId, pstr);
				}
				break;

				case TM_POINT:
				{
					PPOINT ppt = (PPOINT)pData;
					printf("  <== R-TH %d read POINT : (%d, %d)\n", dwThrId, ppt->x, ppt->y);
				}
				break;

				case TM_TIME:
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
		printf("  ======> %d IOCP Packets dequeued by Thread %d\n", uRemCnt, dwThrId);
		if (bIsExit)
			break;
	}
	printf(" *** WorkerProc Thread %d exits\n", dwThrId);

	return 0;
}

void WINAPI APCCallback(ULONG_PTR dwData)
{
	DWORD dwThrId = GetCurrentThreadId();
	printf("  <== APC %d called : %s\n", dwThrId, (PSTR)dwData);
}

void _tmain()
{
	cout << "======= Start MsgNotify Test ========" << endl;

	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

	DWORD  dwThrID;
	HANDLE harWorks[2];
	for (int i = 0; i < 2; i++)
		harWorks[i] = CreateThread(NULL, 0, IOCPWorkerProc, hIocp, 0, &dwThrID);

	char szIn[512];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		if (_strnicmp(szIn, "apc", 3) == 0)
		{
			int len = strlen(szIn);
			int nThrIdx = 0;
			if (len > 3)
			{
				if (szIn[3] == '0')
					nThrIdx = 0;
				else
					nThrIdx = 1;
			}
			QueueUserAPC(APCCallback, harWorks[nThrIdx], (ULONG_PTR)APC_STRS[nThrIdx]);
			continue;
		}

		LONG  lCmd = TM_NONE, lSize = 0;
		PBYTE pData = NULL;
		if (_stricmp(szIn, "time") == 0)
		{
			lSize = sizeof(SYSTEMTIME), lCmd = TM_TIME;
			pData = new BYTE[lSize];

			SYSTEMTIME st;
			GetLocalTime(&st);
			memcpy(pData, &st, lSize);
		}
		else if (_stricmp(szIn, "point") == 0)
		{
			lSize = sizeof(POINT), lCmd = TM_POINT;
			pData = new BYTE[lSize];

			POINT pt;
			pt.x = rand() % 1000; pt.y = rand() % 1000;
			*((PPOINT)pData) = pt;
		}
		else
		{
			lSize = strlen(szIn), lCmd = TM_STR;
			pData = new BYTE[lSize + 1];
			strcpy((char*)pData, szIn);
		}
		PostQueuedCompletionStatus(hIocp, (DWORD)lSize, (ULONG_PTR)lCmd, (LPOVERLAPPED)pData);
	}
	CloseHandle(hIocp);
	WaitForMultipleObjects(2, harWorks, TRUE, INFINITE);

	cout << "======= End MsgNotify Test ==========" << endl;
}
