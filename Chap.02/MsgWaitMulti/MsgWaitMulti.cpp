#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define TM_CMD_EXIT		WM_USER + 1
#define TM_CMD_DATA		WM_USER + 2

#define WAIT_OBJ_CNT	4

DWORD WINAPI ThreadProc(LPVOID pParam)
{
	PHANDLE parSyncs = (PHANDLE)pParam;

	while (true)
	{
		DWORD dwWaitCode = MsgWaitForMultipleObjectsEx
		(
			WAIT_OBJ_CNT, parSyncs, INFINITE, 
			QS_POSTMESSAGE, 
			MWMO_INPUTAVAILABLE
		);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << "MsgWaitForMultipleObjects failed : " << GetLastError() << endl;
			return 0;
		}

		dwWaitCode -= WAIT_OBJECT_0;
		if (dwWaitCode == WAIT_OBJ_CNT)
		{
			MSG msg;
			//while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
			if (msg.message == TM_CMD_EXIT)
				break;

			if (msg.message == TM_CMD_DATA)
			{
				char* pszData = (char*)msg.lParam;
				cout << " ~~~~~ queued data : " << pszData << endl;
				delete[] pszData;
			}
			continue;
		}

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

void _tmain(void)
{
	cout << "======= Start MsgWaitMulti Test ========" << endl;

	HANDLE arhSyncs[WAIT_OBJ_CNT];
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
			PostThreadMessage(dwThreadId, TM_CMD_DATA, 0, (LPARAM)pszData);
		}
	}

	PostThreadMessage(dwThreadId, TM_CMD_EXIT, 0, 0);
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	for (int i = 0; i < 4; i++)
		CloseHandle(arhSyncs[i]);

	cout << "======= End MsgWaitMulti Test ==========" << endl;
}
