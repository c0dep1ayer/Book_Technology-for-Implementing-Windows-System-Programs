#include "stdafx.h"
#include <windows.h>
#include "iostream"
using namespace std;


DWORD CALLBACK CopyProgressRoutine(
	LARGE_INTEGER llTotalSize, LARGE_INTEGER llTranBytes,
	LARGE_INTEGER llStrmSize, LARGE_INTEGER llStrmBytes,
	DWORD dwStrmNum, DWORD dwCbReason,
	HANDLE hSrcFile, HANDLE hDstFile, LPVOID pData
	)
{
	DWORD dwNtexOpt = *((PDWORD)pData);
	int nRate = (int)(llTranBytes.QuadPart * 100LL / llTotalSize.QuadPart);
	printf("  Thr %d => File %d%% copied: %I64d bytes\n",
		GetCurrentThreadId(), nRate, llTranBytes.QuadPart);

	return dwNtexOpt;
}


DWORD WINAPI ThreadProc(PVOID pParam)
{
	PCTSTR* parFiles = (PCTSTR*)pParam;
	HANDLE	hevWait  = (HANDLE)parFiles[3];
	PDWORD  pResult  = (PDWORD)parFiles[4];
	BOOL bIsOK = CopyFileEx
	(
		parFiles[0], parFiles[1], 
		CopyProgressRoutine, (PVOID)parFiles[2], NULL, 0
	);
	*pResult = ((!bIsOK) ? GetLastError() : ERROR_SUCCESS);
	SetEvent(hevWait);

	return 0;
}

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Usage : CopyFileEx SourceFile TargetFile" << endl;
		return;
	}

	HANDLE hevWait = CreateEvent(NULL, TRUE, FALSE, NULL);
	DWORD dwNextOpt = PROGRESS_CONTINUE;
	DWORD hr = ERROR_SUCCESS;
	PCTSTR arpArgs[5] = { argv[1], argv[2], (PCTSTR)&dwNextOpt, (PCTSTR)hevWait, (PCTSTR)&hr };
	QueueUserWorkItem(ThreadProc, arpArgs, 	WT_EXECUTELONGFUNCTION);


	int ch = getchar();
	switch (ch)
	{
		case 'c': dwNextOpt = PROGRESS_CANCEL; break;
		case 's': dwNextOpt = PROGRESS_STOP; break;
		case 'q': dwNextOpt = PROGRESS_QUIET; break;
	}
	WaitForSingleObject(hevWait, INFINITE);

	if (hr != ERROR_SUCCESS)
	{
		if (hr == ERROR_REQUEST_ABORTED)
			cout << "File copy aboarted by user..." << endl;
		else
			cout << "Error occurred in file copying, code=" << hr << endl;
	}
	else
		cout << endl << "File copy successfully completed..." << endl;
}
