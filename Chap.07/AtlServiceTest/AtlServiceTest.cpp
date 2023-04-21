#include "stdafx.h"
#include "resource.h"
#include "AtlServiceTest_i.h"
using namespace ATL;

#include <stdio.h>

class CAtlServiceTestModule : public ATL::CAtlServiceModuleT< CAtlServiceTestModule, IDS_SERVICENAME >
{
	HANDLE m_hevExit;

public:
	DECLARE_LIBID(LIBID_AtlServiceTestLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSERVICETEST, "{0F939D52-D5DB-429E-9999-DAE40049D567}")
	HRESULT InitializeSecurity() throw()
	{
		return S_OK;
	}

	void OnStop();

	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();
};
CAtlServiceTestModule _AtlModule;


extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}



void CAtlServiceTestModule::OnStop()
{
	SetEvent(m_hevExit);
	__super::OnStop();
}

HRESULT CAtlServiceTestModule::PreMessageLoop(int nShowCmd)
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

HRESULT CAtlServiceTestModule::PostMessageLoop()
{
	if (m_hevExit != NULL)
		CloseHandle(m_hevExit);
	return __super::PostMessageLoop();
}

void CAtlServiceTestModule::RunMessageLoop()
{
	WaitForSingleObject(m_hevExit, INFINITE);
}
