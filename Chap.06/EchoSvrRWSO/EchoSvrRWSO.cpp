#include "stdafx.h"
#include "Winsock2.h"
#include "set"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")


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


#define TM_PROG_EXIT	WM_USER + 1
#define TM_CONNECTED	WM_USER + 2
#define TM_CLOSE_SOCK	WM_USER + 3
#define TM_LISTEN_FAIL	WM_USER + 5


struct SOCK_ITEM
{
	HANDLE	_wait;
	HANDLE	_event;
	SOCKET	_sock;
	DWORD	_thrId;

	SOCK_ITEM(SOCKET so, HANDLE ev, DWORD id)
	{
		_wait = NULL, _sock = so, _event = ev, _thrId = id;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;
typedef std::set<PSOCK_ITEM> SOCK_SET;


void WINAPI Handler_SockChild(PVOID pParam, BOOLEAN)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pParam;

	WSANETWORKEVENTS ne;
	WSAEnumNetworkEvents(psi->_sock, psi->_event, &ne);
	try
	{
		if (ne.lNetworkEvents & FD_READ)
		{
			int nErrCode = ne.iErrorCode[FD_READ_BIT];
			if (nErrCode != 0)
				throw nErrCode;

			char szBuff[512];
			int lSockRet = recv(psi->_sock, szBuff, sizeof(szBuff), 0);
			if (lSockRet == SOCKET_ERROR)
				throw WSAGetLastError();

			if (lSockRet == 0)
				throw nErrCode;

			szBuff[lSockRet] = 0;
			cout << " *** Client(" << psi->_sock << ") sent : " << szBuff << endl;
			Sleep(10);

			lSockRet = send(psi->_sock, szBuff, lSockRet, 0);
			if (lSockRet == SOCKET_ERROR)
				throw WSAGetLastError();
		}

		if (ne.lNetworkEvents & FD_CLOSE)
		{
			int nErrCode = ne.iErrorCode[FD_CLOSE_BIT];
			if (nErrCode == WSAECONNABORTED)
				nErrCode = ERROR_SUCCESS;
			throw nErrCode;
		}
	}
	catch (int hr)
	{
		if (hr != WSAECONNABORTED)
		{
			if (hr == WSAECONNRESET)
				hr = ERROR_SUCCESS;
			if (hr == 0)
				cout << " ==> Client " << psi->_sock << " disconnected..." << endl;
			else
				cout << " ==> Error occurred, code = " << hr << endl;

			closesocket(psi->_sock);
			psi->_sock = INVALID_SOCKET;
			PostThreadMessage(psi->_thrId, TM_CLOSE_SOCK, (WPARAM)hr, (LPARAM)psi);
		}
	}
}

void WINAPI Handler_SockListen(PVOID pParam, BOOLEAN)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pParam;

	WSANETWORKEVENTS ne;
	WSAEnumNetworkEvents(psi->_sock, psi->_event, &ne);
	if (!(ne.lNetworkEvents & FD_ACCEPT))
		return;

	try
	{
		int nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
		if (nErrCode != 0)
			throw nErrCode;

		SOCKET sock = accept(psi->_sock, NULL, NULL);
		if (sock == INVALID_SOCKET)
		{
			nErrCode = WSAGetLastError();
			if (nErrCode != WSAEINTR)
				throw nErrCode;
			return;
		}

		HANDLE hEvent = WSACreateEvent();
		PSOCK_ITEM pci = new SOCK_ITEM(sock, hEvent, psi->_thrId);
		HANDLE hWait = NULL;
		BOOL bIsOK = RegisterWaitForSingleObject
		(
			&hWait, hEvent, Handler_SockChild, pci,
			INFINITE, WT_EXECUTEINWAITTHREAD
		);
		if (!bIsOK)
		{
			delete pci;
			throw WSAGetLastError();
		}
		pci->_wait = hWait;
		WSAEventSelect(sock, hEvent, FD_READ | FD_CLOSE);
		cout << " ==> New client " << psi->_sock << " connected" << endl;
		PostThreadMessage(psi->_thrId, TM_CONNECTED, 0, (LPARAM)pci);
	}
	catch (int hr)
	{
		cout << " ==> Listen failed, code = " << hr << endl;
		PostThreadMessage(psi->_thrId, TM_LISTEN_FAIL, hr, (LPARAM)psi);
	}
}


DWORD g_dwTheadId = 0;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	PostThreadMessage(g_dwTheadId, TM_PROG_EXIT, 0, 0);
	return TRUE;
}

void _tmain()
{
	WSADATA	wsd;
	int nInitRet = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nInitRet)
	{
		cout << "WSAStartup failed with error : " << nInitRet << endl;
		return;
	}
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << " MsgWaitForMultipleObjectsEx failed: " << GetLastError() << endl;
		return;
	}
	g_dwTheadId = GetCurrentThreadId();

	SOCKET hsoListen = GetListenSocket(9001);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;

	WSAEVENT hsoEvent = WSACreateEvent();
	SOCK_ITEM sim(hsoListen, hsoEvent, g_dwTheadId);
	HANDLE hWait = NULL;
	BOOL bIsOK = RegisterWaitForSingleObject
	(
		&hWait, hsoEvent, Handler_SockListen, &sim,
		INFINITE, WT_EXECUTEDEFAULT
	);
	sim._wait = hWait;
	WSAEventSelect(hsoListen, hsoEvent, FD_ACCEPT);

	SOCK_SET ss;
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0))
	{
		if (msg.message == TM_PROG_EXIT)
			break;

		int nErrCode = (int)msg.wParam;
		PSOCK_ITEM	psi = (PSOCK_ITEM)msg.lParam;
		switch (msg.message)
		{
			case TM_CONNECTED:
				ss.insert(psi);
				break;
			case TM_CLOSE_SOCK:
				ss.erase(psi);
				UnregisterWaitEx(psi->_wait, INVALID_HANDLE_VALUE);
				CloseHandle(psi->_event);
				delete psi;
				break;
			case TM_LISTEN_FAIL:
				nErrCode = E_UNEXPECTED;
				break;
		}
		if (nErrCode < 0)
			break;
	}
	closesocket(sim._sock);
	UnregisterWaitEx(sim._wait, INVALID_HANDLE_VALUE);
	CloseHandle(sim._event);

	for (SOCK_SET::iterator it = ss.begin(); it != ss.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		closesocket(psi->_sock);
		UnregisterWaitEx(psi->_wait, NULL);
		CloseHandle(psi->_event);
		delete psi;
	}

	cout << "socket closed, program terminates..." << endl;
	WSACleanup();
}