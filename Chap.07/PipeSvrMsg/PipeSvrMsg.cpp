#include "stdafx.h"
#include "Windows.h"
#include "set"
#include "iostream"
using namespace std;


#define PIPE_NAME	_T("\\\\.\\pipe\\PIPE_SYNC_TEST")

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
typedef std::set<PPIPE_ITEM> PIPE_SET;

struct PIPE_ENV
{
	CRITICAL_SECTION _cs;
	PIPE_SET _set;

	PIPE_ENV()
	{
		InitializeCriticalSection(&_cs);
	}
	~PIPE_ENV()
	{
		DeleteCriticalSection(&_cs);
	}
};
typedef PIPE_ENV* PPIPE_ENV;



HANDLE g_hevExit;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	SetEvent(g_hevExit);
	return TRUE;
}

bool BeginForConnection(PPIPE_ENV ppe);

VOID CALLBACK Handler_PipeRead(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	PPIPE_ENV  ppe = PPIPE_ENV (pCtx);
	PPIPE_ITEM ppi = PPIPE_ITEM(pov);

	if (ior != NO_ERROR || (ppi->_connected && dwTrBytes == 0))
		goto $LABEL_CLOSE;

	if (ppi->_connected == TRUE)
	{
		ppi->_buff[dwTrBytes] = 0;
		char szOut[128];
		sprintf(szOut, "SOCK >> %s", ppi->_buff);
		cout << szOut << endl;

		DWORD dwWrotes = 0;
		BOOL bIsOK = WriteFile(ppi->_pipe, ppi->_buff, dwTrBytes, &dwWrotes, NULL);
		if (!bIsOK)
		{
			ior = GetLastError();
			goto $LABEL_CLOSE;
		}
	}
	else
	{
		ppi->_connected = TRUE;
		cout << " ==> Client " << ppi->_pipe << " connected." << endl;

		if (!BeginForConnection(ppe))
			SetEvent(g_hevExit);
	}

	StartThreadpoolIo(ptpIo);
	BOOL bIsOK = ReadFile(ppi->_pipe, ppi->_buff, sizeof(ppi->_buff), NULL, ppi);
	if (!bIsOK)
	{
		ior = GetLastError();
		if (ior != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(ptpIo);
			goto $LABEL_CLOSE;
		}
	}
	return;

$LABEL_CLOSE:
	if (ior == ERROR_OPERATION_ABORTED)
		return;

	if (ior == ERROR_BROKEN_PIPE)
		cout << " **> Client " << ppi->_pipe << " disconnected." << endl;

	EnterCriticalSection(&ppe->_cs);
	ppe->_set.erase(ppi);
	LeaveCriticalSection(&ppe->_cs);

	CloseHandle(ppi->_pipe);
	delete ppi;
	CloseThreadpoolIo(ptpIo);
}


bool BeginForConnection(PPIPE_ENV ppe)
{
	PPIPE_ITEM ppi = new PIPE_ITEM();
	try
	{
		ppi->_pipe = CreateNamedPipe
		(
			PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
			PIPE_UNLIMITED_INSTANCES, BUFF_SIZE, BUFF_SIZE,
			NMPWAIT_USE_DEFAULT_WAIT, NULL
		);
		if (ppi->_pipe == INVALID_HANDLE_VALUE)
			throw GetLastError();

		ppi->_ptpIo = CreateThreadpoolIo(ppi->_pipe, Handler_PipeRead, ppe, NULL);
		if (ppi->_ptpIo == NULL)
			throw GetLastError();

		StartThreadpoolIo(ppi->_ptpIo);
		BOOL bIsOK = ConnectNamedPipe(ppi->_pipe, ppi);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
			{
				CancelThreadpoolIo(ppi->_ptpIo);
				throw GetLastError();
			}
		}
		EnterCriticalSection(&ppe->_cs);
		ppe->_set.insert(ppi);
		LeaveCriticalSection(&ppe->_cs);
	}
	catch (DWORD e)
	{
		cout << "BeginForConnection failed, code = " << e << endl;
		if (ppi->_ptpIo != NULL)
			CloseThreadpoolIo(ppi->_ptpIo);
		if (ppi->_pipe != INVALID_HANDLE_VALUE)
			CloseHandle(ppi->_pipe);
		delete ppi;
		return false;
	}
	return true;
}


void _tmain()
{
	g_hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		return;
	}

	PIPE_ENV pe;
	if (!BeginForConnection(&pe))
		return;

	WaitForSingleObject(g_hevExit, INFINITE);
	for (PIPE_SET::iterator it = pe._set.begin(); it != pe._set.end(); it++)
	{
		PPIPE_ITEM ppi = *it;
		CancelIoEx(ppi->_pipe, NULL);
		WaitForThreadpoolIoCallbacks(ppi->_ptpIo, TRUE);
		CloseThreadpoolIo(ppi->_ptpIo);

		DisconnectNamedPipe(ppi->_pipe);
		CloseHandle(ppi->_pipe);
	}
}
