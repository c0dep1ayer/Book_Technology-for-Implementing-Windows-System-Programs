#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


/*
class DBMngr
{
	static DBMngr* s_inst;
public:
	static DBMngr* GetInstance()
	{
		if (s_inst == NULL)
		{
			s_inst = new DBMngr();
		}
		return _inst;
	}

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
*/

class DBMngr
{
	static INIT_ONCE s_io;
	static BOOL CALLBACK InitOnceProc(PINIT_ONCE pio, PVOID pPrm, PVOID* pCtx);

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
INIT_ONCE  DBMngr::s_io = INIT_ONCE_STATIC_INIT;


BOOL CALLBACK DBMngr::InitOnceProc(PINIT_ONCE pio, PVOID pPrm, PVOID* pCtx)
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
	BOOL bIsOK = InitOnceExecuteOnce(&s_io, InitOnceProc, NULL, &pCtx);
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

	Sleep(1000);
	cout << "Press any key to exit : " << endl;
	getchar();

	WaitForMultipleObjects(10, arhThrs, TRUE, INFINITE);

	cout << "======= End InitOnceSync Test ==========" << endl;
}

