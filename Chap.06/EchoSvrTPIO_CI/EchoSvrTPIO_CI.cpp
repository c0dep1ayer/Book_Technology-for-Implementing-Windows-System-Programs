#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "set"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")


PVOID GetSockExtAPI(SOCKET sock, GUID guidFn)
{
	PVOID pfnEx = NULL;
	GUID guid = guidFn;
	DWORD dwBytes = 0;
	LONG lRet = WSAIoctl
	(
		sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid, sizeof(guid), &pfnEx,
		sizeof(pfnEx), &dwBytes, NULL, NULL
	);
	if (lRet == SOCKET_ERROR)
	{
		cout << "WSAIoctl failed, code : " << WSAGetLastError() << endl;
		return NULL;
	}
	return pfnEx;
}

SOCKET GetListenSocket(short shPortNo, int nBacklog = SOMAXCONN)
{
	SOCKET hsoListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hsoListen == INVALID_SOCKET)
	{
		cout << "socket failed, code : " << WSAGetLastError() << endl;
		return INVALID_SOCKET;
	}

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(shPortNo);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	LONG lSockRet = bind(hsoListen, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "bind failed, code : " << WSAGetLastError() << endl;
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	lSockRet = listen(hsoListen, nBacklog);
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "listen failed, code : " << WSAGetLastError() << endl;
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	return hsoListen;
}



#define	POOL_MAX_SIZE	32
#define	POOL_MIN_SIZE	4
#define PERMIT_INC_CNT	4

struct SOCK_ITEM : OVERLAPPED
{
	SOCKET	_sock;
	PTP_IO	_ptpIo;
	char	_buff[512];

	SOCK_ITEM(SOCKET sock)
	{
		memset(this, 0, sizeof(*this));
		_sock = sock;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;
typedef std::set<PSOCK_ITEM> SOCK_SET;

struct IOCP_ENV
{
	CRITICAL_SECTION _cs;
	SOCK_SET	_pool, _conn;

	WSAEVENT	_evListen;
	PTP_WAIT	_ptpWait;
	SOCKET		_soListen;
	PTP_IO		_ptpIoLsn;
	HANDLE		_evWait;
};
typedef IOCP_ENV* PIOCP_ENV;


void WINAPI Handler_SockChild(PTP_CALLBACK_INSTANCE,
	PVOID, PVOID, ULONG, ULONG_PTR, PTP_IO);

#define CMD_DELETE	0
#define CMD_NEW		1
#define CMD_REUSE	2
bool ManageSockets(UINT uCmd, PIOCP_ENV pEnv, PSOCK_ITEM psi, PTP_CALLBACK_INSTANCE pInst = NULL)
{
	bool bSuccess = true;
	if (uCmd == CMD_DELETE)
		goto $LABEL_CLOSE;

	if (uCmd == CMD_REUSE)
	{
		LPFN_DISCONNECTEX pfnDisconnectEx = (LPFN_DISCONNECTEX)
			GetSockExtAPI(psi->_sock, WSAID_DISCONNECTEX);
		if (!pfnDisconnectEx(psi->_sock, NULL, TF_REUSE_SOCKET, 0))
		{
			cout << "DisconnectEx failed : " << WSAGetLastError() << endl;
			bSuccess = false;
			goto $LABEL_CLOSE;
		}
	}

	if (uCmd == CMD_NEW)
	{
		psi->_ptpIo = CreateThreadpoolIo
			((HANDLE)psi->_sock, Handler_SockChild, pEnv, NULL);
		if (psi->_ptpIo == NULL)
			goto $LABEL_CLOSE;
	}

	StartThreadpoolIo(pEnv->_ptpIoLsn);
	LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
		GetSockExtAPI(pEnv->_soListen, WSAID_ACCEPTEX);
	BOOL bIsOK = pfnAcceptEx
	(
		pEnv->_soListen, psi->_sock, psi->_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, (LPOVERLAPPED)psi
	);
	if (bIsOK == FALSE && WSAGetLastError() != WSA_IO_PENDING)
	{
		cout << "AcceptEx failed : " << WSAGetLastError() << endl;
		bSuccess = false;
		CancelThreadpoolIo(pEnv->_ptpIoLsn);
		goto $LABEL_CLOSE;
	}
	return bSuccess;

$LABEL_CLOSE:
	PTP_IO ptpIo = psi->_ptpIo;
	if (uCmd == CMD_REUSE)
	{
		DisassociateCurrentThreadFromCallback(pInst);
		WaitForThreadpoolIoCallbacks(ptpIo, TRUE);
	}
	if (uCmd == CMD_DELETE)
		SetEventWhenCallbackReturns(pInst, pEnv->_evWait);
	else
		closesocket(psi->_sock);
	delete psi;
	CloseThreadpoolIo(ptpIo);
	return bSuccess;
}


void WINAPI Handler_SockChild(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG dwErrCode, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	PIOCP_ENV  pie = (PIOCP_ENV )pCtx;
	PSOCK_ITEM psi = (PSOCK_ITEM)pov;

	if (dwErrCode != NO_ERROR || dwTrBytes == 0)
		goto $LABEL_CLOSE;

	if ((int)dwTrBytes > 0)
	{
		psi->_buff[dwTrBytes] = 0;
		cout << " *** Client(" << psi->_sock << ") sent : " << psi->_buff << endl;

		int lSockRet = send(psi->_sock, psi->_buff, dwTrBytes, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			dwErrCode = WSAGetLastError();
			goto $LABEL_CLOSE;
		}
	}

	StartThreadpoolIo(ptpIo);
	WSABUF wb; DWORD dwFlags = 0;
	wb.buf = psi->_buff, wb.len = sizeof(psi->_buff);
	int nSockRet = WSARecv(psi->_sock, &wb, 1, NULL, &dwFlags, psi, NULL);
	if (nSockRet == SOCKET_ERROR)
	{
		dwErrCode = WSAGetLastError();
		if (dwErrCode != WSA_IO_PENDING)
		{
			CancelThreadpoolIo(ptpIo);
			goto $LABEL_CLOSE;
		}
	}
	return;

$LABEL_CLOSE:
	if (dwErrCode == ERROR_OPERATION_ABORTED)
		return;

	if (dwErrCode == ERROR_SUCCESS || dwErrCode == ERROR_NETNAME_DELETED)
		cout << " ==> Client " << psi->_sock << " disconnected..." << endl;
	else
		cout << " ==> Error occurred, code = " << dwErrCode << endl;

	EnterCriticalSection(&pie->_cs);
	pie->_conn.erase(psi);
	LeaveCriticalSection(&pie->_cs);
	if (ManageSockets(CMD_REUSE, pie, psi, pInst))
	{
		EnterCriticalSection(&pie->_cs);
		pie->_pool.insert(psi);
		LeaveCriticalSection(&pie->_cs);
	}
	printf("...Client Reused, pool=%d, conn=%d\n",
		pie->_pool.size(), pie->_conn.size());
}

void WINAPI Handler_SockListen(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG dwErrCode, ULONG_PTR dwTrBytes, PTP_IO ptpLsnIo)
{
	PIOCP_ENV  pie = (PIOCP_ENV)pCtx;
	PSOCK_ITEM psi = (PSOCK_ITEM)pov;

	if (dwErrCode != NO_ERROR)
		goto $LABEL_CLOSE;

	cout << " ==> New client " << psi->_sock << " connected..." << endl;
	EnterCriticalSection(&pie->_cs);
	pie->_pool.erase(psi);
	pie->_conn.insert(psi);
	LeaveCriticalSection(&pie->_cs);
	printf("...Client Connected, pool=%d, conn=%d\n",
		pie->_pool.size(), pie->_conn.size());

	Handler_SockChild(pInst, pie, psi, NO_ERROR, -1, psi->_ptpIo);
	StartThreadpoolIo(ptpLsnIo);
	return;

$LABEL_CLOSE:
	if (dwErrCode != ERROR_OPERATION_ABORTED)
		cout << " ==> Error occurred, code = " << dwErrCode << endl;

	EnterCriticalSection(&pie->_cs);
	pie->_conn.erase(psi);
	LeaveCriticalSection(&pie->_cs);
	ManageSockets(CMD_DELETE, pie, psi, pInst);
}

VOID WINAPI Handler_IncreaseSockets(PTP_CALLBACK_INSTANCE, 
	PVOID pCtx, PTP_WAIT pWait, TP_WAIT_RESULT tpRlt)
{
	PIOCP_ENV pie = (PIOCP_ENV)pCtx;

	if (tpRlt == WAIT_TIMEOUT)
	{
		if (pie->_pool.size() > POOL_MIN_SIZE)
		{
			EnterCriticalSection(&pie->_cs);
			SOCK_SET::iterator it = pie->_pool.begin();
			PSOCK_ITEM psi = *it;
			pie->_pool.erase(it);
			LeaveCriticalSection(&pie->_cs);

			closesocket(psi->_sock);
			WaitForSingleObject(pie->_evWait, INFINITE);
			printf("...Timeout expired, pool=%d, conn=%d\n",
				pie->_pool.size(), pie->_conn.size());
		}
	}
	else
	{
		if (tpRlt != WAIT_FAILED)
		{
			WSANETWORKEVENTS ne;
			WSAEnumNetworkEvents(pie->_soListen, pie->_evListen, &ne);
			if (!(ne.lNetworkEvents & FD_ACCEPT))
				return;

			int nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
			if (nErrCode != 0)
			{
				cout << " ==> Listen failed, code = " << nErrCode << endl;
				return;
			}
		}

		if (pie->_pool.size() < POOL_MAX_SIZE)
		{
			for (int i = 0; i < PERMIT_INC_CNT; i++)
			{
				SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (sock == INVALID_SOCKET)
					break;

				PSOCK_ITEM psi = new SOCK_ITEM(sock);
				if (!ManageSockets(CMD_NEW, pie, psi))
					break;
				EnterCriticalSection(&pie->_cs);
				pie->_pool.insert(psi);
				LeaveCriticalSection(&pie->_cs);
			}
		}
		printf("...Listen event signaled, pool=%d, conn=%d\n",
			pie->_pool.size(), pie->_conn.size());
	}

	LARGE_INTEGER ll; ll.QuadPart = -40000000LL;
	FILETIME ft; ft.dwHighDateTime = ll.HighPart;
	ft.dwLowDateTime = ll.LowPart;
	SetThreadpoolWait(pWait, pie->_evListen, &ft);
}


void _tmain()
{
	WSADATA	wsd;
	int nInitCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nInitCode)
	{
		cout << "WSAStartup failed with error : " << nInitCode << endl;
		return;
	}

	IOCP_ENV ie;
	InitializeCriticalSection(&ie._cs);
	try
	{
		ie._soListen = GetListenSocket(9001);
		if (ie._soListen == INVALID_SOCKET)
			throw WSAGetLastError();
		cout << " ==> Waiting for client's connection......" << endl;

		ie._evListen = WSACreateEvent();
		if (ie._evListen == NULL)
			throw WSAGetLastError();
		ie._evWait = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ie._evWait == NULL)
			throw WSAGetLastError();

		ie._ptpWait = CreateThreadpoolWait(Handler_IncreaseSockets, &ie, NULL);
		if (ie._ptpWait == NULL)
			throw (int)GetLastError();

		WSAEventSelect(ie._soListen, ie._evListen, FD_ACCEPT);
		LARGE_INTEGER ll; ll.QuadPart = -40000000LL;
		FILETIME ft; ft.dwHighDateTime = ll.HighPart;
		ft.dwLowDateTime = ll.LowPart;
		SetThreadpoolWait(ie._ptpWait, ie._evListen, &ft);

		ie._ptpIoLsn = CreateThreadpoolIo
			((HANDLE)ie._soListen, Handler_SockListen, &ie, NULL);
		if (ie._ptpIoLsn == NULL)
			throw (int)GetLastError();
		Handler_IncreaseSockets(NULL, &ie, ie._ptpWait, -1);

		getchar();
	}
	catch (int ex)
	{
		cout << "Error occurred in main, " << ex << endl;
	}

	if (ie._ptpIoLsn != NULL)
	{
		WaitForThreadpoolIoCallbacks(ie._ptpIoLsn, TRUE);
		CloseThreadpoolIo(ie._ptpIoLsn);
	}
	if (ie._soListen != INVALID_SOCKET)
		closesocket(ie._soListen);
	if (ie._ptpWait != NULL)
	{
		WaitForThreadpoolWaitCallbacks(ie._ptpWait, TRUE);
		CloseThreadpoolWait(ie._ptpWait);
	}
	if (ie._evListen != NULL)
		CloseHandle(ie._evListen);

	for (SOCK_SET::iterator it = ie._conn.begin(); it != ie._conn.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		CancelIoEx((HANDLE)psi->_sock, psi);
		WaitForThreadpoolIoCallbacks(psi->_ptpIo, TRUE);
		closesocket(psi->_sock);
		CloseThreadpoolIo(psi->_ptpIo);
		delete psi;
	}
	for (SOCK_SET::iterator it = ie._pool.begin(); it != ie._pool.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		closesocket(psi->_sock);
		CloseThreadpoolIo(psi->_ptpIo);
		delete psi;
	}
	if (ie._evWait != NULL)
		CloseHandle(ie._evWait);
	DeleteCriticalSection(&ie._cs);

	cout << "==== Server terminates... ==========================" << endl;
	WSACleanup();
}
