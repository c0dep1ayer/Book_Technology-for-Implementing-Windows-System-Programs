#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


#define BUFF_SIZE	65536
bool g_bCopyCompleted;
DWORD g_dwErrOccurred;

VOID WINAPI ReadAPCProc(DWORD dwErrCode, DWORD dwTranBytes, LPOVERLAPPED pOL)
{
	if (dwErrCode != 0)
	{
		if (dwErrCode == ERROR_HANDLE_EOF)
			g_bCopyCompleted = true;
		else
			g_dwErrOccurred = dwErrCode;
		return;
	}

	pOL->Offset += dwTranBytes;
	printf(" => Read  bytes : %d\n", pOL->Offset);
}

VOID WINAPI WroteAPCProc(DWORD dwErrCode, DWORD dwTranBytes, LPOVERLAPPED pOL)
{
	if (dwErrCode != 0)
	{
		g_dwErrOccurred = dwErrCode;
		return;
	}
	pOL->Offset += dwTranBytes;
	printf(" <= Wrote bytes : %d\n", pOL->Offset);
}

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Uasge : FileCopyAPC SourceFile TargetFile" << endl;
		return;
	}

	HANDLE hSrcFile = CreateFile
	(
		argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED, NULL
	);
	if (hSrcFile == INVALID_HANDLE_VALUE)
	{
		cout << argv[1] << " open failed, code : " << GetLastError() << endl;
		return;
	}

	HANDLE hDstFile = CreateFile
	(
		argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_FLAG_OVERLAPPED, NULL
	);
	if (hDstFile == INVALID_HANDLE_VALUE)
	{
		cout << argv[2] << " open failed, code : " << GetLastError() << endl;
		return;
	}

	OVERLAPPED ro = { 0, };
	OVERLAPPED wo = { 0, };

	DWORD	dwErrCode = 0;
	BYTE	btBuff[BUFF_SIZE];
	while (true)
	{
		BOOL bIsOK = ReadFileEx
		(
			hSrcFile, btBuff, sizeof(btBuff), &ro, ReadAPCProc
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

		DWORD dwWaitRet = SleepEx(INFINITE, TRUE);
		if (dwWaitRet != WAIT_IO_COMPLETION)
		{
			dwErrCode = GetLastError();
			break;
		}
		if (g_bCopyCompleted)
			break;
		if (g_dwErrOccurred != ERROR_SUCCESS)
		{
			dwErrCode = g_dwErrOccurred;
			break;
		}
		//if (ro.Internal != 0)
		//{
		//	if (ro.Internal == STATUS_END_OF_FILE)
		//		dwErrCode = 0;
		//	else
		//		dwErrCode = ro.Internal;
		//	break;
		//}

		bIsOK = WriteFileEx
		(
			hDstFile, btBuff, ro.InternalHigh, &wo, WroteAPCProc
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

		dwWaitRet = SleepEx(INFINITE, TRUE);
		if (dwWaitRet != WAIT_IO_COMPLETION)
		{
			dwErrCode = GetLastError();
			break;
		}
	}
	CloseHandle(hSrcFile);
	CloseHandle(hDstFile);

	if (dwErrCode != ERROR_SUCCESS)
		cout << endl << "Error occurred in file copying, code=" << dwErrCode << endl;
	else
		cout << endl << "File copy successfully completed..." << endl;
}
