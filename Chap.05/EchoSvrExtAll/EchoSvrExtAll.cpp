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
	LONG lRet = ::WSAIoctl
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


#ifndef STATUS_LOCAL_DISCONNECT
#	define STATUS_LOCAL_DISCONNECT	((NTSTATUS)0xC000013BL)	//ERROR_NETNAME_DELETED
#endif
#ifndef STATUS_REMOTE_DISCONNECT
#	define STATUS_REMOTE_DISCONNECT	((NTSTATUS)0xC000013CL)	//ERROR_NETNAME_DELETED
#endif
#ifndef STATUS_CONNECTION_RESET
#	define STATUS_CONNECTION_RESET	((NTSTATUS)0xC000020DL)	//ERROR_NETNAME_DELETED
#endif
#ifndef STATUS_CANCELLED
#	define STATUS_CANCELLED			((NTSTATUS)0xC0000120L)	//ERROR_OPERATION_ABORTED
#endif

#define IOKEY_LISTEN	1
#define IOKEY_CHILD		2

#define TM_PROG_EXIT			WM_USER + 1
#define TM_SOCK_CONNECTED		WM_USER + 2
#define TM_SOCK_DISCONNECTED	WM_USER + 3

#define	POOL_MAX_SIZE	32
#define	POOL_MIN_SIZE	4
#define PERMIT_INC_CNT	4



struct SOCK_ITEM : OVERLAPPED
{
	SOCKET	_sock;
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
	HANDLE	 _iocp;
	DWORD	 _thrid;
};
typedef IOCP_ENV* PIOCP_ENV;


DWORD WINAPI IocpSockRecvProc(PVOID pParam)
{
	PIOCP_ENV	pIE = (PIOCP_ENV)pParam;
	PSOCK_ITEM	psi = NULL;
	DWORD		dwTrBytes = 0;
	ULONG_PTR	upDevKey = 0;

	while (true)
	{
		try
		{
			BOOL bIsOK = GetQueuedCompletionStatus
			(
				pIE->_iocp, &dwTrBytes, &upDevKey, (LPOVERLAPPED*)&psi, INFINITE
			);
			if (bIsOK == FALSE)
			{
				if (psi != NULL)
					throw (int)psi->Internal;

				int nErrCode = WSAGetLastError();
				if (nErrCode != ERROR_ABANDONED_WAIT_0)
					cout << "GQCS failed: " << nErrCode << endl;
				break;
			}

			if (upDevKey == IOKEY_LISTEN)
			{
				CreateIoCompletionPort((HANDLE)psi->_sock, pIE->_iocp, IOKEY_CHILD, 0);
				cout << " ==> New client " << psi->_sock << " connected..." << endl;
				PostThreadMessage(pIE->_thrid, TM_SOCK_CONNECTED, 0, (LPARAM)psi);
			}
			else
			{
				if (dwTrBytes == 0)
					throw (INT)ERROR_SUCCESS;

				psi->_buff[dwTrBytes] = 0;
				cout << " *** Client(" << psi->_sock << ") sent : " << psi->_buff << endl;
				int lSockRet = send(psi->_sock, psi->_buff, dwTrBytes, 0);
				if (lSockRet == SOCKET_ERROR)
					throw WSAGetLastError();
			}

			DWORD dwFlags = 0;
			WSABUF wb;
			wb.buf = psi->_buff, wb.len = sizeof(psi->_buff);
			int nSockRet = WSARecv(psi->_sock, &wb, 1, NULL, &dwFlags, psi, NULL);
			if (nSockRet == SOCKET_ERROR)
			{
				int nErrCode = WSAGetLastError();
				if (nErrCode != WSA_IO_PENDING)
					throw nErrCode;
			}
		}
		catch (int ex)
		{
			if (ex == STATUS_LOCAL_DISCONNECT || ex == STATUS_CANCELLED)
			{
				cout << " ==> Child socket closed." << endl;
				continue;
			}
			if (ex == ERROR_SUCCESS || ex == STATUS_REMOTE_DISCONNECT)
				cout << " ==> Client " << psi->_sock << " disconnected..." << endl;
			else if (ex == STATUS_CONNECTION_RESET)
				cout << " ==> Pending Client " << psi->_sock << " disconnected..." << endl;
			else
				cout << " ==> Client " << psi->_sock << " has error " << ex << endl;

			PostThreadMessage(pIE->_thrid, TM_SOCK_DISCONNECTED, ex, (LPARAM)psi);
		}
	}
	return 0;
}

int IncreaseAcceptSockets(SOCKET hsoListen, int nIncCnt, SOCK_SET& pool)
{
	LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
		GetSockExtAPI(hsoListen, WSAID_ACCEPTEX);

	int nPooledCnt = 0;
	for (; nPooledCnt < nIncCnt; nPooledCnt++)
	{
		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
			break;

		PSOCK_ITEM psi = new SOCK_ITEM(sock);
		BOOL bIsOK = pfnAcceptEx
		(
			hsoListen, sock, psi->_buff, 0,
			sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
			NULL, (LPOVERLAPPED)psi
		);
		if (bIsOK == FALSE)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				cout << "AcceptEx failed : " << WSAGetLastError() << endl;
				closesocket(psi->_sock);
				delete psi;
				break;
			}
		}
		pool.insert(psi);
	}
	return nPooledCnt;
}


DWORD g_dwMainThrId = 0;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	PostThreadMessage(g_dwMainThrId, TM_PROG_EXIT, 0, 0);
	return TRUE;
}

void _tmain()
{
	WSADATA	wsd;
	int nErrCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nErrCode)
	{
		cout << "WSAStartup failed with error : " << nErrCode << endl;
		return;
	}

	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		return;
	}

	SOCKET hsoListen = GetListenSocket(9001);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;

	IOCP_ENV ie;
	ie._thrid = g_dwMainThrId = GetCurrentThreadId();
	ie._iocp  = CreateIoCompletionPort((HANDLE)hsoListen, NULL, 1, 2);

	HANDLE hTheads[2];
	for (int i = 0; i < 2; i++)
	{
		DWORD dwThrId;
		hTheads[i] = CreateThread(NULL, 0, IocpSockRecvProc, &ie, 0, &dwThrId);
	}

	SOCK_SET pool, conn;
	IncreaseAcceptSockets(hsoListen, PERMIT_INC_CNT, pool);

	WSAEVENT hEvent = WSACreateEvent();
	WSAEventSelect(hsoListen, hEvent, FD_ACCEPT);
	while (true)
	{
		DWORD dwWaitRet = MsgWaitForMultipleObjectsEx
		(
			1, &hEvent, 2000, QS_POSTMESSAGE, MWMO_INPUTAVAILABLE
		);
		if (dwWaitRet == WAIT_FAILED)
			break;

		if (dwWaitRet == WAIT_TIMEOUT)
		{
			if (pool.size() > POOL_MIN_SIZE)
			{
				SOCK_SET::iterator it = pool.begin();
				PSOCK_ITEM psi = *it;
				pool.erase(it);
				closesocket(psi->_sock);
				delete psi;

				printf("...Timeout expired, pool=%d, conn=%d\n",
					pool.size(), conn.size());
			}
			continue;
		}

		if (dwWaitRet == 1)
		{
			MSG msg;
			if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				continue;

			if (msg.message == TM_PROG_EXIT)
				break;

			PSOCK_ITEM psi = (PSOCK_ITEM)msg.lParam;
			if (msg.message == TM_SOCK_CONNECTED)
			{
				pool.erase(psi);
				conn.insert(psi);
				printf("...Connection established, pool=%d, conn=%d\n",
					pool.size(), conn.size());
			}
			else if (msg.message == TM_SOCK_DISCONNECTED)
			{
				conn.erase(psi);
				if (pool.size() > POOL_MIN_SIZE)
				{
					closesocket(psi->_sock);
					delete psi;
				}
				else
				{
					LPFN_DISCONNECTEX pfnDisconnectEx = (LPFN_DISCONNECTEX)
						GetSockExtAPI(psi->_sock, WSAID_DISCONNECTEX);
					pfnDisconnectEx(psi->_sock, NULL, TF_REUSE_SOCKET, 0);

					LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
						GetSockExtAPI(hsoListen, WSAID_ACCEPTEX);
					BOOL bIsOK = pfnAcceptEx
					(
						hsoListen, psi->_sock, psi->_buff, 0,
						sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
						NULL, (LPOVERLAPPED)psi
					);
					if (bIsOK == FALSE)
					{
						int nErrCode = WSAGetLastError();
						if (nErrCode != WSA_IO_PENDING)
						{
							cout << "AcceptEx failed : " << WSAGetLastError() << endl;
							closesocket(psi->_sock);
							delete psi;
							continue;
						}
					}
					pool.insert(psi);
				}
				printf("...Connection released, pool=%d, conn=%d\n",
					pool.size(), conn.size());
			}
		}
		else
		{
			WSANETWORKEVENTS ne;
			WSAEnumNetworkEvents(hsoListen, hEvent, &ne);
			if (ne.lNetworkEvents & FD_ACCEPT)
			{
				if (pool.size() < POOL_MAX_SIZE)
					IncreaseAcceptSockets(hsoListen, PERMIT_INC_CNT, pool);
				printf("...Listen event signaled, pool=%d, conn=%d\n", 
					pool.size(), conn.size());
			}
		}
	}
	CloseHandle(ie._iocp);
	WaitForMultipleObjects(2, hTheads, TRUE, INFINITE);

	closesocket(hsoListen);
	CloseHandle(hEvent);
	for (SOCK_SET::iterator it = conn.begin(); it != conn.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		closesocket(psi->_sock);
		delete psi;
	}
	for (SOCK_SET::iterator it = pool.begin(); it != pool.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		closesocket(psi->_sock);
		delete psi;
	}

	cout << "==== Server terminates... ==========================" << endl;
	WSACleanup();
}
