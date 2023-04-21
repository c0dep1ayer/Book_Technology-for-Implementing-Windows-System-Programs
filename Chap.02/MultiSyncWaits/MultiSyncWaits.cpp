#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


DWORD WINAPI ThreadProc(LPVOID pParam)
{
	PHANDLE parSyncs = (PHANDLE)pParam;

	while (true)
	{
		DWORD dwWaitCode = WaitForMultipleObjects(4, parSyncs, FALSE, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << "WaitForSingleObject failed : " << GetLastError() << endl;
			return 0;
		}
		if (dwWaitCode == WAIT_OBJECT_0) // 종료 이벤트 시그널
			break;

		dwWaitCode -= WAIT_OBJECT_0;
		switch (dwWaitCode)
		{
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

void _tmain(void)
{
	cout << "======= Start MultiSyncWaits Test ========" << endl;

	HANDLE arhSyncs[4];
	arhSyncs[0] = CreateEvent(NULL, TRUE, FALSE, NULL);
	arhSyncs[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
	arhSyncs[2] = CreateMutex(NULL, TRUE, NULL);
	arhSyncs[3] = CreateSemaphore(NULL, 0, 1, NULL);

	DWORD dwTheaID = 0;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, arhSyncs, 0, &dwTheaID);

	char szIn[32];
	while (true)
	{
		cin >> szIn;
		if (stricmp(szIn, "quit") == 0)
			break;

		if (stricmp(szIn, "event") == 0)
			SetEvent(arhSyncs[1]);
		else if (stricmp(szIn, "mutex") == 0)
		{
			ReleaseMutex(arhSyncs[2]);
			WaitForSingleObject(arhSyncs[2], INFINITE);
		}
		else if (stricmp(szIn, "semaphore") == 0)
			ReleaseSemaphore(arhSyncs[3], 1, NULL);
	}
	SetEvent(arhSyncs[0]);
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	for (int i = 0; i < 4; i++)
		CloseHandle(arhSyncs[i]);

	cout << "======= End MultiSyncWaits Test ==========" << endl;
}

