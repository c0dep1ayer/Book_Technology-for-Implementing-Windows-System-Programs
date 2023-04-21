#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


class DBMngr
{
	static INIT_ONCE s_io;

public:
	static DBMngr* GetInstance()
	{
		PVOID pCtx = NULL;
		BOOL  fPending = FALSE;
		BOOL bIsOK = InitOnceBeginInitialize(&s_io, INIT_ONCE_ASYNC, &fPending, &pCtx);
		if (!bIsOK)
			throw HRESULT_FROM_WIN32(GetLastError());

		DWORD dwThrId = GetCurrentThreadId();
		if (!fPending)
		{
			printf(" ******** Initializing completed: %d\n", dwThrId);
			return (DBMngr*)pCtx;
		}

		DBMngr* inst = new DBMngr();
		bIsOK = InitOnceComplete(&s_io, INIT_ONCE_ASYNC, (PVOID)inst);
		if (bIsOK)
		{
			printf(" !!!!!!!! One-Time Initialization SUCCEEDED : %d\n", dwThrId);
			return inst;
		}

		delete inst;
		printf(" ******** Already initialized : %d\n", dwThrId);

		bIsOK = InitOnceBeginInitialize(&s_io, INIT_ONCE_CHECK_ONLY, &fPending, &pCtx);
		if (bIsOK && !fPending)
			return (DBMngr*)pCtx;
		throw HRESULT_FROM_WIN32(GetLastError());
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
INIT_ONCE  DBMngr::s_io = INIT_ONCE_STATIC_INIT;


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
	cout << "======= Start InitOnceAsync Test ========" << endl;

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

	cout << "======= End InitOnceAsync Test ==========" << endl;
}
