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
	SOCKET sock = (SOCKET)pParam;

	WIN32_FIND_DATA fd;
	int lSockRet = recv(sock, (PSTR)&fd, sizeof(fd), 0);
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "recv failed : " << WSAGetLastError() << endl;
		return 0;
	}
	if (lSockRet != sizeof(fd))
	{
		printf("Size Mismatch, Recv=%d\n", lSockRet);
		return 0;
	}

	WCHAR szFilePath[MAX_PATH];
	wsprintf(szFilePath, L"D:\\temp\\%s", fd.cFileName);
	HANDLE hFile = CreateFileW
	(
		szFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		cout << fd.cFileName << " create failed, code : " << GetLastError() << endl;
		return 0;
	}
	LARGE_INTEGER llSize;
	llSize.LowPart = fd.nFileSizeLow, llSize.HighPart = fd.nFileSizeHigh;

	BYTE arBuff[4096];
	LONGLONG llWroteSize = 0;
	while (llWroteSize < llSize.QuadPart)
	{
		lSockRet = recv(sock, (PSTR)arBuff, sizeof(arBuff), 0);
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

		DWORD dwTrBytes = 0;
		BOOL  bIsOK = WriteFile
		(
			hFile, arBuff, lSockRet, &dwTrBytes, NULL
		);
		if (!bIsOK)
		{
			cout << "WriteFile failed : " << GetLastError() << endl;
			break;
		}
		llWroteSize += dwTrBytes;
		printf(" => Wrote bytes : %I64d\n", llWroteSize);
	}
	CloseHandle(hFile);
	closesocket(sock);
	printf(" <= File written total %I64d bytes...\n", llWroteSize);

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

	SOCKET hsoListen = GetListenSocket(9001);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	g_sockMain = hsoListen;
	cout << " ==> Waiting for client's connection......" << endl;

	while (true)
	{
		SOCKET sock = accept(hsoListen, NULL, NULL);
		if (sock == INVALID_SOCKET)
		{
			cout << "accept failed, code : " << WSAGetLastError() << endl;
			break;
		}
		cout << " ==> New client " << sock << " connected..." << endl;

		DWORD dwThrId = 0;
		HANDLE hThread = CreateThread(NULL, 0, ChildSockProc, (PVOID)sock, 0, &dwThrId);
	}
	closesocket(hsoListen);

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
