#include "stdafx.h"
#include <windows.h>
#include "iostream"
using namespace std;

BOOL  g_bCancel = FALSE;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	g_bCancel = TRUE;
	return TRUE;
}


DWORD CALLBACK CopyProgressRoutine(
	LARGE_INTEGER llTotalSize, LARGE_INTEGER llTranBytes,
	LARGE_INTEGER llStrmSize, LARGE_INTEGER llStrmBytes,
	DWORD dwStrmNum, DWORD dwCbReason,
	HANDLE hSrcFile, HANDLE hDstFile, LPVOID pData
)
{
	int nRate = (int)(llTranBytes.QuadPart * 100LL / llTotalSize.QuadPart);
	printf("  Thr %d => File %d%% copied: %I64d bytes\n", 
		GetCurrentThreadId(), nRate, llTranBytes.QuadPart);
	return PROGRESS_CONTINUE;
}

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Usage : CopyFileEx SourceFile TargetFile" << endl;
		return;
	}

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	cout << "Thr %d => File copy start..." << GetCurrentThreadId() << endl;

	BOOL bIsOK = CopyFileEx
	(
		argv[1], argv[2], CopyProgressRoutine, NULL, &g_bCancel, 0
	);
	if (!bIsOK)
	{
		DWORD dwErrCode = GetLastError();
		if (dwErrCode == ERROR_REQUEST_ABORTED)
			cout << "File copy aboarted by user..." << endl;
		else
			cout << "Error occurred in file copying, code=" << dwErrCode << endl;
	}
	else
		cout << endl << "File copy successfully completed..." << endl;
}
