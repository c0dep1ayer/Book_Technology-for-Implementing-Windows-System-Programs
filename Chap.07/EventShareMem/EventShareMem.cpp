#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define CMD_NONE	  0
#define CMD_STR		  1
#define CMD_POINT	  2
#define CMD_TIME	  3
#define CMD_EXIT	100

#define SYNC_EVT_EXIT	_T("SYNC_EVT_EXIT")
#define SYNC_EVT_SEND	_T("SYNC_EVT_SEND")
#define SYNC_EVT_RESP	_T("SYNC_EVT_RESP")
#define SHARE_FILE		_T("MMF_SHARE_FILE")

VOID CALLBACK ChildTPollProc(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	HANDLE hevSend  = ((PHANDLE)pCtx)[0];
	HANDLE hevResp  = ((PHANDLE)pCtx)[1];
	PBYTE  pData	= (PBYTE)((PHANDLE)pCtx)[2];

	PBYTE pIter = pData;
	LONG  lCmd	= *((PLONG)pIter); pIter += sizeof(LONG);
	LONG  lSize	= *((PLONG)pIter); pIter += sizeof(LONG);

	switch (lCmd)
	{
		case CMD_STR:
		{
			PSTR pstr = new char[lSize + 1];
			memcpy(pstr, pIter, lSize);
			pstr[lSize] = 0;
			printf("  <== R-TH read STR : %s\n", pstr);
			delete[] pstr;
		}
		break;
			
		case CMD_POINT:
		{
			PPOINT ppt = (PPOINT)pIter;
			printf("  <== R-TH read POINT : (%d, %d)\n", ppt->x, ppt->y);
		}
		break;
			
		case CMD_TIME:
		{
			PSYSTEMTIME pst = (PSYSTEMTIME)pIter;
			printf("  <== R-TH read TIME : %04d-%02d-%02d %02d:%02d:%02d+%03d\n",
				pst->wYear, pst->wMonth, pst->wDay, pst->wHour,
				pst->wMinute, pst->wSecond, pst->wMilliseconds);
		}
		break;
	}
	SetThreadpoolWait(ptpWait, hevSend, NULL);
	SetEventWhenCallbackReturns(pInst, hevResp);
}

void ParentMainProc(HANDLE hevSend, HANDLE hevResp, PBYTE pData)
{
	BYTE arBuff[256];
	char szIn[512];

	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		LONG lCmd = CMD_NONE, lSize = 0;
		PBYTE pIter = arBuff + sizeof(LONG) * 2;
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
			memcpy(pIter, szIn, lSize);
			lCmd = CMD_STR;
		}
		((PLONG)arBuff)[0] = lCmd;
		((PLONG)arBuff)[1] = lSize;

		memcpy(pData, arBuff, lSize + sizeof(LONG) * 2);
		SignalObjectAndWait(hevSend, hevResp, INFINITE, FALSE);
	}
}

void _tmain()
{
	bool   bIsChild = false;
	HANDLE hevExit = NULL, hevSend = NULL, hevResp = NULL;
	HANDLE hmfShared = NULL;
	PBYTE  pData = NULL;

	try
	{
		hmfShared = CreateFileMapping
		(
			INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 65536, SHARE_FILE
		);
		if (hmfShared == NULL)
			throw GetLastError();
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			bIsChild = true;
		pData = (LPBYTE)MapViewOfFileEx
		(
			hmfShared, (bIsChild) ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0, NULL
		);
		if (pData == NULL)
			throw GetLastError();

		hevExit = CreateEvent(NULL, FALSE, FALSE, SYNC_EVT_EXIT);
		if (hevExit == NULL)
			throw GetLastError();

		hevSend = CreateEvent(NULL, FALSE, FALSE, SYNC_EVT_SEND);
		if (hevSend == NULL)
			throw GetLastError();
		hevResp = CreateEvent(NULL, FALSE, FALSE, SYNC_EVT_RESP);
		if (hevResp == NULL)
			throw GetLastError();

		if (!bIsChild)
		{
			cout << "======= START Parent Process ========" << endl;
			TCHAR szRunCmdLine[MAX_PATH] = { 0, };
			GetModuleFileName(NULL, szRunCmdLine, MAX_PATH);

			STARTUPINFO si;
			ZeroMemory(&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			PROCESS_INFORMATION pi;
			ZeroMemory(&pi, sizeof(pi));
			BOOL bIsOK = CreateProcess
			(
				NULL, szRunCmdLine, NULL, NULL, FALSE,
				CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
			);
			if (bIsOK)
			{
				CloseHandle(pi.hThread);
				ParentMainProc(hevSend, hevResp, pData);
				SignalObjectAndWait(hevExit, pi.hProcess, INFINITE, FALSE);
				CloseHandle(pi.hProcess);
			}
			cout << "======= END Parent Process ========" << endl;
		}
		else
		{
			cout << "======= START Child Process ========" << endl;
			HANDLE arSyncs[3] = { hevSend, hevResp, pData };
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
	}
	catch (DWORD hr)
	{
		cout << "======= Error occurred, code=" << hr << endl;
	}
	if (hevSend != NULL)
		CloseHandle(hevSend);
	if (hevResp != NULL)
		CloseHandle(hevResp);
	if (hevExit != NULL)
		CloseHandle(hevExit);
	if (pData != NULL)
		UnmapViewOfFile(pData);
	if (hmfShared != NULL)
		CloseHandle(hmfShared);
}
