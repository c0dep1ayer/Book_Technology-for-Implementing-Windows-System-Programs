#include "stdafx.h"
#include "Windows.h"
#include "WaitPool.h"

#include "WaitItem.h"
#include "WaitSet.h"


WP_ITEM WaitPool::Register(HANDLE hSync, PFN_WICB pfnCB, PVOID pPrm)
{
	ITEM_MAP::iterator it = m_waits.find(hSync);
	if (it != m_waits.end())
	{
		SetLastError(ERROR_ALREADY_EXISTS);
		return NULL;
	}

	PWAIT_SET pws = NULL;
	WaitForSingleObject(m_hmuSet, INFINITE);
	for (ITEM_SET::iterator it = m_sets.begin(); it != m_sets.end(); it++)
	{
		PWAIT_SET pit = PWAIT_SET(*it);
		if (pit->m_count < MAXIMUM_WAIT_OBJECTS - 1)
		{
			pws = pit;
			break;
		}
	}
	if (pws == NULL)
	{
		pws = new WaitSet();
		pws->Init();
		m_sets.insert(pws);
	}
	ReleaseMutex(m_hmuSet);

	PWAIT_ITEM pwi = new WAIT_ITEM(hSync, pfnCB, pPrm, pws);
	pws->Command(CMDT::NEW, pwi);
	WaitForSingleObject(m_hmuMap, INFINITE);
	m_waits.insert(std::make_pair(hSync, pwi));
	ReleaseMutex(m_hmuMap);

	return pwi;
}

bool WaitPool::Unregister(WP_ITEM wpi)
{
	PWAIT_ITEM pwi = (PWAIT_ITEM)wpi;
	ITEM_MAP::iterator it = m_waits.find(pwi->_handle);
	if (it == m_waits.end())
	{
		SetLastError(ERROR_NOT_FOUND);
		return false;
	}

	WaitForSingleObject(m_hmuMap, INFINITE);
	m_waits.erase(it);
	ReleaseMutex(m_hmuMap);

	PWAIT_SET pws = pwi->_base;
	pws->Command(CMDT::DEL, pwi);
	delete pwi;

	WaitForSingleObject(m_hmuSet, INFINITE);
	if (pws->m_count == 0 && m_sets.size() > 1)
	{
		ITEM_SET::iterator it = m_sets.find(pws);
		if (it != m_sets.end())
		{
			pws->Release();
			m_sets.erase(it);
			delete pws;
		}
	}
	ReleaseMutex(m_hmuSet);

	return true;
}

void WaitPool::Start(int nMaxCnt)
{
	m_hmuMap = CreateMutex(NULL, FALSE, NULL);
	m_hmuSet = CreateMutex(NULL, FALSE, NULL);

	WaitForSingleObject(m_hmuSet, INFINITE);
	PWAIT_SET pws = new WaitSet();
	pws->Init();
	m_sets.insert(pws);
	ReleaseMutex(m_hmuSet);
}

void WaitPool::Stop()
{
	WaitForSingleObject(m_hmuSet, INFINITE);
	for (ITEM_SET::iterator it = m_sets.begin(); it != m_sets.end(); it++)
	{
		PWAIT_SET pws = PWAIT_SET(*it);
		pws->Release();
		delete pws;
	}
	ReleaseMutex(m_hmuSet);

	WaitForSingleObject(m_hmuSet, INFINITE);
	for (ITEM_MAP::iterator it = m_waits.begin(); it != m_waits.end(); it++)
	{
		PWAIT_ITEM pwi = PWAIT_ITEM(it->second);
		delete pwi;
	}
	ReleaseMutex(m_hmuMap);

	CloseHandle(m_hmuMap);
	CloseHandle(m_hmuSet);
}
