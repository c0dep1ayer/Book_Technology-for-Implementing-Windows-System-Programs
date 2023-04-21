#include "stdafx.h"
#include "Winsock2.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")


#define BUFF_SIZE	4096
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

struct COPY_ENV
{
	HANDLE	_hIocp;
	LONG	_nCpCnt;
	HANDLE	_hevEnd;
};
typedef COPY_ENV* PCOPY_ENV;

DWORD WINAPI IOCPSendProc(PVOID pParam)
{
	PCOPY_ENV pEnv = (PCOPY_ENV)pParam;
	DWORD	  dwThrId = GetCurrentThreadId();

	while (true)
	{
		DWORD		 dwErrCode = 0;
		PCOPY_CHUNCK pcc = NULL;
		DWORD		 dwTrBytes = 0;
		ULONG_PTR	 ulKey;

		BOOL bIsOK = GetQueuedCompletionStatus
		(
			pEnv->_hIocp, &dwTrBytes, &ulKey, (LPOVERLAPPED*)&pcc, INFINITE
		);
		if (!bIsOK)
		{
			if (pcc == NULL)
				break;
			dwErrCode = GetLastError();
			goto $LABEL_CLOSE;
		}

		if (ulKey == IOKEY_SOCK)
		{
			if (dwTrBytes > 0)
			{
				LARGE_INTEGER ll;
				ll.LowPart = pcc->Offset, ll.HighPart = pcc->OffsetHigh;
				ll.QuadPart += dwTrBytes;
				pcc->Offset = ll.LowPart, pcc->OffsetHigh = ll.HighPart;
				printf(" <= Thr %d sent %I64d bytes...\n", dwThrId, ll.QuadPart);
			}

			bIsOK = ReadFile(pcc->_file, pcc->_arBuff, BUFF_SIZE, NULL, pcc);
		}
		else
			bIsOK = WriteFile((HANDLE)pcc->_sock, pcc->_arBuff, dwTrBytes, NULL, pcc);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				goto $LABEL_CLOSE;
		}
		continue;

	$LABEL_CLOSE:
		if (dwErrCode == ERROR_HANDLE_EOF)
			printf(" ****** Thr %d copy successfully completed...\n", dwThrId);
		else
			printf(" ###### Thr %d copy failed, code : %d\n", dwThrId, dwErrCode);
		CloseHandle(pcc->_file);
		closesocket(pcc->_sock);
		if (InterlockedDecrement(&pEnv->_nCpCnt) == 0)
			SetEvent(pEnv->_hevEnd);
	}
	return 0;
}


#define MAX_COPY_CNT  10
void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		cout << "Uasge : MultiCopyIOCP SourceFile1 SourceFile2 SourceFile3 ..." << endl;
		return;
	}
	if (argc > MAX_COPY_CNT + 1)
		argc = MAX_COPY_CNT + 1;

	WSADATA	wsd;
	int nErrCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nErrCode)
	{
		cout << "WSAStartup failed with error : " << nErrCode << endl;
		return;
	}

	PCOPY_CHUNCK arChunk[MAX_COPY_CNT];
	memset(arChunk, 0, sizeof(PCOPY_CHUNCK) * MAX_COPY_CNT);

	COPY_ENV env;
	env._nCpCnt = 0;
	env._hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	env._hevEnd = CreateEvent(NULL, TRUE, FALSE, NULL);

	for (int i = 1; i < argc; i++)
	{
		HANDLE hFile = CreateFile
		(
			argv[i], GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
		);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			cout << argv[i] << " open failed, code : " << GetLastError() << endl;
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

		CreateIoCompletionPort(hFile, env._hIocp, IOKEY_FILE, 0);
		CreateIoCompletionPort((HANDLE)sock, env._hIocp, IOKEY_SOCK, 0);

		PCOPY_CHUNCK pcc = new COPY_CHUNCK(hFile, sock);
		arChunk[i - 1] = pcc;
		env._nCpCnt++;
	}

	LONG lChnCnt = env._nCpCnt;
	DWORD dwThrID = 0;
	HANDLE harWorks[2];
	for (int i = 0; i < 2; i++)
		harWorks[i] = CreateThread(NULL, 0, IOCPSendProc, &env, 0, &dwThrID);

	for (int i = 0; i < lChnCnt; i++)
	{
		PCOPY_CHUNCK pcc = arChunk[i];
		PostQueuedCompletionStatus(env._hIocp, 0, IOKEY_SOCK, pcc);
	}

	WaitForSingleObject(env._hevEnd, INFINITE);
	CloseHandle(env._hIocp);
	WaitForMultipleObjects(2, harWorks, TRUE, INFINITE);

	for (int i = 0; i < lChnCnt; i++)
	{
		PCOPY_CHUNCK pcc = arChunk[i];
		delete pcc;
	}
	for (int i = 0; 2 < 1; i++)
		CloseHandle(harWorks[i]);
	CloseHandle(env._hevEnd);

	WSACleanup();
}
