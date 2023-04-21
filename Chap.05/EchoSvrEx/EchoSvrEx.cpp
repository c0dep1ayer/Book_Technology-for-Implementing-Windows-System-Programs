#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")


PVOID GetSockExtAPI(SOCKET sock, GUID guidFn)
{
	PVOID pfnEx = NULL;
	GUID guid = guidFn;
	DWORD dwBytes = 0;
	LONG lRet = WSAIoctl
	(
		sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid, sizeof(guid), &pfnEx,
		sizeof(pfnEx), &dwBytes, NULL, NULL
	);
	if (lRet == SOCKET_ERROR)
	{
		cout << "WSAIoctl failed, code : " << WSAGetLastError() << endl;
		return NULL;
	}
	return pfnEx;
}

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

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}

	int nSize = (sizeof(SOCKADDR_IN) + 16) * 2;
	OVERLAPPED	ov;
	char		szBuff[4096];
	DWORD		dwRecvLen = 256;
	memset(&ov, 0, sizeof(ov));

	LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)GetSockExtAPI(hsoListen, WSAID_ACCEPTEX);
	BOOL bIsOK = pfnAcceptEx
	(
		hsoListen, sock, szBuff, dwRecvLen,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &ov
	);
	if (!bIsOK)
	{
		int nErrCode = WSAGetLastError();
		if (nErrCode != WSA_IO_PENDING)
		{
			cout << "AcceptEx failed, code : " << WSAGetLastError() << endl;
			closesocket(sock);
			closesocket(hsoListen);
			WSACleanup();
			return;
		}
	}

	DWORD dwWaitRet = WaitForSingleObject((HANDLE)hsoListen, INFINITE);
	if (dwWaitRet == WAIT_FAILED)
	{
		cout << "WaitForMultipleObjectsEx failed : " << GetLastError() << endl;
		return;
	}


	PSOCKADDR psaLoc = NULL, psaRem = NULL;
	INT nsiLoc = 0, nsiRem = 0;
	WSAEFAULT;
	LPFN_GETACCEPTEXSOCKADDRS pfnGetSockAddr = (LPFN_GETACCEPTEXSOCKADDRS)
		GetSockExtAPI(hsoListen, WSAID_GETACCEPTEXSOCKADDRS);
	pfnGetSockAddr
	(
		szBuff, dwRecvLen,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&psaLoc, &nsiLoc, &psaRem, &nsiRem
	);
	SOCKADDR_IN saLoc = *((PSOCKADDR_IN)psaLoc);
	SOCKADDR_IN saRem = *((PSOCKADDR_IN)psaRem);

	printf(" ==> New client %d  connected...\n", sock);
	printf("     Client %s:%d -> Server%s:%d\n",
		inet_ntoa(saRem.sin_addr), htons(saRem.sin_port),
		inet_ntoa(saLoc.sin_addr), htons(saLoc.sin_port));
	if (dwRecvLen > 0)
	{
		szBuff[ov.InternalHigh] = 0;
		printf("     Recv data : %s\n", szBuff);
	}

	closesocket(sock);
	closesocket(hsoListen);

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
