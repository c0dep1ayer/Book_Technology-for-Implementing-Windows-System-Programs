#include "stdafx.h"
#include "Winsock2.h"
#include "set"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#include "WaitPool.h"

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


#define TM_CMD_EXIT		WM_USER + 1
#define TM_CMD_REMOVE	WM_USER + 2

struct SOCK_ITEM
{
	WP_ITEM _wpi;
	SOCKET	_sock;
	HANDLE	_event;
	DWORD	_thrId;

	SOCK_ITEM(SOCKET so, HANDLE ev, DWORD id)
	{
		_wpi = NULL, _sock = so, _event = ev, _thrId = id;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;
typedef std::set<PSOCK_ITEM> SOCK_SET;


void WINAPI SockSigProc(PVOID pParam)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pParam;

	int nErrCode = 0;
	WSANETWORKEVENTS ne;
	WSAEnumNetworkEvents(psi->_sock, psi->_event, &ne);
	if (ne.lNetworkEvents & FD_READ)
	{
		nErrCode = ne.iErrorCode[FD_READ_BIT];
		if (nErrCode != 0)
			goto $LABEL_CLOSE;
		else
		{
			char szBuff[512];
			int lSockRet = recv(psi->_sock, szBuff, sizeof(szBuff), 0);
			if (lSockRet == SOCKET_ERROR)
			{
				nErrCode = WSAGetLastError();
				goto $LABEL_CLOSE;
			}

			szBuff[lSockRet] = 0;
			cout << " *** Client(" << psi->_sock << ") sent : " << szBuff << endl;
			Sleep(10);

			lSockRet = send(psi->_sock, szBuff, lSockRet, 0);
			if (lSockRet == SOCKET_ERROR)
			{
				nErrCode = WSAGetLastError();
				goto $LABEL_CLOSE;
			}
		}
	}

	if (ne.lNetworkEvents & FD_CLOSE)
	{
		nErrCode = ne.iErrorCode[FD_CLOSE_BIT];
		if (nErrCode != 0)
		{
			if (nErrCode == WSAECONNABORTED)
				nErrCode = 0;
		}
		goto $LABEL_CLOSE;
	}
	return;

$LABEL_CLOSE:
	closesocket(psi->_sock);
	psi->_sock = INVALID_SOCKET;
	PostThreadMessage(psi->_thrId, TM_CMD_REMOVE, (WPARAM)nErrCode, (LPARAM)psi);
}


DWORD g_dwTheadId = 0;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	PostThreadMessage(g_dwTheadId, TM_CMD_EXIT, 0, 0);
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

	SOCK_SET ss;
	WaitPool wq;
	wq.Start();

	WSAEVENT hsoEvent = WSACreateEvent();
	WSAEventSelect(hsoListen, hsoEvent, FD_ACCEPT);

	while (true)
	{
		DWORD dwWaitRet = MsgWaitForMultipleObjectsEx
		(
			1, &hsoEvent, INFINITE, QS_ALLPOSTMESSAGE, MWMO_INPUTAVAILABLE
		);
		if (dwWaitRet == WAIT_FAILED)
		{
			cout << "WSAWaitForMultipleEvents failed, code : " << GetLastError() << endl;
			break;
		}
		if (dwWaitRet == WAIT_OBJECT_0)
		{
			int nErrCode = 0;
			WSANETWORKEVENTS ne;
			WSAEnumNetworkEvents(hsoListen, hsoEvent, &ne);

			if (ne.lNetworkEvents & FD_ACCEPT)
			{
				int nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
				if (nErrCode != 0)
				{
					cout << " ==> Error occurred, code = " << nErrCode << endl;
					break;
				}

				SOCKET sock = accept(hsoListen, NULL, NULL);
				if (sock == INVALID_SOCKET)
				{
					cout << "accept failed, code : " << WSAGetLastError() << endl;
					break;
				}

				HANDLE hEvent = WSACreateEvent();
				PSOCK_ITEM psi = new SOCK_ITEM(sock, hEvent, GetCurrentThreadId());
				psi->_wpi = wq.Register(hEvent, SockSigProc, psi);
				WSAEventSelect(sock, hEvent, FD_READ | FD_CLOSE);
				ss.insert(psi);
				cout << " ==> New client " << sock << " connected" << endl;
			}
		}
		else
		{
			MSG msg;
			PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
			if (msg.message == TM_CMD_EXIT)
				break;

			if (msg.message == TM_CMD_REMOVE)
			{
				LONG lErrCode = (LONG)msg.wParam;
				PSOCK_ITEM psi = (PSOCK_ITEM)msg.lParam;
				if (lErrCode == 0)
					cout << " ==> Client " << psi->_sock << " disconnected..." << endl;
				else
					cout << " ==> Error occurred, code = " << nErrCode << endl;
				ss.erase(psi);
				wq.Unregister(psi->_wpi);
				CloseHandle(psi->_event);
				delete psi;
			}
		}
	}
	wq.Stop();

	for (SOCK_SET::iterator it = ss.begin(); it != ss.end(); it++)
	{
		PSOCK_ITEM psi = *it;
		closesocket(psi->_sock);
		wq.Unregister(psi->_wpi);
		delete psi;
	}

	cout << "socket closed, program terminates..." << endl;
	WSACleanup();
}