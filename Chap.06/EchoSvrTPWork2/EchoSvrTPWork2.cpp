#include "stdafx.h"
#include "Winsock2.h"
#include "list"
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



typedef std::list<SOCKET> SOCK_LIST;
struct WORK_ENV
{
	CRITICAL_SECTION _cs;
	SOCK_LIST		 _list;
};
typedef WORK_ENV* PWORK_ENV;

VOID WINAPI ChildSockProc(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WORK ptpWork)
{
	PWORK_ENV pwe = (PWORK_ENV)pCtx;
	SOCKET sock = INVALID_SOCKET;

	EnterCriticalSection(&pwe->_cs);
	sock = pwe->_list.back();
	LeaveCriticalSection(&pwe->_cs);

	char szBuff[512];
	while (true)
	{
		int lSockRet = recv(sock, szBuff, sizeof(szBuff), 0);
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
	EnterCriticalSection(&pwe->_cs);
	pwe->_list.remove(sock);
	LeaveCriticalSection(&pwe->_cs);
	closesocket(sock);
}


SOCKET g_sockMain = INVALID_SOCKET;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (g_sockMain != INVALID_SOCKET)
		closesocket(g_sockMain);
	return TRUE;
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

	SOCKET hsoListen = GetListenSocket(9001);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	g_sockMain = hsoListen;
	cout << " ==> Waiting for client's connection......" << endl;

	WORK_ENV ev;
	InitializeCriticalSection(&ev._cs);
	PTP_WORK pWork = CreateThreadpoolWork(ChildSockProc, &ev, NULL);
	while (true)
	{
		SOCKET sock = accept(hsoListen, NULL, NULL);
		if (sock == INVALID_SOCKET)
		{
			cout << "accept failed, code : " << WSAGetLastError() << endl;
			break;
		}
		cout << " ==> New client " << sock << " connected..." << endl;

		EnterCriticalSection(&ev._cs);
		ev._list.push_back(sock);
		LeaveCriticalSection(&ev._cs);
		SubmitThreadpoolWork(pWork);
	}

	for (SOCK_LIST::iterator it = ev._list.begin(); it != ev._list.end(); it++)
		closesocket(*it);
	WaitForThreadpoolWorkCallbacks(pWork, TRUE);
	CloseThreadpoolWork(pWork);
	DeleteCriticalSection(&ev._cs);

	cout << "==== Server terminates... ==========================" << endl;
	WSACleanup();
}
