#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;

#include "Ntsecapi.h"

#define TEST_CASE	0

#if(TEST_CASE != 2)
//#include "ntstatus.h"
#	ifndef STATUS_END_OF_FILE	//ERROR_HANDLE_EOF
#		define STATUS_END_OF_FILE 0xC0000011L
#	endif
#endif
#define BUFF_SIZE	65536

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Uasge : FileCopySync SourceFile TargetFile" << endl;
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
	ro.Offset = ro.OffsetHigh = wo.Offset = wo.OffsetHigh = 0;
#if (TEST_CASE == 1)
	ro.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	wo.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	ro.hEvent = wo.hEvent = NULL;
#endif

	DWORD	dwErrCode = 0;
	BYTE	btBuff[BUFF_SIZE];
	while (true)
	{
#if (TEST_CASE != 2)
		BOOL bIsOK = ReadFile
		(
			hSrcFile, btBuff, sizeof(btBuff), NULL, &ro
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

#if (TEST_CASE == 0)
		DWORD dwWaitRet = WaitForSingleObject(hSrcFile, INFINITE);
#else
		DWORD dwWaitRet = WaitForSingleObject(ro.hEvent, INFINITE);
#endif
		if (dwWaitRet != WAIT_OBJECT_0)
		{
			dwErrCode = GetLastError();
			break;
		}
		if (ro.Internal != 0)
		{
			if (ro.Internal == STATUS_END_OF_FILE)
				dwErrCode = 0;
			else
				dwErrCode = ro.Internal;
			break;
		}
		ro.Offset += ro.InternalHigh;
		printf(" => Read  bytes : %d\n", ro.Offset);

		bIsOK = WriteFile
		(
			hDstFile, btBuff, ro.InternalHigh, NULL, &wo
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

#if (TEST_CASE == 0)
		dwWaitRet = WaitForSingleObject(hDstFile, INFINITE);
#else
		dwWaitRet = WaitForSingleObject(wo.hEvent, INFINITE);
#endif
		if (dwWaitRet != WAIT_OBJECT_0)
		{
			dwErrCode = GetLastError();
			break;
		}
		wo.Offset += wo.InternalHigh;
		printf(" <= Wrote bytes : %d\n", wo.Offset);
#else
		BOOL bIsOK = ReadFile
		(
			hSrcFile, btBuff, sizeof(btBuff), NULL, &ro
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

		DWORD dwReadBytes = 0;
		bIsOK = GetOverlappedResult(hSrcFile, &ro, &dwReadBytes, TRUE);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode == ERROR_HANDLE_EOF)
				dwErrCode = 0;
			break;
		}
		ro.Offset += dwReadBytes;
		printf(" => Read  bytes : %d\n", ro.Offset);

		bIsOK = WriteFile
		(
			hDstFile, btBuff, dwReadBytes, NULL, &wo
		);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				break;
		}

		DWORD dwWroteBytes = 0;
		bIsOK = GetOverlappedResult(hDstFile, &wo, &dwWroteBytes, TRUE);
		if (!bIsOK)
		{
			dwErrCode = GetLastError();
			break;
		}
		wo.Offset += dwWroteBytes;
		printf(" <= Wrote bytes : %d\n", wo.Offset);
#endif
	}
	CloseHandle(hSrcFile);
	CloseHandle(hDstFile);
#if (TEST_CASE == 1)
	CloseHandle(ro.hEvent);
	CloseHandle(wo.hEvent);
#endif
	if (dwErrCode != ERROR_SUCCESS)
		cout << endl << "Error occurred in file copying, code=" << dwErrCode << endl;
	else
		cout << endl << "File copy successfully completed..." << endl;
}
