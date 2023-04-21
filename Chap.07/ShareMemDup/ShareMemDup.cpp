#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define WM_NOTI_WRITE	WM_USER + 100
#define WM_PROG_EXIT	WM_USER + 101

DWORD WINAPI Handler_StdRead(PVOID pParam)
{
	HANDLE hDstProc  = ((PHANDLE)pParam)[0];
	DWORD  dwDstThrId = (DWORD)((PHANDLE)pParam)[1];
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

	char szIn[256];
	while (true)
	{
		int nReadCnt = 0;
		if (!ReadFile(hStdIn, szIn, sizeof(szIn), (PDWORD)&nReadCnt, NULL))
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_OPERATION_ABORTED)
				cout << "ReadFile from STDIN error : " << dwErrCode << endl;
			break;
		}

		nReadCnt -= 2;
		if (nReadCnt <= 0)
			continue;
		szIn[nReadCnt] = 0;
		if (stricmp(szIn, "quit") == 0)
			break;

		HANDLE hmfSend = NULL, hmfDup = NULL;
		HANDLE hevResp = NULL, hevDup = NULL;
		try
		{
			hmfSend = CreateFileMapping
			(
				INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 65536, NULL
			);
			if (hmfSend == NULL)
				throw GetLastError();
			BOOL bIsOK = DuplicateHandle
			(
				GetCurrentProcess(), hmfSend, hDstProc, &hmfDup,
				0, FALSE, DUPLICATE_SAME_ACCESS
			);
			if (!bIsOK)
				throw GetLastError();

			hevResp = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (hevResp == NULL)
				throw GetLastError();
			bIsOK = DuplicateHandle
			(
				GetCurrentProcess(), hevResp, hDstProc, &hevDup,
				0, FALSE, DUPLICATE_SAME_ACCESS
			);

			PBYTE pView = (LPBYTE)MapViewOfFile
			(
				hmfSend, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0
			);
			if (pView == NULL)
				throw GetLastError();

			PBYTE pIter = (PBYTE)pView;
			*((PLONG)pIter) = nReadCnt; pIter += sizeof(LONG);
			memcpy(pIter, szIn, nReadCnt);
			UnmapViewOfFile(pView);

			PostThreadMessage(dwDstThrId, WM_NOTI_WRITE, (WPARAM)hmfDup, (LPARAM)hevDup);
			WaitForSingleObject(hevResp, INFINITE);
		}
		catch (DWORD hr)
		{
			cout << "======= Error occurred, code=" << hr << endl;
			if (hevDup != NULL)
				DuplicateHandle(hDstProc, hevDup, hDstProc, NULL, 
								0, FALSE, DUPLICATE_CLOSE_SOURCE);
			if (hmfDup  != NULL)
				DuplicateHandle(hDstProc, hmfDup, hDstProc, NULL, 
								0, FALSE, DUPLICATE_CLOSE_SOURCE);
		}
		if (hevResp != NULL) CloseHandle(hevResp);
		if (hmfSend != NULL) CloseHandle(hmfSend);
	}
	CloseHandle(hStdIn);
	PostThreadMessage(dwDstThrId, WM_PROG_EXIT, 0, 0);

	return 0;
}


void _tmain(int argc, TCHAR* argv[])
{
	bool	bIsChild = (argc > 1);
	HANDLE	hthrStd = NULL;
	DWORD	dwMsgThrId = 0;
	HANDLE  hDstProc = NULL;
	try
	{

		BOOL bIsOK = FALSE;
		if (!bIsChild)
		{
			cout << "======= START Parent Process ========" << endl;
			TCHAR szRunCmdLine[MAX_PATH] = { 0, };
			GetModuleFileName(NULL, szRunCmdLine, MAX_PATH);
			int nLen = _tcslen(szRunCmdLine);
			wsprintf(szRunCmdLine + nLen, _T(" %d %d"), 
				GetCurrentThreadId(), GetCurrentProcessId());

			STARTUPINFO si;
			ZeroMemory(&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			PROCESS_INFORMATION pi;
			ZeroMemory(&pi, sizeof(pi));
			bIsOK = CreateProcess
			(
				NULL, szRunCmdLine, NULL, NULL, FALSE,
				CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
			);
			if (bIsOK)
			{
				hDstProc	= pi.hProcess;
				dwMsgThrId	= pi.dwThreadId;
				CloseHandle(pi.hThread);
			}
		}
		else
		{
			cout << "======= START Child Process ========" << endl;
			SetConsoleTitle(_T("CHILD PROCESS"));
			dwMsgThrId = (DWORD)_ttoi(argv[1]);
			DWORD dwProcId = (DWORD)_ttoi(argv[2]);
			hDstProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcId);
			if (hDstProc == NULL)
				throw GetLastError();
		}

		DWORD  dwThrId = 0;
		HANDLE arPrms[2] = { hDstProc, (HANDLE)dwMsgThrId };
		hthrStd = CreateThread(NULL, 0, Handler_StdRead, arPrms, 0, &dwThrId);
		if (hthrStd == NULL)
			throw GetLastError();

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			if (msg.message == WM_PROG_EXIT)
				break;

			if (msg.message == WM_NOTI_WRITE)
			{
				HANDLE hmfSent = (HANDLE)msg.wParam;
				HANDLE hevResp = (HANDLE)msg.lParam;

				PBYTE pView = (LPBYTE)MapViewOfFile
				(
					hmfSent, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0
				);
				if (pView != NULL)
				{
					PBYTE pIter = pView;
					LONG  lSize = *((PLONG)pIter); pIter += sizeof(LONG);
					pIter[lSize] = 0;
					cout << " ==> RECV : " << (PCSTR)pIter << endl;
					UnmapViewOfFile(pView);
				}
				SetEvent(hevResp);
				CloseHandle(hmfSent);
				CloseHandle(hevResp);
			}
		}
		CancelSynchronousIo(hthrStd);
		WaitForSingleObject(hthrStd, INFINITE);

		if (!bIsChild)
			cout << "======= END Parent Process ========" << endl;
		else
			cout << "======= END Child Process ========" << endl;
	}
	catch (DWORD hr)
	{
		cout << "======= Error occurred, code=" << hr << endl;
	}
	if (hthrStd != NULL) CloseHandle(hthrStd);
	if (hDstProc != NULL) CloseHandle(hDstProc);
}
