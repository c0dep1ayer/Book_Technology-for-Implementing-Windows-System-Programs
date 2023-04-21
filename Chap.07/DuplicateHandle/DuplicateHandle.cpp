#include "stdafx.h"
#include "Windows.h"
#include "Shlwapi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Shlwapi.lib")

void _tmain(void)
{
	cout << "======= Start WaitForInputIdle Test ========" << endl;

	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL)
	{
		cout << "CreateEvent failed : " << GetLastError() << endl;
		return;
	}

	HANDLE hDupEvent = NULL;
	BOOL bIsOK = DuplicateHandle
	(
		GetCurrentProcess(),
		hEvent,
		GetCurrentProcess(),
		&hDupEvent,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS
	);
	if (!bIsOK)
	{
		cout << "DuplicateHandle failed : " << GetLastError() << endl;
		CloseHandle(hEvent);
		return;
	}

	TCHAR szRunCmdLine[MAX_PATH] = { 0, };
	DWORD dwPathLen = GetModuleFileName(NULL, szRunCmdLine, MAX_PATH);
	PathRemoveFileSpec(szRunCmdLine);
	int nLen = _tcslen(szRunCmdLine);
#ifdef _WIN64
	wsprintf(szRunCmdLine + nLen, _T("%\\TestGUIApp.exe %I64d"), hDupEvent);
#else
	wsprintf(szRunCmdLine + nLen, _T("%\\TestGUIApp.exe %d"), hDupEvent);
#endif

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	bIsOK = CreateProcess
	(
		NULL, szRunCmdLine, NULL, NULL,
		TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
	);
	if (!bIsOK)
	{
		cout << "CreateProcess failed : " << GetLastError() << endl;
		CloseHandle(hDupEvent);
		CloseHandle(hEvent);
		return;
	}

	DWORD dwWaitCode = WaitForSingleObject(hDupEvent, INFINITE);
	if (dwWaitCode == WAIT_FAILED)
		cout << "WaitForInputIdle failed : " << GetLastError() << endl;
	else
		cout << "TestGUIApp.exe initialization completed...." << endl;

	CloseHandle(hDupEvent);
	CloseHandle(hEvent);
	
	cout << "======= End WaitForInputIdle Test ==========" << endl;
}
