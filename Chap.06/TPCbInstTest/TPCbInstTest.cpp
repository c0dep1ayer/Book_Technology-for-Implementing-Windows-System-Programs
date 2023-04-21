#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


VOID CALLBACK ThreadPoolWaitProc(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WORK pWork)
{
	PCRITICAL_SECTION pCS = (PCRITICAL_SECTION)pCtx;
	EnterCriticalSection(pCS);
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		cout << "..." << "SubThread " << GetCurrentThreadId() << " => ";
		cout << st.wYear << '/' << st.wMonth << '/' << st.wDay << ' ';
		cout << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '+';
		cout << st.wMilliseconds << endl;
	}
	LeaveCriticalSectionWhenCallbackReturns(pInst, pCS);
}


#define MAX_THR_CNT	10
void _tmain(void)
{
	cout << "======= Start Mutex Test ========" << endl;
	CRITICAL_SECTION cs;
	InitializeCriticalSection(&cs);
	PTP_WORK ptpWork = CreateThreadpoolWork(ThreadPoolWaitProc, &cs, NULL);
	for (int i = 0; i < 5; i++)
		SubmitThreadpoolWork(ptpWork);

	Sleep(1000);
	WaitForThreadpoolWorkCallbacks(ptpWork, FALSE);

	DeleteCriticalSection(&cs);
	cout << "======= End Mutex Test ==========" << endl;
}
