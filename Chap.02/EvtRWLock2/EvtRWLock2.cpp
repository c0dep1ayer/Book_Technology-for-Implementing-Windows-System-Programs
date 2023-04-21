#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

struct ERWLOCK
{
	HANDLE	_hevRO;
	LONG	_nRoRefs;

	HANDLE	_hevWO;
	HANDLE	_hmuWO;

	ERWLOCK()
	{
		_hevRO = _hevWO = _hmuWO = NULL;
		_nRoRefs = 0;
	}
	~ERWLOCK()
	{
		if (_hevRO != NULL) CloseHandle(_hevRO);
		if (_hevWO != NULL) CloseHandle(_hevWO);
		if (_hmuWO != NULL) CloseHandle(_hmuWO);
	}
};
typedef ERWLOCK* PERWLOCK;

#define DELAY_WRITE	5000
#define DELAY_READ	200
LONG	g_nValue;
LONG	g_bIsExit;

DWORD WINAPI ReaderProc(PVOID pParam)
{
	PERWLOCK prwl = (PERWLOCK)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (g_bIsExit == 0)
	{
		WaitForSingleObject(prwl->_hevWO, INFINITE);
		ResetEvent(prwl->_hevRO);
		InterlockedIncrement(&prwl->_nRoRefs);

		cout << "    => TH " << dwThrId << " Reads : " << g_nValue << endl;
			
		if (InterlockedDecrement(&prwl->_nRoRefs) == 0)
			SetEvent(prwl->_hevRO);

		Sleep(rand() % DELAY_READ);
	}

	return 0;
}

DWORD WINAPI WriterProc(PVOID pParam)
{
	PERWLOCK prwl = (PERWLOCK)pParam;
	DWORD dwThrId = GetCurrentThreadId();

	while (g_bIsExit == 0)
	{
		Sleep(rand() % DELAY_WRITE);

		ResetEvent(prwl->_hevWO);
		WaitForSingleObject(prwl->_hevRO, INFINITE);
		WaitForSingleObject(prwl->_hmuWO, INFINITE);

		g_nValue++;
		cout << " <= TH " << dwThrId << " Writes value : " << g_nValue << endl;

		ReleaseMutex(prwl->_hmuWO);
		SetEvent(prwl->_hevWO);
	}

	return 0;
}

#define MAX_READ_CNT	5
#define MAX_WRITE_CNT	2
void _tmain()
{
	ERWLOCK rwl;
	rwl._hevRO = CreateEvent(NULL, TRUE, TRUE, NULL);
	rwl._hevWO = CreateEvent(NULL, TRUE, TRUE, NULL);
	rwl._hmuWO = CreateMutex(NULL, FALSE, NULL);

	HANDLE woThrs[MAX_WRITE_CNT];
	for (int i = 0; i < MAX_WRITE_CNT; i++)
	{
		DWORD dwThrId = 0;
		woThrs[i] = CreateThread(NULL, 0, WriterProc, &rwl, 0, &dwThrId);
	}
	HANDLE roThrs[MAX_READ_CNT];
	for (int i = 0; i < MAX_READ_CNT; i++)
	{
		DWORD dwThrId = 0;
		roThrs[i] = CreateThread(NULL, 0, ReaderProc, &rwl, 0, &dwThrId);
	}

	getchar();
	g_bIsExit = TRUE;

	WaitForMultipleObjects(MAX_READ_CNT , roThrs, TRUE, INFINITE);
	WaitForMultipleObjects(MAX_WRITE_CNT, woThrs, TRUE, INFINITE);
}

