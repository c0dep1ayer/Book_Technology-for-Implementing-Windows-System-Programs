#include "stdafx.h"
#include "Windows.h"
#include "Shlwapi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Shlwapi.lib")

void _tmain(void)
{
	cout << "======= Start WaitForInputIdle Test ========" << endl;

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	HANDLE hEvent = CreateEvent(&sa, TRUE, FALSE, NULL);
	if (hEvent == NULL)
	{
		cout << "CreateEvent failed : " << GetLastError() << endl;
		return;
	}

	TCHAR szRunCmdLine[MAX_PATH] = { 0, };
	DWORD dwPathLen = GetModuleFileName(NULL, szRunCmdLine, MAX_PATH);
	PathRemoveFileSpec(szRunCmdLine);
	int nLen = _tcslen(szRunCmdLine);
#ifdef _WIN64
	wsprintf(szRunCmdLine + nLen, _T("%\\TestGUIApp.exe %I64d"), hEvent);
#else
	wsprintf(szRunCmdLine + nLen, _T("%\\TestGUIApp.exe %d"), hEvent);
#endif

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL bIsOK = CreateProcess
	(
		NULL, szRunCmdLine,  NULL, NULL,
		TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
	);
	if (!bIsOK)
	{
		cout << "CreateProcess failed : " << GetLastError() << endl;
		CloseHandle(hEvent);
		return;
	}

	DWORD dwWaitCode = WaitForSingleObject(hEvent, INFINITE);
	if (dwWaitCode == WAIT_FAILED)
		cout << "WaitForInputIdle failed : " << GetLastError() << endl;
	else
		cout << "TestGUIApp.exe initialization completed...." << endl;

	CloseHandle(hEvent);
	
	cout << "======= End WaitForInputIdle Test ==========" << endl;
}

