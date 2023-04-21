#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


struct MY_INIT_ONCE
{
	CRITICAL_SECTION _cs;
	PVOID			 _ctx;

	MY_INIT_ONCE()
	{
		InitializeCriticalSection(&_cs);
		_ctx = NULL;
	}
	~MY_INIT_ONCE()
	{
		DeleteCriticalSection(&_cs);
	}
};
typedef MY_INIT_ONCE* PMY_INIT_ONCE;

typedef BOOL (*PMY_INIT_ONCE_FN) (PMY_INIT_ONCE pMio, PVOID pPrm, PVOID* ppCtx);


BOOL MyInitOnceExecuteOnce(PMY_INIT_ONCE pIo, PMY_INIT_ONCE_FN pfnInit, PVOID pPrm, PVOID* ppCtx)
{
	BOOL bIsOK = TRUE;

	EnterCriticalSection(&pIo->_cs);
	if (pIo->_ctx == NULL)
	{
		bIsOK = pfnInit(pIo, pPrm, ppCtx);
		if (bIsOK)
			pIo->_ctx = *ppCtx;
	}
	else
		*ppCtx = pIo->_ctx;
	LeaveCriticalSection(&pIo->_cs);

	return bIsOK;
}

class DBMngr
{
	static MY_INIT_ONCE s_io;
	static BOOL InitOnceProc(PMY_INIT_ONCE pio, PVOID pPrm, PVOID* pCtx);

public:
	static DBMngr* GetInstance();

private:
	HANDLE m_hMutex;

public:
	DBMngr()
	{
		m_hMutex = CreateMutex(NULL, FALSE, NULL);
		if (m_hMutex == NULL)
			throw HRESULT_FROM_WIN32(GetLastError());
	}
	~DBMngr()
	{
		if (m_hMutex != NULL)
			CloseHandle(m_hMutex);
	}

	void Query()
	{
		WaitForSingleObject(m_hMutex, INFINITE);
		cout << " => Queried from Thread " << GetCurrentThreadId() << endl;
		ReleaseMutex(m_hMutex);
	}
};

MY_INIT_ONCE DBMngr::s_io;

BOOL DBMngr::InitOnceProc(PMY_INIT_ONCE pio, PVOID pPrm, PVOID* pCtx)
{
	cout << " ====> CALLBACK InitOnceProc called in " << GetCurrentThreadId() << endl;
	DBMngr* inst = NULL;
	try
	{
		inst = new DBMngr();
		*pCtx = inst;
		return TRUE;
	}
	catch (HRESULT hr)
	{
		*pCtx = (PVOID)hr;
	}
	return FALSE;
}

DBMngr* DBMngr::GetInstance()
{
	PVOID pCtx = NULL;
	BOOL bIsOK = MyInitOnceExecuteOnce(&s_io, InitOnceProc, NULL, &pCtx);
	if (!bIsOK)
		throw (HRESULT)pCtx;
	return (DBMngr*)pCtx;
}


DWORD WINAPI ThreadProc(LPVOID pParam)
{
	try
	{
		DBMngr::GetInstance()->Query();
	}
	catch (HRESULT hr)
	{
		cout << "~~~~DBMngr->Query failed, error code : " << hr << endl;
	}
	return 0;
}

void _tmain(void)
{
	cout << "======= Start InitOnceSync Test ========" << endl;

	HANDLE arhThrs[10];
	for (int i = 0; i < 10; i++)
	{
		DWORD dwTheaID = 0;
		arhThrs[i] = CreateThread(NULL, 0, ThreadProc, NULL, 0, &dwTheaID);
	}

	int nExit;
	cin >> nExit;

	WaitForMultipleObjects(10, arhThrs, TRUE, INFINITE);

	cout << "======= End InitOnceSync Test ==========" << endl;
}

