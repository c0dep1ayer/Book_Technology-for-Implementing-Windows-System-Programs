
#pragma once
#include "BkgrdWorker.h"

// CCopyFileCBDlg 대화 상자
class CCopyFileCBDlg : public CDialogEx
{
	static void WINAPI Handler_DoWork(BkgrdWorker*, DoWorkArgs*);
	static void WINAPI Handler_Report(BkgrdWorker*, ProgressChangedArgs*);
	static void WINAPI Handler_Completed(BkgrdWorker*, RunWorkerCompletedArgs*);

	void DoWork(DoWorkArgs*);
	void Report(ProgressChangedArgs*);
	void Completed(RunWorkerCompletedArgs*);

	CString m_szSrc;
	CString m_szDst;

	HANDLE	m_hPause;
	BkgrdWorker m_bw;

// 생성입니다.
public:
	CCopyFileCBDlg(CWnd* pParent = NULL);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
	enum { IDD = IDD_COPYFILECB_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedBtnSrc();
	afx_msg void OnBnClickedBtnDst();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedPause();
};
