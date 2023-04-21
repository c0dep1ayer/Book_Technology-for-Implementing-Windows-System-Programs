#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	256
struct PIPE_ITEM : OVERLAPPED
{
	HANDLE  _pipe;
	PTP_IO	_ptpIo;
	BOOL	_connected;
	char	_buff[BUFF_SIZE];

	PIPE_ITEM()
	{
		memset(this, 0, sizeof(*this));
		_pipe = INVALID_HANDLE_VALUE;
	}
};
typedef PIPE_ITEM* PPIPE_ITEM;


VOID CALLBACK Handler_PipeRead(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	HANDLE hThread = (HANDLE)pCtx;
	PPIPE_ITEM ppi = (PPIPE_ITEM)pov;
	if (ior != NO_ERROR || (ppi->_connected && dwTrBytes == 0))
	{
		if (ior != ERROR_OPERATION_ABORTED && ior != ERROR_PIPE_NOT_CONNECTED)
			cout << "Handler_PipeRead error : " << ior << endl;
		CancelSynchronousIo(hThread);
		return;
	}

	if (ppi->_connected == TRUE)
	{
		ppi->_buff[dwTrBytes] = 0;
		char szOut[128];
		sprintf(szOut, "SOCK >> %s", ppi->_buff);
		cout << szOut << endl;
	}
	else
		ppi->_connected = TRUE;

	StartThreadpoolIo(ptpIo);
	BOOL bIsOK = ReadFile(ppi->_pipe, ppi->_buff, sizeof(ppi->_buff), NULL, ppi);
	if (!bIsOK)
	{
		ior = GetLastError();
		if (ior != ERROR_IO_PENDING)
		{
			cout << "ReadFile error : " << ior << endl;
			CancelThreadpoolIo(ptpIo);
			CancelSynchronousIo(hThread);
		}
	}
}

DWORD WINAPI Handler_StdRead(PVOID pParam)
{
	HANDLE hPipe = (HANDLE)pParam;
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

		szIn[nReadCnt] = 0;
		if (stricmp(szIn, "quit") == 0)
			break;

		DWORD dwWrotes = 0;
		BOOL bIsOK = WriteFile(hPipe, szIn, nReadCnt, &dwWrotes, NULL);
		if (!bIsOK)
		{
			cout << "WriteFile to SOCK error : " << GetLastError() << endl;
			break;
		}
	}
	CloseHandle(hStdIn);

	return 0;
}

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		cout << "arguments error....." << endl;
		return;
	}

	PIPE_ITEM pi;
	HANDLE hThread = NULL;
	try
	{
		TCHAR	szPipeName[MAX_PATH];
		_tcscpy(szPipeName, _T("\\\\.\\PIPE\\"));
		_tcscat(szPipeName, argv[1]);

		pi._pipe = CreateFile
		(
			szPipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
		);
		if (pi._pipe == INVALID_HANDLE_VALUE)
			throw GetLastError();

		DWORD dwMode = PIPE_READMODE_MESSAGE;
		BOOL bIsOK = SetNamedPipeHandleState(pi._pipe, &dwMode, NULL, NULL);
		if (!bIsOK)
			throw GetLastError();

		DWORD dwThrId = 0;
		hThread = CreateThread
		(
			NULL, 0, Handler_StdRead, pi._pipe, CREATE_SUSPENDED, &dwThrId
		);
		if (hThread == NULL)
			throw GetLastError();

		pi._ptpIo = CreateThreadpoolIo(pi._pipe, Handler_PipeRead, hThread, NULL);
		if (pi._ptpIo == NULL)
			throw GetLastError();

		Handler_PipeRead(NULL, hThread, &pi, 0, 0, pi._ptpIo);

		ResumeThread(hThread);
		WaitForSingleObject(hThread, INFINITE);
		DisconnectNamedPipe(pi._pipe);
	}
	catch (DWORD e)
	{
		cout << "Main Thread error occurred : " << e << endl;
	}
	if (pi._ptpIo != NULL)
	{
		CancelIoEx(pi._pipe, NULL);
		WaitForThreadpoolIoCallbacks(pi._ptpIo, TRUE);
		CloseThreadpoolIo(pi._ptpIo);
	}
	if (pi._pipe != INVALID_HANDLE_VALUE) 
		CloseHandle(pi._pipe);
	if (hThread != NULL)
		CloseHandle(hThread);
}
