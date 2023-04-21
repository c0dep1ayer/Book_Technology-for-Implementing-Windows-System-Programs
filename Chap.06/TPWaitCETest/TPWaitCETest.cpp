#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

VOID CALLBACK ThreadPoolWorkProc(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WORK ptpWork)
{
	int nWorkId = (int)pCtx;
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("=> WorkItem %d(ID : %d) started at : %02d:%02d:%02d.%03d\n",
		nWorkId, GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	Sleep(nWorkId * 1000);

	GetLocalTime(&st);
	printf("...WorkItem %d(ID : %d) ended at : %02d:%02d:%02d.%03d\n",
		nWorkId, GetCurrentThreadId(),
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void _tmain()
{
	TP_CALLBACK_ENVIRON ce;
	InitializeThreadpoolEnvironment(&ce);

	try
	{
		SetThreadpoolCallbackPersistent(&ce);
		//SetThreadpoolCallbackRunsLong(&ce);

		getchar();
		PTP_WORK ptpWork = CreateThreadpoolWork(ThreadPoolWorkProc, (PVOID)10, NULL);// &ce);
		for (int i = 0; i < 10; i++)
		{
			SubmitThreadpoolWork(ptpWork);
			Sleep(1000);
		}
		getchar();
		WaitForThreadpoolWorkCallbacks(ptpWork, FALSE);
		CloseThreadpoolWork(ptpWork);
	}
	catch (LONG ex)
	{
		cout << "Error occurred, code = " << ex << endl;
	}
	DestroyThreadpoolEnvironment(&ce);
	cout << "==== Program exits... =====================" << endl;
}
