//
// IOCP와 윈속 확장 API를 이용해서 ACCEPT 및 RECV를 IOCP에 연결하여 비동기 처리를 수행.
// 접속 해제 시 소켓을 닫지 않고 ACCEPT에 재사용하도록 소켓 풀 처리.
// 소켓 풀이 부족할 경우 증가 처리를 위해 WSAEventSelect API를 사용.
// 위의 과정을 통해 대량 클라이언트 접속 처리의 예를 보여주는 간단한 에코 서버 구현.
//
#include "stdafx.h"
#include "Winsock2.h"
#include "Mswsock.h"
#include "set"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")


// 비동기 입출력을 위한 OVERLAPPED 상속 구초체 정의
struct SOCK_ITEM : OVERLAPPED
{
	SOCKET	_sock;
	char	_buff[4096];

	SOCK_ITEM()
	{
		hEvent = NULL;
		Offset = OffsetHigh = 0;
		memset(_buff, 0, sizeof(_buff));
		_sock = INVALID_SOCKET;
	}
	SOCK_ITEM(SOCKET sock)
	{
		hEvent = NULL;
		Offset = OffsetHigh = 0;
		memset(_buff, 0, sizeof(_buff));
		_sock = sock;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;
// 자식 소켓 및 소켓 풀 관리를 위한 SET 정의
typedef std::set<PSOCK_ITEM>  SOCK_SET;


// 쓰레드 풀 쓰레드에 매개변수로 넘겨줄 정보를 담고 있는 구조체
struct IOCP_ENV
{
	CRITICAL_SECTION _cs;
	SOCK_SET		_socks;		// 연결된 자식 소켓 관리용 셋
	SOCK_SET		_spool;		// 접속 대기 소켓 풀 관리용 셋
	HANDLE			_iocp;		// ICOP 핸들
	SOCKET			_listen;	// 리슨 소켓

	IOCP_ENV()
	{
		_iocp   = NULL;
		_listen = INVALID_SOCKET;
		InitializeCriticalSection(&_cs);
	}
	~IOCP_ENV()
	{
		DeleteCriticalSection(&_cs);
	}
};
typedef IOCP_ENV* PIOCP_ENV;

#define SVR_DEF_PORT	9001
#define PERMIT_INC_CNT	50	// 소켓 풀 엔트리 증가 수

// IOCP 내의 식별 키
#define IOCP_KEY_LISTEN	 1	// 리슨 소켓 시그널
#define IOCP_KEY_CHILD	 2	// 자식 소켓 시그널
#define IOCP_KEY_EXIT	 3	// 서버 종료 시그널


// 콘솔 상에서의 Ctrl-C 키 등의 처리를 위한 핸들러
HANDLE g_hevExit = NULL;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (g_hevExit != NULL)
		SetEvent(g_hevExit);
	return TRUE;
}



// 쓰레드 풀 쓰레드 엔트리 함수
DWORD WINAPI IocpSockRecvProc(PVOID pParam);

PVOID GetSockExtAPI(SOCKET sock, GUID guidFn);
SOCKET GetListenSocket(short shPortNo, int nBacklog = SOMAXCONN);
void RemoveFailedSockect(PIOCP_ENV pIE, PSOCK_ITEM pSI);
void IncreaseAcceptSockets(PIOCP_ENV pIE, int nIncCnt = PERMIT_INC_CNT);


void _tmain()
{
	WSADATA	wsd;
	int nErrCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nErrCode)
	{
		cout << "WSAStartup failed with error : " << nErrCode << endl;
		return;
	}

	// 서버 종료 이벤트 생성
	g_hevExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_hevExit == NULL)
	{
		cout << "CreateEvent failed, code : " << GetLastError() << endl;
		WSACleanup();
		return;
	}

	// Ctrl Handler 설치
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		WSACleanup();
		return;
	}


	// CPU 개수 획득
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	PHANDLE parThreads = new HANDLE[si.dwNumberOfProcessors];

	IOCP_ENV ie;
	try
	{
		// 리슨 소켓 획득
		ie._listen = GetListenSocket(SVR_DEF_PORT);
		if (ie._listen == INVALID_SOCKET)
			throw HRESULT_FROM_WIN32(WSAGetLastError());
		cout << " ==> Waiting for client's connection......" << endl;

		// IOCP 생성 및 리슨 소켓을 IOCP에 등록
		ie._iocp = CreateIoCompletionPort
		(
			(HANDLE)ie._listen, NULL, IOCP_KEY_LISTEN, si.dwNumberOfProcessors
		);
		if (ie._listen == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());

		// 쓰레드 풀을 위한 쓰레드 생성(CPU 개수만큼)
		for (DWORD i = 0; i < si.dwNumberOfProcessors; i++)
		{
			DWORD dwThreadId;
			parThreads[i] = CreateThread(NULL, 0, IocpSockRecvProc, &ie, 0, &dwThreadId);
			if (parThreads[i] == NULL)
				throw HRESULT_FROM_WIN32(GetLastError());
		}

		// Accept 대기를 위한 소켓 풀 소켓 미리 생성
		IncreaseAcceptSockets(&ie, PERMIT_INC_CNT);

		// 소켓 풀 추가 시점 탐지를 위한 이벤트 생성 및 리슨 소켓과의 연결 처리
		WSAEVENT hEvent = WSACreateEvent();
		if (hEvent == NULL)
			throw HRESULT_FROM_WIN32(WSAGetLastError());
		WSAEventSelect(ie._listen, hEvent, FD_ACCEPT);

		HANDLE arSyncObjs[2] = { g_hevExit, hEvent };
		while (true)
		{
			DWORD dwWaitRet = WaitForMultipleObjects(2, arSyncObjs, FALSE, INFINITE);
			if (dwWaitRet == WAIT_FAILED)
			{
				CloseHandle(hEvent);
				throw HRESULT_FROM_WIN32(GetLastError());
			}

			// Ctrl-C에 의한 서버 종료
			if (dwWaitRet == WAIT_OBJECT_0)
				break;

			// 리슨 소켓에 연결된 이벤트 시그널, 소켓 풀의 Accept 소켓 바닥난 상태.
			WSANETWORKEVENTS ne;
			WSAEnumNetworkEvents(ie._listen, hEvent, &ne);
			if (ne.lNetworkEvents & FD_ACCEPT)
			{
				int nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
				if (nErrCode > 0)
				{
					CloseHandle(hEvent);
					throw HRESULT_FROM_WIN32(nErrCode);
				}

				// Accept 대기를 위한 소켓 풀 소켓 추가 생성
				IncreaseAcceptSockets(&ie, PERMIT_INC_CNT);
			}
		}
		CloseHandle(hEvent);
	}
	catch (HRESULT hr)
	{
		cout << "server error occurred, code : " << hr << endl;
	}

	// 쓰레드풀 쓰레드의 종료 대기 및 관련 리소스 해제
	for (DWORD i = 0; i < si.dwNumberOfProcessors; i++)
		PostQueuedCompletionStatus(ie._iocp, 0, (ULONG_PTR)IOCP_KEY_EXIT, NULL);
	WaitForMultipleObjects(si.dwNumberOfProcessors, parThreads, TRUE, INFINITE);
	for (DWORD i = 0; i < si.dwNumberOfProcessors; i++)
		CloseHandle(parThreads[i]);
	delete[] parThreads;

	// 리슨 소켓과 자식 소켓 닫기
	if (ie._listen != INVALID_SOCKET)
		closesocket(ie._listen);
	for (SOCK_SET::iterator it = ie._socks.begin(); it != ie._socks.end(); it++)
	{
		PSOCK_ITEM pSI = *it;
		closesocket(pSI->_sock);
		delete pSI;
	}
	for (SOCK_SET::iterator it = ie._spool.begin(); it != ie._spool.end(); it++)
	{
		PSOCK_ITEM pSI = *it;
		closesocket(pSI->_sock);
		delete pSI;
	}

	// IOCP 닫기
	if (ie._iocp != NULL)
		CloseHandle(ie._iocp);

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}

DWORD WINAPI IocpSockRecvProc(PVOID pParam)
{
	PIOCP_ENV	pIE = (PIOCP_ENV)pParam;
	OVERLAPPED*	pOL = NULL;
	DWORD		dwTranBytes = 0;
	ULONG_PTR	upDevKey = 0;

	while (true)
	{
		BOOL bEndRlt = GetQueuedCompletionStatus
		(
			pIE->_iocp, &dwTranBytes, &upDevKey, &pOL, INFINITE
		);
		if (bEndRlt == FALSE)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_NETNAME_DELETED)
			{
				cout << "GetQueuedCompletionStatus failed : " << dwErrCode << endl;
				break;
			}
		}

		// 쓰레드 풀 쓰레드 종료 키 수신
		if (upDevKey == IOCP_KEY_EXIT)
			break;

		PSOCK_ITEM pSI = (PSOCK_ITEM)pOL;
		// 리슨 소켓 시그널 : 접속이 이루어 짐.
		if (upDevKey == IOCP_KEY_LISTEN)
		{
			// 자식 소켓을 IOCP에 연결
			CreateIoCompletionPort((HANDLE)pSI->_sock, pIE->_iocp, IOCP_KEY_CHILD, 0);
			printf(" ==> New client %d connected...\n", pSI->_sock);

			// 소켓 풀에서 소켓 획득
			EnterCriticalSection(&pIE->_cs);
			pIE->_spool.erase(pSI);
			pIE->_socks.insert(pSI);
			LeaveCriticalSection(&pIE->_cs);
		}
		// 자식 소켓 시그널 : 클라이언트로부터의 데이터 수신 또는 연결 닫힘 또는 에러 발생
		else
		{
			if (dwTranBytes == 0 || pOL->Internal != 0)
			{
				// 에러 발생, 소켓 종료 처리
				if (pOL->Internal != 0)
				{
					printf(" ~~~ Error occurred with OVERLAPPED.Internal = %d\n", pOL->Internal);
					RemoveFailedSockect(pIE, pSI);
				}
				// 클라이언트가 연결을 끊음
				else
				{
					printf(" ==> Client %d disconnected...\n", pSI->_sock);

					// 소켓 재사용을 위해 연결을 끊기만 하고 닫지 않음.
					LPFN_DISCONNECTEX pfnDisconnectEx =
						(LPFN_DISCONNECTEX)GetSockExtAPI(pSI->_sock, WSAID_DISCONNECTEX);
					if (pfnDisconnectEx == NULL)
					{
						RemoveFailedSockect(pIE, pSI);
						continue;
					}
					pfnDisconnectEx(pSI->_sock, NULL, TF_REUSE_SOCKET, 0);

					// 소켓 풀에 닫힌 소켓 반환
					EnterCriticalSection(&pIE->_cs);
					pIE->_socks.erase(pSI);
					pIE->_spool.insert(pSI);
					LeaveCriticalSection(&pIE->_cs);

					// 비동기 수용 대기 상태로...
					LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
						GetSockExtAPI(pIE->_listen, WSAID_ACCEPTEX);
					if (pfnAcceptEx == NULL)
					{
						RemoveFailedSockect(pIE, pSI);
						continue;
					}
					BOOL bIsOK = pfnAcceptEx
					(
						pIE->_listen, pSI->_sock, pSI->_buff, 0,
						sizeof(SOCKADDR_IN) + 16,
						sizeof(SOCKADDR_IN) + 16,
						NULL, (LPOVERLAPPED)pSI
					);
					if (bIsOK == FALSE)
					{
						int nErrCode = WSAGetLastError();
						if (nErrCode != WSA_IO_PENDING)
						{
							printf(" ~~~ AcceptEx failed, ErrorCode = %d\n", nErrCode);
							RemoveFailedSockect(pIE, pSI);
						}
					}
				}
				continue;
			}

			pSI->_buff[dwTranBytes] = 0;
			printf(" *** Client(%d) sent : %s\n", pSI->_sock, pSI->_buff);

			// 에코 데이터를 클라이언트에 전송
			int lSockRet = send(pSI->_sock, pSI->_buff, dwTranBytes, 0);
			if (lSockRet == SOCKET_ERROR)
			{
				printf(" ~~~ send failed, ErrorCode = %d\n", WSAGetLastError());
				RemoveFailedSockect(pIE, pSI);
			}
		}

		// 비동기 수신 대기 상태로...
		if (!ReadFile((HANDLE)pSI->_sock, pSI->_buff,
			sizeof(pSI->_buff), NULL, (LPOVERLAPPED)pSI))
		{
			int nErrCode = WSAGetLastError();
			if (nErrCode != WSA_IO_PENDING)
			{
				printf(" ~~~ ReadFile failed, ErrorCode = %d\n", nErrCode);
				RemoveFailedSockect(pIE, pSI);
			}
		}
	}

	return 0;
}


PVOID GetSockExtAPI(SOCKET sock, GUID guidFn)
{
	PVOID pfnEx = NULL;
	GUID  guid = guidFn;
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

SOCKET GetListenSocket(short shPortNo, int nBacklog)
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

void RemoveFailedSockect(PIOCP_ENV pIE, PSOCK_ITEM pSI)
{
	EnterCriticalSection(&pIE->_cs);
	pIE->_socks.erase(pSI);
	LeaveCriticalSection(&pIE->_cs);
	closesocket(pSI->_sock);
	delete pSI;
}

void IncreaseAcceptSockets(PIOCP_ENV pIE, int nIncCnt)
{
	for (int i = 0; i < nIncCnt; i++)
	{
		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
			throw HRESULT_FROM_WIN32(WSAGetLastError());

		PSOCK_ITEM pSI = new SOCK_ITEM(sock);
		EnterCriticalSection(&pIE->_cs);
		pIE->_spool.insert(pSI);
		LeaveCriticalSection(&pIE->_cs);

		LPFN_ACCEPTEX pfnAcceptEx = (LPFN_ACCEPTEX)
			GetSockExtAPI(pIE->_listen, WSAID_ACCEPTEX);
		if (pfnAcceptEx == NULL)
			throw HRESULT_FROM_WIN32(WSAGetLastError());
		BOOL bIsOK = pfnAcceptEx
		(
			pIE->_listen, sock, pSI->_buff, 0,
			sizeof(SOCKADDR_IN) + 16,
			sizeof(SOCKADDR_IN) + 16,
			NULL, (LPOVERLAPPED)pSI
		);
		if (bIsOK == FALSE)
		{
			int nErrCode = WSAGetLastError();
			if (nErrCode != WSA_IO_PENDING)
				throw HRESULT_FROM_WIN32(nErrCode);
		}
	}
}
