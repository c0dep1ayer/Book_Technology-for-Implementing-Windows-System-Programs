#include "stdafx.h"
#include "Winsock2.h"
#include "NTSecApi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define USE_WSA 1

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

DWORD WINAPI SockProc(PVOID pParam)
{
	PVOID* pargs = (PVOID*)pParam;

	SOCKET soListen = (SOCKET)pargs[0];
	SOCKET* psock   = (SOCKET*)pargs[1];
	PBOOL	pbWSA	= (PBOOL)pargs[2];

	OVERLAPPED	ov = { 0, };
	char		szBuff[4096];
	INT			nErrCode = ERROR_SUCCESS;

$LABEL_ACCEPT:
	*psock = accept(soListen, NULL, NULL);
	if (*psock == INVALID_SOCKET)
	{
		cout << "accept failed, code : " << WSAGetLastError() << endl;
		return 0;
	}
	cout << " ==> New client " << *psock << " connected..." << endl;

	memset(&ov, 0, sizeof(ov));
	while (true)
	{
		if (!ReadFile((HANDLE)*psock, szBuff, sizeof(szBuff), NULL, &ov))
		{
			int nErrCode = WSAGetLastError();
			if (nErrCode != WSA_IO_PENDING)
			{
				cout << "ReadFile failed, code : " << WSAGetLastError() << endl;
				break;
			}
		}
		cout << " <== WAIT FOR RECV....." << endl;

		DWORD dwTrBytes = 0;
		if (*pbWSA == FALSE)
		{
			if (!GetOverlappedResult((HANDLE)*psock, &ov, &dwTrBytes, TRUE))
			{
				printf("GOR => ERROR=%d, STATUS=0x%08X\n", WSAGetLastError(), ov.Internal);
				break;
			}
		}
		else
		{
			DWORD dwFlags = 0;
			if (!WSAGetOverlappedResult(*psock, &ov, &dwTrBytes, TRUE, &dwFlags))
			{
				printf("WSA => ERROR=%d, STATUS=0x%08X\n", WSAGetLastError(), ov.Internal);
				break;
			}
		}
		if (dwTrBytes == 0)
		{
			cout << " ==> Client " << *psock << " disconnected." << endl;
			break;
		}

		szBuff[dwTrBytes] = 0;
		cout << " *** Client(" << *psock << ") sent : " << szBuff << endl;
		int lSockRet = send(*psock, szBuff, ov.InternalHigh, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "send failed : " << WSAGetLastError() << endl;
			break;
		}
	}
	closesocket(*psock);
	goto $LABEL_ACCEPT;

	return 0;
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

	BOOL   bWSA = FALSE;
	SOCKET sock = INVALID_SOCKET;
	PVOID arSocks[3] = { (PVOID)hsoListen, &sock, (PVOID)&bWSA };

	DWORD dwThrID = 0;
	HANDLE hThread = NULL;

$LABEL_CREATE:
	cout << "======= " << ((bWSA) ? "WSA" : "GOR") << " ==================" << endl;
	hThread = CreateThread(NULL, 0, SockProc, arSocks, 0, &dwThrID);

	char szIn[256];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		if (_stricmp(szIn, "close") == 0)
		{
			if (sock != INVALID_SOCKET)
				closesocket(sock);
		}
		else if (_stricmp(szIn, "cancel") == 0)
		{
			if (sock != INVALID_SOCKET)
				CancelIoEx((HANDLE)sock, NULL);
		}
		else if (_stricmp(szIn, "gor") == 0)
		{
			TerminateThread(hThread, 0);
			bWSA = FALSE;
			goto $LABEL_CREATE;
		}
		else if(_stricmp(szIn, "wsa") == 0)
		{
			TerminateThread(hThread, 0);
			bWSA = TRUE;
			goto $LABEL_CREATE;
		}
	}
	if (sock != INVALID_SOCKET)
		closesocket(sock);
	closesocket(hsoListen);
	WaitForSingleObject(hThread, INFINITE);

	WSACleanup();
}
