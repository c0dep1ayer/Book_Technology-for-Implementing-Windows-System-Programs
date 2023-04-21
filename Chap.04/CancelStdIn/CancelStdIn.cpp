#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


#define TM_EXIT	WM_USER + 100
#define TM_DATA	WM_USER + 101

DWORD WINAPI StdReadProc(PVOID pParam)
{
	DWORD dwThrId = (DWORD)pParam;
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

	char szIn[256];
	while (true)
	{
		int nReadCnt = 0;
		if (!ReadFile(hStdIn, szIn, sizeof(szIn), (PDWORD)&nReadCnt, NULL))
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode == ERROR_OPERATION_ABORTED)
				cout << " --> I/O operation canceled, exit thread..." << endl;
			else
				cout << " --> ReadFile from STDIN error : " << dwErrCode << endl;
			break;
		}

		nReadCnt -= 2;
		if (nReadCnt <= 0)
			continue;

		szIn[nReadCnt] = 0;
		if (stricmp(szIn, "quit") == 0)
		{
			PostThreadMessage(dwThrId, TM_EXIT, 0, 0);
			break;
		}

		PSTR pstr = new char[nReadCnt + 1];
		strcpy(pstr, szIn);
		PostThreadMessage(dwThrId, TM_DATA, 0, (LPARAM)pstr);
	}
	CloseHandle(hStdIn);

	return 0;
}


DWORD g_dwMainId;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	PostThreadMessage(g_dwMainId, TM_EXIT, 0, 0);
	return TRUE;
}

void _tmain()
{
	g_dwMainId = GetCurrentThreadId();
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, StdReadProc, (PVOID)g_dwMainId, 0, &dwThrID);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == TM_EXIT)
			break;

		PSTR pstr = (PSTR)msg.lParam;
		cout << " ==> DATA : " << pstr << endl;
		delete[] pstr;
	}

	CancelSynchronousIo(hThread);
	WaitForSingleObject(hThread, INFINITE);
	cout << "*** Exit CancelStdIn..." << endl;
}
