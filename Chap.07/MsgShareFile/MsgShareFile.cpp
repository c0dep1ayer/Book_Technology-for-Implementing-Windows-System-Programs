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
#define SHARE_FILE		_T("c:\\temp\\__$share.tmp")
#define WM_NOTI_WRITE	WM_USER + 100
#define WM_PROG_EXIT	WM_USER + 101


LRESULT CALLBACK ChildWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg != WM_NOTI_WRITE && uMsg != WM_PROG_EXIT)
	{
		if (uMsg == WM_CREATE)
		{
			LPCREATESTRUCT pCS = (LPCREATESTRUCT)lParam;
			HANDLE hevInit = (HANDLE*)pCS->lpCreateParams;
			SetEvent(hevInit);
			return 0;
		}
		if (uMsg == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	if (uMsg == WM_PROG_EXIT)
	{
		DestroyWindow(hWnd);
		return 0;
	}

	LONG	lCmd  = (LONG)wParam;
	LONG	lSize = (LONG)lParam;
	PBYTE	pIter = new BYTE[lSize];
	HANDLE	hFile = CreateFile
	(
		SHARE_FILE, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL
	);
	DWORD dwReads = 0;
	ReadFile(hFile, pIter, lSize, &dwReads, NULL);
	CloseHandle(hFile);

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
	delete pIter;

	return 0;
}

void ParentMainProc(HWND hWnd)
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
			SHARE_FILE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL
		);
		DWORD dwWrotes = 0;
		WriteFile(hFile, arBuff, lSize, &dwWrotes, NULL);
		CloseHandle(hFile);

		SendMessage(hWnd, WM_NOTI_WRITE, lCmd, lSize);
	}
}

void _tmain(int argc, TCHAR* argv[])
{
	bool bIsChild = false;
	HANDLE hevInit = NULL;

	try
	{
		hevInit = CreateEvent(NULL, FALSE, FALSE, MSG_CLS_NAME);
		if (hevInit == NULL)
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
				WaitForSingleObject(hevInit, INFINITE);

				HWND hWnd = FindWindowEx(HWND_MESSAGE, NULL, MSG_CLS_NAME, NULL);
				ParentMainProc(hWnd);
				SendMessage(hWnd, WM_PROG_EXIT, 0, 0);
				CloseHandle(pi.hProcess);
			}
			cout << "======= END Parent Process ========" << endl;
		}
		else
		{
			cout << "======= START Child Process ========" << endl;

			WNDCLASSEX wc;
			memset(&wc, 0, sizeof(wc));
			wc.cbSize		 = sizeof(wc);
			wc.lpfnWndProc	 = ChildWndProc;
			wc.hInstance	 = GetModuleHandle(NULL);
			wc.lpszClassName = MSG_CLS_NAME;
			RegisterClassEx(&wc);

			HWND hWnd = CreateWindowEx
			(
				0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0,
				HWND_MESSAGE, NULL, wc.hInstance, hevInit
			);

			MSG msg;
			while (GetMessage(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			cout << "======= END Child Process ========" << endl;
		}
	}
	catch (DWORD hr)
	{
		cout << "======= Error occurred, code=" << hr << endl;
	}
	if (hevInit != NULL)
		CloseHandle(hevInit);
}
