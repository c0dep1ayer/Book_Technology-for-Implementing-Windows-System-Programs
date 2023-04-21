#include "stdafx.h"
#include "Winsock2.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

void _tmain()
{
	WSADATA	wsd;
	int nErrCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nErrCode)
	{
		cout << "WSAStartup failed with error : " << nErrCode << endl;
		return;
	}

	SOCKET hsoListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hsoListen == INVALID_SOCKET)
	{
		cout << "socket failed, code : " << WSAGetLastError() << endl;
		return;
	}

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(9001);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	int lSockRet = bind(hsoListen, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "bind failed, code : " << WSAGetLastError() << endl;
		return;
	}

	lSockRet = listen(hsoListen, SOMAXCONN);
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "listen failed, code : " << WSAGetLastError() << endl;
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;

	ULONG ul = 1;
	lSockRet = ioctlsocket(hsoListen, FIONBIO, &ul);
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "ioctlsocket failed : " << WSAGetLastError() << endl;
		return;
	}

	SOCKET sock = INVALID_SOCKET;
	while (true)
	{
		sock = accept(hsoListen, NULL, NULL);
		if (sock != INVALID_SOCKET)
			break;

		int nErrCode = WSAGetLastError();
		if (nErrCode != WSAEWOULDBLOCK)
		{
			cout << "accept failed : " << nErrCode << endl;
			closesocket(sock);
			return;
		}
		Sleep(50);
	}
	cout << " ==> New client " << sock << " connected..." << endl;

	char szBuff[512];
	while (true)
	{
		bool bIsExit = false;
		while (true)
		{
			lSockRet = recv(sock, szBuff, sizeof(szBuff), 0);
			if (lSockRet != SOCKET_ERROR)
				break;

			int nErrCode = WSAGetLastError();
			if (nErrCode != WSAEWOULDBLOCK)
			{
				cout << "recv failed : " << WSAGetLastError() << endl;
				bIsExit = false;
				break;
			}
			Sleep(10);
		}
		if (bIsExit)
			break;

		if (lSockRet == 0)
		{
			cout << " ==> Client " << sock << " disconnected..." << endl;
			break;
		}

		szBuff[lSockRet] = 0;
		cout << " *** Client(" << sock << ") sent : " << szBuff << endl;
		Sleep(10);

		lSockRet = send(sock, szBuff, lSockRet, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "send failed : " << WSAGetLastError() << endl;
			break;
		}
	}
	closesocket(sock);
	closesocket(hsoListen);
	
	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
