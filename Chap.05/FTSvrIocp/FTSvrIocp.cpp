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

#define BUFF_SIZE	4096
#define IOKEY_NEW	0
#define IOKEY_FILE	1
#define IOKEY_SOCK	2

struct COPY_CHUNCK : OVERLAPPED
{
	HANDLE	_file;
	SOCKET	_sock;
	BYTE	_arBuff[BUFF_SIZE];

	COPY_CHUNCK(HANDLE file, SOCKET sock)
	{
		memset(this, 0, sizeof(*this));
		_file = file, _sock = sock;
	}
};
typedef COPY_CHUNCK* PCOPY_CHUNCK;
typedef std::set<PCOPY_CHUNCK> COPY_SET;

struct COPY_ENV
{
	CRITICAL_SECTION _cs;
	COPY_SET		 _set;
	HANDLE			 _iocp;
};
typedef COPY_ENV* PCOPY_ENV;

DWORD WINAPI IOCPRecvProc(PVOID pParam)
{
	PCOPY_ENV pEnv = (PCOPY_ENV)pParam;
	DWORD	  dwThrId = GetCurrentThreadId();

	while (true)
	{
		DWORD		 dwErrCode = ERROR_SUCCESS;
		PCOPY_CHUNCK pcc = NULL;
		DWORD		 dwTrBytes = 0;
		ULONG_PTR	 ulKey;

		BOOL bIsOK = GetQueuedCompletionStatus
		(
			pEnv->_iocp, &dwTrBytes, &ulKey, (LPOVERLAPPED*)&pcc, INFINITE
		);
		if (!bIsOK)
		{
			if (pcc == NULL)
				break;
			dwErrCode = GetLastError();
			goto $LABEL_CLOSE;
		}

		switch (ulKey)
		{
			case IOKEY_NEW:
				EnterCriticalSection(&pEnv->_cs);
				pEnv->_set.insert(pcc);
				LeaveCriticalSection(&pEnv->_cs);
			case IOKEY_FILE:
				if (ulKey != IOKEY_NEW)
				{
					LARGE_INTEGER ll;
					ll.LowPart = pcc->Offset, ll.HighPart = pcc->OffsetHigh;
					ll.QuadPart += dwTrBytes;
					pcc->Offset = ll.LowPart, pcc->OffsetHigh = ll.HighPart;
					printf(" <= Thr %d wrote %I64d bytes...\n", dwThrId, ll.QuadPart);
				}
				bIsOK = ReadFile((HANDLE)pcc->_sock, pcc->_arBuff, BUFF_SIZE, NULL, pcc);
			break;

			case IOKEY_SOCK:
				if (dwTrBytes == 0)
					goto $LABEL_CLOSE;

				bIsOK = WriteFile(pcc->_file, pcc->_arBuff, dwTrBytes, NULL, pcc);
			break;
		}
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				goto $LABEL_CLOSE;
		}
		continue;

	$LABEL_CLOSE:
		if (dwErrCode == ERROR_SUCCESS)
			printf(" ****** Thr %d copy successfully completed...\n", dwThrId);
		else
			printf(" ###### Thr %d copy failed, code : %d\n", dwThrId, dwErrCode);
		CloseHandle(pcc->_file);
		closesocket(pcc->_sock);

		EnterCriticalSection(&pEnv->_cs);
		pEnv->_set.erase(pcc);
		LeaveCriticalSection(&pEnv->_cs);
		delete pcc;
	}
	return 0;
}


SOCKET g_sockMain = INVALID_SOCKET;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (g_sockMain != INVALID_SOCKET)
		closesocket(g_sockMain);
	return TRUE;
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

	COPY_ENV env;
	InitializeCriticalSection(&env._cs);
	env._iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);

	DWORD dwThrID = 0;
	HANDLE harWorks[2];
	for (int i = 0; i < 2; i++)
		harWorks[i] = CreateThread(NULL, 0, IOCPRecvProc, &env, 0, &dwThrID);

	while (true)
	{
		SOCKET sock = accept(hsoListen, NULL, NULL);
		if (sock == INVALID_SOCKET)
		{
			cout << "accept failed, code : " << WSAGetLastError() << endl;
			break;
		}
		cout << " ==> New client " << sock << " connected..." << endl;

		GUID guid; WCHAR szGuid[128];
		CoCreateGuid(&guid);
		StringFromGUID2(guid, szGuid, 128);
		WCHAR szFilePath[MAX_PATH];
		wsprintf(szFilePath, L"D:\\temp\\%s", szGuid);
		HANDLE hFile = CreateFileW
		(
			szFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL
		);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			cout << szGuid << " create failed, code : " << GetLastError() << endl;
			break;
		}

		CreateIoCompletionPort((HANDLE)sock, env._iocp, IOKEY_SOCK, 0);
		CreateIoCompletionPort(hFile, env._iocp, IOKEY_FILE, 0);

		PCOPY_CHUNCK pcc = new COPY_CHUNCK(hFile, sock);
		PostQueuedCompletionStatus(env._iocp, 0, IOKEY_NEW, pcc);
	}
	closesocket(hsoListen);
	CloseHandle(env._iocp);
	WaitForMultipleObjects(2, harWorks, TRUE, INFINITE);

	for (COPY_SET::iterator it = env._set.begin(); it != env._set.end(); it++)
	{
		PCOPY_CHUNCK pcc = *it;
		closesocket(pcc->_sock);
		CloseHandle(pcc->_file);
		delete pcc;
	}
	DeleteCriticalSection(&env._cs);
	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
