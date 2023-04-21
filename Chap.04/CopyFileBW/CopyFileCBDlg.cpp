
// CopyFileCBDlg.cpp : 구현 파일
//

#include "stdafx.h"
#include "CopyFileCB.h"
#include "CopyFileCBDlg.h"
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


// CCopyFileCBDlg 대화 상자



CCopyFileCBDlg::CCopyFileCBDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CCopyFileCBDlg::IDD, pParent)
	, m_szSrc(_T(""))
	, m_szDst(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_hPause = NULL;
}

void CCopyFileCBDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SRC, m_szSrc);
	DDX_Text(pDX, IDC_EDIT_DST, m_szDst);
}

BEGIN_MESSAGE_MAP(CCopyFileCBDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_SRC, &CCopyFileCBDlg::OnBnClickedBtnSrc)
	ON_BN_CLICKED(IDC_BTN_DST, &CCopyFileCBDlg::OnBnClickedBtnDst)
	ON_BN_CLICKED(IDOK, &CCopyFileCBDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_BTN_PAUSE, &CCopyFileCBDlg::OnBnClickedPause)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// CCopyFileCBDlg 메시지 처리기
BOOL CCopyFileCBDlg::OnInitDialog()
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
	((CProgressCtrl*)GetDlgItem(IDC_PRG_COPY))->SetRange(0, 100);

	m_hPause = CreateEvent(NULL, TRUE, TRUE, NULL);

	m_bw.Initialize(GetSafeHwnd(), Handler_DoWork);
	m_bw.SetReportHandler(Handler_Report);
	m_bw.SetCompletedHandler(Handler_Completed);

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CCopyFileCBDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	m_bw.Uninitialize();
	if (m_hPause != NULL)
		CloseHandle(m_hPause);
}

void CCopyFileCBDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CCopyFileCBDlg::OnPaint()
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
HCURSOR CCopyFileCBDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CCopyFileCBDlg::OnBnClickedBtnSrc()
{
	CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY, _T("All Files(*.*)|*.*||"));
	if (dlg.DoModal() == IDOK)
	{
		m_szSrc = dlg.GetPathName();
		UpdateData(FALSE);
	}
}


void CCopyFileCBDlg::OnBnClickedBtnDst()
{
	CFileDialog dlg(FALSE, NULL, NULL, 
		OFN_CREATEPROMPT | OFN_FILEMUSTEXIST | 
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, 
		_T("All Files(*.*)|*.*||"));
	if (dlg.DoModal() == IDOK)
	{
		m_szDst = dlg.GetPathName();
		UpdateData(FALSE);
	}
}

void CCopyFileCBDlg::OnBnClickedOk()
{
	UpdateData();

	CString szTitle;
	if (!m_bw.IsBusy)
	{
		m_bw.RunWorkerAsync(this);

		SetDlgItemText(IDOK, _T("취소"));
		GetDlgItem(IDC_BTN_PAUSE)->ShowWindow(TRUE);
		GetDlgItem(IDC_BTN_PAUSE)->EnableWindow();
	}
	else
	{
		ResetEvent(m_hPause);
		INT nRlt = AfxMessageBox(_T("복사중입니다. 종료하시겠습니까?"), MB_YESNO);
		if (nRlt == IDYES)
			m_bw.CancelAsync();
		SetEvent(m_hPause);
	}
}

void CCopyFileCBDlg::OnBnClickedPause()
{
	CString szTitle;
	GetDlgItemText(IDC_BTN_PAUSE, szTitle);
	if (szTitle == _T("멈춤"))
	{
		GetDlgItem(IDOK)->EnableWindow(FALSE);
		SetDlgItemText(IDC_BTN_PAUSE, _T("계속"));
		ResetEvent(m_hPause);
	}
	else
	{
		GetDlgItem(IDOK)->EnableWindow();
		SetDlgItemText(IDC_BTN_PAUSE, _T("멈춤"));
		SetEvent(m_hPause);
	}
}

void CCopyFileCBDlg::OnCancel()
{
	if (m_bw.IsBusy)
	{
		ResetEvent(m_hPause);
		if (AfxMessageBox(_T("복사중입니다. 종료하시겠습니까?"), MB_YESNO) == IDYES)
			m_bw.CancelAsync();
		SetEvent(m_hPause);
	}

	CDialogEx::OnCancel();
}


void WINAPI CCopyFileCBDlg::Handler_DoWork(BkgrdWorker* pBW, DoWorkArgs* e)
{
	CCopyFileCBDlg* pDlg = (CCopyFileCBDlg*)e->Argument;
	pDlg->DoWork(e);
}

void WINAPI CCopyFileCBDlg::Handler_Report(BkgrdWorker* pBW, ProgressChangedArgs* e)
{
	CCopyFileCBDlg*	pDlg = (CCopyFileCBDlg*)CWnd::FromHandle(pBW->Owner());
	pDlg->Report(e);
}

void WINAPI CCopyFileCBDlg::Handler_Completed(BkgrdWorker* pBW, RunWorkerCompletedArgs* e)
{
	CCopyFileCBDlg*	pDlg = (CCopyFileCBDlg*)CWnd::FromHandle(pBW->Owner());
	pDlg->Completed(e);
}


void CCopyFileCBDlg::DoWork(DoWorkArgs* e)
{
	DWORD	dwErrCode = 0;
	BYTE	btBuff[65536];
	HANDLE	hSrcFile = INVALID_HANDLE_VALUE;
	HANDLE	hDstFile = INVALID_HANDLE_VALUE;

	__int64* psi = new _int64[2];
	psi[0] = psi[1] = 0LL;

	__try
	{
		hSrcFile = CreateFile
		(
			m_szSrc, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL
		);
		if (hSrcFile == INVALID_HANDLE_VALUE)
			throw HRESULT_FROM_WIN32(GetLastError());

		hDstFile = CreateFile
		(
			m_szDst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL
		);
		if (hDstFile == INVALID_HANDLE_VALUE)
			throw HRESULT_FROM_WIN32(GetLastError());

		LARGE_INTEGER llSrcSize;
		GetFileSizeEx(hSrcFile, &llSrcSize);
		psi[1] = llSrcSize.QuadPart;
		m_bw.ReportProgress(0, psi);

		LARGE_INTEGER llCopiedSize;
		llCopiedSize.QuadPart = 0;
		while (llCopiedSize.QuadPart < llSrcSize.QuadPart)
		{
			if (m_bw.CancellationPending)
			{
				e->Cancel = true;
				break;
			}

			DWORD dwReadBytes = 0;
			BOOL bIsOK = ReadFile
			(
				hSrcFile, btBuff, sizeof(btBuff), &dwReadBytes, NULL
			);
			if (!bIsOK)
				throw HRESULT_FROM_WIN32(GetLastError());

			DWORD dwWroteBytes = 0;
			bIsOK = WriteFile
			(
				hDstFile, btBuff, dwReadBytes, &dwWroteBytes, NULL
			);
			if (!bIsOK)
				throw HRESULT_FROM_WIN32(GetLastError());

			llCopiedSize.QuadPart += dwWroteBytes;

			int nPercent = (int)(llCopiedSize.QuadPart * 100LL / llSrcSize.QuadPart);
			psi[0] = llCopiedSize.QuadPart;
			m_bw.ReportProgress(nPercent, psi);

			WaitForSingleObject(m_hPause, INFINITE);
		}
	}
	__finally
	{
		e->Result = psi;
		if (hSrcFile != INVALID_HANDLE_VALUE)
			CloseHandle(hSrcFile);
		if (hDstFile != INVALID_HANDLE_VALUE)
			CloseHandle(hDstFile);
	}
}

void CCopyFileCBDlg::Report(ProgressChangedArgs* e)
{
	((CProgressCtrl*)GetDlgItem(IDC_PRG_COPY))->SetPos(e->ProgressPercentage);
	__int64* psi = (__int64*)e->UserState;

	CString szLog;
	szLog.Format(_T("%I64d / %I64d => %d%% Copied...."), psi[0], psi[1], e->ProgressPercentage);
	SetDlgItemText(IDC_EDIT_INFO, szLog);
}

void CCopyFileCBDlg::Completed(RunWorkerCompletedArgs* e)
{
	//if (m_bClosing)
	//{
	//	pDlg->DestroyWindow();
	//	return;
	//}

	__int64* psi = (__int64*)e->Result;
	CString szLog;

	if (FAILED(e->Error))
		szLog.Format(_T("Error occurred, code is %08X...\n"), e->Error);
	else
	{
		if (e->Cancelled)
			szLog.Format(_T("%I64d bytes copied, but canceled...\n"), psi[0]);
		else
			szLog.Format(_T("Total %I64d bytes copy completed...\n"), psi[1]);
	}
	SetDlgItemText(IDC_EDIT_INFO, szLog);
	delete[] psi;

	SetDlgItemText(IDOK, _T("복사"));
	GetDlgItem(IDC_BTN_PAUSE)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_PAUSE)->ShowWindow(FALSE);
	SetDlgItemText(IDC_BTN_PAUSE, _T("멈춤"));
}
