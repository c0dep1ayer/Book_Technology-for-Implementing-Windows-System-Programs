#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define USE_OL	0

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
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		cout << argv[1] << " open failed, code : " << GetLastError() << endl;
		return;
	}
	LARGE_INTEGER llSize;
	GetFileSizeEx(hFile, &llSize);

	LPFN_TRANSMITFILE pfnTransFile = (LPFN_TRANSMITFILE)
		GetSockExtAPI(sock, WSAID_TRANSMITFILE);
#if (USE_OL == 0)
	BOOL bIsOK = pfnTransFile
	(
		sock, hFile, 0, 65536, NULL, NULL, 0
	);
	if (!bIsOK)
		cout << "TransmitFile failed : " << WSAGetLastError() << endl;
#else
	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	BOOL bIsOK = pfnTransFile
	(
		sock, hFile, 0, 65536, &ov, NULL, 0
	);
	if (!bIsOK)
	{
		int nErrCode = WSAGetLastError();
		if (nErrCode != WSA_IO_PENDING)
			cout << "TransmitFile failed : " << nErrCode << endl;
	}
	WaitForSingleObject((HANDLE)sock, INFINITE);
#endif

	closesocket(sock);
	CloseHandle(hFile);
	printf(" <= Sent total %I64d bytes, program terminates...\n", llSize.QuadPart);

	WSACleanup();
}
