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



#define MAX_COPY_CNT  11
void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		cout << "Uasge : MultiCopyIOCP SourceFile1 SourceFile2 SourceFile3 ..." << endl;
		return;
	}

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
	sa.sin_port = htons(9001);
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
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
		argv[1], GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		cout << argv[1] << " open failed, code : " << GetLastError() << endl;
		return;
	}
	LARGE_INTEGER llSize;
	GetFileSizeEx(hFile, &llSize);

	HANDLE hMmf = CreateFileMapping
	(
		hFile, NULL, PAGE_READONLY, llSize.HighPart, llSize.LowPart, NULL
	);
	if (hMmf == NULL)
	{
		cout << argv[1] << "CreateFileMapping failed: " << GetLastError() << endl;
		return;
	}

#define COPY_UNIT 1048576
	LARGE_INTEGER llTrans = { 0, };
	while (llTrans.QuadPart < llSize.QuadPart)
	{
		DWORD dwCpySize = COPY_UNIT;
		if (llTrans.QuadPart + dwCpySize > llSize.QuadPart)
			dwCpySize = (llSize.QuadPart - llTrans.QuadPart);
		PVOID pData = MapViewOfFile(hMmf, FILE_MAP_READ, llTrans.HighPart, llTrans.LowPart, dwCpySize);

		TRANSMIT_PACKETS_ELEMENT tp;
		tp.dwElFlags = TP_ELEMENT_MEMORY;
		tp.cLength	 = dwCpySize;
		tp.pBuffer	 = pData;

		LPFN_TRANSMITPACKETS pfnTransPackets = (LPFN_TRANSMITPACKETS)
			GetSockExtAPI(sock, WSAID_TRANSMITPACKETS);
		BOOL bIsOK = pfnTransPackets
		(
			sock, &tp, 1, 0, NULL, 0
		);
		if (!bIsOK)
		{
			cout << "TransmitFile failed : " << GetLastError() << endl;
			break;
		}
		UnmapViewOfFile(pData);
		llTrans.QuadPart += dwCpySize;
	}
	closesocket(sock);
	CloseHandle(hMmf);
	CloseHandle(hFile);

	WSACleanup();
}
