#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	256
struct SHARE_INFO
{
	HANDLE	 _hthStd;

	PBYTE	 _pvSend;
	HANDLE	 _hevSnds[2];
	
	PBYTE	 _pvRecv;
	HANDLE	 _hevRcvs[2];
	PTP_WAIT _ptpWait;

	SHARE_INFO()
	{
		memset(this, 0, sizeof(*this));
	}
};
typedef SHARE_INFO* PSHARE_INFO;

#define EVT_SEND_P2C	_T("EVT_SEND_P2C")
#define EVT_SEND_C2P	_T("EVT_SEND_C2P")
#define EVT_RECV_P2C	_T("EVT_RECV_P2C")
#define EVT_RECV_C2P	_T("EVT_RECV_C2P")
#define SHARE_FILE		_T("MMF_SHARE_FILE")

VOID CALLBACK Handler_MMFRead(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	PSHARE_INFO psi = PSHARE_INFO(pCtx);

	PBYTE pIter = psi->_pvRecv;
	LONG  lSize	= *((PLONG)pIter); pIter += sizeof(LONG);
	if (lSize < 0)
	{
		CancelSynchronousIo(psi->_hthStd);
		return;
	}
	pIter[lSize] = 0;
	cout << (PSTR)pIter << endl;

	SetEventWhenCallbackReturns(pInst, psi->_hevRcvs[1]);
	SetThreadpoolWait(ptpWait, psi->_hevRcvs[0], NULL);
}


DWORD WINAPI Handler_StdRead(PVOID pParam)
{
	PSHARE_INFO psi = PSHARE_INFO(pParam);
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

	char szIn[BUFF_SIZE];
	while (true)
	{
		int nReadCnt = 0;
		if (!ReadFile(hStdIn, szIn, BUFF_SIZE, (PDWORD)&nReadCnt, NULL))
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_OPERATION_ABORTED)
				cout << "ReadFile from STDIN error : " << dwErrCode << endl;
			break;
		}

		nReadCnt -= 2;
		if (nReadCnt <= 0)
			continue;

		PBYTE pIter = psi->_pvSend;
		szIn[nReadCnt] = 0;
		if (stricmp(szIn, "quit") == 0)
		{
			*((PLONG)pIter) = -1;
			SetEvent(psi->_hevSnds[0]);
			break;
		}

		*((PLONG)pIter) = nReadCnt; pIter += sizeof(LONG);
		memcpy(pIter, szIn, nReadCnt);
		SignalObjectAndWait(psi->_hevSnds[0], psi->_hevSnds[1], INFINITE, FALSE);
	}
	CloseHandle(hStdIn);

	return 0;
}


void _tmain()
{
	HANDLE hmfShared = NULL;
	SHARE_INFO si;

	SYSTEM_INFO s;
	GetSystemInfo(&s);
	try
	{
		hmfShared = CreateFileMapping
		(
			INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, s.dwAllocationGranularity * 2, SHARE_FILE
		);
		if (hmfShared == NULL)
			throw GetLastError();
		bool bIsChild = (GetLastError() == ERROR_ALREADY_EXISTS);

		for (int i = 0; i < 2; i++)
		{
			PBYTE*	ppView = NULL;
			PHANDLE	parEvs = NULL;
			PCTSTR	pszNames[2];
			if (i == 0)
			{
				ppView = (!bIsChild) ? &si._pvSend : &si._pvRecv;
				parEvs = (!bIsChild) ? si._hevSnds : si._hevRcvs;
				pszNames[0] = EVT_SEND_P2C, pszNames[1] = EVT_SEND_C2P;
			}
			else
			{
				ppView = (!bIsChild) ? &si._pvRecv : &si._pvSend;
				parEvs = (!bIsChild) ? si._hevRcvs : si._hevSnds;
				pszNames[0] = EVT_RECV_C2P, pszNames[1] = EVT_RECV_P2C;
			}

			DWORD  dwOffset = s.dwAllocationGranularity * i;
			*ppView = (PBYTE)MapViewOfFile
			(
				hmfShared, FILE_MAP_READ | FILE_MAP_WRITE, 0, dwOffset, s.dwAllocationGranularity
			);
			if (*ppView == NULL)
				throw GetLastError();

			for (int j = 0; j < 2; j++)
			{
				parEvs[j] = CreateEvent(NULL, FALSE, FALSE, pszNames[j]);
				if (parEvs[j] == NULL)
					throw GetLastError();
			}
		}

		DWORD dwThrId = 0;
		si._hthStd = CreateThread
		(
			NULL, 0, Handler_StdRead, &si, 0, &dwThrId
		);
		if (si._hthStd == NULL)
			throw GetLastError();

		si._ptpWait = CreateThreadpoolWait(Handler_MMFRead, &si, NULL);
		if (si._ptpWait == NULL)
			throw GetLastError();

		BOOL bIsOK = FALSE;
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
			bIsOK = CreateProcess
			(
				NULL, szRunCmdLine, NULL, NULL, FALSE,
				CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
			);
			if (bIsOK)
			{
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
			}
		}
		else
			cout << "======= START Child Process ========" << endl;
		SetThreadpoolWait(si._ptpWait, si._hevRcvs[0], NULL);
		WaitForSingleObject(si._hthStd, INFINITE);
		WaitForThreadpoolWaitCallbacks(si._ptpWait, TRUE);

		if (!bIsChild)
			cout << "======= END Parent Process ========" << endl;
		else
			cout << "======= END Child Process ========" << endl;
	}
	catch (DWORD hr)
	{
		cout << "======= Error occurred, code=" << hr << endl;
	}
	if (si._ptpWait != NULL)
		CloseThreadpoolWait(si._ptpWait);
	if (si._hthStd != NULL)
		CloseHandle(si._hthStd);
	for (int i = 0; i < 2; i++)
	{
		PBYTE pView = (i == 0) ? si._pvSend : si._pvRecv;
		if (pView != NULL)
			UnmapViewOfFile(pView);

		PHANDLE parSyns = (i == 0) ? si._hevSnds : si._hevRcvs;
		for (int j = 0; j < 2; j++)
		{
			if (parSyns[j] != NULL)
				CloseHandle(parSyns[j]);
		}
	}
	if (hmfShared != NULL)
		CloseHandle(hmfShared);
}
