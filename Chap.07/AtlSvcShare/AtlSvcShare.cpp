#include "stdafx.h"
#include "resource.h"
#include "AtlSvcShare_i.h"
using namespace ATL;

#include <Sddl.h>
#include <AccCtrl.h>
#include <Aclapi.h>
#include <tlhelp32.h>
#include <Wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")

#define RM_USER_SESS1		1
#define RM_SYSTEM_SESS1		2

#define RUN_MODE RM_USER_SESS1


class CAtlSvcShareModule : public ATL::CAtlServiceModuleT< CAtlSvcShareModule, IDS_SERVICENAME >
{
	HANDLE m_hevExit;

public:
	DECLARE_LIBID(LIBID_AtlSvcShareLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSVCSHARE, "{9F8A54F6-BEFD-4686-984A-FD655BE34293}")
	HRESULT InitializeSecurity() throw()
	{
		return S_OK;
	}

	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();
	void OnStop();
};
CAtlSvcShareModule _AtlModule;


extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}



HRESULT CAtlSvcShareModule::PreMessageLoop(int nShowCmd)
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

HRESULT CAtlSvcShareModule::PostMessageLoop()
{
	CloseHandle(m_hevExit);
	return __super::PostMessageLoop();
}
void CAtlSvcShareModule::OnStop()
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

		// WinLogon 프로세스의 토큰 획득.
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

		// WinLogon 프로세스의 토큰과 같은 토큰을 생성.
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

void InitSecuInfo(PSECURITY_ATTRIBUTES pSA)
{
	pSA->lpSecurityDescriptor = NULL;

	PSECURITY_DESCRIPTOR pSD = PSECURITY_DESCRIPTOR(new BYTE[SECURITY_DESCRIPTOR_MIN_LENGTH]);
	if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
	{
		delete[] PBYTE(pSD);
		return;
	}
	if (!SetSecurityDescriptorDacl(pSD, TRUE, NULL, FALSE))
	{
		delete[] PBYTE(pSD);
		return;
	}
	pSA->nLength = sizeof(SECURITY_ATTRIBUTES);
	pSA->bInheritHandle = TRUE;
	pSA->lpSecurityDescriptor = pSD;
}

void SetObjectToLowIntegrity(HANDLE hObject)
{
	PCTSTR LOW_INTEGRITY_SDDL_SACL = _T("S:(ML;;NW;;;LW)");

	PSECURITY_DESCRIPTOR pSD = NULL;
	try
	{
		BOOL bIsOK = ConvertStringSecurityDescriptorToSecurityDescriptor
			(
			LOW_INTEGRITY_SDDL_SACL, SDDL_REVISION_1, &pSD, NULL
			);
		if (!bIsOK)
			throw GetLastError();

		PACL pSacl = NULL;
		BOOL fSaclCur = FALSE;
		BOOL fSaclDef = FALSE;
		bIsOK = GetSecurityDescriptorSacl(pSD, &fSaclCur, &pSacl, &fSaclDef);
		if (!bIsOK)
			throw GetLastError();

		DWORD dwErrCode = SetSecurityInfo
			(
			hObject, SE_KERNEL_OBJECT, LABEL_SECURITY_INFORMATION,
			NULL, NULL, NULL, pSacl
			);
		if (dwErrCode != ERROR_SUCCESS)
			throw GetLastError();
		LocalFree(pSD);
	}
	catch (DWORD hr)
	{
		if (pSD != NULL) LocalFree(pSD);
		throw hr;
	}
}

void CAtlSvcShareModule::RunMessageLoop()
{
	TCHAR szCmdLine[MAX_PATH] = { 0, };
	GetModuleFileName(NULL, szCmdLine, MAX_PATH);
	PathRemoveFileSpec(szCmdLine);

	HANDLE hToken = NULL;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	HANDLE hEvent = NULL;
	SECURITY_ATTRIBUTES sa;
	try
	{
#if (RUN_MODE == RM_USER_SESS1)
		InitSecuInfo(&sa);
		if (sa.lpSecurityDescriptor == NULL)
			throw GetLastError();
#else
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = NULL;

#endif
		hEvent = CreateEvent(&sa, TRUE, FALSE, NULL);
		if (hEvent == NULL)
			throw GetLastError();

#if (RUN_MODE == RM_USER_SESS1)
		delete[] PBYTE(sa.lpSecurityDescriptor);
		SetObjectToLowIntegrity(hEvent);
#endif

		int nLen = _tcslen(szCmdLine);
#ifdef _WIN64
		wsprintf(szCmdLine + nLen, _T("\\TestToolApp.exe %I64d"), hEvent);
#else
		wsprintf(szCmdLine + nLen, _T("\\TestToolApp.exe %d"), hEvent);
#endif

		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
#if (RUN_MODE == RM_USER_SESS1)
		if (!WTSQueryUserToken(dwSessionId, &hToken))
			throw GetLastError();
#else
		hToken = AcquireTokenForLocalSystem(dwSessionId);
		if (hToken == NULL)
			throw GetLastError();
#endif

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		BOOL bIsOK = CreateProcessAsUser
		(
			hToken,
			NULL, szCmdLine, NULL, NULL,
			TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
		);
		if (!bIsOK)
			throw GetLastError();
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(hToken);

		WaitForSingleObject(hEvent, INFINITE);
	}
	catch (DWORD hr)
	{
		ATLTRACE(_T("Error occurred, code=%d"), hr);
		if (hToken != NULL) CloseHandle(hToken);
	}

	WaitForSingleObject(m_hevExit, INFINITE);
}
