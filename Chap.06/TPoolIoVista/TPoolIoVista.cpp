#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


VOID CALLBACK ThreadPoolIoProc(PTP_CALLBACK_INSTANCE pnst, 
	PVOID pctx, PVOID pol, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO pio)
{
	LPOVERLAPPED pov = (LPOVERLAPPED)pol;
	DWORD dwThrId = GetCurrentThreadId();

	if (ior != 0)
	{
		printf(" => Thread %d reads %d error occurred: %d\n", dwThrId, ior);
		return;
	}

	LARGE_INTEGER ll;
	ll.LowPart = pov->Offset, ll.HighPart = pov->OffsetHigh;
	printf(" => Thread %d reads %d bytes, offset=%I64d\n", 
			dwThrId, dwTrBytes, ll.QuadPart);
}

void _tmain()
{
	HANDLE hFile = CreateFile(_T("c:\\temp\\music.mp3"),
		GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	LARGE_INTEGER llSize;
	GetFileSizeEx(hFile, &llSize);

	PTP_IO ptpIo = CreateThreadpoolIo(hFile, ThreadPoolIoProc, NULL, NULL);

	OVERLAPPED ov;
	ov.Offset = ov.OffsetHigh = 0;
	ov.hEvent = NULL;
	while (llSize.QuadPart > 0)
	{
		BYTE arrBuff[4096];

		StartThreadpoolIo(ptpIo);
		if (!ReadFile(hFile, arrBuff, sizeof(arrBuff), NULL, &ov))
		{
			int nErrCode = GetLastError();
			if (nErrCode != ERROR_IO_PENDING)
			{
				cout << "MainThread -> Error occurred : " << nErrCode << endl;
				CancelThreadpoolIo(ptpIo);
				break;
			}
		}
		WaitForThreadpoolIoCallbacks(ptpIo, FALSE);
		llSize.QuadPart -= ov.InternalHigh;

		LARGE_INTEGER ll;
		ll.LowPart = ov.Offset, ll.HighPart = ov.OffsetHigh;
		ll.QuadPart += ov.InternalHigh;
		ov.Offset = ll.LowPart, ov.OffsetHigh = ll.HighPart;
	}
	CloseThreadpoolIo(ptpIo);
	CloseHandle(hFile);
}
