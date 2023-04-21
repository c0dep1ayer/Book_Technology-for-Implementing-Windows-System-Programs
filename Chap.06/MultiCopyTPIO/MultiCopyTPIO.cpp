#include "stdafx.h"
#include "windows.h"
#include "Ntsecapi.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	65536

struct COPY_CHUNCK : OVERLAPPED
{
	HANDLE	_hfSrc, _hfDst;
	PTP_IO	_ioSrc, _ioDst;
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
	LONG	_nCpCnt;
	HANDLE	_hevEnd;
};
typedef COPY_ENV* PCOPY_ENV;

VOID CALLBACK ReadCompleted(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pOL, ULONG dwErrCode, ULONG_PTR dwTrBytes, PTP_IO pio)
{
	PCOPY_ENV	 pce = (PCOPY_ENV)pCtx;
	PCOPY_CHUNCK pcc = (PCOPY_CHUNCK)pOL;
	DWORD	     dwThrId = GetCurrentThreadId();

	if (dwErrCode != 0)
		goto $LABEL_CLOSE;

	printf(" => Thr %d Read bytes : %d\n", dwThrId, pcc->Offset);
	StartThreadpoolIo(pcc->_ioDst);
	BOOL bIsOK = WriteFile(pcc->_hfDst, pcc->_arBuff, dwTrBytes, NULL, pcc);
	if (!bIsOK)
	{
		dwErrCode = GetLastError();
		if (dwErrCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(pcc->_ioDst);
			goto $LABEL_CLOSE;
		}
	}
	return;

$LABEL_CLOSE:
	if (dwErrCode == ERROR_HANDLE_EOF)
		printf(" ****** Thr %d copy successfully completed...\n", dwThrId);
	else
		printf(" ###### Thr %d copy failed, code : %d\n", dwThrId, dwErrCode);
	CloseHandle(pcc->_hfSrc);
	CloseHandle(pcc->_hfDst);
	if (InterlockedDecrement(&pce->_nCpCnt) == 0)
		SetEvent(pce->_hevEnd);
}

VOID CALLBACK WriteCompleted(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pOL, ULONG dwErrCode, ULONG_PTR dwTrBytes, PTP_IO pio)
{
	PCOPY_ENV	 pce = (PCOPY_ENV)pCtx;
	PCOPY_CHUNCK pcc = (PCOPY_CHUNCK)pOL;
	DWORD	     dwThrId = GetCurrentThreadId();

	if (dwErrCode != 0)
		goto $LABEL_CLOSE;

	pcc->Offset += dwTrBytes;
	printf(" <= Thr %d Wrote bytes : %d\n", dwThrId, pcc->Offset);

	StartThreadpoolIo(pcc->_ioSrc);
	BOOL bIsOK = ReadFile(pcc->_hfSrc, pcc->_arBuff, BUFF_SIZE, NULL, pcc);
	if (!bIsOK)
	{
		dwErrCode = GetLastError();
		if (dwErrCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(pcc->_ioSrc);
			goto $LABEL_CLOSE;
		}
	}
	return;

$LABEL_CLOSE:
	if (dwErrCode == ERROR_HANDLE_EOF)
		printf(" ****** Thr %d copy successfully completed...\n", dwThrId);
	else
		printf(" ###### Thr %d copy failed, code : %d\n", dwThrId, dwErrCode);
	CloseHandle(pcc->_hfSrc);
	CloseHandle(pcc->_hfDst);
	if (InterlockedDecrement(&pce->_nCpCnt) == 0)
		SetEvent(pce->_hevEnd);
}


#define MAX_COPY_CNT  10
void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		cout << "Uasge : MultiCopyBICC SourceFile1 SourceFile2 SourceFile3 ..." << endl;
		return;
	}
	if (argc > MAX_COPY_CNT + 1)
		argc = MAX_COPY_CNT + 1;

	PCOPY_CHUNCK arChunk[MAX_COPY_CNT];
	memset(arChunk, 0, sizeof(PCOPY_CHUNCK) * MAX_COPY_CNT);

	COPY_ENV env;
	env._nCpCnt = 0;
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

		PCOPY_CHUNCK pcc = new COPY_CHUNCK(hSrcFile, hDstFile);
		pcc->_ioSrc = CreateThreadpoolIo(hSrcFile, ReadCompleted , &env, NULL);
		pcc->_ioDst = CreateThreadpoolIo(hDstFile, WriteCompleted, &env, NULL);
		arChunk[i - 1] = pcc;
		env._nCpCnt++;
	}

	for (int i = 0; i < env._nCpCnt; i++)
	{
		PCOPY_CHUNCK pcc = arChunk[i];
		StartThreadpoolIo(pcc->_ioSrc);
		BOOL bIsOK = ReadFile
		(
			pcc->_hfSrc, pcc->_arBuff, BUFF_SIZE, NULL, pcc
		);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
			{
				CancelThreadpoolIo(pcc->_ioSrc);
				break;
			}
		}
	}

	WaitForSingleObject(env._hevEnd, INFINITE);
	for (int i = 0; i < env._nCpCnt; i++)
	{
		PCOPY_CHUNCK pcc = arChunk[i];
		CloseThreadpoolIo(pcc->_ioSrc);
		CloseThreadpoolIo(pcc->_ioDst);
		delete pcc;
	}
	CloseHandle(env._hevEnd);
}
