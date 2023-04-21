#include "stdafx.h"
#include "Windows.h"
#include "Shlwapi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Shlwapi.lib")

#define SHARE_NAME	_T("MySharedEvent")

void _tmain(void)
{
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, SHARE_NAME);
	if (hEvent == NULL)
	{
		cout << "CreateEvent failed : " << GetLastError() << endl;
		return;
	}
	DWORD dwErrCode = GetLastError();
	if (dwErrCode == ERROR_ALREADY_EXISTS)
	{
		cout << "======= Start Second ShareWithName2 ========" << endl;
		SetEvent(hEvent);
		cout << "  Press any key to exit..." << endl;
		getchar();
		CloseHandle(hEvent);
		cout << "======= End Second ShareWithName2 ==========" << endl;
	}
	else
	{
		cout << "======= Start First ShareWithName2 ========" << endl;
		TCHAR szRunCmdLine[MAX_PATH] = { 0, };
		GetModuleFileName(NULL, szRunCmdLine, MAX_PATH);

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		BOOL bIsOK = CreateProcess
		(
			NULL, szRunCmdLine, NULL, NULL, FALSE, 
			CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
		);
		if (!bIsOK)
		{
			cout << "CreateProcess failed : " << GetLastError() << endl;
			CloseHandle(hEvent);
			return;
		}
		CloseHandle(pi.hThread);

		DWORD dwWaitCode = WaitForSingleObject(hEvent, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
			cout << "Second ShareWithName2 failed : " << GetLastError() << endl;
		else
		{
			cout << "Second ShareWithName2 launch OK!!!" << endl;
			WaitForSingleObject(pi.hProcess, INFINITE);
		}
		CloseHandle(pi.hThread);
		CloseHandle(hEvent);
		cout << "======= End First ShareWithName2 ==========" << endl;
	}
}
