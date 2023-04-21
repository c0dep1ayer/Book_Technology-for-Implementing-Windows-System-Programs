#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;


#define WQI_CMD_EXIT	0
#define WQI_CMD_STR		1
#define WQI_CMD_INT		2

struct WQUE_ITEM
{
	INT		_cmd;
	PVOID	_data;
};
typedef std::list<WQUE_ITEM> ITEM_QUE;

struct WAIT_QUE
{
	HANDLE	 _hMutx;
	HANDLE	 _hSema;
	ITEM_QUE _queue;
};
typedef WAIT_QUE* PWAIT_QUE;

void WQ_Enqueue(PWAIT_QUE pWQ, INT cmd, PVOID data)
{
	WQUE_ITEM wqi;
	wqi._cmd = cmd, wqi._data = data;

	WaitForSingleObject(pWQ->_hMutx, INFINITE);
	pWQ->_queue.push_back(wqi);
	ReleaseMutex(pWQ->_hMutx);

	ReleaseSemaphore(pWQ->_hSema, 1, NULL);
}

INT WQ_Dequeue(PWAIT_QUE pWQ, PVOID& data)
{
	WaitForSingleObject(pWQ->_hSema, INFINITE);

	WQUE_ITEM wqi;
	WaitForSingleObject(pWQ->_hMutx, INFINITE);
	ITEM_QUE::iterator it = pWQ->_queue.begin();
	wqi = *it;
	pWQ->_queue.pop_front();
	ReleaseMutex(pWQ->_hMutx);

	data = wqi._data;
	return wqi._cmd;
}


DWORD WINAPI QueueWorkerProc(LPVOID pParam)
{
	PWAIT_QUE pWQ = (PWAIT_QUE)pParam;

	while (true)
	{
		PVOID data = NULL;
		INT cmd = WQ_Dequeue(pWQ, data);
		if (cmd == WQI_CMD_EXIT)
			break;

		if (cmd == WQI_CMD_INT)
			printf(" => CMD_INT queued : %d\n", (int)data);
		else
		{
			printf(" => CMD_STR queued: %s\n", (char*)data);
			delete[]((char*)data);
		}
	}
	printf("......QueueWorkerProc Thread Exit!!!\n");

	return 0;
}

void _tmain()
{
	cout << "======= Start WQSemaphore Test ========" << endl;

	WAIT_QUE wq;
	wq._hSema = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
	wq._hMutx = CreateMutex(NULL, FALSE, NULL);

	DWORD dwThreadId;
	HANDLE hThread = CreateThread(NULL, 0, QueueWorkerProc, &wq, 0, &dwThreadId);

	char szIn[1024];
	while (true)
	{
		cin >> szIn;

		INT   cmd = 0;
		PVOID data = NULL;
		if (_stricmp(szIn, "quit") == 0)
			cmd  = WQI_CMD_EXIT;
		else
		{
			int nVal = atoi(szIn);
			if (nVal == 0)
			{
				cmd = WQI_CMD_STR;
				data = new char[strlen(szIn) + 1];
				strcpy((char*)data, szIn);
			}
			else
			{
				cmd = WQI_CMD_INT;
				data = (PVOID)nVal;
			}
		}

		WQ_Enqueue(&wq, cmd, data);
		if (cmd == WQI_CMD_EXIT)
			break;
	}

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	CloseHandle(wq._hSema);
	CloseHandle(wq._hMutx);

	cout << "======= End WQSemaphore Test ==========" << endl;
}
