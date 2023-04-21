#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

LONG ExpFilter(LPEXCEPTION_POINTERS pEx)
{
	PEXCEPTION_RECORD pER = pEx->ExceptionRecord;
	CHAR szOut[512];

	int nLen = sprintf(szOut, "Code = %x, Address = %p",
		pER->ExceptionCode, pER->ExceptionAddress);

	if (pER->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
	{
		sprintf(szOut + nLen, "\nAttempt to %s data at address %p",
			pER->ExceptionInformation[0] ? "write" : "read",
			pER->ExceptionInformation[1]);
	}
	cout << szOut << endl;

	return EXCEPTION_EXECUTE_HANDLER;
}


DWORD WINAPI ThreadProc(PVOID pParam)
{
	PCSTR pszTest = (PCSTR)pParam;
	PSTR  pBuff = new char[strlen(pszTest) + 1];
	PINT  pnVal = NULL;

	strcpy(pBuff, pszTest);
	__try
	{
		cout << ">>>>> Thread " << GetCurrentThreadId() << endl;
		cout << "  copied data : " << pBuff << endl;

		*pnVal = strlen(pszTest);
		delete[] pBuff;
	}
	__except (ExpFilter(GetExceptionInformation()))
	{
		DWORD dwExCode = GetExceptionCode();
		printf("===== Exception occurred in thread %d, code=%08X\n",
			GetCurrentThreadId(), dwExCode);
		delete[] pBuff;
		return dwExCode;
	}
	return 0;
}

void _tmain()
{
	cout << "Main thread creating sub thread..." << endl;

	PCSTR pszTest = "Thread Test...";
	DWORD dwThrID = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, (PVOID)pszTest, 0, &dwThrID);
	if (hThread == NULL)
	{
		cout << "~~~ CreateThread failed, error code : " 
			 << GetLastError() << endl;
		return;
	}

	getchar();
	WaitForSingleObject(hThread, INFINITE);
	
	DWORD dwExitCode = 0;
	GetExitCodeThread(hThread, &dwExitCode);
	printf("....Sub thread %d terminated with ExitCode 0x%08X\n", dwThrID, dwExitCode);
	CloseHandle(hThread);
}
