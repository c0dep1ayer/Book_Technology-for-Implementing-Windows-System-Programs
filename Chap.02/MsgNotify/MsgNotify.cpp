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


DWORD WINAPI WorkerProc(PVOID pParam)
{
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		MSG msg;
		GetMessage(&msg, NULL, 0, 0);

		if (msg.message == TM_EXIT)
			break;

		long  lSize = (long )msg.wParam;
		PBYTE pData = (PBYTE)msg.lParam;
		switch (msg.message)
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
	cout << " *** WorkerProc Thread exits..." << endl;

	return 0;
}

void _tmain()
{
	cout << "======= Start MsgNotify Test ========" << endl;

	DWORD  dwThrID;
	HANDLE hThread = CreateThread(NULL, 0, WorkerProc, NULL, 0, &dwThrID);
	
	char szIn[512];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		LONG  lCmd  = TM_NONE, lSize = 0;
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
		PostThreadMessage(dwThrID, (UINT)lCmd, (WPARAM)lSize, (LPARAM)pData);
	}

	PostThreadMessage(dwThrID, TM_EXIT, 0, 0);
	WaitForSingleObject(hThread, INFINITE);

	cout << "======= End MsgNotify Test ==========" << endl;
}
