#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "Shlwapi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Shlwapi.lib")


PVOID GetSockExtAPI(SOCKET sock, GUID guidFn)
{
	PVOID pfnEx = NULL;
	GUID guid = guidFn;
	DWORD dwBytes = 0;
	LONG lRet = WSAIoctl
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

	BY_HANDLE_FILE_INFORMATION bhfi;
	GetFileInformationByHandle(hFile, &bhfi);
	WIN32_FIND_DATA fd;
	memset(&fd, 0, sizeof(fd));
	fd.dwFileAttributes = bhfi.dwFileAttributes;
	fd.ftCreationTime	= bhfi.ftCreationTime;
	fd.ftLastAccessTime = bhfi.ftLastAccessTime;
	fd.ftLastWriteTime	= bhfi.ftLastWriteTime;
	fd.nFileSizeHigh	= bhfi.nFileSizeHigh;
	fd.nFileSizeLow		= bhfi.nFileSizeLow;

	PCTSTR pszFileName =PathFindFileName(argv[1]);
	wcscpy(fd.cFileName, pszFileName);

	TRANSMIT_FILE_BUFFERS tfb;
	tfb.Head = &fd;
	tfb.HeadLength = sizeof(fd);
	tfb.Tail = NULL;
	tfb.TailLength = 0;

	LPFN_TRANSMITFILE pfnTransFile = (LPFN_TRANSMITFILE)
		GetSockExtAPI(sock, WSAID_TRANSMITFILE);
	BOOL bIsOK = pfnTransFile
	(
		sock, hFile, 0, 65536, NULL, &tfb, 0
	);
	if (!bIsOK)
		cout << "TransmitFile failed : " << WSAGetLastError() << endl;

	closesocket(sock);
	CloseHandle(hFile);
	printf(" <= Sent total %I64d bytes, program terminates...\n", llSize.QuadPart);

	WSACleanup();
}
