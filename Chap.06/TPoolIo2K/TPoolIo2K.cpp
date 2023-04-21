#include "stdafx.h"
#include "Windows.h"
#include "time.h"
#include "iostream"
using namespace std;

HANDLE g_hevWait;

VOID WINAPI ThreadPoolIoProc(DWORD dwErrCode, DWORD dwTrBytes, LPOVERLAPPED pOL)
{
	DWORD dwThrId = GetCurrentThreadId();

	if (dwErrCode != 0)
	{
		printf(" => Thread %d reads %d error occurred: %d\n", dwThrId, dwErrCode);
		return;
	}

	LARGE_INTEGER ll;
	ll.LowPart = pOL->Offset, ll.HighPart = pOL->OffsetHigh;
	printf(" => Thread %d reads %d bytes, offset=%I64d\n", dwThrId, dwTrBytes, ll.QuadPart);
	SetEvent(g_hevWait);
}

void _tmain()
{
	HANDLE hFile = CreateFile(_T("c:\\temp\\music.mp3"),
		GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	LARGE_INTEGER llSize;
	GetFileSizeEx(hFile, &llSize);

	g_hevWait = CreateEvent(NULL, FALSE, FALSE, NULL);
	BindIoCompletionCallback(hFile, ThreadPoolIoProc, 0);

	OVERLAPPED ov;
	ov.Offset = ov.OffsetHigh = 0;
	ov.hEvent = NULL;
	while (llSize.QuadPart > 0)
	{
		BYTE arrBuff[4096];
		if (!ReadFile(hFile, arrBuff, sizeof(arrBuff), NULL, &ov))
		{
			int nErrCode = GetLastError();
			if (nErrCode != ERROR_IO_PENDING)
			{
				cout << "MainThread -> Error occurred : " << nErrCode << endl;
				break;
			}
		}
		WaitForSingleObject(g_hevWait, INFINITE);
		llSize.QuadPart -= ov.InternalHigh;

		LARGE_INTEGER ll;
		ll.LowPart = ov.Offset, ll.HighPart = ov.OffsetHigh;
		ll.QuadPart += ov.InternalHigh;
		ov.Offset = ll.LowPart, ov.OffsetHigh = ll.HighPart;
	}
	CloseHandle(hFile);
	CloseHandle(g_hevWait);
}
