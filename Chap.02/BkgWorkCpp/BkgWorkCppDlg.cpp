
// BkgWorkCppDlg.cpp : 구현 파일
//

#include "stdafx.h"
#include "BkgWorkCpp.h"
#include "BkgWorkCppDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CBkgWorkCppDlg 대화 상자

CBkgWorkCppDlg::CBkgWorkCppDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CBkgWorkCppDlg::IDD, pParent)
	, m_dwDelay(200), m_bClosing(false)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CBkgWorkCppDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDT_DELAY, m_dwDelay);
}

BEGIN_MESSAGE_MAP(CBkgWorkCppDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_START, &CBkgWorkCppDlg::OnBnClickedBtnStart)
	ON_BN_CLICKED(IDC_BTN_PAUSE, &CBkgWorkCppDlg::OnBnClickedBtnPause)
	ON_BN_CLICKED(IDC_BTN_CANCEL, &CBkgWorkCppDlg::OnBnClickedBtnCancel)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CBkgWorkCppDlg 메시지 처리기
BOOL CBkgWorkCppDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	m_hPause = CreateEvent(NULL, TRUE, TRUE, NULL);

	m_bw.Initialize(GetSafeHwnd(), Handler_DoWork);
	m_bw.SetReportHandler(Handler_Report);
	m_bw.SetCompletedHandler(Handler_Completed);

	GetPrgBar()->SetRange(0, 100);


	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CBkgWorkCppDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	m_bw.Uninitialize();
	if (m_hPause != NULL)
		CloseHandle(m_hPause);
}

void CBkgWorkCppDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 응용 프로그램의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CBkgWorkCppDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CBkgWorkCppDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void WINAPI CBkgWorkCppDlg::Handler_DoWork(BkgrdWorker* pBW, DoWorkArgs* e)
{
	DWORD dwThrId = GetCurrentThreadId();
	CBkgWorkCppDlg*	pDlg = (CBkgWorkCppDlg*)e->Argument;

	pBW->ReportProgress(0, (PVOID)dwThrId);

	for (int i = 1; i <= 100; i++)
	{
		if (pBW->CancellationPending)
		{
			e->Cancel = true;
			break;
		}

		Sleep(pDlg->m_dwDelay);
		pBW->ReportProgress(i);
		if (i == 100)
			break;
		WaitForSingleObject(pDlg->m_hPause, INFINITE);
	}
}

void WINAPI CBkgWorkCppDlg::Handler_Report(BkgrdWorker* pBW, ProgressChangedArgs* e)
{
	DWORD dwThrId = GetCurrentThreadId();
	CBkgWorkCppDlg*	pDlg = (CBkgWorkCppDlg*)CWnd::FromHandle(pBW->Owner());

	CEdit* pLog = pDlg->GetLogEdit();
	int nLen = pLog->GetWindowTextLength();
	CString szLog;

	pDlg->GetPrgBar()->SetPos(e->ProgressPercentage);
	if (e->ProgressPercentage == 0)
	{
		szLog.Format(_T("DoWork, Id = %d\xd\xa\xd\xa"), (DWORD)e->UserState);
		pLog->SetSel(nLen, nLen);
		pLog->ReplaceSel(szLog);
	}
	else
	{
		szLog.Format(_T("ProgressChanged => %d%%, Id = %d\xd\xa"), e->ProgressPercentage, dwThrId);

		CString szCur;
		pLog->GetWindowText(szCur);
		szCur = szCur.Left(szCur.GetLength() - 2);
		int nIdx = szCur.ReverseFind(_T('\xd'));
		if (nIdx >= 0)
			pLog->SetSel(nIdx + 2, nLen);
		else
			pLog->SetSel(nLen, nLen);
		pLog->ReplaceSel(szLog);
	}
}

void WINAPI CBkgWorkCppDlg::Handler_Completed(BkgrdWorker* pBW, RunWorkerCompletedArgs* e)
{
	DWORD dwThrId = GetCurrentThreadId();
	CBkgWorkCppDlg*	pDlg = (CBkgWorkCppDlg*)CWnd::FromHandle(pBW->Owner());

	if (pDlg->m_bClosing)
	{
		pDlg->DestroyWindow();
		return;
	}

	CEdit* pLog = pDlg->GetLogEdit();
	int nLen = pLog->GetWindowTextLength();
	CString szLog;

	szLog.Format(_T("RunWorker%s, Id = %d\xd\xa"), 
		(e->Cancelled) ? _T("Canceled") : _T("Completed"), dwThrId);
	pLog->SetSel(nLen, nLen);
	pLog->ReplaceSel(szLog);

	pDlg->GetDlgItem(IDC_BTN_START)->EnableWindow();
	pDlg->GetDlgItem(IDC_BTN_PAUSE)->EnableWindow(FALSE);
	pDlg->GetDlgItem(IDC_BTN_CANCEL)->EnableWindow(FALSE);
}

void CBkgWorkCppDlg::OnBnClickedBtnStart()
{
	UpdateData();

	DWORD dwThrId = GetCurrentThreadId();
	CString szLog;
	szLog.Format(_T("RunWorkerAsync, Id = %d\xd\xa"), dwThrId);
	SetDlgItemText(IDC_EDIT_LOG, szLog);

	m_bw.RunWorkerAsync(this);

	GetDlgItem(IDC_BTN_START)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_PAUSE)->EnableWindow();
	GetDlgItem(IDC_BTN_CANCEL)->EnableWindow();
}


void CBkgWorkCppDlg::OnBnClickedBtnPause()
{
	if (!m_bw.IsBusy)
		return;

	DWORD dwThrId = GetCurrentThreadId();
	CEdit* pLog = GetLogEdit();
	int nLen = pLog->GetWindowTextLength();
	CString szLog;

	if (WaitForSingleObject(m_hPause, 0) == WAIT_OBJECT_0)
	{
		GetDlgItem(IDC_BTN_PAUSE)->SetWindowText(_T("Restart"));
		ResetEvent(m_hPause);
		szLog.Format(_T(" ** Paused, Id = %d\xd\xa\xd\xa"), dwThrId);
	}
	else
	{
		GetDlgItem(IDC_BTN_PAUSE)->SetWindowText(_T("Pause"));
		SetEvent(m_hPause);
		szLog.Format(_T(" ** Resumed, Id = %d\xd\xa\xd\xa"), dwThrId);
	}
	pLog->SetSel(nLen, nLen);
	pLog->ReplaceSel(szLog);
}


void CBkgWorkCppDlg::OnBnClickedBtnCancel()
{
	if (!m_bw.IsBusy)
		return;

	if (WaitForSingleObject(m_hPause, 0) == WAIT_OBJECT_0)
		ResetEvent(m_hPause);

	int dr = AfxMessageBox(_T("현재작업을 취소하시겠습니까?"), MB_YESNO);
	if (dr == IDYES)
		m_bw.CancelAsync();
	SetEvent(m_hPause);
}


void CBkgWorkCppDlg::OnCancel()
{
	if (!m_bw.IsBusy)
	{
		CDialogEx::OnCancel();
		return;
	}

	if (WaitForSingleObject(m_hPause, 0) == WAIT_OBJECT_0)
		ResetEvent(m_hPause);

	int dr = AfxMessageBox(_T("현재작업을 취소하시겠습니까?"), MB_YESNO);
	if (dr == IDYES)
	{
		m_bClosing = true;
		m_bw.CancelAsync();
	}
	SetEvent(m_hPause);
}
