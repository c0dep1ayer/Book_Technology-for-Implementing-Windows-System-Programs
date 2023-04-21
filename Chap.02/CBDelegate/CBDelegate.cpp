#include "stdafx.h"
#include "Windows.h"
#include "list"
#include "iostream"
using namespace std;

#define POST_WND	1

typedef void(WINAPI *PFN_WICB)(PVOID pParma);

#define WM_NOTI_EXIT	WM_USER + 100
#define WM_DELEGATE		WM_USER + 101

#if (POST_WND == 0)
DWORD WINAPI DelegateProc(PVOID pParam)
{
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == WM_NOTI_EXIT)
			break;

		if (msg.message == WM_DELEGATE)
		{
			PFN_WICB pfnCB = (PFN_WICB)msg.wParam;
			PVOID	 pArgs = (PVOID)msg.lParam;
			pfnCB(pArgs);
		}
	}

	return 0;
}
#else
#define MY_CLS_NAME	_T("MESSAGE_ONLY")

LRESULT CALLBACK MyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_DELEGATE:
		{
			PFN_WICB pfnCB = (PFN_WICB)wParam;
			PVOID	 pArgs = (PVOID)lParam;
			pfnCB(pArgs);
		}
		return 0;

		case WM_NOTI_EXIT:
			DestroyWindow(hWnd);
		return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

DWORD WINAPI DelegateProc(PVOID pParam)
{
	WNDCLASSEX wc;
	memset(&wc, 0, sizeof(wc));
	wc.cbSize		 = sizeof(wc);
	wc.lpfnWndProc	 = MyWndProc;
	wc.lpszClassName = MY_CLS_NAME;
	RegisterClassEx(&wc);

	HWND hWnd = CreateWindowEx
	(
		0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0, 
		HWND_MESSAGE, NULL, NULL, NULL
	);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
		DispatchMessage(&msg);
	
	return msg.wParam;
}
#endif

void WINAPI TestCallback(PVOID pParam)
{
	PSTR pszData = (PSTR)pParam;
	printf(" => Thread %d : %s\n", GetCurrentThreadId(), pszData);
	delete[] pszData;
}


void _tmain()
{
	cout << "======= Start CBDelegate Test ========" << endl;

	DWORD dwThrRunId;
	HANDLE hThread = CreateThread(NULL, 0, DelegateProc, NULL, 0, &dwThrRunId);

#if (POST_WND == 1)
	HWND hWnd = NULL;
	while ((hWnd = FindWindow(MY_CLS_NAME, NULL)) == NULL)
		Sleep(50);
#endif

	char szIn[1024];
	while (true)
	{
		cin >> szIn;

		if (_stricmp(szIn, "quit") == 0)
		{
#if (POST_WND == 0)
			PostThreadMessage(dwThrRunId, TM_CMD_EXIT, 0, 0);
#else
			PostMessage(hWnd, WM_NOTI_EXIT, 0, 0);
#endif
			break;
		}

		PSTR pszData = new char[strlen(szIn) + 1];
		strcpy(pszData, szIn);
#if (POST_WND == 0)
		PostThreadMessage
		(
			dwThrRunId, TM_CMD_DELEGATE, (WPARAM)TestCallback, (LPARAM)pszData
		);
#else
		PostMessage
		(
			hWnd, WM_DELEGATE, (WPARAM)TestCallback, (LPARAM)pszData
		);
#endif
	}

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);

	cout << "======= End CBDelegate Test ==========" << endl;
}
