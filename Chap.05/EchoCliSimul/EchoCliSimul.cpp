#include "stdafx.h"
#include "Winsock2.h"
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


SOCKET g_sockMain = INVALID_SOCKET;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (g_sockMain != INVALID_SOCKET)
		closesocket(g_sockMain);
	return TRUE;
}

DWORD WINAPI ChildSockProc(PVOID pParam)
{
	HANDLE hExit = (HANDLE)pParam;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		cout << "socket failed, code : " << WSAGetLastError() << endl;
		return 0;
	}

	WaitForSingleObject(hExit, INFINITE);

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(9001);
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	int lSockRet = connect(sock, (LPSOCKADDR)&sa, sizeof(sa));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "connect failed : " << WSAGetLastError() << endl;
		closesocket(sock);
		return 0;
	}
	cout << "==> Connection to server Success!!!" << endl;

	char szIn[512];
	while (true)
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		lSockRet = sprintf(szIn, "... Thread %d test message => "
			"%04d/%02d/%02d-%02d:%02d:%02d+%03d\n",
			GetCurrentThreadId(), st.wYear, st.wMonth, st.wDay, 
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
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

		DWORD dwDelay = (DWORD)(rand() % 3000 + 10);
		if (WaitForSingleObject(hExit, dwDelay) == WAIT_OBJECT_0)
			break;
	}
	closesocket(sock);
	cout << "==> socket closed, program terminates..." << endl;

	return 0;
}


#define MAX_CLI_CNT	10

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

	HANDLE hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	for (int i = 0; i < 256; i++)
	{
		DWORD dwThrId = 0;
		HANDLE hThread = CreateThread(NULL, 0, ChildSockProc, hExit, 0, &dwThrId);
	}

	getchar();
	SetEvent(hExit);
	Sleep(10);
	ResetEvent(hExit);
	getchar();
	SetEvent(hExit);

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
