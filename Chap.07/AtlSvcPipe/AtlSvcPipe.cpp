#include "stdafx.h"
#include "resource.h"
#include "AtlSvcPipe_i.h"
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
		ATLTRACE(_T("WSAIoctl failed, code : %d\n"), WSAGetLastError());
		return NULL;
	}
	return pfnEx;
}

SOCKET GetListenSocket(short shPortNo, int nBacklog = SOMAXCONN)
{
	SOCKET hsoListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hsoListen == INVALID_SOCKET)
		return INVALID_SOCKET;

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(shPortNo);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	LONG lSockRet = bind(hsoListen, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
	if (lSockRet == SOCKET_ERROR)
	{
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	lSockRet = listen(hsoListen, nBacklog);
	if (lSockRet == SOCKET_ERROR)
	{
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	return hsoListen;
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



#define BUFF_SIZE	256
struct SOCK_ITEM : OVERLAPPED
{
	UINT    _id;
	SOCKET	_sock;
	PTP_IO	_ptpIo;
	char	_buff[BUFF_SIZE];

	SOCK_ITEM(UINT id, SOCKET sock)
	{
		memset(this, 0, sizeof(*this));
		_id = id;
		_sock = sock;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;

struct PIPE_ITEM : OVERLAPPED
{
	UINT    _id;
	HANDLE  _proc;
	HANDLE  _pipe;
	PTP_IO	_ptpIo;
	TCHAR	_name[16];
	char	_buff[BUFF_SIZE];

	PIPE_ITEM(UINT id, HANDLE pipe, HANDLE proc)
	{
		memset(this, 0, sizeof(*this));
		_id = id;
		_pipe = pipe, _proc = proc;
	}
};
typedef PIPE_ITEM* PPIPE_ITEM;

struct SESSION
{
	PSOCK_ITEM _psi;
	PPIPE_ITEM _ppi;

	SESSION(PSOCK_ITEM psi, PPIPE_ITEM ppi) { _psi = psi, _ppi = ppi; }
};
typedef std::map<UINT, SESSION> SESS_MAP;


class CAtlSvcPipeModule : public ATL::CAtlServiceModuleT< CAtlSvcPipeModule, IDS_SERVICENAME >
{
	static UINT s_incIds;

	static VOID CALLBACK Handler_SockRead(PTP_CALLBACK_INSTANCE,
		PVOID, PVOID, ULONG, ULONG_PTR, PTP_IO);
	static VOID CALLBACK Handler_PipeRead(PTP_CALLBACK_INSTANCE pInst,
		PVOID, PVOID, ULONG, ULONG_PTR, PTP_IO);

	void SockListen(bool bFirst, DWORD dwTrBytes, DWORD dwStatus, PSOCK_ITEM pov, PTP_IO ptpIo);
	void SockRead(bool bFirst, DWORD dwTrBytes, DWORD dwStatus, PSOCK_ITEM pov, PTP_IO ptpIo);
	void PipeRead(bool bFirst, DWORD dwTrBytes, DWORD dwStatus, PPIPE_ITEM pov, PTP_IO ptpIo);
	HANDLE CreatePipeItem(PSOCKADDR_IN psai, HANDLE&);

	HANDLE m_hevExit;

	SOCKET m_soListen;
	SESS_MAP m_ssMap;
	CRITICAL_SECTION m_cs;

public:
	DECLARE_LIBID(LIBID_AtlSvcPipeLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSVCPIPE, "{85EEB80A-2EFC-42BB-8C01-DC992CC5F6C2}")
	HRESULT InitializeSecurity() throw()
	{
		return S_OK;
	}

	void OnStop();

	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();
};
UINT CAtlSvcPipeModule::s_incIds;
CAtlSvcPipeModule _AtlModule;

extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}



VOID CALLBACK CAtlSvcPipeModule::Handler_SockRead(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	CAtlSvcPipeModule* pThis = (CAtlSvcPipeModule*)pCtx;
	pThis->SockRead((pInst == NULL), dwTrBytes, ior, (PSOCK_ITEM)pov, ptpIo);
}
void CAtlSvcPipeModule::SockRead(bool bFirst, DWORD dwTrBytes, DWORD dwStatus, PSOCK_ITEM psi, PTP_IO ptpIo)
{
	PPIPE_ITEM ppi = NULL;
	EnterCriticalSection(&m_cs);
	SESS_MAP::iterator it = m_ssMap.find(psi->_id);
	if (it != m_ssMap.end())
		ppi = it->second._ppi;
	LeaveCriticalSection(&m_cs);

	if (dwStatus != NO_ERROR || (!bFirst && dwTrBytes == 0))
		goto $LABEL_CLOSE;

	if (!bFirst)
	{
		DWORD dwWrotes = 0;
		BOOL bIsOK = WriteFile(ppi->_pipe, psi->_buff, dwTrBytes, &dwWrotes, NULL);
		if (!bIsOK)
		{
			dwStatus = GetLastError();
			goto $LABEL_CLOSE;
		}
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

	EnterCriticalSection(&m_cs);
	if (it != m_ssMap.end())
		m_ssMap.erase(it);
	LeaveCriticalSection(&m_cs);

	closesocket(psi->_sock);
	delete psi;
	CloseThreadpoolIo(ptpIo);
	if (ppi != NULL)
	{
		CancelIoEx(ppi->_pipe, NULL);
		WaitForThreadpoolIoCallbacks(ppi->_ptpIo, TRUE);
		CloseThreadpoolIo(ppi->_ptpIo);
		DisconnectNamedPipe(ppi->_pipe);
		CloseHandle(ppi->_pipe);
		delete ppi;
	}
}

VOID CALLBACK CAtlSvcPipeModule::Handler_PipeRead(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	CAtlSvcPipeModule* pThis = (CAtlSvcPipeModule*)pCtx;
	pThis->PipeRead((pInst == NULL), dwTrBytes, ior, (PPIPE_ITEM)pov, ptpIo);
}
void CAtlSvcPipeModule::PipeRead(bool bFirst, DWORD dwTrBytes, DWORD dwStatus, PPIPE_ITEM ppi, PTP_IO ptpIo)
{
	PSOCK_ITEM psi = NULL;
	EnterCriticalSection(&m_cs);
	SESS_MAP::iterator it = m_ssMap.find(ppi->_id);
	if (it != m_ssMap.end())
		psi = it->second._psi;
	LeaveCriticalSection(&m_cs);

	if (dwStatus != NO_ERROR || (!bFirst && dwTrBytes == 0))
		goto $LABEL_CLOSE;

	if (!bFirst)
	{
		int lSockRet = send(psi->_sock, ppi->_buff, dwTrBytes, 0);
		if (lSockRet == SOCKET_ERROR)
		{
			dwStatus = WSAGetLastError();
			goto $LABEL_CLOSE;
		}
	}

	StartThreadpoolIo(ptpIo);
	BOOL bIsOK = ReadFile(ppi->_pipe, ppi->_buff, BUFF_SIZE, NULL, ppi);
	if (!bIsOK)
	{
		dwStatus = GetLastError();
		if (dwStatus != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(ptpIo);
			goto $LABEL_CLOSE;
		}
	}
	return;

$LABEL_CLOSE:
	if (dwStatus == ERROR_OPERATION_ABORTED)
		return;

	EnterCriticalSection(&m_cs);
	if (it != m_ssMap.end())
		m_ssMap.erase(it);
	LeaveCriticalSection(&m_cs);

	CloseHandle(ppi->_pipe);
	delete ppi;
	CloseThreadpoolIo(ptpIo);
	if (psi != NULL)
	{
		CancelIoEx((HANDLE)psi->_sock, NULL);
		WaitForThreadpoolIoCallbacks(psi->_ptpIo, TRUE);
		closesocket(psi->_sock);
		CloseThreadpoolIo(psi->_ptpIo);
		delete psi;
	}
}

HANDLE CAtlSvcPipeModule::CreatePipeItem(PSOCKADDR_IN psai, HANDLE& hPipe)
{
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	HANDLE hToken = NULL;
	SECURITY_ATTRIBUTES sa;
	try
	{
		InitSecuInfo(&sa);
		if (sa.lpSecurityDescriptor == NULL)
			throw GetLastError();

		TCHAR szIDName[64];
		wsprintf(szIDName, _T("%08X_%04X"), psai->sin_addr.s_addr, psai->sin_port);
		TCHAR szPipeName[64];
		wsprintf(szPipeName, _T("\\\\.\\PIPE\\%s"), szIDName);
		hPipe = CreateNamedPipe
		(
			szPipeName, 
			FILE_FLAG_FIRST_PIPE_INSTANCE | PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 1, BUFF_SIZE, BUFF_SIZE, 0, &sa
		);
		delete[] PBYTE(sa.lpSecurityDescriptor);
		if (hPipe == INVALID_HANDLE_VALUE)
			throw GetLastError();
		SetObjectToLowIntegrity(hPipe);

		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		if (!WTSQueryUserToken(dwSessionId, &hToken))
			throw GetLastError();

		TCHAR szCmdLine[256];
		GetModuleFileName(NULL, szCmdLine, MAX_PATH);
		PathRemoveFileSpec(szCmdLine);
		int nLen = _tcslen(szCmdLine);
		wsprintf(szCmdLine + nLen, _T("\\PipeConsole.exe %s"), szIDName);

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		si.lpTitle = szPipeName + 4;
		BOOL bIsOK = CreateProcessAsUser
		(
			hToken, NULL, szCmdLine, NULL, NULL, TRUE,
			NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE,
			NULL, NULL, &si, &pi
		);
		if (!bIsOK)
			throw GetLastError();

		bIsOK = ConnectNamedPipe(hPipe, NULL);
		if (!bIsOK)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_PIPE_CONNECTED)
				throw dwErrCode;
		}

		CloseHandle(pi.hThread);
		CloseHandle(hToken);
	}
	catch (DWORD e)
	{
		ATLTRACE(_T("CreatePipeItem failed, code=%d"), e);
		if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
		if (hToken != NULL) CloseHandle(hToken);
		return NULL;
	}
	return pi.hProcess;
}



void CAtlSvcPipeModule::OnStop()
{
	SetEvent(m_hevExit);

	__super::OnStop();
}

HRESULT CAtlSvcPipeModule::PreMessageLoop(int nShowCmd)
{
	WSADATA	wsd;
	int nInitCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nInitCode != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(nInitCode);

	m_hevExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_hevExit == NULL)
		return HRESULT_FROM_WIN32(GetLastError());

	InitializeCriticalSection(&m_cs);

	if (::InterlockedCompareExchange(&m_status.dwCurrentState,
		SERVICE_RUNNING, SERVICE_START_PENDING) == SERVICE_START_PENDING)
	{
		LogEvent(_T("Service started/resumed"));
		::SetServiceStatus(m_hServiceStatus, &m_status);
	}
	return S_OK;
}

HRESULT CAtlSvcPipeModule::PostMessageLoop()
{
	DeleteCriticalSection(&m_cs);
	CloseHandle(m_hevExit);
	WSACleanup();

	return __super::PostMessageLoop();
}


void CAtlSvcPipeModule::RunMessageLoop()
{
	m_soListen = GetListenSocket(9001);
	if (m_soListen == INVALID_SOCKET)
		return;

	WSAEVENT hevListen = WSACreateEvent();
	WSAEventSelect(m_soListen, hevListen, FD_ACCEPT);

	HANDLE arhEvents[2] = { m_hevExit, hevListen };
	while (true)
	{
		DWORD dwWaitRet = WaitForMultipleObjects(2, arhEvents, FALSE, INFINITE);
		if (dwWaitRet == WAIT_FAILED)
		{
			ATLTRACE(_T("MsgWaitForMultipleObjects failed, code=%d"), GetLastError());
			break;
		}
		if (dwWaitRet == WAIT_OBJECT_0)
			break;

		WSANETWORKEVENTS ne;
		WSAEnumNetworkEvents(m_soListen, hevListen, &ne);
		if ((ne.lNetworkEvents & FD_ACCEPT) != FD_ACCEPT)
			continue;

		int nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
		if (nErrCode != 0)
		{
			ATLTRACE(_T(" ==> Error occurred, code=%d"), nErrCode);
			break;
		}

		SOCKADDR_IN sai;
		int saLen = sizeof(SOCKADDR_IN);
		SOCKET sock = accept(m_soListen, (PSOCKADDR)&sai, &saLen);
		if (sock == INVALID_SOCKET)
			break;

		HANDLE hPipe = INVALID_HANDLE_VALUE;
		HANDLE hProc = NULL;
		PTP_IO ptpSock = NULL, ptpPipe = NULL;
		try
		{
			hProc = CreatePipeItem(&sai, hPipe);
			if (hProc == NULL)
				throw GetLastError();

			ptpSock = CreateThreadpoolIo((HANDLE)sock, Handler_SockRead, this, NULL);
			if (ptpSock == NULL)
				throw GetLastError();
			ptpPipe = CreateThreadpoolIo(hPipe, Handler_PipeRead, this, NULL);
			if (ptpPipe == NULL)
				throw GetLastError();
		}
		catch (DWORD e)
		{
			ATLTRACE(_T("RunMessageLoop failed, code=%d"), e);
			closesocket(sock);
			if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
			if (hProc != NULL) CloseHandle(hProc);
			if (ptpSock != NULL) CloseThreadpoolIo(ptpSock);
			if (ptpPipe != NULL) CloseThreadpoolIo(ptpPipe);
			continue;
		}

		PSOCK_ITEM psi = new SOCK_ITEM(s_incIds, sock);
		psi->_ptpIo = ptpSock;
		PPIPE_ITEM ppi = new PIPE_ITEM(s_incIds, hPipe, hProc);
		ppi->_ptpIo = ptpPipe;
		SESSION ss(psi, ppi);

		EnterCriticalSection(&m_cs);
		m_ssMap.insert(std::make_pair(psi->_id, ss));
		LeaveCriticalSection(&m_cs);
		s_incIds++;

		Handler_SockRead(NULL, this, psi, 0, 0, ptpSock);
		Handler_PipeRead(NULL, this, ppi, 0, 0, ptpPipe);
	}

	for (SESS_MAP::iterator it = m_ssMap.begin(); it != m_ssMap.end(); it++)
	{
		PSOCK_ITEM psi = it->second._psi;
		PPIPE_ITEM ppi = it->second._ppi;
		CancelIoEx((HANDLE)psi->_sock, NULL);
		CancelIoEx(ppi->_pipe, NULL);

		WaitForThreadpoolIoCallbacks(psi->_ptpIo, TRUE);
		closesocket(psi->_sock);
		CloseThreadpoolIo(psi->_ptpIo);
		delete psi;

		WaitForThreadpoolIoCallbacks(ppi->_ptpIo, TRUE);
		CloseHandle(ppi->_pipe);
		CloseThreadpoolIo(ppi->_ptpIo);
		WaitForSingleObject(ppi->_proc, INFINITE);
		delete ppi;
	}
}
