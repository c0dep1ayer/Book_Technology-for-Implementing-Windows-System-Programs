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
	LONG lRet = ::WSAIoctl
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
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	LONG lSockRet = bind(sock, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "bind failed, code : " << WSAGetLastError() << endl;
		closesocket(sock);
		return;
	}

	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));

	sa.sin_port = htons(9001);
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	LPFN_CONNECTEX pfnConnectEx = (LPFN_CONNECTEX)GetSockExtAPI(sock, WSAID_CONNECTEX);
	BOOL bIsOK = pfnConnectEx
	(
		sock, (LPSOCKADDR)&sa, sizeof(sa),
		NULL, 0, NULL, &ov
	);
	if (!bIsOK)
	{
		int nErrCode = WSAGetLastError();
		if (nErrCode != WSA_IO_PENDING)
		{
			cout << "ConnectEx failed : " << nErrCode << endl;
			closesocket(sock);
			return;
		}
	}

	DWORD dwWaitCode = WaitForSingleObject((HANDLE)sock, 3000);
	if (dwWaitCode == WAIT_FAILED)
	{
		cout << "WaitForSingleObject failed : " << nErrCode << endl;
		closesocket(sock);
		return;
	}
	
	if (dwWaitCode == WAIT_TIMEOUT)
	{
		cout << "==> Timeout occurred!!!" << endl;
		closesocket(sock);
		return;
	}

	// Connection OK!!!
	cout << "==> Connection to server Success!!!" << endl;

	char szIn[512];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		int lSockRet = strlen(szIn);
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
