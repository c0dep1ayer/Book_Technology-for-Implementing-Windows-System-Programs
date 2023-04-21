#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "set"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")


SOCKET GetListenSocket(short shPortNo, int nBacklog = SOMAXCONN)
{
	SOCKET hsoListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hsoListen == INVALID_SOCKET)
		return INVALID_SOCKET;

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(shPortNo);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	LONG lSockRet = bind(hsoListen, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
	if (lSockRet == SOCKET_ERROR)
	{
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	lSockRet = listen(hsoListen, nBacklog);
	if (lSockRet == SOCKET_ERROR)
	{
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	return hsoListen;
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

	SOCKET hsoListen = GetListenSocket(9001, 1);
$LABEL_ACCEPT:
	SOCKET sock = accept(hsoListen, NULL, NULL);
	if (sock == INVALID_SOCKET)
	{
		cout << "" << endl;
		return;
	}

	char szIn[256];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		int nRet = send(sock, (const char*)szIn, strlen(szIn), 0);
		if (nRet == SOCKET_ERROR)
		{
			cout << "send failed : " << WSAGetLastError() << endl;
			closesocket(sock);
			goto $LABEL_ACCEPT;
		}
	}
	closesocket(sock);
	closesocket(hsoListen);

	WSACleanup();
}
