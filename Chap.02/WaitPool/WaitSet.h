#pragma once

class WaitSet
{
	friend class WaitPool;

	static DWORD WINAPI WaitSetProc(PVOID pParam);
	void InnerWaitSetProc();

	HANDLE		m_thread;
	HANDLE		m_noti;
	HANDLE		m_resp;

	WAIT_ITEM*	m_arg;
	CMDT		m_cmd;

	int			m_count;
	
	WaitPool*	m_base;

public:
	WaitSet()
	{
		m_count = 0;
	}

public:
	void Init();
	void Release();
	void Command(CMDT cmd, WAIT_ITEM* arg);

};
typedef WaitSet* PWAIT_SET;

