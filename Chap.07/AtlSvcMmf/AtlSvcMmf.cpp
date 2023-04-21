#include "stdafx.h"
#include "resource.h"
#include "AtlSvcMmf_i.h"
using namespace ATL;

#include <Sddl.h>
#include <AccCtrl.h>
#include <Aclapi.h>
#include <tlhelp32.h>
#include <Wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")

#include "Winsock2.h"
#include "Mswsock.h"
#pragma comment(lib, "Ws2_32.lib")

#include <map>
using namespace std;


PVOID GetSockExtAPI(SOCKET sock, GUID guidFn)
{
	PVOID pfnEx = NULL;
	GUID guid = guidFn;
	DWORD dwBytes = 0;
	LONG lRet = WSAIoctl
		(
		sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid, sizeof(guid), &pfnEx,
		sizeof(pfnEx), &dwBytes, NULL, NULL
		);
	if (lRet == SOCKET_ERROR)
	{
		return NULL;
	}
	return pfnEx;
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



#define TM_EXIT  WM_USER + 1
#define TM_DATA	 WM_USER + 2

#define SYNC_EVT_EXIT	_T("Global\\SYNC_EVT_EXIT")
#define SYNC_EVT_SEND	_T("Global\\SYNC_EVT_SEND")
#define SYNC_EVT_RESP	_T("Global\\SYNC_EVT_RESP")
#define SYNC_SEM_REFS	_T("Global\\SYNC_SEM_REFS")
#define SHARE_FILE		_T("Global\\MMF_SHARE_FILE")

#define MAX_PROC_CNT 4

#define BUFF_SIZE	256
struct SOCK_ITEM : OVERLAPPED
{
	SOCKET		_sock;
	PTP_IO		_ptpIo;
	PTP_TIMER	_timer;
	BOOL		_connected;
	char		_buff[BUFF_SIZE];

	SOCK_ITEM()
	{
		memset(this, 0, sizeof(*this));
		_sock = INVALID_SOCKET;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;

struct PROC_INFO
{
	HANDLE		_proc;
	PTP_WAIT	_wait;

	PROC_INFO()
	{
		memset(this, 0, sizeof(*this));
	}
};


class CAtlSvcMmfModule : public ATL::CAtlServiceModuleT< CAtlSvcMmfModule, IDS_SERVICENAME >
{
	static VOID CALLBACK Handler_SockRead(PTP_CALLBACK_INSTANCE,
		PVOID, PVOID, ULONG, ULONG_PTR, PTP_IO);
	static VOID CALLBACK Handler_ConnectTimer(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_TIMER ptpTimer);
	static VOID CALLBACK Handler_ManageProcess(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT);


	void SockRead(DWORD dwTrBytes, DWORD dwStatus, PSOCK_ITEM pov, PTP_IO ptpIo);
	void ConnectProc(PTP_TIMER ptpTimer);
	void ManageProcess(PTP_WAIT ptpWait);

	HANDLE SpawnProcess();

	DWORD		m_dwMsgId;
	SOCK_ITEM	m_si;

	HANDLE m_hseRefs;
	HANDLE m_hevExit, m_hevSend, m_hevResp;
	HANDLE m_hMmf;
	PBYTE  m_pView;

	PROC_INFO m_arpis[MAX_PROC_CNT];

public:
	DECLARE_LIBID(LIBID_AtlSvcMmfLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSVCMMF, "{FB8FE1A1-6E37-443A-AC6D-A253D00A32E7}")
	HRESULT InitializeSecurity() throw()
	{
		return S_OK;
	}

	CAtlSvcMmfModule()
	{
		m_dwMsgId = 0;
		m_hevExit = m_hevSend = m_hevResp =
		m_hseRefs = m_hseRefs = NULL;
		m_pView = NULL;
	}

	void OnStop();
	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();
};
CAtlSvcMmfModule _AtlModule;


VOID CALLBACK CAtlSvcMmfModule::Handler_SockRead(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	CAtlSvcMmfModule* pThis = (CAtlSvcMmfModule*)pCtx;
	pThis->SockRead(dwTrBytes, ior, (PSOCK_ITEM)pov, ptpIo);
}
void CAtlSvcMmfModule::SockRead(DWORD dwTrBytes, DWORD dwStatus, PSOCK_ITEM psi, PTP_IO ptpIo)
{
	if (!psi->_connected)
	{
		if (dwStatus != NO_ERROR)
		{
			ULARGE_INTEGER ll; ll.QuadPart = -10000000LL;
			FILETIME ft; ft.dwHighDateTime = ll.HighPart, ft.dwLowDateTime = ll.LowPart;
			SetThreadpoolTimer(psi->_timer, &ft, 0, 0);
			return;
		}
		SetThreadpoolTimer(psi->_timer, NULL, 0, 0);
		psi->_connected = TRUE;
	}
	else
	{
		if (dwStatus != NO_ERROR || dwTrBytes == 0)
			goto $LABEL_CLOSE;

		PBYTE pData = new BYTE[dwTrBytes];
		memcpy(pData, psi->_buff, dwTrBytes);
		PostThreadMessage(m_dwMsgId, TM_DATA, (WPARAM)dwTrBytes, (LPARAM)pData);
	}

	StartThreadpoolIo(ptpIo);
	WSABUF wb; DWORD dwFlags = 0;
	wb.buf = psi->_buff, wb.len = BUFF_SIZE;
	int nSockRet = WSARecv(psi->_sock, &wb, 1, NULL, &dwFlags, psi, NULL);
	if (nSockRet == SOCKET_ERROR)
	{
		dwStatus = WSAGetLastError();
		if (dwStatus != WSA_IO_PENDING)
		{
			CancelThreadpoolIo(ptpIo);
			goto $LABEL_CLOSE;
		}
	}
	return;

$LABEL_CLOSE:
	if (dwStatus == ERROR_OPERATION_ABORTED)
		return;

	LPFN_DISCONNECTEX pfnDisconnectEx =
		(LPFN_DISCONNECTEX)GetSockExtAPI(psi->_sock, WSAID_DISCONNECTEX);
	pfnDisconnectEx(psi->_sock, NULL, TF_REUSE_SOCKET, 0);
	psi->_connected = FALSE;

	ULARGE_INTEGER ll; ll.QuadPart = -10000000LL;
	FILETIME ft; ft.dwHighDateTime = ll.HighPart, ft.dwLowDateTime = ll.LowPart;
	SetThreadpoolTimer(psi->_timer, &ft, 0, 0);
}

VOID CALLBACK CAtlSvcMmfModule::Handler_ConnectTimer(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_TIMER ptpTimer)
{
	CAtlSvcMmfModule* pThis = (CAtlSvcMmfModule*)pCtx;
	pThis->ConnectProc(ptpTimer);
}
void CAtlSvcMmfModule::ConnectProc(PTP_TIMER ptpTimer)
{
	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family		= AF_INET;
	sa.sin_port			= htons(9001);
	sa.sin_addr.s_addr	= inet_addr("127.0.0.1");

	StartThreadpoolIo(m_si._ptpIo);
	LPFN_CONNECTEX pfnConnectEx = (LPFN_CONNECTEX)GetSockExtAPI(m_si._sock, WSAID_CONNECTEX);
	BOOL bIsOK = pfnConnectEx
	(
		m_si._sock, (PSOCKADDR)&sa, sizeof(sa), NULL, 0, NULL, &m_si
	);
	if (!bIsOK)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode != WSA_IO_PENDING)
		{
			CancelThreadpoolIo(m_si._ptpIo);

			ULARGE_INTEGER ll; ll.QuadPart = -10000000LL;
			FILETIME ft; ft.dwHighDateTime = ll.HighPart, ft.dwLowDateTime = ll.LowPart;
			SetThreadpoolTimer(m_si._timer, &ft, 0, 0);
		}
	}
}

VOID CALLBACK CAtlSvcMmfModule::Handler_ManageProcess(PTP_CALLBACK_INSTANCE, PVOID pCtx, PTP_WAIT ptpWait, TP_WAIT_RESULT)
{
	CAtlSvcMmfModule* pThis = (CAtlSvcMmfModule*)pCtx;
	pThis->ManageProcess(ptpWait);
}
void CAtlSvcMmfModule::ManageProcess(PTP_WAIT ptpWait)
{
	int i = 0;
	for (; i < MAX_PROC_CNT; i++)
	{
		if (m_arpis[i]._wait == ptpWait)
			break;
	}
	if (i < MAX_PROC_CNT)
	{
		CloseHandle(m_arpis[i]._proc);
		m_arpis[i]._proc = SpawnProcess();
		SetThreadpoolWait(m_arpis[i]._wait, m_arpis[i]._proc, NULL);
	}
}

extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}


HANDLE CAtlSvcMmfModule::SpawnProcess()
{
	TCHAR szCmdLine[256];
	GetModuleFileName(NULL, szCmdLine, MAX_PATH);
	PathRemoveFileSpec(szCmdLine);
	_tcscat_s(szCmdLine, _T("\\MmfConsole.exe"));

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	HANDLE hToken = NULL;
	try
	{
		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		if (!WTSQueryUserToken(dwSessionId, &hToken))
			throw GetLastError();
		
		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		BOOL bIsOK = CreateProcessAsUser
		(
			hToken, NULL, szCmdLine, NULL, NULL, TRUE,
			NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE,
			NULL, NULL, &si, &pi
		);
		if (!bIsOK)
			throw GetLastError();

		CloseHandle(pi.hThread);
		CloseHandle(hToken);
	}
	catch (DWORD e)
	{
		if (hToken != NULL) CloseHandle(hToken);
		throw e;
	}
	return pi.hProcess;
}

void CAtlSvcMmfModule::OnStop()
{
	PostThreadMessage(m_dwMsgId, TM_EXIT, 0, 0);

	__super::OnStop();
}

HRESULT CAtlSvcMmfModule::PreMessageLoop(int nShowCmd)
{
	WSADATA	wsd;
	int nInitCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nInitCode != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(nInitCode);
	m_dwMsgId = GetCurrentThreadId();

	SECURITY_ATTRIBUTES sa;
	try
	{
		InitSecuInfo(&sa);
		if (sa.lpSecurityDescriptor == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());

		m_hMmf = CreateFileMapping
		(
			INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, 65536, SHARE_FILE
		);
		if (m_hMmf == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
		SetObjectToLowIntegrity(m_hMmf);

		m_pView = (LPBYTE)MapViewOfFileEx
		(
			m_hMmf, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0, NULL
		);
		if (m_pView == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());

		m_hseRefs = CreateSemaphore(&sa, 0, MAX_PROC_CNT, SYNC_SEM_REFS);
		if (m_hseRefs == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
		SetObjectToLowIntegrity(m_hseRefs);

		m_hevExit = CreateEvent(&sa, TRUE, FALSE, SYNC_EVT_EXIT);
		if (m_hevExit == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
		SetObjectToLowIntegrity(m_hevExit);
		m_hevSend = CreateEvent(&sa, TRUE, FALSE, SYNC_EVT_SEND);
		if (m_hevSend == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
		SetObjectToLowIntegrity(m_hevSend);
		m_hevResp = CreateEvent(&sa, TRUE, FALSE, SYNC_EVT_RESP);
		if (m_hevResp == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
		SetObjectToLowIntegrity(m_hevResp);


		m_si._sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_si._sock == INVALID_SOCKET)
			throw HRESULT_FROM_WIN32(WSAGetLastError());

		SOCKADDR_IN	sai;
		memset(&sai, 0, sizeof(SOCKADDR_IN));
		sai.sin_family = AF_INET;
		sai.sin_addr.s_addr = htonl(INADDR_ANY);
		LONG lSockRet = bind(m_si._sock, (PSOCKADDR)&sai, sizeof(SOCKADDR_IN));
		if (lSockRet == SOCKET_ERROR)
			throw HRESULT_FROM_WIN32(WSAGetLastError());

		m_si._ptpIo = CreateThreadpoolIo((HANDLE)m_si._sock, Handler_SockRead, this, NULL);
		if (m_si._ptpIo == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());

		m_si._timer = CreateThreadpoolTimer(Handler_ConnectTimer, this, NULL);
		if (m_si._timer == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
	}
	catch (HRESULT hr)
	{
		if (sa.lpSecurityDescriptor != NULL)
			delete[] PBYTE(sa.lpSecurityDescriptor);

		if (m_si._sock != INVALID_SOCKET)
			closesocket(m_si._sock);
		if (m_si._ptpIo != NULL)
			CloseThreadpoolIo(m_si._ptpIo);
		if (m_si._timer != NULL)
			CloseThreadpoolTimer(m_si._timer);

		if (m_hseRefs != NULL)	CloseHandle(m_hseRefs);
		if (m_hevResp != NULL)	CloseHandle(m_hevResp);
		if (m_hevSend != NULL)	CloseHandle(m_hevSend);
		if (m_pView != NULL)	UnmapViewOfFile(m_pView);
		if (m_hMmf != NULL)		CloseHandle(m_hMmf);
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

HRESULT CAtlSvcMmfModule::PostMessageLoop()
{
	if (m_si._ptpIo != NULL)
	{
		CancelIoEx((HANDLE)m_si._sock, NULL);
		WaitForThreadpoolIoCallbacks(m_si._ptpIo, TRUE);
		CloseThreadpoolIo(m_si._ptpIo);
		closesocket(m_si._sock);
	}
	if (m_si._timer != NULL)
	{
		WaitForThreadpoolTimerCallbacks(m_si._timer, TRUE);
		CloseThreadpoolTimer(m_si._timer);
	}

	if (m_hseRefs != NULL)	CloseHandle(m_hseRefs);
	if (m_hevResp != NULL)	CloseHandle(m_hevResp);
	if (m_hevSend != NULL)	CloseHandle(m_hevSend);
	if (m_hevExit != NULL)	CloseHandle(m_hevExit);
	if (m_pView != NULL)	UnmapViewOfFile(m_pView);
	if (m_hMmf != NULL)		CloseHandle(m_hMmf);

	WSACleanup();

	return __super::PostMessageLoop();
}

void CAtlSvcMmfModule::RunMessageLoop()
{
	TP_CALLBACK_ENVIRON ce;
	InitializeThreadpoolEnvironment(&ce);

	PTP_POOL pPool = NULL;
	PTP_CLEANUP_GROUP pCG = NULL;
	try
	{
		pPool = CreateThreadpool(NULL);
		if (pPool == NULL)
			throw GetLastError();
		SetThreadpoolThreadMaximum(pPool, 1);
		if (!SetThreadpoolThreadMinimum(pPool, 1))
			throw GetLastError();

		pCG = CreateThreadpoolCleanupGroup();
		if (pCG == NULL)
			throw GetLastError();
		SetThreadpoolCallbackPool(&ce, pPool);

		for (int i = 0; i < MAX_PROC_CNT; i++)
		{
			m_arpis[i]._proc = SpawnProcess();
			m_arpis[i]._wait = CreateThreadpoolWait(Handler_ManageProcess, this, &ce);
			if (m_arpis[i]._wait == NULL)
				throw GetLastError();
			SetThreadpoolWait(m_arpis[i]._wait, m_arpis[i]._proc, NULL);
		}

		FILETIME ft; ft.dwHighDateTime = ft.dwLowDateTime = 0;
		SetThreadpoolTimer(m_si._timer, &ft, 0, 0);

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			if (msg.message == TM_EXIT)
				break;

			while (WaitForSingleObject(m_hseRefs, 0) == WAIT_OBJECT_0);

			int nLen = (int)msg.wParam;
			PBYTE pData = (PBYTE)msg.lParam;
			*((PINT)m_pView) = nLen;
			memcpy(m_pView + sizeof(int), pData, nLen);
			delete[] pData;
			SignalObjectAndWait(m_hevSend, m_hevResp, INFINITE, FALSE);
			ResetEvent(m_hevResp);
		}
	}
	catch (DWORD e)
	{
		ATLTRACE(_T("CreatePipeItem failed, code=%d"), e);
	}
	SetEvent(m_hevExit);

	if (pPool != NULL)
		CloseThreadpool(pPool);
	if (pCG != NULL)
	{
		CloseThreadpoolCleanupGroupMembers(pCG, FALSE, NULL);
		CloseThreadpoolCleanupGroup(pCG);
	}
	for (int i = 0; i < MAX_PROC_CNT; i++)
	{
		if (m_arpis[i]._proc != NULL)
			CloseHandle(m_arpis[i]._proc);
	}

	DestroyThreadpoolEnvironment(&ce);
}
