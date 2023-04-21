#include "stdafx.h"
#include "Winsock2.h"
#include "map"
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
	HANDLE	_event;
	SOCKET	_sock;
	DWORD	_thrId;

	SOCK_ITEM(SOCKET so, HANDLE ev, DWORD id)
	{
		_sock = so, _event = ev, _thrId = id;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;
typedef std::map<PTP_WAIT, PSOCK_ITEM> SOCK_MAP;


void WINAPI Handler_SockChild(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT pWait, TP_WAIT_RESULT)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pCtx;

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
		SetThreadpoolWait(pWait, psi->_event, NULL);
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
			PostThreadMessage(psi->_thrId, TM_CLOSE_SOCK, (WPARAM)hr, (LPARAM)psi);
		}
	}
}

VOID WINAPI Handler_SockListen(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT pWait, TP_WAIT_RESULT)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pCtx;

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
		PTP_WAIT psoWait = CreateThreadpoolWait(Handler_SockChild, pci, NULL);
		WSAEventSelect(sock, hEvent, FD_READ | FD_CLOSE);
		cout << " ==> New client " << psi->_sock << " connected" << endl;
		PostThreadMessage(psi->_thrId, TM_CONNECTED, (WPARAM)pci, (LPARAM)psoWait);
		SetThreadpoolWait(psoWait, hEvent, NULL);

		SetThreadpoolWait(pWait, psi->_event, NULL);
	}
	catch (int hr)
	{
		cout << " ==> Listen failed, code = " << hr << endl;
		PostThreadMessage(psi->_thrId, TM_LISTEN_FAIL, hr, (LPARAM)pWait);
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

	PTP_WAIT psoWait = CreateThreadpoolWait(Handler_SockListen, &sim, NULL);
	WSAEventSelect(hsoListen, hsoEvent, FD_ACCEPT);
	SetThreadpoolWait(psoWait, hsoEvent, NULL);

	SOCK_MAP sm;
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0))
	{
		if (msg.message == TM_PROG_EXIT)
			break;

		int nErrCode = ERROR_SUCCESS;
		PTP_WAIT pWait = (PTP_WAIT)msg.lParam;
		switch (msg.message)
		{
			case TM_CONNECTED:
			{
				PSOCK_ITEM	psi = (PSOCK_ITEM)msg.wParam;
				sm.insert(std::make_pair(pWait, psi));
			}
			break;

			case TM_CLOSE_SOCK:
			{
				SOCK_MAP::iterator it = sm.find(pWait);
				if (it != sm.end())
				{
					WaitForThreadpoolWaitCallbacks(pWait, TRUE);
					CloseThreadpoolWait(pWait);
					sm.erase(it);

					PSOCK_ITEM psi = it->second;
					CloseHandle(psi->_event);
					delete psi;
				}
			}
			break;
			
			case TM_LISTEN_FAIL:
				nErrCode = (int)msg.wParam;
			break;
		}
		if (nErrCode != ERROR_SUCCESS)
			break;
	}
	closesocket(hsoListen);
	WaitForThreadpoolWaitCallbacks(psoWait, TRUE);
	CloseThreadpoolWait(psoWait);
	CloseHandle(hsoEvent);

	for (SOCK_MAP::iterator it = sm.begin(); it != sm.end(); it++)
	{
		PSOCK_ITEM psi = it->second;
		closesocket(psi->_sock);
		WaitForThreadpoolWaitCallbacks(it->first, TRUE);
		CloseThreadpoolWait(it->first);
		CloseHandle(psi->_event);
		delete psi;
	}

	cout << "socket closed, program terminates..." << endl;
	WSACleanup();
}