#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;


#define CMD_TERM	0
#define CMD_CTRL1	1
#define CMD_CTRL2	2
#define CMD_DATA	3

struct WAIT_QUE
{
	struct NOTI_ITEM
	{
		INT		_cmd;
		PVOID	_data;
	};
	typedef std::list<NOTI_ITEM> ITEM_QUE;

	HANDLE	 m_hMutx;
	HANDLE	 m_hSema;
	ITEM_QUE m_queue;

	WAIT_QUE()
	{
		m_hMutx = m_hSema = NULL;
	}
	~WAIT_QUE()
	{
		if (m_hMutx != NULL) CloseHandle(m_hMutx);
		if (m_hSema != NULL) CloseHandle(m_hSema);
	}

	void Init()
	{
		m_hSema = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
		m_hMutx = CreateMutex(NULL, FALSE, NULL);
	}

	void Enque(INT cmd, PVOID data = NULL)
	{
		NOTI_ITEM ni;
		ni._cmd = cmd;
		ni._data = data;

		WaitForSingleObject(m_hMutx, INFINITE);
		m_queue.push_back(ni);
		ReleaseMutex(m_hMutx);

		ReleaseSemaphore(m_hSema, 1, NULL);
	}

	PVOID Deque(INT& cmd)
	{
		NOTI_ITEM ni;
		WaitForSingleObject(m_hMutx, INFINITE);
		ITEM_QUE::iterator it = m_queue.begin();
		ni = *it;
		m_queue.pop_front();
		ReleaseMutex(m_hMutx);

		cmd = ni._cmd;
		return ni._data;
	}
};
typedef WAIT_QUE* PWAIT_QUE;
WAIT_QUE g_wqP2C, g_wqC2P;

struct WAIT_ENV
{
	HANDLE	_hevSend;
	HANDLE	_hevResp;
	char	_szIn[64];
};

DWORD WINAPI ProviderProc(LPVOID pParam)
{
	WAIT_ENV* pwe = (WAIT_ENV*)pParam;

	HANDLE arhWaits[2] = { pwe->_hevSend, g_wqC2P.m_hSema };
	bool bRunnig = true;
	while (bRunnig)
	{
		DWORD dwWaitCode = WaitForMultipleObjects(2, arhWaits, FALSE, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitCode == WAIT_OBJECT_0)
		{
			INT   cmd = -1;
			char* data = NULL;

			char code = pwe->_szIn[0];
			if (code == '0')
			{
				cmd = CMD_TERM;
				bRunnig = false;
			}
			else if (code == '1')
				cmd = CMD_CTRL1;
			else if (code == '2')
				cmd = CMD_CTRL2;
			else
			{
				cmd = CMD_DATA;
				int len = strlen(pwe->_szIn);
				data = new char[len + 1];
				strcpy(data, pwe->_szIn);
			}
			g_wqP2C.Enque(cmd, data);
			SetEvent(pwe->_hevResp);
		}
		else
		{
			int cmd = -1;
			char* data = (char*)g_wqC2P.Deque(cmd);
			cout << "C -> P : " << data << endl;
			delete[] data;
		}
	}
	cout << " *** Provider Thread exits..." << endl;

	return 0;
}


DWORD WINAPI ConsumerProc(LPVOID pParam)
{
	DWORD dwDelay = 1000;
	bool bRunnig = true;
	while (bRunnig)
	{
		DWORD dwWaitCode = WaitForSingleObject(g_wqP2C.m_hSema, dwDelay);
		if (dwWaitCode == WAIT_FAILED)
		{
			cout << " ~~~ WaitForSingleObject failed : " << GetLastError() << endl;
			break;
		}

		if (dwWaitCode == WAIT_TIMEOUT)
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			char szTime[128];
			int len = sprintf(szTime, "%04d-%02d-%02d %02d:%02d:%02d+%03d",
				st.wYear, st.wMonth, st.wDay, st.wHour,
				st.wMinute, st.wSecond, st.wMilliseconds);
			char* data = new char[len + 1];
			strcpy(data, szTime);
			g_wqC2P.Enque(CMD_DATA, data);
		}
		else
		{
			int cmd = -1;
			char* data = (char*)g_wqP2C.Deque(cmd);
			switch (cmd)
			{
				case CMD_TERM:
					bRunnig = false;
					break;
				case CMD_CTRL1:
					dwDelay = 1000;
					break;
				case CMD_CTRL2:
					dwDelay = 2000;
					break;
				case CMD_DATA:
					cout << "P -> C : " << data << endl;
					break;
			}
		}
	}
	cout << " *** Consumer Thread exits..." << endl;

	return 0;
}

void _tmain()
{
	cout << "======= Start EventNotify Test ========" << endl;

	g_wqC2P.Init();
	g_wqP2C.Init();

	WAIT_ENV we;
	we._hevSend = CreateEvent(NULL, FALSE, FALSE, NULL);
	we._hevResp = CreateEvent(NULL, FALSE, FALSE, NULL);

	DWORD  dwThreadID;
	HANDLE hthProv = CreateThread(NULL, 0, ProviderProc, &we, 0, &dwThreadID);
	HANDLE hthCons = CreateThread(NULL, 0, ConsumerProc, NULL, 0, &dwThreadID);

	while (true)
	{
		cin >> we._szIn;
		SetEvent(we._hevSend);
		WaitForSingleObject(we._hevResp, INFINITE);
		if (we._szIn[0] == '0')
			break;
	}
	
	HANDLE hThrs[2] = { hthProv, hthCons };
	WaitForMultipleObjects(2, hThrs, TRUE, INFINITE);
	CloseHandle(hthProv);
	CloseHandle(hthCons);
	CloseHandle(we._hevResp);
	CloseHandle(we._hevSend);

	cout << "======= End EventNotify Test ==========" << endl;
}
