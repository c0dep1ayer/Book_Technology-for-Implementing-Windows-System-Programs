#include "stdafx.h"
#include "Windows.h"
#include "Shlwapi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Shlwapi.lib")

void _tmain(void)
{
	cout << "======= Start WaitForInputIdle Test ========" << endl;

	TCHAR szRunCmdLine[MAX_PATH] = { 0, };
	DWORD dwPathLen = ::GetModuleFileName(NULL, szRunCmdLine, MAX_PATH);
	::PathRemoveFileSpec(szRunCmdLine);
	_tcscat_s(szRunCmdLine, _T("\\TestToolApp.exe"));

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL bIsOK = CreateProcess
	(
		NULL, szRunCmdLine, NULL, NULL,
		FALSE, 0, NULL, NULL, &si, &pi
	);
	if (!bIsOK)
	{
		cout << "CreateProcess failed : " << GetLastError() << endl;
		return;
	}

	DWORD dwWaitCode = WaitForInputIdle(pi.hProcess, INFINITE);
	if (dwWaitCode == WAIT_FAILED)
		cout << "WaitForInputIdle failed : " << GetLastError() << endl;
	else
		cout << "TestGUIApp.exe initialization completed...." << endl;
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	
	cout << "======= End WaitForInputIdle Test ==========" << endl;
}

