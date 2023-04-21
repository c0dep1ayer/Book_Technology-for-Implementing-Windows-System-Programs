#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define CMD_NONE	  0
#define CMD_STR		  1
#define CMD_POINT	  2
#define CMD_TIME	  3
#define CMD_EXIT	100

#define MSG_CLS_NAME	_T("MSG_CLS_NAME")
#define SYNC_EVT_RESP	_T("SYNC_EVT_RESP")
#define SHARE_FILE		_T("c:\\temp\\__$share.tmp")
#define WM_NOTI_WRITE	WM_USER + 100
#define WM_PROG_EXIT	WM_USER + 101


void ChildThrProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	LONG  lCmd  = (LONG)wParam;
	LONG  lSize = (LONG)lParam;
	PBYTE pData = new BYTE[lSize + 1];
	HANDLE	hFile = CreateFile
	(
		SHARE_FILE, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL
	);
	DWORD dwReads = 0;
	ReadFile(hFile, pData, lSize, &dwReads, NULL);
	CloseHandle(hFile);

	switch (lCmd)
	{
		case CMD_STR:
		{
			pData[lSize] = 0;
			printf("  <== R-TH read STR : %s\n", pData);
		}
		break;
			
		case CMD_POINT:
		{
			PPOINT ppt = (PPOINT)pData;
			printf("  <== R-TH read POINT : (%d, %d)\n", ppt->x, ppt->y);
		}
		break;
			
		case CMD_TIME:
		{
			PSYSTEMTIME pst = (PSYSTEMTIME)pData;
			printf("  <== R-TH read TIME : %04d-%02d-%02d %02d:%02d:%02d+%03d\n",
				pst->wYear, pst->wMonth, pst->wDay, pst->wHour,
				pst->wMinute, pst->wSecond, pst->wMilliseconds);
		}
		break;
	}
	delete pData;
}

void ParentMainProc(DWORD dwThrId, HANDLE hevResp)
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
			memcpy(arBuff, &st, sizeof(st));
			lCmd = CMD_TIME, lSize = sizeof(st);
		}
		else if (_stricmp(szIn, "point") == 0)
		{
			POINT pt;
			pt.x = rand() % 1000; pt.y = rand() % 1000;
			*((PPOINT)arBuff) = pt;
			lCmd = CMD_POINT, lSize = sizeof(pt);
		}
		else
		{
			lSize = strlen(szIn);
			memcpy(arBuff, szIn, lSize);
			lCmd = CMD_STR;
		}

		HANDLE hFile = CreateFile
		(
			SHARE_FILE, GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			CREATE_ALWAYS, 0, NULL
		);
		DWORD dwWrotes = 0;
		WriteFile(hFile, arBuff, lSize, &dwWrotes, NULL);
		CloseHandle(hFile);

		PostThreadMessage(dwThrId, WM_NOTI_WRITE, lCmd, lSize);
		WaitForSingleObject(hevResp, INFINITE);
	}
}

void _tmain(int argc, TCHAR* argv[])
{
	bool bIsChild = false;
	HANDLE hevResp = NULL;

	try
	{
		hevResp = CreateEvent(NULL, FALSE, FALSE, MSG_CLS_NAME);
		if (hevResp == NULL)
			throw GetLastError();
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			bIsChild = true;

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

				ParentMainProc(pi.dwThreadId, hevResp);
				PostThreadMessage(pi.dwThreadId, WM_PROG_EXIT, 0, 0);
				WaitForSingleObject(pi.hProcess, INFINITE);
				CloseHandle(pi.hProcess);
			}
			cout << "======= END Parent Process ========" << endl;
		}
		else
		{
			cout << "======= START Child Process ========" << endl;
			MSG msg;
			while (GetMessage(&msg, NULL, 0, 0))
			{
				if (msg.message == WM_PROG_EXIT)
					break;

				if (msg.message == WM_NOTI_WRITE)
				{
					ChildThrProc(msg.message, msg.wParam, msg.lParam);
					SetEvent(hevResp);
				}
			}
			cout << "======= END Child Process ========" << endl;
		}
	}
	catch (DWORD hr)
	{
		cout << "======= Error occurred, code=" << hr << endl;
	}
	if (hevResp != NULL)
		CloseHandle(hevResp);
}
