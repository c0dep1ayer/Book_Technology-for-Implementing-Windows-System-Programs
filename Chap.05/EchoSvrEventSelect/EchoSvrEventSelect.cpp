#include "stdafx.h"
#include "Winsock2.h"
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

DWORD g_dwTheadId = 0;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	PostThreadMessage(g_dwTheadId, WM_QUIT, 0, 0);
	return TRUE;
}


#define MAX_CLI_CNT	63
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
	g_dwTheadId = GetCurrentThreadId();

	SOCKET hsoListen = GetListenSocket(9001, MAX_CLI_CNT);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;

	WSAEVENT hEvent = WSACreateEvent();
	WSAEventSelect(hsoListen, hEvent, FD_ACCEPT);

	int		 nSockCnt = 0;
	WSAEVENT arEvents[MAX_CLI_CNT + 1];
	SOCKET	 arSocks [MAX_CLI_CNT + 1];
	memset(arEvents,    0, (MAX_CLI_CNT + 1) * sizeof(HANDLE));
	memset(arSocks , 0xFF, (MAX_CLI_CNT + 1) * sizeof(SOCKET));
	arEvents[0] = hEvent;
	arSocks[0]  = hsoListen;

	while (true)
	{
		DWORD dwWaitRet = MsgWaitForMultipleObjectsEx
		(
			nSockCnt + 1, arEvents, FALSE, INFINITE, QS_POSTMESSAGE
		);
		if (dwWaitRet == WAIT_FAILED)
		{
			cout << "MsgWaitForMultipleObjects failed: " << GetLastError() << endl;
			break;
		}
		if (dwWaitRet == WAIT_OBJECT_0 + nSockCnt + 1)
			break;

		int nIndex = (int)(dwWaitRet - WAIT_OBJECT_0);
		SOCKET sock = arSocks[nIndex];

		int nErrCode = 0;
		WSANETWORKEVENTS ne;
		WSAEnumNetworkEvents(sock, arEvents[nIndex], &ne);

		if (ne.lNetworkEvents & FD_ACCEPT)
		{
			nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
			if (nErrCode != 0)
			{
				cout << " ==> Error occurred, code = " << nErrCode << endl;
				break;
			}

			SOCKET hsoChild = accept(sock, NULL, NULL);
			if (hsoChild == INVALID_SOCKET)
			{
				cout << "accept failed, code : " << WSAGetLastError() << endl;
				break;
			}

			hEvent = WSACreateEvent();
			WSAEventSelect(hsoChild, hEvent, FD_READ | FD_CLOSE);

			arEvents[nSockCnt + 1] = hEvent;
			arSocks [nSockCnt + 1] = hsoChild;
			nSockCnt++;
			cout << " ==> New client " << hsoChild << " connected" << endl;
			continue;
		}

		if (ne.lNetworkEvents & FD_READ)
		{
			nErrCode = ne.iErrorCode[FD_READ_BIT];
			if (nErrCode != 0)
				goto $LABEL_CLOSE;

			char szBuff[512];
			int lSockRet = recv(sock, szBuff, sizeof(szBuff), 0);
			if (lSockRet == SOCKET_ERROR)
			{
				nErrCode = WSAGetLastError();
				goto $LABEL_CLOSE;
			}

			szBuff[lSockRet] = 0;
			cout << " *** Client(" << sock << ") sent : " << szBuff << endl;
			Sleep(10);

			lSockRet = send(sock, szBuff, lSockRet, 0);
			if (lSockRet == SOCKET_ERROR)
			{
				nErrCode = WSAGetLastError();
				goto $LABEL_CLOSE;
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
		continue;

$LABEL_CLOSE:
		closesocket(sock);
		CloseHandle(arEvents[nIndex]);

		for (int i = nIndex; i < nSockCnt; i++)
		{
			arEvents[i] = arEvents[i + 1];
			arSocks [i] = arSocks [i + 1];
		}
		arEvents[nSockCnt] = NULL;
		arSocks [nSockCnt] = INVALID_SOCKET;
		nSockCnt--;
		if (nErrCode == 0)
			cout << " ==> Client " << sock << " disconnected..." << endl;
		else
			cout << " ==> Error occurred, code = " << nErrCode << endl;
	}

	for (int i = 0; i < nSockCnt + 1; i++)
	{
		if (arEvents[i] != NULL)
			CloseHandle(arEvents[i]);
		if (arSocks[i] != INVALID_SOCKET)
			closesocket(arSocks[i]);
	}

	cout << "socket closed, program terminates..." << endl;
	WSACleanup();
}