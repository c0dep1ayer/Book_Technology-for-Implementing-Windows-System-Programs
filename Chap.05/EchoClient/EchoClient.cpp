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


	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		cout << "socket failed, code : " << WSAGetLastError() << endl;
		return;
	}

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family		= AF_INET;
	sa.sin_port			= htons(9001);
	sa.sin_addr.s_addr	= inet_addr("127.0.0.1");

	int lSockRet = connect(sock, (LPSOCKADDR)&sa, sizeof(sa));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "connect failed : " << WSAGetLastError() << endl;
		closesocket(sock);
		return;
	}
	cout << "==> Connection to server Success!!!" << endl;

	char szIn[512];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		lSockRet = strlen(szIn);
		lSockRet = send(sock, szIn, lSockRet, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "send failed : " << WSAGetLastError() << endl;
			break;
		}
		
		Sleep(10);
		
		lSockRet = recv(sock, szIn, sizeof(szIn), 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "recv failed : " << WSAGetLastError() << endl;
			break;
		}
		if (lSockRet == 0)
		{
			cout << "==> Disconnected from server!!!" << endl;
			break;
		}
		szIn[lSockRet] = 0;
		cout << " *** Received : " << szIn << endl;
	}
	closesocket(sock);
	cout << "==> socket closed, program terminates..." << endl;

	WSACleanup();
}
