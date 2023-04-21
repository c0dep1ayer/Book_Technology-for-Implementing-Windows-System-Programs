#pragma once

struct DoWorkArgs
{
	bool  Cancel;
	PVOID Argument;
	PVOID Result;

	DoWorkArgs()
	{
		Cancel = false;
		Result = NULL;
		Argument = NULL;
	}
	DoWorkArgs(PVOID argument)
	{
		Cancel = false;
		Result = NULL;
		Argument = argument;
	}
};

struct ProgressChangedArgs
{
	int		ProgressPercentage;
	PVOID	UserState;

	ProgressChangedArgs(int progressPercentage, PVOID userState)
	{
		ProgressPercentage = progressPercentage;
		UserState = userState;
	}
};

struct RunWorkerCompletedArgs
{
	bool	Cancelled;
	HRESULT Error;
	PVOID	Result;

	RunWorkerCompletedArgs(PVOID result, HRESULT error, bool cancelled)
	{
		Error = error;
		Cancelled = cancelled;
		Result = result;
	}
};


class BkgrdWorker;
typedef void (WINAPI *DoWorkHandler)(BkgrdWorker* pBW, DoWorkArgs* e);
typedef void (WINAPI *ProgressChangedHandler)(BkgrdWorker* pBW, ProgressChangedArgs* e);
typedef void (WINAPI *RunWorkerCompletedHandler)(BkgrdWorker* pBW, RunWorkerCompletedArgs* e);

class BkgrdWorker
{
	static DWORD WINAPI BkgrdWorkerProc(PVOID pParam);
	void InnerBkgrdWorkerProc();

	static LRESULT CALLBACK BkgrdWndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static WNDPROC	sm_pfnOldWP;
	static LONG		sm_spRefCnt;
	static UINT		WM_BKW_REPORT, WM_BKW_COMPLETED;

	HANDLE	m_hThread;
	HWND	m_hWnd;

	DoWorkArgs					m_argDoWork;
	DoWorkHandler				m_pfnDoWork;
	ProgressChangedHandler		m_pfnReport;
	RunWorkerCompletedHandler	m_pfnCompleted;

public:
	bool CancellationPending;
	bool IsBusy;

	HWND Owner() { return m_hWnd; }

	void SetReportHandler(ProgressChangedHandler handler)
	{
		m_pfnReport = handler;
	}
	void SetCompletedHandler(RunWorkerCompletedHandler handler)
	{
		m_pfnCompleted = handler;
	}

public:
	BkgrdWorker();
	~BkgrdWorker();

	void Initialize(HWND hWnd, DoWorkHandler pfnDoWork);
	void Uninitialize();

	void RunWorkerAsync(PVOID pParam);
	void ReportProgress(int nPercent, PVOID pParam = NULL);
	void CancelAsync();
};

