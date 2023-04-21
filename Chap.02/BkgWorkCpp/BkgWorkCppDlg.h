
// BkgWorkCppDlg.h : 헤더 파일
//

#pragma once

#include "BkgrdWorker.h"

// CBkgWorkCppDlg 대화 상자
class CBkgWorkCppDlg : public CDialogEx
{
	static void WINAPI Handler_DoWork(BkgrdWorker*, DoWorkArgs*);
	static void WINAPI Handler_Report(BkgrdWorker*, ProgressChangedArgs*);
	static void WINAPI Handler_Completed(BkgrdWorker*, RunWorkerCompletedArgs*);

// 생성입니다.
public:
	CBkgWorkCppDlg(CWnd* pParent = NULL);	// 표준 생성자입니다.
	CProgressCtrl* GetPrgBar() { return (CProgressCtrl*)GetDlgItem(IDC_PROGRESS); }
	CEdit* GetLogEdit() { return (CEdit*)GetDlgItem(IDC_EDIT_LOG); }

// 대화 상자 데이터입니다.
	enum { IDD = IDD_BKGWORKCPP_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

	bool m_bClosing;
	HANDLE m_hPause;
	BkgrdWorker m_bw;

// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();

	afx_msg void OnBnClickedBtnStart();
	afx_msg void OnBnClickedBtnPause();
	afx_msg void OnBnClickedBtnCancel();
	DECLARE_MESSAGE_MAP()
public:
	DWORD m_dwDelay;
	virtual void OnCancel();
	afx_msg void OnDestroy();
};
