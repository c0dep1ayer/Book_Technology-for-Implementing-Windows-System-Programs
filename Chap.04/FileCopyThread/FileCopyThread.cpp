#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	65536

DWORD WINAPI ThreadProc(PVOID pParam)
{
	PCTSTR* parFiles = (PCTSTR*)pParam;
	DWORD   hr = 0;

	HANDLE hSrcFile = INVALID_HANDLE_VALUE;
	HANDLE hDstFile = INVALID_HANDLE_VALUE;

	try
	{
		hSrcFile = CreateFile
		(
			parFiles[0], GENERIC_READ, 0, NULL,
			OPEN_EXISTING, 0, NULL
		);
		if (hSrcFile == INVALID_HANDLE_VALUE)
			throw GetLastError();
		LARGE_INTEGER llSize;
		GetFileSizeEx(hSrcFile, &llSize);

		HANDLE hDstFile = CreateFile
		(
			parFiles[1], GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, 0, NULL
		);
		if (hDstFile == INVALID_HANDLE_VALUE)
			throw GetLastError();

		PLONG plReason = (PLONG)parFiles[2];
		BYTE  btBuff[BUFF_SIZE];

		LONGLONG llCopied = 0LL;
		while (llCopied < llSize.QuadPart)
		{
			DWORD dwReadBytes = 0;
			BOOL bIsOK = ReadFile
			(
				hSrcFile, btBuff, sizeof(btBuff),
				&dwReadBytes, NULL
			);
			if (!bIsOK)
				throw GetLastError();

			DWORD dwWroteBytes = 0;
			bIsOK = WriteFile
			(
				hDstFile, btBuff, dwReadBytes,
				&dwWroteBytes, NULL
			);
			if (!bIsOK)
				throw GetLastError();
			llCopied += dwWroteBytes;
			int nRate = (int)(llCopied * 100LL / llSize.QuadPart);
			printf(" ....File %d%% copied: %I64d bytes\n", nRate, llCopied);

			if (*plReason != 0)
				throw (DWORD)ERROR_OPERATION_ABORTED;
		}
	}
	catch (DWORD ex)
	{
		hr = ex;
	}
	if (hSrcFile != INVALID_HANDLE_VALUE)
		CloseHandle(hSrcFile);
	if (hDstFile != INVALID_HANDLE_VALUE)
		CloseHandle(hDstFile);

	return hr;
}

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Usage : FileCopyThread SourceFile TargetFile" << endl;
		return;
	}

	LONG lReason = 0;
	PCTSTR arpArgs[3] = { argv[1], argv[2], (PCTSTR)&lReason };
	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, arpArgs, 0, &dwThrID);

	getchar();
	lReason = 1;
	WaitForSingleObject(hThread, INFINITE);

	DWORD hr = 0;
	GetExitCodeThread(hThread, &hr);
	CloseHandle(hThread);

	if (hr != ERROR_SUCCESS)
	{
		if (hr == ERROR_OPERATION_ABORTED)
			cout << "File copy aboarted by user..." << endl;
		else
			cout << "Error occurred in file copying, code=" << hr << endl;
	}
	else
		cout << endl << "File copy successfully completed..." << endl;
}
