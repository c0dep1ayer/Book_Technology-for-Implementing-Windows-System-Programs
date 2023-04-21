#include "stdafx.h"
#include "Windows.h"


LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst,
	PTSTR szCmdLine, int nCmdShow)
{
	static TCHAR szAppName[] = TEXT("HelloWin");


	////////////////////////////////////////////////////////////////////////////
	//윈도우즈 클래스 등록
	////////////////////////////////////////////////////////////////////////////
	WNDCLASSEX wc;
	memset(&wc, 0, sizeof(wc));
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.style		 = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	 = WndProc;
	wc.hInstance	 = hInst;
	wc.hIcon		 = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName	 = NULL;
	wc.lpszClassName = szAppName;

	if (!RegisterClassEx(&wc))
	{
		DWORD dwErr = GetLastError();
		MessageBox(NULL, TEXT("This program requires Windows NT!"),
			szAppName, MB_ICONERROR);
		return 0;
	}
	////////////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////////////////
	//윈도우즈 생성
	////////////////////////////////////////////////////////////////////////////
	HWND hWnd = CreateWindowEx
	(
		WS_EX_APPWINDOW,
		szAppName,					// window class name
		TEXT("The Hello Program"),	// window caption
		WS_OVERLAPPEDWINDOW,		// window style
		CW_USEDEFAULT,				// initial x position
		CW_USEDEFAULT,				// initial y position
		400,						// initial x size
		300,						// initial y size
		NULL,						// parent window handle
		NULL,						// window menu handle
		hInst,						// program instance handle
		NULL						// creation parameters
	);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	////////////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////////////////
	//메시지 루프
	////////////////////////////////////////////////////////////////////////////
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	////////////////////////////////////////////////////////////////////////////

	return msg.wParam;
}
////////////////////////////////////////////////////////////////////////////////


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static TCHAR szOutBuf[] = _T("The Most Simple Windows GUI Program by YHD.");

	switch (uMsg)
	{
		case WM_PAINT:
		{
			RECT rc;
			GetClientRect(hWnd, &rc);

			PAINTSTRUCT ps;
			HDC hDC = BeginPaint(hWnd, &ps);
			DrawText(hDC, szOutBuf, lstrlen(szOutBuf),
				&rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			EndPaint(hWnd, &ps);
		}
		return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
////////////////////////////////////////////////////////////////////////////////