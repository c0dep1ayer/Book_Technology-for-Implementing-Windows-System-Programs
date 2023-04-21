#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "set"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")


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


#define BUFF_SIZE	256
struct SOCK_ITEM : OVERLAPPED
{
	SOCKET	_sock;
	PTP_IO	_ptpIo;
	BOOL	_connected;
	char	_buff[BUFF_SIZE];

	SOCK_ITEM()
	{
		memset(this, 0, sizeof(*this));
		_sock = INVALID_SOCKET;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;


VOID CALLBACK Handler_SockRead(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	HANDLE hThread = (HANDLE)pCtx;
	PSOCK_ITEM psi = (PSOCK_ITEM)pov;
	if (ior != NO_ERROR || (psi->_connected && dwTrBytes == 0))
	{
		if (!psi->_connected || (dwTrBytes != 0 && ior != ERROR_OPERATION_ABORTED))
			cout << "Handler_SockRead error : " << ior << endl;
		CancelSynchronousIo(hThread);
		return;
	}

	if (psi->_connected == TRUE)
	{
		psi->_buff[dwTrBytes] = 0;
		char szOut[128];
		sprintf(szOut, "PIPE >> %s", psi->_buff);
		cout << szOut << endl;
	}
	else
	{
		SOCKADDR_IN sa;
		int saLen = sizeof(sa);
		getsockname(psi->_sock, (PSOCKADDR)&sa, &saLen);
		TCHAR szTitle[64];
		wsprintf(szTitle, _T("SOCK\\%08X_%04X"), sa.sin_addr.s_addr, sa.sin_port);
		SetConsoleTitle(szTitle);
		psi->_connected = TRUE;
	}

	StartThreadpoolIo(ptpIo);
	WSABUF wb; DWORD dwFlags = 0;
	wb.buf = psi->_buff, wb.len = sizeof(psi->_buff);
	int nSockRet = WSARecv(psi->_sock, &wb, 1, NULL, &dwFlags, psi, NULL);
	if (nSockRet == SOCKET_ERROR)
	{
		ior = WSAGetLastError();
		if (ior != WSA_IO_PENDING)
		{
			cout << "WSARecv error : " << ior << endl;
			CancelThreadpoolIo(ptpIo);
			CancelSynchronousIo(hThread);
		}
	}
}

DWORD WINAPI Handler_StdRead(PVOID pParam)
{
	SOCKET sock = (SOCKET)pParam;
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

	char szIn[BUFF_SIZE];
	while (true)
	{
		int nReadCnt = 0;
		if (!ReadFile(hStdIn, szIn, BUFF_SIZE, (PDWORD)&nReadCnt, NULL))
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_OPERATION_ABORTED)
				cout << "ReadFile from STDIN error : " << dwErrCode << endl;
			break;
		}

		nReadCnt -= 2;
		if (nReadCnt <= 0)
			continue;

		szIn[nReadCnt] = 0;
		if (stricmp(szIn, "quit") == 0)
			break;

		int lSockRet = send(sock, szIn, nReadCnt, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			cout << "send to PIPE error : " << WSAGetLastError() << endl;
			break;
		}
	}
	CloseHandle(hStdIn);

	return 0;
}

void _tmain()
{
	WSADATA	wsd;
	int nInitCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nInitCode)
	{
		cout << "WSAStartup failed with error : " << nInitCode << endl;
		return;
	}

	SOCK_ITEM si;
	HANDLE hThread = NULL;
	try
	{
		si._sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (si._sock == INVALID_SOCKET)
			throw (DWORD)WSAGetLastError();

		SOCKADDR_IN	sa;
		memset(&sa, 0, sizeof(SOCKADDR_IN));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
		LONG lSockRet = bind(si._sock, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
		if (lSockRet == SOCKET_ERROR)
			throw (DWORD)WSAGetLastError();

		DWORD dwThrId = 0;
		hThread = CreateThread
		(
			NULL, 0, Handler_StdRead, (PVOID)si._sock, 0, &dwThrId
		);
		if (hThread == NULL)
			throw GetLastError();

		si._ptpIo = CreateThreadpoolIo((HANDLE)si._sock, Handler_SockRead, hThread, NULL);
		if (si._ptpIo == NULL)
			throw GetLastError();

		StartThreadpoolIo(si._ptpIo);
		sa.sin_port = htons(9001);
		sa.sin_addr.s_addr = inet_addr("127.0.0.1");
		LPFN_CONNECTEX pfnConnectEx = (LPFN_CONNECTEX)GetSockExtAPI(si._sock, WSAID_CONNECTEX);
		BOOL bIsOK = pfnConnectEx
		(
			si._sock, (PSOCKADDR)&sa, sizeof(sa), NULL, 0, NULL, &si
		);
		if (!bIsOK)
		{
			DWORD dwErrCode = WSAGetLastError();
			if (dwErrCode != WSA_IO_PENDING)
			{
				CancelThreadpoolIo(si._ptpIo);
				throw dwErrCode;
			}
		}

		WaitForSingleObject(hThread, INFINITE);
	}
	catch (DWORD e)
	{
		cout << "Main Thread error occurred : " << e << endl;
	}
	if (si._ptpIo != NULL)
	{
		CancelIoEx((HANDLE)si._sock, NULL);
		WaitForThreadpoolIoCallbacks(si._ptpIo, TRUE);
		CloseThreadpoolIo(si._ptpIo);
	}
	if (si._sock != INVALID_SOCKET)
		closesocket(si._sock);
	if (hThread != NULL)
		CloseHandle(hThread);

	WSACleanup();
}
