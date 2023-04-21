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
	SOCK_SET	_conn;
	SOCKET		_listen;
	WSAEVENT	_event;
};
typedef IOCP_ENV* PIOCP_ENV;



void WINAPI Handler_SockChild(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG dwErrCode, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	PIOCP_ENV  pie = (PIOCP_ENV )pCtx;
	PSOCK_ITEM psi = (PSOCK_ITEM)pov;

	if (dwTrBytes > 0 && dwErrCode == NO_ERROR)
	{
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
	}

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

	closesocket(psi->_sock);
	delete psi;
	CloseThreadpoolIo(ptpIo);
}

VOID WINAPI Handler_SockListen(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	PIOCP_ENV pie = (PIOCP_ENV)pCtx;

	WSANETWORKEVENTS ne;
	WSAEnumNetworkEvents(pie->_listen, pie->_event, &ne);
	if (!(ne.lNetworkEvents & FD_ACCEPT))
		return;

	int nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
	if (nErrCode != 0)
	{
		cout << " ==> Listen failed, code = " << nErrCode << endl;
		return;
	}

	SOCKET sock = accept(pie->_listen, NULL, NULL);
	if (sock == INVALID_SOCKET)
	{
		nErrCode = WSAGetLastError();
		if (nErrCode != WSAEINTR)
			cout << " ==> Listen failed, code = " << nErrCode << endl;
		return;
	}
	cout << " ==> New client " << sock << " connected..." << endl;

	PSOCK_ITEM psi = new SOCK_ITEM(sock);
	psi->_ptpIo = CreateThreadpoolIo((HANDLE)psi->_sock, Handler_SockChild, pie, NULL);

	EnterCriticalSection(&pie->_cs);
	pie->_conn.insert(psi);
	LeaveCriticalSection(&pie->_cs);

	Handler_SockChild(pInst, pie, psi, NO_ERROR, -1, psi->_ptpIo);

	SetThreadpoolWait(ptpWait, pie->_event, NULL);
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

	PTP_WAIT psoWait = NULL;
	IOCP_ENV ie;
	InitializeCriticalSection(&ie._cs);
	try
	{
		ie._listen = GetListenSocket(9001);
		if (ie._listen == INVALID_SOCKET)
			throw WSAGetLastError();
		cout << " ==> Waiting for client's connection......" << endl;

		psoWait = CreateThreadpoolWait(Handler_SockListen, &ie, NULL);
		if (psoWait == NULL)
			throw (int)GetLastError();

		ie._event = WSACreateEvent();
		if (ie._event == NULL)
			throw WSAGetLastError();
		SetThreadpoolWait(psoWait, ie._event, NULL);
		WSAEventSelect(ie._listen, ie._event, FD_ACCEPT);

		getchar();
	}
	catch (int ex)
	{
		cout << "Error occurred in main, " << ex << endl;
	}

	if (ie._listen != INVALID_SOCKET)
		closesocket(ie._listen);
	if (psoWait != NULL)
	{
		WaitForThreadpoolWaitCallbacks(psoWait, TRUE);
		CloseThreadpoolWait(psoWait);
	}
	if (ie._event != NULL)
		CloseHandle(ie._event);

	for (SOCK_SET::iterator it = ie._conn.begin(); it != ie._conn.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		CancelIoEx((HANDLE)psi->_sock, NULL);
		WaitForThreadpoolIoCallbacks(psi->_ptpIo, TRUE);
		closesocket(psi->_sock);
		CloseThreadpoolIo(psi->_ptpIo);
		delete psi;
	}
	DeleteCriticalSection(&ie._cs);

	cout << "==== Server terminates... ==========================" << endl;
	WSACleanup();
}
