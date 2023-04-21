#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


BOOL EnablePrivilege(PCTSTR szPrivName, BOOL fEnable = TRUE)
{
	BOOL bOK = FALSE;
	HANDLE hToken;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {

		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;

		LookupPrivilegeValue(0, szPrivName, &tp.Privileges[0].Luid);
		bOK = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		if (!bOK)
		{
			DWORD dwErr = GetLastError();
		}
		if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
		{
			printf("The token does not have the specified privilege. \n");
			return TRUE;
		}
		CloseHandle(hToken);
	}
	return bOK;
}

#define BUFF_SIZE	65536
#define BEGIN_KEY	0
#define READ_KEY	1
#define WRITE_KEY	2

struct COPY_CHUNCK : OVERLAPPED
{
	HANDLE			_hfSrc, _hfDst;
	LARGE_INTEGER	_llSize;
	PBYTE			_buff;

	COPY_CHUNCK(HANDLE hfSrc, HANDLE hfDst, LARGE_INTEGER llSize)
	{
		memset(this, 0, sizeof(*this));
		_hfSrc = hfSrc, _hfDst = hfDst;
		_llSize = llSize;

		_buff = (PBYTE)VirtualAlloc(NULL, BUFF_SIZE, MEM_COMMIT, PAGE_READWRITE);
	}
	~COPY_CHUNCK()
	{
		if (_buff)
			VirtualFree(_buff, 0, MEM_RELEASE);
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
		DWORD		 dwErrCode = ERROR_SUCCESS;
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
			bIsOK = WriteFile
			(
				pcc->_hfDst, pcc->_buff, BUFF_SIZE, NULL, pcc
			);
		}
		else
		{
			LARGE_INTEGER li;
			li.LowPart = pcc->Offset, li.HighPart = pcc->OffsetHigh;
			if (dwTrBytes > 0)
			{
				li.QuadPart += (LONGLONG)dwTrBytes;
				pcc->Offset = li.LowPart, pcc->OffsetHigh += li.HighPart;
				//printf(" <= Thr %d copy bytes : %I64d\n", dwThrId, li.QuadPart);
			}

			if (li.QuadPart >= pcc->_llSize.QuadPart)
				goto $LABEL_CLOSE;

			bIsOK = ReadFile
			(
				pcc->_hfSrc, pcc->_buff, BUFF_SIZE, NULL, pcc
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
		CloseHandle(pcc->_hfSrc);
		if (dwErrCode == ERROR_SUCCESS)
		{
			pcc->_hfDst = ReOpenFile
			(
				pcc->_hfDst, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0
			);
			if (pcc->_hfDst == INVALID_HANDLE_VALUE)
				cout << "ReOpenFile ERROR :  " << GetLastError() << endl;
			else
			{
				SetFilePointerEx(pcc->_hfDst, pcc->_llSize, NULL, FILE_BEGIN);
				SetEndOfFile(pcc->_hfDst);
			}
			printf(" ****** Thr %d copy successfully completed...\n", dwThrId);
		}
		else
			printf(" ###### Thr %d copy failed, code : %d\n", dwThrId, dwErrCode);
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

	if (!EnablePrivilege(SE_LOCK_MEMORY_NAME, TRUE))
		return;

	PCOPY_CHUNCK arChunk[MAX_COPY_CNT];
	memset(arChunk, 0, sizeof(PCOPY_CHUNCK) * MAX_COPY_CNT);

	COPY_ENV env;
	env._nCpCnt = 0;
	env._hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	env._hevEnd = CreateEvent(NULL, TRUE, FALSE, NULL);

	FILE_READ_ATTRIBUTES;
	for (int i = 1; i < argc; i++)
	{
		TCHAR* pszSrcFile = argv[i];
		HANDLE hSrcFile = CreateFile
		(
			pszSrcFile, GENERIC_READ | FILE_READ_ATTRIBUTES, 0, NULL, OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL
		);
		if (hSrcFile == INVALID_HANDLE_VALUE)
		{
			cout << pszSrcFile << " open failed, code : " << GetLastError() << endl;
			return;
		}
		LARGE_INTEGER llSrcSize;
		GetFileSizeEx(hSrcFile, &llSrcSize);

		LARGE_INTEGER llDstSize = llSrcSize;
		DWORD dwPad = (llDstSize.QuadPart % BUFF_SIZE);
		if (dwPad > 0)
			llDstSize.QuadPart = llDstSize.QuadPart + (BUFF_SIZE - dwPad);

		TCHAR szDstFile[MAX_PATH];
		_tcscpy(szDstFile, pszSrcFile);
		_tcscat(szDstFile, _T(".copied"));
		HANDLE hDstFile = CreateFile
		(
			szDstFile, GENERIC_WRITE | FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
			FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL
		);
		if (hDstFile == INVALID_HANDLE_VALUE)
		{
			cout << szDstFile << " open failed, code : " << GetLastError() << endl;
			return;
		}
		SetFilePointerEx(hDstFile, llDstSize, NULL, FILE_BEGIN);
		SetEndOfFile(hDstFile);

		CreateIoCompletionPort(hSrcFile, env._hIocp, READ_KEY, 0);
		CreateIoCompletionPort(hDstFile, env._hIocp, WRITE_KEY, 0);

		(PCOPY_CHUNCK)VirtualAlloc(NULL, BUFF_SIZE, MEM_COMMIT, PAGE_READWRITE);
		PCOPY_CHUNCK pcc = new COPY_CHUNCK(hSrcFile, hDstFile, llSrcSize);
		if(!SetFileIoOverlappedRange(hSrcFile, pcc->_buff, BUFF_SIZE))
			cout << " SetFileIoOverlappedRange failed : " << GetLastError() << endl;
		if (!SetFileIoOverlappedRange(hDstFile, pcc->_buff, BUFF_SIZE))
			cout << " SetFileIoOverlappedRange failed : " << GetLastError() << endl;
		arChunk[i - 1] = pcc;
		env._nCpCnt++;
	}

	LONG lChnCnt = env._nCpCnt;
	DWORD dwThrID = 0;
	HANDLE harWorks[2];
	for (int i = 0; i < 2; i++)
		harWorks[i] = CreateThread(NULL, 0, IOCPCopyProc, &env, 0, &dwThrID);

	for (int i = 0; i < lChnCnt; i++)
	{
		PCOPY_CHUNCK pcc = arChunk[i];
		PostQueuedCompletionStatus(env._hIocp, 0, WRITE_KEY, pcc);
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
