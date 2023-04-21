#include "stdafx.h"
#include "Winsock2.h"
#include "NTSecApi.h"
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


void _tmain()
{
	WSADATA	wsd;
	int nErrCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nErrCode)
	{
		cout << "WSAStartup failed with error : " << nErrCode << endl;
		return;
	}

	SOCKET hsoListen = GetListenSocket(9001, 1);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;


	OVERLAPPED	ov;
	SOCKET		sock = INVALID_SOCKET;
	char		szBuff[4096];

$LABEL_ACCEPT:
	sock = accept(hsoListen, NULL, NULL);
	if (sock == INVALID_SOCKET)
	{
		cout << "accept failed, code : " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}
	cout << " ==> New client " << sock << " connected..." << endl;

	memset(&ov, 0, sizeof(ov));
	while (true)
	{
		if (!ReadFile((HANDLE)sock, szBuff, sizeof(szBuff), NULL, &ov))
		{
			int nErrCode = WSAGetLastError();
			if (nErrCode != WSA_IO_PENDING)
			{
				cout << "ReadFile failed, code : " << WSAGetLastError() << endl;
				break;
			}
		}

		DWORD dwTrBytes = 0;
		//BOOL bIsOK = GetOverlappedResult((HANDLE)sock, &ov, &dwTrBytes, TRUE);
		//if (!bIsOK)
		//{
		//	int nErrCode = WSAGetLastError();
		//	if (nErrCode != ERROR_NETNAME_DELETED)
		//		cout << " ==> Client " << sock << " disconnected..." << endl;
		//	else
		//		cout << " ==> Error occurred: " << nErrCode << endl;
		//	closesocket(sock);
		//	goto $LABEL_ACCEPT;
		//}

		//if (dwTrBytes == 0)
		//{
		//	cout << " ==> Client " << sock << " disconnected..." << endl;
		//	closesocket(sock);
		//	goto $LABEL_ACCEPT;
		//}

		DWORD dwWaitCode = WaitForSingleObject((HANDLE)sock, INFINITE);
		//DWORD dwWaitCode = WSAWaitForMultipleEvents(1, (PHANDLE)&sock, FALSE, INFINITE, FALSE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << "WaitForSingleObject failed: " << GetLastError() << endl;
			break;
		}

		if (ov.Internal != 0)
		{
			int nErrCode = LsaNtStatusToWinError(ov.Internal);
			if (nErrCode != ERROR_NETNAME_DELETED)
				cout << " ==> Client " << sock << " disconnected..." << endl;
			else
				cout << " ==> Error occurred: " << nErrCode << endl;
			closesocket(sock);
			goto $LABEL_ACCEPT;
		}
		if (ov.InternalHigh == 0)
		{
			cout << " ==> Client " << sock << " disconnected..." << endl;
			closesocket(sock);
			goto $LABEL_ACCEPT;
		}

		szBuff[ov.InternalHigh] = 0;
		cout << " *** Client(" << sock << ") sent : " << szBuff << endl;
		int lSockRet = send(sock, szBuff, ov.InternalHigh, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "send failed : " << WSAGetLastError() << endl;
			break;
		}
	}

	if (sock != INVALID_SOCKET)
		closesocket(sock);
	if (hsoListen != INVALID_SOCKET)
		closesocket(hsoListen);

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
