#include "stdafx.h"
#include "Winsock2.h"
#include "set"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define USE_CANCELIO	1


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


WSAEVENT g_hevExit;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (g_hevExit != NULL)
		SetEvent(g_hevExit);
	return TRUE;
}

struct SOCK_ITEM;
typedef std::set<SOCK_ITEM*> SOCK_SET;
struct SOCK_ITEM : OVERLAPPED
{
	SOCKET		_sock;
	char		_buff[512];
	SOCK_SET*	_pSet;

	SOCK_ITEM(SOCKET sock, SOCK_SET* pSet)
	{
		hEvent = NULL;
		Offset = OffsetHigh = 0;
		memset(_buff, 0, sizeof(_buff));
		_sock = sock;
		_pSet = pSet;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;


VOID CALLBACK RecvSockCallback(DWORD dwErrCode, DWORD dwTranBytes, LPOVERLAPPED pOL, DWORD dwFlags)
{
	PSOCK_ITEM pSI = (PSOCK_ITEM)pOL;

	if (dwTranBytes > 0 && dwErrCode == 0)
	{
		pSI->_buff[dwTranBytes] = 0;
		cout << " *** Client(" << pSI->_sock << ") sent : " << pSI->_buff << endl;

		int lSockRet = send(pSI->_sock, pSI->_buff, dwTranBytes, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			dwErrCode = WSAGetLastError();
			goto $LABEL_CLOSE;
		}

		WSABUF wb;
		wb.buf = pSI->_buff, wb.len = sizeof(pSI->_buff);
		int nSockRet = WSARecv(pSI->_sock, &wb, 1, NULL, &dwFlags, pSI, RecvSockCallback);
		if (nSockRet == SOCKET_ERROR)
		{
			dwErrCode = WSAGetLastError();
			if (dwErrCode != WSA_IO_PENDING)
				goto $LABEL_CLOSE;
		}
		return;
	}

$LABEL_CLOSE:
#if (USE_CANCELIO == 0)
	if (dwErrCode != WSAECONNABORTED)
#else
	if (dwErrCode != ERROR_OPERATION_ABORTED)
#endif
	{
		if (dwErrCode == ERROR_SUCCESS || dwErrCode == WSAECONNRESET)
			cout << " ==> Client " << pSI->_sock << " disconnected..." << endl;
		else
			cout << " ==> Error occurred, code = " << dwErrCode << endl;
		closesocket(pSI->_sock);
		pSI->_pSet->erase(pSI);
		delete pSI;
	}
	else
		cout << " ==> Child socket " << pSI->_sock << " closed." << endl;
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
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		return;
	}
	g_hevExit = CreateEvent(NULL, TRUE, FALSE, NULL);

	SOCKET hsoListen = GetListenSocket(9001);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;

	WSAEVENT hEvent = WSACreateEvent();
	WSAEventSelect(hsoListen, hEvent, FD_ACCEPT);

	SOCK_SET siols;
	HANDLE arSynObjs[2] = { g_hevExit, hEvent };
	while (true)
	{
		DWORD dwWaitRet = WaitForMultipleObjectsEx(2, arSynObjs, FALSE, INFINITE, TRUE);
		if (dwWaitRet == WAIT_FAILED)
		{
			cout << "WaitForMultipleObjectsEx failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitRet == WAIT_OBJECT_0)
			break;

		if (dwWaitRet == WAIT_IO_COMPLETION)
			continue;

		WSANETWORKEVENTS ne;
		WSAEnumNetworkEvents(hsoListen, hEvent, &ne);
		if (ne.lNetworkEvents & FD_ACCEPT)
		{
			int nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
			if (nErrCode != 0)
			{
				cout << " ==> Error occurred, code = " << nErrCode << endl;
				break;
			}

			SOCKET sock = accept(hsoListen, NULL, NULL);
			if (sock == INVALID_SOCKET)
			{
				nErrCode = WSAGetLastError();
				if (nErrCode != WSAEINTR)
					cout << " ==> Error occurred, code = " << nErrCode << endl;
				break;
			}
			cout << " ==> New client " << sock << " connected" << endl;

			PSOCK_ITEM pSI = new SOCK_ITEM(sock, &siols);

			WSABUF wb;
			wb.buf = pSI->_buff, wb.len = sizeof(pSI->_buff);
			DWORD dwFlags = 0;
			int nSockRet = WSARecv(sock, &wb, 1, NULL, &dwFlags, pSI, RecvSockCallback);
			if (nSockRet == SOCKET_ERROR && (nErrCode = WSAGetLastError()) != WSA_IO_PENDING)
			{
				cout << "ReadFile failed : " << nErrCode << endl;
				closesocket(sock);
				delete pSI;
			}
			else
				siols.insert(pSI);
		}
	}

	if (hsoListen != INVALID_SOCKET)
		closesocket(hsoListen);
	for (SOCK_SET::iterator it = siols.begin(); it != siols.end(); it++)
	{
		PSOCK_ITEM pSI = *it;
#if (USE_CANCELIO == 0)
		closesocket(pSI->_sock);
		SleepEx(INFINITE, TRUE);
#else
		CancelIoEx((HANDLE)pSI->_sock, NULL);
		SleepEx(INFINITE, TRUE);
		closesocket(pSI->_sock);
#endif
		delete pSI;
	}
	CloseHandle(hEvent);
	CloseHandle(g_hevExit);

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
