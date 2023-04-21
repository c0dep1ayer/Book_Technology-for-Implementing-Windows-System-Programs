#include "stdafx.h"
#include "resource.h"
#include "AtlSvcApc_i.h"
using namespace ATL;

#include <Sddl.h>
#include <AccCtrl.h>
#include <Aclapi.h>
#include <Wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")
#include "iostream"
using namespace std;


void SetACLToObject(HANDLE hObject)
{
	PCTSTR LOW_INTEGRITY_SDDL_SACL = _T("S:(ML;;NW;;;LW)");

	DWORD dwErrCode = NO_ERROR;
	PSECURITY_DESCRIPTOR pSDD = NULL, pSDS = NULL;
	try
	{
		pSDD = PSECURITY_DESCRIPTOR(new BYTE[SECURITY_DESCRIPTOR_MIN_LENGTH]);
		if (!InitializeSecurityDescriptor(pSDD, SECURITY_DESCRIPTOR_REVISION))
			throw GetLastError();
		if (!SetSecurityDescriptorDacl(pSDD, TRUE, NULL, FALSE))
			throw GetLastError();
		PACL pDacl = NULL;
		BOOL fDaclCur = FALSE, fDaclDef = FALSE;
		if (!GetSecurityDescriptorDacl(pSDD, &fDaclCur, &pDacl, &fDaclDef))
			throw GetLastError();

		if(!ConvertStringSecurityDescriptorToSecurityDescriptor
			(LOW_INTEGRITY_SDDL_SACL, SDDL_REVISION_1, &pSDS, NULL))
			throw GetLastError();
		PACL pSacl = NULL;
		BOOL fSaclCur = FALSE, fSaclDef = FALSE;
		if(!GetSecurityDescriptorSacl(pSDS, &fSaclCur, &pSacl, &fSaclDef))
			throw GetLastError();

		DWORD dwRet = SetSecurityInfo
		(
			hObject, SE_KERNEL_OBJECT, 
			LABEL_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
			NULL, NULL, pDacl, pSacl
		);
		if (dwRet != ERROR_SUCCESS)
			throw dwRet;
	}
	catch (DWORD hr)
	{
		dwErrCode = hr;
		ATLTRACE(_T("Error occurred, code=%d"), hr);
	}
	if (pSDD == NULL)
		delete[] PBYTE(pSDD);
	if (pSDS != NULL)
		LocalFree(pSDS);

	if (dwErrCode != NO_ERROR)
		throw dwErrCode;
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

#define SHARE_FILE	_T("c:\\temp\\__$share.tmp")
#define EVENT_RESP	_T("Global\\EVENT_RESP")

class CAtlSvcApc : public ATL::CAtlServiceModuleT< CAtlSvcApc, IDS_SERVICENAME >
{
	static VOID CALLBACK Handler_APCChildProc(ULONG_PTR dwParam);
	HANDLE m_hevResp;
	HANDLE m_hChProc;

public:
	void APCChildProc(LONG lSize);

public:
	DECLARE_LIBID(LIBID_AtlSvcApcLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSVCAPC, "{0028AB8B-DD65-4478-B3AD-48773FC7633E}")
	HRESULT InitializeSecurity() throw()
	{
		return S_OK;
	}

	void OnStop();

	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();
};
CAtlSvcApc _AtlModule;

VOID CALLBACK CAtlSvcApc::Handler_APCChildProc(ULONG_PTR dwParam)
{
	_AtlModule.APCChildProc(LONG(dwParam));
}
void CAtlSvcApc::APCChildProc(LONG lSize)
{
	PSTR	pIter = new char[lSize];
	HANDLE	hFile = CreateFile
	(
		SHARE_FILE, GENERIC_ALL, 0, NULL, OPEN_EXISTING, 0, NULL
	);
	DWORD dwReads = 0;
	ReadFile(hFile, pIter, lSize, &dwReads, NULL);
	LONG lLoops = lSize / 2;
	for (LONG i = 0; i < lLoops; i++)
	{
		char ch = pIter[i];
		pIter[i] = pIter[lSize - i - 1];
		pIter[lSize - i - 1] = ch;
	}
	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	WriteFile(hFile, pIter, lSize, &dwReads, NULL);
	CloseHandle(hFile);
	SetEvent(m_hevResp);
}


extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}



void CAtlSvcApc::OnStop()
{
	TerminateProcess(m_hChProc, 0);
	__super::OnStop();
}

HRESULT CAtlSvcApc::PreMessageLoop(int nShowCmd)
{
	m_hChProc = NULL;

	SECURITY_ATTRIBUTES sa;
	try
	{
		InitSecuInfo(&sa);
		if (sa.lpSecurityDescriptor == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
		m_hevResp = CreateEvent(&sa, FALSE, FALSE, EVENT_RESP);
		if (m_hevResp == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
		SetObjectToLowIntegrity(m_hevResp);
	}
	catch (HRESULT hr)
	{
		if (sa.lpSecurityDescriptor != NULL)
			delete[] PBYTE(sa.lpSecurityDescriptor);
		if (m_hevResp != NULL)	CloseHandle(m_hevResp);
		return hr;
	}

	if (::InterlockedCompareExchange(&m_status.dwCurrentState,
		SERVICE_RUNNING, SERVICE_START_PENDING) == SERVICE_START_PENDING)
	{
		LogEvent(_T("Service started/resumed"));
		::SetServiceStatus(m_hServiceStatus, &m_status);
	}
	return S_OK;
}

HRESULT CAtlSvcApc::PostMessageLoop()
{
	if (m_hChProc != NULL) CloseHandle(m_hChProc);
	if (m_hevResp != NULL) CloseHandle(m_hevResp);

	return __super::PostMessageLoop();
}

void CAtlSvcApc::RunMessageLoop()
{
	TCHAR szCmdLine[512] = { 0, };
	GetModuleFileName(NULL, szCmdLine, MAX_PATH);
	PathRemoveFileSpec(szCmdLine);
	int nLen = _tcslen(szCmdLine);
#if _WIN64
	wsprintf(szCmdLine + nLen, _T("\\AtlCliApc.exe %d %I64d"),
			GetCurrentThreadId(), Handler_APCChildProc);
#else
	wsprintf(szCmdLine + nLen, _T("\\AtlCliApc.exe %d %d"),
			GetCurrentThreadId(), Handler_APCChildProc);
#endif

	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(pi));
	HANDLE hToken = NULL, hMyThr = NULL;
	try
	{
		BOOL bIsOK = DuplicateHandle
		(
			GetCurrentProcess(), GetCurrentThread(),
			GetCurrentProcess(), &hMyThr, 0, FALSE, DUPLICATE_SAME_ACCESS
		);
		if (!bIsOK)
			throw GetLastError();
		SetACLToObject(hMyThr);

		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		if (!WTSQueryUserToken(dwSessionId, &hToken))
			throw GetLastError();

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		bIsOK = CreateProcessAsUser
		(
			hToken, NULL, szCmdLine, NULL, NULL,
			FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
		);
		if (!bIsOK)
			throw GetLastError();
		m_hChProc = pi.hProcess;

		while (true)
		{
			DWORD dwWaitCode = WaitForSingleObjectEx(pi.hProcess, INFINITE, TRUE);
			if (dwWaitCode != WAIT_IO_COMPLETION)
				break;
		}
	}
	catch (DWORD hr)
	{
		ATLTRACE(_T("Error occurred, code=%d"), hr);
	}
	if (hToken != NULL) CloseHandle(hToken);
	if (hMyThr != NULL) CloseHandle(hMyThr);
}
