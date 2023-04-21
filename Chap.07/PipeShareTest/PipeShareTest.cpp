#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define CMD_NONE	  0
#define CMD_STR		  1
#define CMD_POINT	  2
#define CMD_TIME	  3
#define CMD_EXIT	100

#define SYNC_EVT_RESP	_T("SYNC_EVT_RESP")
#define SHARE_PIPE		_T("\\\\.\\PIPE\\SHARE_FILE")

#define BUFF_SIZE	4096
struct PIPE_BUFF : OVERLAPPED
{
	BYTE	_buff[BUFF_SIZE];
	PIPE_BUFF()
	{
		memset(this, 0, sizeof(*this));
	}
};
typedef PIPE_BUFF* PPIPE_BUFF;

VOID CALLBACK ChildTPoolProc(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	HANDLE hpiShared = ((PHANDLE)pCtx)[0];
	HANDLE hevResp   = ((PHANDLE)pCtx)[1];
	HANDLE hevExit	 = ((PHANDLE)pCtx)[2];

	if (ior != NO_ERROR)
	{
		if (ior != ERROR_OPERATION_ABORTED && ior != ERROR_BROKEN_PIPE)
			cout << "Error occurred: " << ior << endl;
		SetEvent(hevExit);
		return;
	}

	PPIPE_BUFF pdb = (PPIPE_BUFF)pov;
	if (dwTrBytes > 0)
	{
		PBYTE pIter = pdb->_buff;
		LONG  lCmd  = *((PLONG)pIter); pIter += sizeof(LONG);
		LONG  lSize = *((PLONG)pIter); pIter += sizeof(LONG);

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
		SetEventWhenCallbackReturns(pInst, hevResp);
	}

	StartThreadpoolIo(ptpIo);
	BOOL bIsOK = ReadFile(hpiShared, pdb->_buff, BUFF_SIZE, NULL, pdb);
	if (!bIsOK)
	{
		DWORD dwErrCode = GetLastError();
		if (dwErrCode != ERROR_IO_PENDING)
		{
			cout << "PIPE : ReadFile failed: " << GetLastError() << endl;
			CancelThreadpoolIo(ptpIo);
			SetEvent(hevExit);
		}
	}
}

void ParentMainProc(HANDLE hpiShared, HANDLE hevResp)
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

		DWORD dwWrotes = 0;
		BOOL bIsOK = WriteFile(hpiShared, arBuff, lSize + sizeof(LONG) * 2, &dwWrotes, NULL);
		if (bIsOK)
			WaitForSingleObject(hevResp, INFINITE);
		else
			cout << "PIPE : WriteFile failed: " << GetLastError() << endl;
	}
}

void _tmain()
{
	bool   bIsChild = false;
	HANDLE hevResp = NULL;
	HANDLE hpiShared = NULL;

	try
	{
		hevResp = CreateEvent(NULL, FALSE, FALSE, SYNC_EVT_RESP);
		if (hevResp == NULL)
			throw GetLastError();
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			bIsChild = true;

		if (!bIsChild)
		{
			cout << "======= START Parent Process ========" << endl;
			hpiShared = CreateNamedPipe
			(
				SHARE_PIPE, PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
				PIPE_TYPE_BYTE,	1, BUFF_SIZE, 0, INFINITE, NULL
			);
			if (hpiShared == INVALID_HANDLE_VALUE)
				throw GetLastError();

			OVERLAPPED ov;
			memset(&ov, 0, sizeof(ov));
			if (!ConnectNamedPipe(hpiShared, &ov))
			{
				DWORD dwErrCode = GetLastError();
				if (dwErrCode != ERROR_IO_PENDING)
					throw GetLastError();
			}

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
				CloseHandle(pi.hProcess);
				WaitForSingleObject(hpiShared, INFINITE);

				ParentMainProc(hpiShared, hevResp);
				DisconnectNamedPipe(hpiShared);
			}
			cout << "======= END Parent Process ========" << endl;
		}
		else
		{
			cout << "======= START Child Process ========" << endl;
			hpiShared = CreateFile
			(
				SHARE_PIPE, GENERIC_READ, 0, NULL,
				OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
			);
			if (hpiShared == INVALID_HANDLE_VALUE)
				throw GetLastError();

			HANDLE hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);
			HANDLE arSyncs[3] = { hpiShared, hevResp, hevExit };
			PTP_IO ptpPipe = CreateThreadpoolIo(hpiShared, ChildTPoolProc, arSyncs, NULL);
			if (ptpPipe != NULL)
			{
				PIPE_BUFF pb;
				ChildTPoolProc(NULL, arSyncs, &pb, 0, 0, ptpPipe);
				WaitForSingleObject(hevExit, INFINITE);

				CancelIoEx(hpiShared, NULL);
				WaitForThreadpoolIoCallbacks(ptpPipe, TRUE);
				CloseThreadpoolIo(ptpPipe);
				CloseHandle(hevExit);
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
	if (hpiShared != INVALID_HANDLE_VALUE)
		CloseHandle(hpiShared);
}
