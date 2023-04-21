#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


class MyClass
{
	static LONG ExpFilter(LPEXCEPTION_POINTERS pEx);
	static DWORD WINAPI MyThreadProc(PVOID pParam);
	void InnerThreadProc();

	HANDLE	m_hThread;
	DWORD	m_dwThreadId;
	bool	m_bExit;
	DWORD	m_dwDelay;

public:
	MyClass() : m_hThread(NULL), m_bExit(false), m_dwDelay(1000)
	{
	}
	~MyClass()
	{
		if (m_hThread != NULL)
			CloseHandle(m_hThread);
	}

public:
	HRESULT Start()
	{
		m_hThread = CreateThread(NULL, 0, MyThreadProc, this, 0, &m_dwThreadId);
		if (m_hThread == NULL)
			return HRESULT_FROM_WIN32(GetLastError());
		return S_OK;
	}

	void Stop()
	{
		m_bExit = true;
		WaitForSingleObject(m_hThread, INFINITE);
	}
};


LONG MyClass::ExpFilter(LPEXCEPTION_POINTERS pEx)
{
	PEXCEPTION_RECORD pER = pEx->ExceptionRecord;
	printf("~~~~ Exception : Code = %x, Address = %p",
		pER->ExceptionCode, pER->ExceptionAddress);

	return EXCEPTION_EXECUTE_HANDLER;
}

DWORD WINAPI MyClass::MyThreadProc(PVOID pParam)
{
	MyClass* pThis = (MyClass*)pParam;
	__try
	{
		pThis->InnerThreadProc();
	}
	__except (ExpFilter(GetExceptionInformation()))
	{
	}
	return 0;
}

void MyClass::InnerThreadProc()
{
	while (!m_bExit)
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		printf("Thread %d, delay %d => %5d/%02d/%02d-%02d:%02d:%02d+%03d\n",
			m_dwThreadId, m_dwDelay, st.wYear, st.wMonth, st.wDay,
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		Sleep(m_dwDelay);
	}
}


void _tmain()
{
	cout << "Main thread creating sub thread..." << endl;

	MyClass mc;
	HRESULT hr = mc.Start();
	if (FAILED(hr))
	{
		cout << "MyClass start failed, error code is " << hr;
		return;
	}

	getchar();

	mc.Stop();

	cout << "Main thread creating sub thread..." << endl;
}
