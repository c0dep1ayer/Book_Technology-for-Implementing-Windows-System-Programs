#include "stdafx.h"
#include "Windows.h"


HANDLE g_hevExit = NULL;

BOOL CtrlHandler(DWORD fdwCtrlType)
{
	BOOL bCtrlRet = FALSE;
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. 
		case CTRL_C_EVENT:
		printf("Ctrl-C event\n\n");
		bCtrlRet = TRUE;
		break;

		// CTRL-CLOSE: confirm that the user wants to exit. 
		case CTRL_CLOSE_EVENT:
		printf("Ctrl-Close event\n\n");
		bCtrlRet = TRUE;
		break;

		// Pass other signals to the next handler. 
		case CTRL_BREAK_EVENT:
		printf("Ctrl-Break event\n\n");
		break;

		case CTRL_LOGOFF_EVENT:
		printf("Ctrl-Logoff event\n\n");
		break;

		case CTRL_SHUTDOWN_EVENT:
		printf("Ctrl-Shutdown event\n\n");
		break;
	}
	SetEvent(g_hevExit);
	return bCtrlRet;
}

void _tmain()
{
	g_hevExit = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		printf("\nERROR: Could not set control handler");
		return;
	}
	printf("\nThe Control Handler is installed.\n\n");

	while (true)
	{
		DWORD dwWaitRet = WaitForSingleObject(g_hevExit, INFINITE);
		if (dwWaitRet == WAIT_OBJECT_0)
			break;
	}
	CloseHandle(g_hevExit);
	printf("\nProgram terminated...\n\n");

}

