#include "stdafx.h"
#include "Winsock2.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

void _tmain(int argc, _TCHAR* argv[])
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
	sa.sin_family		= AF_INET;
	sa.sin_port			= htons(9001);
	sa.sin_addr.s_addr	= inet_addr("127.0.0.1");

	int lSockRet = connect(sock, (LPSOCKADDR)&sa, sizeof(sa));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "connect failed : " << WSAGetLastError() << endl;
		closesocket(sock);
		return;
	}
	cout << "==> Connection to server Success!!!" << endl;

	HANDLE hFile = CreateFile
	(
		argv[1], GENERIC_READ, 0, NULL,
		OPEN_EXISTING, 0, NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		cout << argv[1] << " open failed, code : " << GetLastError() << endl;
		return;
	}

	LONGLONG llSentSize = 0;
	BYTE arBuff[4096];
	while (true)
	{
		DWORD dwTrBytes = 0;
		BOOL bIsOK = ReadFile
		(
			hFile, arBuff, sizeof(arBuff), &dwTrBytes, NULL
		);
		if (!bIsOK)
		{
			cout << "ReadFile failed : " << GetLastError() << endl;
			break;
		}
		if (dwTrBytes == 0)
			break;

		lSockRet = send(sock, (PCSTR)arBuff, dwTrBytes, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "send failed : " << WSAGetLastError() << endl;
			break;
		}
		llSentSize += dwTrBytes;
		printf(" <= Sent bytes : %I64d\n", llSentSize);
	}
	closesocket(sock);
	CloseHandle(hFile);
	printf(" <= Sent total %I64d bytes, program terminates...\n", llSentSize);

	WSACleanup();
}
