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



#define	POOL_MAX_SIZE	32
#define	POOL_MIN_SIZE	4
#define PERMIT_INC_CNT	2

struct SOCK_ITEM
{
	OVERLAPPED	_ov;

	SOCKET		_sock;
	PTP_IO		_ptpSock;
	SOCKET		_listen;
	PTP_IO		_ptpLsn;
	char		_buff[512];

	SOCK_ITEM(SOCKET sock)
	{
		memset(this, 0, sizeof(*this));
		_sock = sock;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;
typedef std::set<PSOCK_ITEM> SOCK_SET;


void WINAPI Handler_SockChild(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG dwErrCode, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pCtx;

	if (dwErrCode != NO_ERROR || dwTrBytes == 0)
		goto $LABEL_CLOSE;

	if ((int)dwTrBytes > 0)
	{
		psi->_buff[dwTrBytes] = 0;
		cout << " *** Client(" << psi->_sock << ") sent : " << psi->_buff << endl;

		int lSockRet = send(psi->_sock, psi->_buff, dwTrBytes, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			dwErrCode = WSAGetLastError();
			goto $LABEL_CLOSE;
		}
	}

	StartThreadpoolIo(ptpIo);
	WSABUF wb; DWORD dwFlags = 0;
	wb.buf = psi->_buff, wb.len = sizeof(psi->_buff);
	int nSockRet = WSARecv(psi->_sock, &wb, 1, NULL, &dwFlags, &psi->_ov, NULL);
	if (nSockRet == SOCKET_ERROR)
	{
		dwErrCode = WSAGetLastError();
		if (dwErrCode != WSA_IO_PENDING)
		{
			CancelThreadpoolIo(ptpIo);
			goto $LABEL_CLOSE;
		}
	}
	return;

$LABEL_CLOSE:
	if (dwErrCode == ERROR_OPERATION_ABORTED)
		return;

	if (dwErrCode == ERROR_SUCCESS || dwErrCode == ERROR_NETNAME_DELETED)
		cout << " ==> Client " << psi->_sock << " disconnected..." << endl;
	else
		cout << " ==> Error occurred, code = " << dwErrCode << endl;

	LPFN_DISCONNECTEX pfnDisconnectEx = (LPFN_DISCONNECTEX)
		GetSockExtAPI(psi->_sock, WSAID_DISCONNECTEX);
	if (!pfnDisconnectEx(psi->_sock, NULL, TF_REUSE_SOCKET, 0))
	{
		cout << "DisconnectEx failed : " << WSAGetLastError() << endl;
		goto $LABEL_CLOSE;
	}

	StartThreadpoolIo(psi->_ptpLsn);
	LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
		GetSockExtAPI(psi->_listen, WSAID_ACCEPTEX);
	BOOL bIsOK = pfnAcceptEx
	(
		psi->_listen, psi->_sock, psi->_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, (LPOVERLAPPED)psi
	);
	if (bIsOK == FALSE && WSAGetLastError() != WSA_IO_PENDING)
	{
		cout << "AcceptEx failed : " << WSAGetLastError() << endl;
		CancelThreadpoolIo(psi->_ptpLsn);
		goto $LABEL_CLOSE;
	}

}

void WINAPI Handler_SockListen(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG dwErrCode, ULONG_PTR dwTrBytes, PTP_IO ptpLsnIo)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pov;

	if (dwErrCode != NO_ERROR)
		goto $LABEL_CLOSE;

	cout << " ==> New client " << psi->_sock << " connected..." << endl;
	Handler_SockChild(pInst, psi, &psi->_ov, NO_ERROR, -1, psi->_ptpSock);

	StartThreadpoolIo(ptpLsnIo);

	return;

$LABEL_CLOSE:
	if (dwErrCode != ERROR_OPERATION_ABORTED)
		cout << " ==> Error occurred, code = " << dwErrCode << endl;
}


VOID CALLBACK MyCGCancelCallback(PVOID pObjCtx, PVOID pCleanCtx)
{
	PSOCK_ITEM psi = (PSOCK_ITEM)pObjCtx;
	closesocket(psi->_sock);
	Sleep(1000);
	//WaitForThreadpoolIoCallbacks(psi->_ptpLsn, TRUE);
	delete psi;
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

	TP_CALLBACK_ENVIRON ce;
	InitializeThreadpoolEnvironment(&ce);
	PTP_POOL pPool = NULL;
	PTP_CLEANUP_GROUP pCG = NULL;

	SOCKET soListen = INVALID_SOCKET;
	PTP_IO ptpIoLsn = NULL;
	try
	{
		pPool = CreateThreadpool(NULL);
		if (pPool == NULL)
			throw GetLastError();
		pCG = CreateThreadpoolCleanupGroup();
		if (pCG == NULL)
			throw GetLastError();
		SetThreadpoolCallbackPool(&ce, pPool);
		SetThreadpoolCallbackCleanupGroup(&ce, pCG, MyCGCancelCallback);

		soListen = GetListenSocket(9001);
		if (soListen == INVALID_SOCKET)
			throw WSAGetLastError();
		cout << " ==> Waiting for client's connection......" << endl;

		ptpIoLsn = CreateThreadpoolIo
			((HANDLE)soListen, Handler_SockListen, (PVOID)soListen, NULL);
		if (ptpIoLsn == NULL)
			throw (int)GetLastError();

		for (int i = 0; i < PERMIT_INC_CNT; i++)
		{
			SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sock == INVALID_SOCKET)
				break;

			PSOCK_ITEM psi = new SOCK_ITEM(sock);
			psi->_ptpSock = CreateThreadpoolIo
				((HANDLE)psi->_sock, Handler_SockChild, psi, &ce);
			if (psi->_ptpSock == NULL)
				break;
			psi->_ptpLsn = ptpIoLsn;
			psi->_listen = soListen;

			StartThreadpoolIo(ptpIoLsn);
			LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
				GetSockExtAPI(soListen, WSAID_ACCEPTEX);
			BOOL bIsOK = pfnAcceptEx
			(
				soListen, psi->_sock, psi->_buff, 0,
				sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &psi->_ov
			);
			if (bIsOK == FALSE && WSAGetLastError() != WSA_IO_PENDING)
			{
				cout << "AcceptEx failed : " << WSAGetLastError() << endl;
				CancelThreadpoolIo(ptpIoLsn);
				break;
			}
		}

		getchar();
	}
	catch (int ex)
	{
		cout << "Error occurred in main, " << ex << endl;
	}

	if (ptpIoLsn != NULL)
	{
		WaitForThreadpoolIoCallbacks(ptpIoLsn, TRUE);
		CloseThreadpoolIo(ptpIoLsn);
	}
	if (soListen != INVALID_SOCKET)
		closesocket(soListen);

	if (pCG != NULL)
	{
		CloseThreadpoolCleanupGroupMembers(pCG, TRUE, (PVOID)128);
		CloseThreadpoolCleanupGroup(pCG);
	}
	if (pPool != NULL)
		CloseThreadpool(pPool);

	cout << "==== Server terminates... ==========================" << endl;
	WSACleanup();
}
