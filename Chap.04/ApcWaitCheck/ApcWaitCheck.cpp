#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;


#define TM_NONE		  0
#define TM_STR		100
#define TM_POINT	101
#define TM_TIME		102
#define TM_EXIT		200

struct APC_ITEM
{
	INT		_cmd;
	long	_size;
	PBYTE	_data;
};
typedef APC_ITEM* PAPC_ITEM;

DWORD g_dwTlsIdx = 0;

void WINAPI APCCallback(ULONG_PTR dwData)
{
	PSTR pstr = (PSTR)dwData;
	DWORD dwThrId = GetCurrentThreadId();
	printf("  ==> Thread %d running APC: %s\n", dwThrId, pstr);
	delete[] pstr;
}

void WINAPI APCCallback2(ULONG_PTR dwData)
{
	DWORD dwDelsy = (DWORD)dwData;
	DWORD dwThrId = GetCurrentThreadId();
	printf("  <== Thread %d sleeps %d ms\n", dwThrId, dwDelsy);
	Sleep(dwDelsy);
}

DWORD WINAPI WorkerProc(PVOID pParam)
{
	HANDLE hevExit = (HANDLE)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (true)
	{
		DWORD dwWaitCode = WaitForSingleObjectEx(hevExit, INFINITE, TRUE);
		if (dwWaitCode == WAIT_OBJECT_0)
			break;

		if (dwWaitCode == WAIT_IO_COMPLETION)
		{
			printf("****** Thread %d wakes from WAITING...\n", dwThrId);
			continue;
		}
	}
	cout << " *** WorkerProc Thread exits..." << endl;

	return 0;
}

void _tmain()
{
	cout << "======= Start ApcWaitCheck Test ========" << endl;
	HANDLE hevExit = CreateEvent(NULL, FALSE, FALSE, NULL);

	DWORD  dwThrID;
	HANDLE hThread = CreateThread(NULL, 0, WorkerProc, hevExit, 0, &dwThrID);

	int nIndex = 0;
	char szIn[512];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		int val = atoi(szIn);
		if (val > 100)
			QueueUserAPC(APCCallback2, hThread, (ULONG_PTR)val);
		else
		{
			int len = strlen(szIn);
			PSTR pstr = new char[len + 1];
			strcpy(pstr, szIn);
			QueueUserAPC(APCCallback, hThread, (ULONG_PTR)pstr);
		}
	}

	SetEvent(hevExit);
	WaitForSingleObject(hThread, INFINITE);

	CloseHandle(hThread);
	CloseHandle(hevExit);
	cout << "======= End ApcWaitCheck Test ==========" << endl;
}
