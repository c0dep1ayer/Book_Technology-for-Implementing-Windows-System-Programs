#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	65536

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Usage : FileCopySync SourceFile TargetFile" << endl;
		return;
	}

	HANDLE hSrcFile = CreateFile
	(
		argv[1], GENERIC_READ, 0, NULL,
		OPEN_EXISTING, 0, NULL
	);
	if (hSrcFile == INVALID_HANDLE_VALUE)
	{
		cout << argv[1] << " open failed, code : " << GetLastError() << endl;
		return;
	}

	HANDLE hDstFile = CreateFile
	(
		argv[2], GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, 0, NULL
	);
	if (hDstFile == INVALID_HANDLE_VALUE)
	{
		cout << argv[2] << " open failed, code : " << GetLastError() << endl;
		return;
	}

	DWORD	dwTotalWroteBytes = 0, dwTotalReadBytes = 0;
	DWORD	dwErrCode = 0;
	BYTE	btBuff[BUFF_SIZE];

	while (true)
	{
		DWORD dwReadBytes = 0;
		BOOL bIsOK = ReadFile
		(
			hSrcFile, btBuff, sizeof(btBuff), &dwReadBytes, NULL
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			break;
		}
		if (!dwReadBytes)
			break;

		dwTotalReadBytes += dwReadBytes;
		printf(" => Read  bytes : %d\n", dwTotalReadBytes);

		DWORD dwWroteBytes = 0;
		bIsOK = WriteFile
		(
			hDstFile, btBuff, dwReadBytes, &dwWroteBytes, NULL
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			break;
		}
		dwTotalWroteBytes += dwWroteBytes;
		printf(" <= Wrote bytes : %d\n", dwTotalWroteBytes);
	}
	CloseHandle(hSrcFile);
	CloseHandle(hDstFile);
	if (dwErrCode != ERROR_SUCCESS)
		cout << endl << "Error occurred in file copying, code=" << dwErrCode << endl;
	else
		cout << endl << "File copy successfully completed..." << endl;
}
