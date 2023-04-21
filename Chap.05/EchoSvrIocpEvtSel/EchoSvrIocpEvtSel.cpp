#include "stdafx.h"
#include "Winsock2.h"
#include "Ntsecapi.h"
#include "set"
#include "map"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

SOCKET GetListenSocket(short shPortNo, int nBacklog = SOMAXCONN)
{
	SOCKET hsoListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hsoListen == INVALID_SOCKET)
	{
		cout << "socket failed, code : " << WSAGetLastError() << endl;
		return INVALID_SOCKET;
	}

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(shPortNo);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	LONG lSockRet = bind(hsoListen, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "bind failed, code : " << WSAGetLastError() << endl;
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	lSockRet = listen(hsoListen, nBacklog);
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "listen failed, code : " << WSAGetLastError() << endl;
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	return hsoListen;
}

PCTSTR c_pszActions[] =
{
	_T("ADDED"),
	_T("REMOVED"),
	_T("MODIFIED"),
	_T("RENAMED_OLD_NAME"),
	_T("RENAMED_NEW_NAME")
};
#define DIR_NOTI_FILTER	FILE_NOTIFY_CHANGE_FILE_NAME |	\
						FILE_NOTIFY_CHANGE_DIR_NAME |	\
						FILE_NOTIFY_CHANGE_SIZE |		\
						FILE_NOTIFY_CHANGE_LAST_WRITE

void PrintDirModEntries(PCSTR pszDir, PBYTE pIter)
{
	cout << " => MonDir " << pszDir;
	while (true)
	{
		PFILE_NOTIFY_INFORMATION pFNI = (PFILE_NOTIFY_INFORMATION)pIter;
		TCHAR szFileName[MAX_PATH];
		memcpy(szFileName, pFNI->FileName, pFNI->FileNameLength * sizeof(TCHAR));
		szFileName[pFNI->FileNameLength / sizeof(TCHAR)] = 0;
		_tprintf(_T(" : File %s %s\n"), szFileName, c_pszActions[pFNI->Action - 1]);
		pIter += pFNI->NextEntryOffset;

		if (pFNI->NextEntryOffset == 0)
			break;
	}
}


#ifndef STATUS_LOCAL_DISCONNECT
#	define STATUS_LOCAL_DISCONNECT	((NTSTATUS)0xC000013BL)
#endif
#ifndef STATUS_REMOTE_DISCONNECT
#	define STATUS_REMOTE_DISCONNECT	((NTSTATUS)0xC000013CL)
#endif


#define IOKEY_NEW_SOCK		1
#define IOKEY_RECV_SOCK		2
#define IOKEY_MON_DIR		3

#define TM_MSG_TEST			100
#define TM_SOCK_CLOSED		101
#define TM_DIR_CLOSED		102
#define TM_DIR_CONFIG		103

#define BUFF_SIZE	4096

typedef std::string String;
struct DIR_ITEM : OVERLAPPED
{
	HANDLE	_dir;
	String	_path;
	BYTE	_buff[BUFF_SIZE];

	DIR_ITEM(HANDLE hDir, PCSTR path)
	{
		memset(this, 0, sizeof(OVERLAPPED));
		memset(_buff, 0, sizeof(_buff));
		_dir = hDir;
		_path = path;
	}
	~DIR_ITEM()
	{
		if (_dir != INVALID_HANDLE_VALUE)
			CloseHandle(_dir);
	}
};
typedef DIR_ITEM* PDIR_ITEM;
typedef std::map<std::string, PDIR_ITEM> DIR_MAP;

struct SOCK_ITEM : OVERLAPPED
{
	SOCKET	_sock;
	char	_buff[BUFF_SIZE];

	SOCK_ITEM(SOCKET sock)
	{
		memset(this, 0, sizeof(*this));
		_sock = sock;
	}
	~SOCK_ITEM()
	{
		if (_sock != INVALID_SOCKET)
			closesocket(_sock);
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;

typedef std::set<LPOVERLAPPED> OV_SET;



class MainMngr
{
	static DWORD WINAPI IocpWorkProc(PVOID pParam);
	static DWORD WINAPI MainMngrProc(PVOID pParam);

	DWORD	 m_dwThrId;
	HANDLE	 m_hMainThr;
	DIR_MAP	 m_dirMap;

	OV_SET	 m_ovSet;
	HANDLE	 m_hIocp;
	PHANDLE	 m_parWorkers;
	INT		 m_nWorkerCnt;

	SOCKET	 m_soListen;
	WSAEVENT m_evListen;

	HANDLE	 m_evMonReg;
	HKEY	 m_regMonKey;

#define	MAIN_WAIT_CNT	3
	HANDLE	 m_hevExit;

protected:
	void Init();
	void Uninit();
	void Run();

	bool Handler_Measage();
	bool Handler_Accept();
	bool Handler_Registry(bool bNoti = true);

public:
	MainMngr()
	{
		m_dwThrId = 0;
		m_hIocp = NULL;
		m_parWorkers = NULL;

		m_soListen = INVALID_SOCKET;
		m_evListen = NULL;

		m_evMonReg = NULL;
		m_regMonKey = NULL;

		m_hevExit = NULL;
	}
	~MainMngr()
	{
		Uninit();
	}

	void Start();
	void Stop();

	void ConfigMonDir(bool bRemove, PSTR pszPath, bool del = false)
	{
		INT opt = MAKELONG((BOOL)bRemove, (BOOL)del);
		PostThreadMessage(m_dwThrId, TM_DIR_CONFIG, (WPARAM)opt, (LPARAM)pszPath);
	}
};



DWORD WINAPI MainMngr::IocpWorkProc(PVOID pParam)
{
	MainMngr*		pMgr = (MainMngr*)pParam;
	LPOVERLAPPED	pov = NULL;
	DWORD			dwTrBytes = 0;
	ULONG_PTR		upDevKey = 0;

	while (true)
	{
		try
		{
			BOOL bIsOK = GetQueuedCompletionStatus
			(
				pMgr->m_hIocp, &dwTrBytes, &upDevKey, (LPOVERLAPPED*)&pov, INFINITE
			);
			if (bIsOK == FALSE)
			{
				if (pov != NULL)
					throw (int)pov->Internal;

				int nErrCode = WSAGetLastError();
				if (nErrCode != ERROR_ABANDONED_WAIT_0)
					cout << "GQCS failed: " << nErrCode << endl;
				break;
			}

			switch (upDevKey)
			{
				case IOKEY_NEW_SOCK:
				case IOKEY_RECV_SOCK:
				{
					PSOCK_ITEM psi = (PSOCK_ITEM)pov;
					if (upDevKey == IOKEY_NEW_SOCK)
					{
						CreateIoCompletionPort((HANDLE)psi->_sock, pMgr->m_hIocp, IOKEY_RECV_SOCK, 0);
						cout << " ==> New client " << psi->_sock << " connected..." << endl;
					}
					else
					{
						if (dwTrBytes == 0)
							throw (INT)ERROR_SUCCESS;

						psi->_buff[dwTrBytes] = 0;
						cout << " *** Client(" << psi->_sock << ") sent : " << psi->_buff << endl;
						int lSockRet = send(psi->_sock, psi->_buff, dwTrBytes, 0);
						if (lSockRet == SOCKET_ERROR)
							throw WSAGetLastError();
					}

					DWORD dwFlags = 0;
					WSABUF wb;
					wb.buf = psi->_buff, wb.len = sizeof(psi->_buff);
					int nSockRet = WSARecv(psi->_sock, &wb, 1, NULL, &dwFlags, psi, NULL);
					if (nSockRet == SOCKET_ERROR)
					{
						int nErrCode = WSAGetLastError();
						if (nErrCode != WSA_IO_PENDING)
							throw nErrCode;
					}
				}
				break;

				case IOKEY_MON_DIR:
				{
					if (pov->Internal != 0)
						throw (int)LsaNtStatusToWinError(pov->Internal);

					PDIR_ITEM pdi = (PDIR_ITEM)pov;
					if (dwTrBytes == 0)
					{
						CreateIoCompletionPort(pdi->_dir, pMgr->m_hIocp, IOKEY_MON_DIR, 0);
						cout << " ==> NonDir  " << pdi->_path.c_str() << " added..." << endl;
					}
					else
						PrintDirModEntries(pdi->_path.c_str(), pdi->_buff);

					bIsOK = ReadDirectoryChangesW
					(
						pdi->_dir, pdi->_buff, BUFF_SIZE, FALSE,
						DIR_NOTI_FILTER, NULL, pdi, NULL
					);
					if (!bIsOK)
						throw (int)GetLastError();
				}
				break;
			}
		}
		catch (int ex)
		{
			UINT msg = (upDevKey == IOKEY_MON_DIR) ? TM_DIR_CLOSED : TM_SOCK_CLOSED;
			PostThreadMessage(pMgr->m_dwThrId, msg, ex, (LPARAM)pov);
		}
	}
	return 0;
}

DWORD WINAPI MainMngr::MainMngrProc(PVOID pParam)
{
	MainMngr* pMgr = (MainMngr*)pParam;
	try
	{
		pMgr->Run();
	}
	catch (HRESULT hr)
	{
		cout << "Thr " << GetCurrentThreadId() << "catch Exception: " << hr << endl;
	}
	return 0;
}

void MainMngr::Init()
{
	m_hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_hevExit == NULL)
		throw HRESULT_FROM_WIN32(GetLastError());

	m_soListen = GetListenSocket(9001);
	if (m_soListen == INVALID_SOCKET)
		throw HRESULT_FROM_WIN32(WSAGetLastError());
	m_evListen = WSACreateEvent();
	if (m_evListen == NULL)
		throw HRESULT_FROM_WIN32(WSAGetLastError());
	if (WSAEventSelect(m_soListen, m_evListen, FD_ACCEPT) == SOCKET_ERROR)
		throw HRESULT_FROM_WIN32(WSAGetLastError());

	LONG lErrCode = RegOpenKeyEx(HKEY_CURRENT_USER,
		_T("Software\\00_Test"), 0, KEY_NOTIFY, &m_regMonKey);
	if (lErrCode != ERROR_SUCCESS)
		throw HRESULT_FROM_WIN32(lErrCode);
	m_evMonReg = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_evMonReg == NULL)
		throw HRESULT_FROM_WIN32(GetLastError());
	

	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_hIocp == NULL)
		throw HRESULT_FROM_WIN32(GetLastError());

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	m_nWorkerCnt = si.dwNumberOfProcessors * 2;
	m_parWorkers = new HANDLE[m_nWorkerCnt];
	for (int i = 0; i < m_nWorkerCnt; i++)
	{
		DWORD dwThrId;
		m_parWorkers[i] = CreateThread(NULL, 0, IocpWorkProc, this, 0, &dwThrId);
		if (m_parWorkers[i] == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
	}
}

void MainMngr::Uninit()
{
	if (m_soListen != INVALID_SOCKET)
	{
		closesocket(m_soListen); m_soListen = INVALID_SOCKET;
	}
	if (m_evListen != NULL)
	{
		CloseHandle(m_evListen); m_evListen = NULL;
	}
	if (m_regMonKey != NULL)
	{
		RegCloseKey(m_regMonKey); m_regMonKey = NULL;
	}
	if (m_evMonReg != NULL)
	{
		CloseHandle(m_evMonReg); m_evMonReg = NULL;
	}
	if (m_hIocp != NULL)
	{
		CloseHandle(m_hIocp); m_hIocp = NULL;
	}

	if (m_parWorkers != NULL)
	{
		for (int i = 0; i < m_nWorkerCnt; i++)
			CloseHandle(m_parWorkers[i]);
		delete[] m_parWorkers;
		m_parWorkers = NULL;
		m_nWorkerCnt = 0;
	}
	if (m_hMainThr != NULL)
	{
		CloseHandle(m_hMainThr); m_hMainThr = NULL;
		m_dwThrId = 0;
	}
	if (m_hevExit != NULL)
	{
		CloseHandle(m_hevExit); m_hevExit = NULL;
	}
}

void MainMngr::Run()
{
	HANDLE arSynObjs[MAIN_WAIT_CNT] = { m_hevExit, m_evListen, m_evMonReg };

	Handler_Registry(false);

	bool bContinue = true;
	while (bContinue)
	{
		DWORD dwWaitRet = MsgWaitForMultipleObjectsEx
		(
			MAIN_WAIT_CNT, arSynObjs, INFINITE, QS_POSTMESSAGE, MWMO_INPUTAVAILABLE
		);
		if (dwWaitRet == WAIT_FAILED)
		{
			cout << "MWFMO failed: " << GetLastError() << endl;
			break;
		}

		if (dwWaitRet == WAIT_OBJECT_0)
			break;

		switch (dwWaitRet)
		{
			case MAIN_WAIT_CNT    : bContinue = Handler_Measage(); break;
			case WAIT_OBJECT_0 + 1: bContinue = Handler_Accept(); break;
			case WAIT_OBJECT_0 + 2: bContinue = Handler_Registry(); break;
		}
	}
	CloseHandle(m_hIocp);
	WaitForMultipleObjects(m_nWorkerCnt, m_parWorkers, TRUE, INFINITE);
	m_hIocp = NULL;
}


void MainMngr::Start()
{
	Init();

	m_hMainThr = CreateThread(NULL, 0, MainMngrProc, this, 0, &m_dwThrId);
	if (m_hMainThr == NULL)
		throw HRESULT_FROM_WIN32(GetLastError());

	if (!PostThreadMessage(m_dwThrId, TM_MSG_TEST, 0, 0))
	{
		DWORD dwErrCode = GetLastError();
		if (dwErrCode != ERROR_INVALID_THREAD_ID)
			throw HRESULT_FROM_WIN32(dwErrCode);
		Sleep(10);
	}
}

void MainMngr::Stop()
{
	SetEvent(m_hevExit);
	WaitForSingleObject(m_hMainThr, INFINITE);

	for (DIR_MAP::iterator it = m_dirMap.begin(); it != m_dirMap.end(); it++)
	{
		PDIR_ITEM pdi = (PDIR_ITEM)it->second;
		m_ovSet.erase(pdi);
		delete pdi;
	}

	for (OV_SET::iterator it = m_ovSet.begin(); it != m_ovSet.end(); it++)
	{
		PSOCK_ITEM psi = (PSOCK_ITEM)*it;
		delete psi;
	}
	Uninit();
}


bool MainMngr::Handler_Accept()
{
	WSANETWORKEVENTS ne;
	WSAEnumNetworkEvents(m_soListen, m_evListen, &ne);
	if ((ne.lNetworkEvents & FD_ACCEPT) != FD_ACCEPT)
		return true;

	INT nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
	if (nErrCode != ERROR_SUCCESS)
	{
		cout << "Accept Noti error : " << nErrCode << endl;
		return false;
	}

	SOCKET sock = accept(m_soListen, NULL, NULL);
	if (sock == INVALID_SOCKET)
	{
		cout << "accept failed, code : " << WSAGetLastError() << endl;
		return false;
	}

	PSOCK_ITEM psi = new SOCK_ITEM(sock);
	m_ovSet.insert(psi);
	PostQueuedCompletionStatus(m_hIocp, 0, IOKEY_NEW_SOCK, psi);
	return true;
}

bool MainMngr::Handler_Measage()
{
	MSG msg;
	PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);

	if (msg.message == TM_MSG_TEST)
		return true;

	if (msg.message == TM_DIR_CONFIG)
	{
		LONG  lOpt = (LONG)msg.wParam;
		PCSTR pMonPath = (PCSTR)msg.lParam;

		DIR_MAP::iterator it = m_dirMap.find(pMonPath);
		if (LOWORD(lOpt))
		{
			if (it != m_dirMap.end())
				CloseHandle(it->second->_dir);
		}
		else
		{
			DIR_MAP::iterator it = m_dirMap.find(pMonPath);
			if (it != m_dirMap.end())
				return true;
			
			HANDLE hMonDir = CreateFileA
			(
				pMonPath, GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL
			);
			if (hMonDir == INVALID_HANDLE_VALUE)
			{
				cout << pMonPath << " open failed : " << GetLastError() << endl;
				return true;
			};

			PDIR_ITEM pdi = new DIR_ITEM(hMonDir, pMonPath);
			m_dirMap.insert(std::make_pair(pMonPath, pdi));
			m_ovSet.insert(pdi);
			PostQueuedCompletionStatus(m_hIocp, 0, IOKEY_MON_DIR, pdi);
		}
		if (HIWORD(lOpt))
			delete[] pMonPath;
	}
	else
	{
		if (msg.message != TM_SOCK_CLOSED && msg.message != TM_DIR_CLOSED)
			return true;

		LONG lErrCode	 = (LONG)msg.wParam;
		LPOVERLAPPED pov = (LPOVERLAPPED)msg.lParam;

		m_ovSet.erase(pov);
		if (msg.message == TM_SOCK_CLOSED)
		{
			PSOCK_ITEM psi = (PSOCK_ITEM)pov;
			if (lErrCode != STATUS_LOCAL_DISCONNECT)
			{
				if (lErrCode == ERROR_SUCCESS || lErrCode == STATUS_REMOTE_DISCONNECT)
					cout << " ==> Client " << psi->_sock << " disconnected..." << endl;
				else
					cout << " ==> Error occurred, code = " << lErrCode << endl;
			}
			else
				psi->_sock = INVALID_SOCKET;
			delete psi;
		}
		else
		{
			PDIR_ITEM pdi = (PDIR_ITEM)pov;
			if (lErrCode != ERROR_NOTIFY_CLEANUP)
				cout << " ==> Error occurred, code = " << lErrCode << endl;
			else
			{
				cout << " ==> DirMon " << pdi->_path.c_str() << " stopped..." << endl;
				pdi->_dir = INVALID_HANDLE_VALUE;
			}
			DIR_MAP::iterator it = m_dirMap.find(pdi->_path);
			if (it != m_dirMap.end())
				m_dirMap.erase(it);
			delete pdi;
		}
	}
	return true;
}

bool MainMngr::Handler_Registry(bool bNoti)
{
	if (bNoti)
		cout << " => Registry Entry Change has occurred..." << endl;

	DWORD  dwFilter = REG_NOTIFY_CHANGE_NAME |
		REG_NOTIFY_CHANGE_ATTRIBUTES |
		REG_NOTIFY_CHANGE_LAST_SET |
		REG_NOTIFY_CHANGE_SECURITY;
	LONG lErrCode = RegNotifyChangeKeyValue
	(
		m_regMonKey, TRUE, dwFilter, m_evMonReg, TRUE
	);
	if (lErrCode != ERROR_SUCCESS)
		throw HRESULT_FROM_WIN32(lErrCode);

	return true;
}





void _tmain()
{
	WSADATA	wsd;
	int nErrCode = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (nErrCode)
	{
		cout << "WSAStartup failed with error : " << nErrCode << endl;
		return;
	}

	MainMngr mngr;
	try
	{
		mngr.Start();
		mngr.ConfigMonDir(false, "c:\\temp");

		char szIn[512];
		while (true)
		{
			cin >> szIn;
			if (stricmp(szIn, "quit") == 0)
				break;

			if (strnicmp(szIn, "dir:", 4) == 0)
			{
				bool bRemove = false;
				PSTR pit = szIn + 4;
				if (strnicmp(pit, "rem:", 4) == 0)
					bRemove = true;
				else if (strnicmp(pit, "add:", 4) != 0)
					continue;
				pit += 4;
				int len = strlen(pit);
				if (len == 0)
					continue;

				PSTR path = new char[len + 1];
				strcpy(path, pit);
				mngr.ConfigMonDir(bRemove, path, true);
			}
		}
	}
	catch (HRESULT hr)
	{
		printf("......Error in Main, code=0x%08X\n", hr);
	}
	mngr.Stop();

	cout << "==== Server terminates... ==========================" << endl;
	WSACleanup();
}
