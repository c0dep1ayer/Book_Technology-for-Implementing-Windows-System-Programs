#include "stdafx.h"
#include "Windows.h"

#define SAMPLE_NO	3

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
	printf("===== MainThread(ID : %d) started...\n\n", GetCurrentThreadId());
	
#if (SAMPLE_NO == 0)
	PTP_WORK arptpWorks[5];
	for (int i = 0; i < 5; i++)
		arptpWorks[i] = CreateThreadpoolWork(ThreadPoolWorkProc, (PVOID)i, NULL);

	for (int i = 0; i < 5; i++)
		SubmitThreadpoolWork(arptpWorks[i]);

	for (int i = 0; i < 5; i++)
		WaitForThreadpoolWorkCallbacks(arptpWorks[i], FALSE);

	for (int i = 0; i < 5; i++)
		CloseThreadpoolWork(arptpWorks[i]);

#elif (SAMPLE_NO == 1) 
	PTP_WORK ptpWork = CreateThreadpoolWork(ThreadPoolWorkProc, (PVOID)3, NULL);
	for (int i = 0; i < 3; i++)
		SubmitThreadpoolWork(ptpWork);

	WaitForThreadpoolWorkCallbacks(ptpWork, FALSE);
	CloseThreadpoolWork(ptpWork);

#elif (SAMPLE_NO == 2) 
	PTP_WORK ptpWork = CreateThreadpoolWork(ThreadPoolWorkProc, (PVOID)3, NULL);
	for (int i = 0; i < 3; i++)
	{
		SubmitThreadpoolWork(ptpWork);
		WaitForThreadpoolWorkCallbacks(ptpWork, FALSE);
	}
	CloseThreadpoolWork(ptpWork);
#elif (SAMPLE_NO == 3) 
	PTP_WORK ptpWork = CreateThreadpoolWork(ThreadPoolWorkProc, (PVOID)5, NULL);
	SubmitThreadpoolWork(ptpWork);
	Sleep(4000);
	//WaitForThreadpoolWorkCallbacks(ptpWork, TRUE);
	//CloseThreadpoolWork(ptpWork);
	getchar();
#endif

	printf("\n===== MainThread(ID : %d) terminated...\n", GetCurrentThreadId());
}
