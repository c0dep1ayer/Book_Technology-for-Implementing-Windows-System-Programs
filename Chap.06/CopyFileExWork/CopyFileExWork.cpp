#include "stdafx.h"
#include "stdafx.h"
#include <windows.h>
#include "stdlib.h"

BOOL g_bIsCancel = FALSE;

struct CP_PRM
{
	TCHAR	_szSrcFile[MAX_PATH];
	TCHAR	_szDstFile[MAX_PATH];
	DWORD	_dwReason;
	BOOL	_bIsCancel;
};


DWORD CALLBACK CopyProgressRoutine(
	LARGE_INTEGER llTotalSize,
	LARGE_INTEGER llTranBytes,
	LARGE_INTEGER llStrmSize,
	LARGE_INTEGER llStrmBytes,
	DWORD dwStrmNum,
	DWORD dwCbReason,
	HANDLE hSrcFile,
	HANDLE hDstFile,
	LPVOID pData
	)
{
	CP_PRM* pCP = (CP_PRM*)pData;
	if (pCP->_dwReason == 0)
	{
		int nPerc = (int)(llTranBytes.QuadPart * 100LL / llTotalSize.QuadPart);
		printf("%d %% copied : %d\r", nPerc, GetCurrentThreadId());
		return PROGRESS_CONTINUE;
	}

	PROGRESS_CONTINUE;
	PROGRESS_CANCEL;
	PROGRESS_STOP;
	PROGRESS_QUIET;
	return pCP->_dwReason;
}

DWORD WINAPI CopyExProc(PVOID pParam)
{
	CP_PRM* pCP = (CP_PRM*)pParam;

	BOOL bIsOK = FALSE;

$LABEL_START:
	bIsOK = CopyFileEx(pCP->_szSrcFile, pCP->_szDstFile,
		CopyProgressRoutine, pCP, &pCP->_bIsCancel, 0);
	if (!bIsOK)
	{
		DWORD dwErrCode = GetLastError();
		if (dwErrCode == ERROR_REQUEST_ABORTED)
		{
			if (pCP->_dwReason == PROGRESS_CANCEL)
				return;
			if (pCP->_dwReason == PROGRESS_STOP)
				goto $LABEL_START;
		}
		else if (dwErrCode == ERROR_FILE_EXISTS)
		{

		}
		else
		{

		}
	}
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		printf("사용예 : CopyFileCui 소스파일명 타겟파일명\n");
		return -1;
	}

	CP_PRM cpm;
	_tcscpy_s(cpm._szSrcFile, MAX_PATH, argv[1]);
	_tcscpy_s(cpm._szDstFile, MAX_PATH, argv[2]);
	cpm._dwReason = 0;
	cpm._bIsCancel = FALSE;

	QueueUserWorkItem(CopyExProc, &cpm, 0);
	printf("Main Thread : %d\n", GetCurrentThreadId());
	while (true)
	{
		int c = getchar();
		if (c == 'c' || c == 'C')
			cpm._dwReason = PROGRESS_CANCEL;
		else
			cpm._dwReason = PROGRESS_STOP;
		cpm._bIsCancel = TRUE;
	}
	return 0;
}

