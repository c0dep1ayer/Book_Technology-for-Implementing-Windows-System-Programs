#include "stdafx.h"
#include "Winsock2.h"
#include "set"
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


HWND g_hMsgWnd = NULL;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (g_hMsgWnd != NULL)
		PostMessage(g_hMsgWnd, WM_DESTROY, 0, 0);
	return TRUE;
}

#define WM_ASYNC_SOCKET	WM_USER + 500
typedef std::set<SOCKET> SOCK_SET;

LRESULT CALLBACK WndSockProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static SOCK_SET* s_pSocks = NULL;

	switch (uMsg)
	{
		case WM_CREATE:
		{
			LPCREATESTRUCT pCS = (LPCREATESTRUCT)lParam;
			s_pSocks = (SOCK_SET*)pCS->lpCreateParams;
		}
		return 0;

		case WM_DESTROY:
		{
			PostQuitMessage(0);
		}
		return 0;

		case WM_ASYNC_SOCKET:
		{
			LONG lErrCode = WSAGETSELECTERROR(lParam);
			if (lErrCode != 0)	//WSAECONNABORTED
			{
				SOCKET sock = (SOCKET)wParam;
				cout << "~~~ socket " << sock << " failed: " << lErrCode << endl;
				closesocket(sock);
				s_pSocks->erase(sock);
				return 0;
			}

			switch (WSAGETSELECTEVENT(lParam))
			{
				case FD_ACCEPT:
				{
					SOCKET hsoListen = (SOCKET)wParam;
					SOCKET sock = accept(hsoListen, NULL, NULL);
					if (sock == INVALID_SOCKET)
					{
						cout << "accept failed, code : " << WSAGetLastError() << endl;
						break;
					}

					WSAAsyncSelect(sock, hWnd, WM_ASYNC_SOCKET, FD_READ | FD_CLOSE);
					s_pSocks->insert(sock);
					cout << " ==> New client " << sock << " connected" << endl;
				}
				break;

				case FD_READ:
				{
					SOCKET sock = (SOCKET)wParam;
					char szBuff[512];
					int lSockRet = recv(sock, szBuff, sizeof(szBuff), 0);
					szBuff[lSockRet] = 0;
					cout << " *** Client(" << sock << ") sent : " << szBuff << endl;

					lSockRet = send(sock, szBuff, lSockRet, 0);
					if (lSockRet == SOCKET_ERROR)
						cout << "send failed, code : " << WSAGetLastError() << endl;
				}
				break;

				case FD_CLOSE:
				{
					SOCKET sock = (SOCKET)wParam;
					closesocket(sock);
					s_pSocks->erase(sock);
					cout << " ==> Client " << sock << " disconnected..." << endl;
				}
				break;
			}
		}
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
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

	SOCK_SET socks;

	WNDCLASS wcls;
	memset(&wcls, 0, sizeof(wcls));
	wcls.lpfnWndProc = WndSockProc;
	wcls.hInstance = GetModuleHandle(NULL);
	wcls.lpszClassName = _T("WSAAyncSelect");
	if (!RegisterClass(&wcls))
	{
		cout << "send failed, code : " << GetLastError() << endl;
		return;
	}

	HWND hWnd = CreateWindowEx
	(
		0, wcls.lpszClassName, NULL, 0, 0, 0, 0, 0,
		HWND_MESSAGE, NULL, wcls.hInstance,
		&socks
	);
	if (!hWnd)
	{
		cout << "send failed, code : " << GetLastError() << endl;
		return;
	}
	g_hMsgWnd = hWnd;
	cout << " ==> Creating hidden window success!!!" << endl;

	SOCKET hsoListen = GetListenSocket(9001);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	WSAAsyncSelect(hsoListen, hWnd, WM_ASYNC_SOCKET, FD_ACCEPT);
	cout << " ==> Waiting for client's connection......" << endl;

	////////////////////////////////////////////////////////////////////////////
	// Message Loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	////////////////////////////////////////////////////////////////////////////

	if (hsoListen != INVALID_SOCKET)
		closesocket(hsoListen);
	for (SOCK_SET::iterator it = socks.begin(); it != socks.end(); it++)
		closesocket(*it);
	cout << "Listen socket closed, program terminates..." << endl;

	WSACleanup();
}
