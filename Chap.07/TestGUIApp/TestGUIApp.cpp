
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

	CTestGUIAppDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();

	return FALSE;
}

