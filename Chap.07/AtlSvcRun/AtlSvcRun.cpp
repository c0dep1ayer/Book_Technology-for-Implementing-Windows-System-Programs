#include "stdafx.h"
#include "resource.h"
#include "AtlSvcRun_i.h"
using namespace ATL;

#include <Sddl.h>
#include <AccCtrl.h>
#include <Aclapi.h>
#include <tlhelp32.h>
#include <Wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")


#define RM_SYSTEM_SESS0		0
#define RM_USER_SESS1		1
#define RM_SYSTEM_SESS1		2

#define RUN_MODE RM_USER_SESS1


class CAtlSvcRunModule : public ATL::CAtlServiceModuleT< CAtlSvcRunModule, IDS_SERVICENAME >
{
	HANDLE m_hevExit;

public:
	DECLARE_LIBID(LIBID_AtlSvcRunLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSVCRUN, "{B17FA563-CEA2-4DFB-A06E-A15F49E6BE73}")
	HRESULT InitializeSecurity() throw()
	{
		return S_OK;
	}

	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();
	void OnStop();
};
CAtlSvcRunModule _AtlModule;


extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}



HRESULT CAtlSvcRunModule::PreMessageLoop(int nShowCmd)
{
	m_hevExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_hevExit == NULL)
		return HRESULT_FROM_WIN32(GetLastError());

	if (::InterlockedCompareExchange(&m_status.dwCurrentState,
		SERVICE_RUNNING, SERVICE_START_PENDING) == SERVICE_START_PENDING)
	{
		LogEvent(_T("Service started/resumed"));
		::SetServiceStatus(m_hServiceStatus, &m_status);
	}
	return S_OK;
}

HRESULT CAtlSvcRunModule::PostMessageLoop()
{
	CloseHandle(m_hevExit);
	return __super::PostMessageLoop();
}
void CAtlSvcRunModule::OnStop()
{
	SetEvent(m_hevExit);

	__super::OnStop();
}


HANDLE AcquireTokenForLocalSystem(DWORD dwSessionId)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return NULL;

	DWORD dwWinLogonId = 0;
	PROCESSENTRY32	pe;
	pe.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnap, &pe))
	{
		CloseHandle(hSnap);
		return NULL;
	}

	do
	{
		if (_tcsicmp(pe.szExeFile, _T("winlogon.exe")) == 0)
		{
			DWORD dwWLSessId = 0;
			if (ProcessIdToSessionId(pe.th32ProcessID, &dwWLSessId))
			{
				if (dwWLSessId == dwSessionId)
				{
					dwWinLogonId = pe.th32ProcessID;
					break;
				}
			}
		}
	} while (Process32Next(hSnap, &pe));
	CloseHandle(hSnap);
	if (dwWinLogonId == 0)
		return NULL;

	HANDLE	hWLToken = NULL;	// WinLogon.exe Token
	HANDLE	hWLProc = NULL;		// WinLogon.exe Process ID
	HANDLE	hNewToken = NULL;	// OUT : New Token for New Process
	HRESULT	hr = S_OK;

	try
	{
		hWLProc = OpenProcess(MAXIMUM_ALLOWED, FALSE, dwWinLogonId);
		if (!hWLProc)
			throw GetLastError();

		BOOL bIsOK = OpenProcessToken
		(
			hWLProc,
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE |
			TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID |
			TOKEN_READ | TOKEN_WRITE,
			&hWLToken
		);
		if (!bIsOK)
			throw GetLastError();

		bIsOK = DuplicateTokenEx
		(
			hWLToken, MAXIMUM_ALLOWED, NULL,
			SecurityIdentification, TokenPrimary, &hNewToken
		);
		if (!bIsOK)
			throw GetLastError();
	}
	catch (DWORD hr)
	{
		ATLTRACE(_T("Error occurred, code=%d"), hr);
	}
	if (hWLProc != NULL)
		CloseHandle(hWLProc);
	if (hWLToken != NULL)
		CloseHandle(hWLToken);

	return hNewToken;
}

void CAtlSvcRunModule::RunMessageLoop()
{
	TCHAR szCmdLine[MAX_PATH] = { 0, };
	GetModuleFileName(NULL, szCmdLine, MAX_PATH);
	PathRemoveFileSpec(szCmdLine);
	_tcscat_s(szCmdLine, _T("\\TestGuiApp.exe"));

	HANDLE hToken = NULL;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	try
	{
#if (RUN_MODE == RM_SYSTEM_SESS0)
		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);

		BOOL bIsOK = CreateProcess
		(
			NULL, szCmdLine, NULL, NULL,
			FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
		);
#elif (RUN_MODE == RM_USER_SESS1)
		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		if (!WTSQueryUserToken(dwSessionId, &hToken))
			throw GetLastError();

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);

		BOOL bIsOK = CreateProcessAsUser
		(
			hToken,
			NULL, szCmdLine, NULL, NULL,
			FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
		);
#else
		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		hToken = AcquireTokenForLocalSystem(dwSessionId);
		if (hToken == NULL)
			throw GetLastError();

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);

		BOOL bIsOK = CreateProcessAsUser
		(
			hToken,
			NULL, szCmdLine, NULL, NULL,
			FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
		);
#endif
		if (!bIsOK)
			throw GetLastError();
		CloseHandle(pi.hThread);

		WaitForInputIdle(pi.hProcess, INFINITE);
		CloseHandle(hToken);
		CloseHandle(pi.hProcess);
	}
	catch (DWORD hr)
	{
		ATLTRACE(_T("Error occurred, code=%d"), hr);
		if (hToken != NULL) CloseHandle(hToken);
	}

	WaitForSingleObject(m_hevExit, INFINITE);
}
