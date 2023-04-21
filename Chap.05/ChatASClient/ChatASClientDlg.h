#pragma once
#include "Winsock2.h"
#pragma comment(lib, "Ws2_32.lib")

#define WM_SOCK_NOTI	WM_USER + 100

// CChatASClientDlg 대화 상자
class CChatASClientDlg : public CDialogEx
{
	SOCKET m_hSock;

	void UpdateChatView(PCTSTR);

// 생성입니다.
public:
	CChatASClientDlg(CWnd* pParent = NULL);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
	enum { IDD = IDD_CHATASCLIENT_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedOk();
	afx_msg LRESULT OnSockNoti(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDestroy();
};
