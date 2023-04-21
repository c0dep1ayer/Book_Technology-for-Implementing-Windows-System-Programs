
// TestGUIApp.cpp : 응용 프로그램에 대한 클래스 동작을 정의합니다.
//

#include "stdafx.h"
#include "TestGUIApp.h"
#include "TestGUIAppDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTestGUIAppApp

BEGIN_MESSAGE_MAP(CTestGUIAppApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CTestGUIAppApp 생성
CTestGUIAppApp::CTestGUIAppApp()
{
	// TODO: 여기에 생성 코드를 추가합니다.
	// InitInstance에 모든 중요한 초기화 작업을 배치합니다.
}


// 유일한 CTestGUIAppApp 개체입니다.

CTestGUIAppApp theApp;


// CTestGUIAppApp 초기화
BOOL CTestGUIAppApp::InitInstance()
{
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	Sleep(15000);
	HANDLE hEvent = NULL;
	int nArgCount = 0;
	if (_tcsclen(m_lpCmdLine) > 0)
	{
		PWSTR* pszArgList = CommandLineToArgvW(m_lpCmdLine, &nArgCount);
		if (pszArgList == NULL)
			return FALSE;

		if (nArgCount > 0)
		{
#ifdef _WIN64
			hEvent = (HANDLE)_tstoi64(pszArgList[0]);
#else
			hEvent = (HANDLE)_tstoi(pszArgList[0]);
#endif
			if (hEvent == NULL)
				hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, pszArgList[0]);
		}
		LocalFree(pszArgList);
		if (hEvent == NULL)
		{
			AfxMessageBox(_T("Event acquire failed..."));
			return FALSE;
		}
	}

	CTestGUIAppDlg dlg(hEvent);
	m_pMainWnd = &dlg;
	dlg.DoModal();

	return FALSE;
}

