#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	65536
#define BEGIN_KEY	0
#define READ_KEY	1
#define WROTE_KEY	2

struct COPY_CHUNCK : OVERLAPPED
{
	HANDLE	_hfSrc, _hfDst;
	BYTE	_arBuff[BUFF_SIZE];

	COPY_CHUNCK(HANDLE hfSrc, HANDLE hfDst)
	{
		memset(this, 0, sizeof(*this));
		_hfSrc = hfSrc, _hfDst = hfDst;
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

DWORD WINAPI IOCPCopyProc(PVOID pParam)
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
		if (ulKey == READ_KEY)
		{
			printf(" => Thr %d Read bytes : %d\n", dwThrId, pcc->Offset);

			bIsOK = WriteFile
			(
				pcc->_hfDst, pcc->_arBuff, dwTrBytes, NULL, pcc
			);
		}
		else
		{
			if (ulKey != BEGIN_KEY)
			{
				pcc->Offset += dwTrBytes;
				printf(" <= Thr %d Wrote bytes : %d\n", dwThrId, pcc->Offset);
			}

			bIsOK = ReadFile
			(
				pcc->_hfSrc, pcc->_arBuff, BUFF_SIZE, NULL, pcc
			);
		}
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
		CloseHandle(pcc->_hfSrc);
		CloseHandle(pcc->_hfDst);
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

	PCOPY_CHUNCK arChunk[MAX_COPY_CNT];
	memset(arChunk, 0, sizeof(PCOPY_CHUNCK) * MAX_COPY_CNT);

	COPY_ENV env;
	env._nCpCnt = 0;
	env._hIocp  = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	env._hevEnd = CreateEvent(NULL, TRUE, FALSE, NULL);

	for (int i = 1; i < argc; i++)
	{
		TCHAR* pszSrcFile = argv[i];
		HANDLE hSrcFile = CreateFile
		(
			pszSrcFile, GENERIC_READ, 0, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
		);
		if (hSrcFile == INVALID_HANDLE_VALUE)
		{
			cout << pszSrcFile << " open failed, code : " << GetLastError() << endl;
			return;
		}

		TCHAR szDstFile[MAX_PATH];
		_tcscpy(szDstFile, pszSrcFile);
		_tcscat(szDstFile, _T(".copied"));
		HANDLE hDstFile = CreateFile
		(
			szDstFile, GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL
		);
		if (hDstFile == INVALID_HANDLE_VALUE)
		{
			cout << szDstFile << " open failed, code : " << GetLastError() << endl;
			return;
		}

		CreateIoCompletionPort(hSrcFile, env._hIocp, READ_KEY, 0);
		CreateIoCompletionPort(hDstFile, env._hIocp, WROTE_KEY, 0);

		PCOPY_CHUNCK pcc = new COPY_CHUNCK(hSrcFile, hDstFile);
		arChunk[i - 1] = pcc;
		env._nCpCnt++;
	}

	LONG lChnCnt  = env._nCpCnt;
	DWORD dwThrID = 0;
	HANDLE harWorks[2];
	for (int i = 0; i < 2; i++)
		harWorks[i] = CreateThread(NULL, 0, IOCPCopyProc, &env, 0, &dwThrID);

	for (int i = 0; i < lChnCnt; i++)
	{
		PCOPY_CHUNCK pcc = arChunk[i];
		PostQueuedCompletionStatus(env._hIocp, 0, BEGIN_KEY, pcc);
	}

	WaitForSingleObject(env._hevEnd, INFINITE);
	CloseHandle(env._hIocp);
	WaitForMultipleObjects(2, harWorks, TRUE, INFINITE);

	for (int i = 0; i < lChnCnt; i++)
	{
		PCOPY_CHUNCK pcc = arChunk[i];
		delete pcc;
	}
	for (int i = 0; i < 2; i++)
		CloseHandle(harWorks[i]);
	CloseHandle(env._hevEnd);
}
