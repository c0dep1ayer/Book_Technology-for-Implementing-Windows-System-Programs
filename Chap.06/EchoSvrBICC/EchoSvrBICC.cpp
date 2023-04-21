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
#	define STATUS_LOCAL_DISCONNECT	((NTSTATUS)0xC000013BL)
#endif
#ifndef STATUS_REMOTE_DISCONNECT
#	define STATUS_REMOTE_DISCONNECT	((NTSTATUS)0xC000013CL)
#endif
#ifndef STATUS_CANCELLED
#	define STATUS_CANCELLED			((NTSTATUS)0xC0000120L)
#endif


struct IOCP_ENV;
struct SOCK_ITEM : OVERLAPPED
{
	SOCKET		_sock;
	IOCP_ENV*	_env;
	char		_buff[512];

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
	SOCK_SET		 _set;
	SOCKET			 _listen;
};
typedef IOCP_ENV* PIOCP_ENV;


void WINAPI Handler_SockChild(DWORD dwErrCode, DWORD dwTranBytes, LPOVERLAPPED pov)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pov;
	PIOCP_ENV  pie = psi->_env;

	if (dwTranBytes > 0 && dwErrCode == 0)
	{
		if ((int)dwTranBytes > 0)
		{
			psi->_buff[dwTranBytes] = 0;
			cout << " *** Client(" << psi->_sock << ") sent : " << psi->_buff << endl;

			int lSockRet = send(psi->_sock, psi->_buff, dwTranBytes, 0);
			if (lSockRet == SOCKET_ERROR)
			{
				dwErrCode = WSAGetLastError();
				goto $LABEL_CLOSE;
			}
		}

		DWORD dwFlags = 0;
		WSABUF wb;
		wb.buf = psi->_buff, wb.len = sizeof(psi->_buff);
		int nSockRet = WSARecv(psi->_sock, &wb, 1, NULL, &dwFlags, psi, NULL);
		if (nSockRet == SOCKET_ERROR)
		{
			dwErrCode = WSAGetLastError();
			if (dwErrCode != WSA_IO_PENDING)
				goto $LABEL_CLOSE;
		}
		return;
	}

$LABEL_CLOSE:
	if (dwErrCode != STATUS_LOCAL_DISCONNECT)
	{
		if (dwErrCode == ERROR_SUCCESS || dwErrCode == STATUS_REMOTE_DISCONNECT)
			cout << " ==> Client " << psi->_sock << " disconnected..." << endl;
		else
			cout << " ==> Error occurred, code = " << dwErrCode << endl;
		closesocket(psi->_sock);
		EnterCriticalSection(&pie->_cs);
		pie->_set.erase(psi);
		LeaveCriticalSection(&pie->_cs);
		delete psi;
	}
	else
		cout << " ==> Child socket closed." << endl;
}

void WINAPI Handler_SockListen(DWORD dwErrCode, DWORD dwTranBytes, LPOVERLAPPED pov)
{
	if (dwErrCode != 0)
		goto $LABEL_CLOSE;

	PIOCP_ENV  pie = NULL;
	PSOCK_ITEM psi = NULL;
	if ((int)dwTranBytes >= 0)
	{
		psi = (PSOCK_ITEM)pov;
		pie = psi->_env;

		BindIoCompletionCallback((HANDLE)psi->_sock, Handler_SockChild, 0);
		cout << " ==> New client " << psi->_sock << " connected..." << endl;

		EnterCriticalSection(&pie->_cs);
		psi->_env->_set.insert(psi);
		LeaveCriticalSection(&pie->_cs);

		Handler_SockChild(0, -1, psi);
	}
	else
		pie = (PIOCP_ENV)pov;

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
		return;
	psi = new SOCK_ITEM(sock);
	psi->_env = pie;
	LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
		GetSockExtAPI(pie->_listen, WSAID_ACCEPTEX);
	BOOL bIsOK = pfnAcceptEx
	(
		pie->_listen, psi->_sock, psi->_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, (LPOVERLAPPED)psi
	);
	if (bIsOK == FALSE)
	{
		dwErrCode = WSAGetLastError();
		if (dwErrCode != WSA_IO_PENDING)
			goto $LABEL_CLOSE;
	}
	return;

$LABEL_CLOSE:
	if (dwErrCode != STATUS_CANCELLED)
	{
		cout << " ==> Error occurred, code = " << dwErrCode << endl;
		closesocket(psi->_sock);
		EnterCriticalSection(&pie->_cs);
		pie->_set.erase(psi);
		LeaveCriticalSection(&pie->_cs);
		delete psi;
	}
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

	SOCKET hsoListen = GetListenSocket(9001);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;

	IOCP_ENV ie;
	InitializeCriticalSection(&ie._cs);
	ie._listen = hsoListen;
	BindIoCompletionCallback((HANDLE)hsoListen, Handler_SockListen, 0);

	Handler_SockListen(0, -1, (LPOVERLAPPED)&ie);

	getchar();
	closesocket(hsoListen);
	for (SOCK_SET::iterator it = ie._set.begin(); it != ie._set.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		closesocket(psi->_sock);
	}
	Sleep(1000);
	for (SOCK_SET::iterator it = ie._set.begin(); it != ie._set.end(); it++)
		delete *it;

	DeleteCriticalSection(&ie._cs);
	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
