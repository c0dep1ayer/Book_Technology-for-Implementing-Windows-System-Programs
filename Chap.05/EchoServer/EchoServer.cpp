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
	LONG lSockRet = bind(hsoListen, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
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

	SOCKET sock = accept(hsoListen, NULL, NULL);
	if (sock == INVALID_SOCKET)
	{
		cout << "accept failed, code : " << WSAGetLastError() << endl;
		return;
	}
	cout << " ==> New client " << sock << " connected..." << endl;

	char szBuff[512];
	while (true)
	{
		lSockRet = recv(sock, szBuff, sizeof(szBuff), 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "recv failed : " << WSAGetLastError() << endl;
			break;
		}
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
