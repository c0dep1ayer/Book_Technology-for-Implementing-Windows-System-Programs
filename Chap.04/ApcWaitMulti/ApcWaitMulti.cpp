#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define WAIT_OBJ_CNT	4

DWORD WINAPI ThreadProc(LPVOID pParam)
{
	PHANDLE parSyncs = (PHANDLE)pParam;
	PBOOL   pbIsExit = (PBOOL)parSyncs[WAIT_OBJ_CNT];

	while (true)
	{
		DWORD dwWaitCode = WaitForMultipleObjectsEx
		(
			WAIT_OBJ_CNT, parSyncs, FALSE, INFINITE, TRUE
		);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << "WaitForMultipleObjectsEx failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitCode == WAIT_IO_COMPLETION)
		{
			if (*pbIsExit)
				break;
			continue;
		}

		dwWaitCode -= WAIT_OBJECT_0;
		switch (dwWaitCode)
		{
			case 0: // 대기가능 타이머 시그널
				cout << " &&&&& WaitableTimer Singnaled!!!" << endl;
			break;
			case 1 : // 이벤트 시그널
				cout << " +++++ Event Singnaled!!!" << endl;
			break;
			case 2:	// 뮤텍스 시그널
				cout << " ***** Mutex Singnaled!!!" << endl;
				ReleaseMutex(parSyncs[dwWaitCode]);
			break;
			case 3:	// 세마포어 시그널
				cout << " ##### Semaphore Singnaled!!!" << endl;
			break;
		}
	}
	return 0;
}

VOID WINAPI StringApcProc(ULONG_PTR upParam)
{
	char* pszData = (char*)upParam;
	cout << " ~~~~~ queued data : " << pszData << endl;
	delete[] pszData;
}

VOID WINAPI ExitApcProc(ULONG_PTR upParam)
{
	PBOOL pbIsExit = (PBOOL)upParam;
	*pbIsExit = TRUE;
}

void _tmain(void)
{
	cout << "======= Start ApcWaitMulti Test ========" << endl;

	BOOL bIsExit = FALSE;
	HANDLE arhSyncs[WAIT_OBJ_CNT + 1];
	arhSyncs[WAIT_OBJ_CNT] = (HANDLE)&bIsExit;

	arhSyncs[0] = CreateWaitableTimer(NULL, FALSE, NULL);
	arhSyncs[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
	arhSyncs[2] = CreateMutex(NULL, TRUE, NULL);
	arhSyncs[3] = CreateSemaphore(NULL, 0, 1, NULL);

	DWORD dwThreadId = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, arhSyncs, 0, &dwThreadId);

	char szIn[32];
	while (true)
	{
		cin >> szIn;
		if (_stricmp(szIn, "quit") == 0)
			break;

		if (_stricmp(szIn, "event") == 0)
			SetEvent(arhSyncs[1]);
		else if (_stricmp(szIn, "mutex") == 0)
		{
			ReleaseMutex(arhSyncs[2]);
			WaitForSingleObject(arhSyncs[2], INFINITE);
		}
		else if (_stricmp(szIn, "semaphore") == 0)
			ReleaseSemaphore(arhSyncs[3], 1, NULL);
		else if (_stricmp(szIn, "timer") == 0)
		{
			LARGE_INTEGER li;
			li.QuadPart = 0;
			SetWaitableTimer(arhSyncs[0], &li, 0, NULL, NULL, FALSE);
		}
		else
		{
			char* pszData = new char[strlen(szIn) + 1];
			strcpy(pszData, szIn);
			QueueUserAPC(StringApcProc, hThread, (ULONG_PTR)pszData);
		}
	}

	QueueUserAPC(ExitApcProc, hThread, (ULONG_PTR)&bIsExit);
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	for (int i = 0; i < WAIT_OBJ_CNT; i++)
		CloseHandle(arhSyncs[i]);

	cout << "======= End ApcWaitMulti Test ==========" << endl;
}
