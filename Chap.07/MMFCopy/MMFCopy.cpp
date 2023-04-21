#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define	BLOCK_LEN	65536

void _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "MMFCopy SoruceFile TargetFile" << endl;
		return;
	}

	HANDLE hSrcFile = INVALID_HANDLE_VALUE, hDstFile = INVALID_HANDLE_VALUE;
	HANDLE hSrcMmf = NULL, hDstMmf = NULL;
	try
	{
		hSrcFile = CreateFile(argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hSrcFile == INVALID_HANDLE_VALUE)
			throw MAKE_HRESULT(1, FACILITY_WIN32, GetLastError());
		LARGE_INTEGER llFileSize;
		GetFileSizeEx(hSrcFile, &llFileSize);

		hSrcMmf = CreateFileMapping(hSrcFile, NULL, PAGE_READONLY, llFileSize.HighPart, llFileSize.LowPart, NULL);
		if (hSrcMmf == NULL)
			throw MAKE_HRESULT(1, FACILITY_WIN32, GetLastError());

		hDstFile = CreateFile(argv[2], GENERIC_WRITE|GENERIC_READ, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (hDstFile == INVALID_HANDLE_VALUE)
			throw MAKE_HRESULT(1, FACILITY_WIN32, GetLastError());

		hDstMmf = CreateFileMapping(hDstFile, NULL, PAGE_READWRITE, llFileSize.HighPart, llFileSize.LowPart, NULL);
		if (hDstMmf == NULL)
			throw MAKE_HRESULT(1, FACILITY_WIN32, GetLastError());

		DWORD dwCopyLen = BLOCK_LEN;
		LARGE_INTEGER llCopySize;
		llCopySize.QuadPart = 0LL;
		while(llCopySize.QuadPart < llFileSize.QuadPart)
		{
			if (llCopySize.QuadPart + BLOCK_LEN >= llFileSize.QuadPart)
				dwCopyLen = DWORD(llFileSize.QuadPart - llCopySize.QuadPart);
			else
				dwCopyLen = BLOCK_LEN;

			PVOID pSrcMap = MapViewOfFile(hSrcMmf, FILE_MAP_READ, llCopySize.HighPart, llCopySize.LowPart, dwCopyLen);
			if (!pSrcMap)
				throw MAKE_HRESULT(1, FACILITY_WIN32, GetLastError());

			PVOID pDstMap = MapViewOfFile(hDstMmf, FILE_MAP_WRITE, llCopySize.HighPart, llCopySize.LowPart, dwCopyLen);
			if (!pDstMap)
			{
				UnmapViewOfFile(pSrcMap);
				throw MAKE_HRESULT(1, FACILITY_WIN32, GetLastError());
			}
	
			CopyMemory(pDstMap, pSrcMap, dwCopyLen);

			llCopySize.QuadPart += dwCopyLen;
			printf("Total : %I64d, %I64d bytes copied...\n", llFileSize.QuadPart, llCopySize.QuadPart);

			UnmapViewOfFile(pSrcMap);
			UnmapViewOfFile(pDstMap);
		}

		SetFilePointer(hDstFile, llFileSize.LowPart, &llFileSize.HighPart, FILE_BEGIN);
		SetEndOfFile(hDstFile);
	}
	catch(HRESULT hr)
	{
		printf("File copy error occurred, code = 0x%08X\n", hr);
	}
	if (hSrcMmf != NULL)
		CloseHandle(hSrcMmf);
	if (hSrcFile != INVALID_HANDLE_VALUE)
		CloseHandle(hSrcFile);

	if (hDstMmf != NULL)
		CloseHandle(hDstMmf);
	if (hDstFile != INVALID_HANDLE_VALUE)
		CloseHandle(hDstFile);
}
