#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "map"
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

struct SOCK_ITEM : OVERLAPPED
{
	SOCKET	_sock;
	char	_buff[512];

	SOCK_ITEM(SOCKET sock)
	{
		memset(this, 0, sizeof(*this));
		_sock = sock;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;


#define MAX_CLI_CNT	63
void _tmain()
{
	WSADATA	wsd;
	int nIniCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nIniCode)
	{
		cout << "WSAStartup failed with error : " << nIniCode << endl;
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

	int			nSockCnt = 0;
	SOCKET		arSocks[MAX_CLI_CNT + 1];
	PSOCK_ITEM	arItems[MAX_CLI_CNT + 1];
	memset(arSocks, 0xFF, (MAX_CLI_CNT + 1) * sizeof(SOCKET));
	memset(arItems,    0, (MAX_CLI_CNT + 1) * sizeof(PSOCK_ITEM));
	arSocks[0] = hsoListen;

	while (true)
	{
		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
			break;

		SOCK_ITEM si(sock);
		LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)GetSockExtAPI(hsoListen, WSAID_ACCEPTEX);
		BOOL bIsOK = pfnAcceptEx
		(
			hsoListen, sock, si._buff, 0,
			sizeof(SOCKADDR_IN) + 16,
			sizeof(SOCKADDR_IN) + 16,
			NULL, (LPOVERLAPPED)&si
		);
		if (!bIsOK)
		{
			int nErrCode = WSAGetLastError();
			if (nErrCode != WSA_IO_PENDING)
			{
				cout << "AcceptEx failed, code : " << WSAGetLastError() << endl;
				closesocket(sock);
				break;
			}
		}

		DWORD dwWaitRet = MsgWaitForMultipleObjects
		(
			nSockCnt + 1, (PHANDLE)arSocks, FALSE, INFINITE, QS_ALLPOSTMESSAGE
		);
		if (dwWaitRet == WAIT_FAILED)
		{
			cout << "WaitForMultipleObjectsEx failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitRet == WAIT_OBJECT_0 + nSockCnt + 1)
			break;

		DWORD		dwFlags	 = 0;
		LONG		nErrCode = ERROR_SUCCESS;
		PSOCK_ITEM	pSI		 = NULL;
		if (dwWaitRet == WAIT_OBJECT_0)
		{
			pSI = new SOCK_ITEM(sock);
			arSocks[nSockCnt + 1] = sock;
			arItems[nSockCnt + 1] = pSI;
			nSockCnt++;
			cout << " ==> New client " << sock << " connected" << endl;
		}
		else
		{
			int nIndex = (int)(dwWaitRet - WAIT_OBJECT_0);
			SOCKET sock = arSocks[nIndex];
			pSI = arItems[nIndex];

			DWORD dwTrBytes = 0;
			if (!WSAGetOverlappedResult(sock, pSI, &dwTrBytes, FALSE, &dwFlags))
			{
				nErrCode = WSAGetLastError();
				goto $LABEL_CLOSE;
			}
			else
			{
				if (dwTrBytes == 0)
					goto $LABEL_CLOSE;

				pSI->_buff[dwTrBytes] = 0;
				cout << " *** Client(" << pSI->_sock << ") sent : " << pSI->_buff << endl;

				int lSockRet = send(pSI->_sock, pSI->_buff, dwTrBytes, 0);
				if (lSockRet == SOCKET_ERROR)
				{
					nErrCode = WSAGetLastError();
					goto $LABEL_CLOSE;
				}
			}
		}

		WSABUF wb;
		wb.buf = pSI->_buff, wb.len = sizeof(pSI->_buff);
		int nSockRet = WSARecv(pSI->_sock, &wb, 1, NULL, &dwFlags, pSI, NULL);
		if (nSockRet == SOCKET_ERROR)
		{
			nErrCode = WSAGetLastError();
			if (nErrCode != WSA_IO_PENDING)
				goto $LABEL_CLOSE;
		}
		continue;

	$LABEL_CLOSE:
		int nIndex = (int)(dwWaitRet - WAIT_OBJECT_0);
		for (int i = nIndex; i < nSockCnt; i++)
		{
			arSocks[i] = arSocks[i + 1];
			arItems[i] = arItems[i + 1];
		}
		arSocks[nSockCnt] = INVALID_SOCKET;
		arItems[nSockCnt] = NULL;
		nSockCnt--;

		if (nErrCode != WSAECONNABORTED)
		{
			if (nErrCode == ERROR_SUCCESS || nErrCode == WSAECONNRESET)
				cout << " ==> Client " << pSI->_sock << " disconnected..." << endl;
			else
				cout << " ==> Error occurred, code = " << nErrCode << endl;
			closesocket(pSI->_sock);
			delete pSI;
		}
	}

	if (hsoListen != INVALID_SOCKET)
		closesocket(hsoListen);
	for (int i = 1; i < nSockCnt + 1; i++)
	{
		if (arSocks[i] != INVALID_SOCKET)
			closesocket(arSocks[i]);
		if (arItems[i] != NULL)
			delete arItems[i];
	}

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
