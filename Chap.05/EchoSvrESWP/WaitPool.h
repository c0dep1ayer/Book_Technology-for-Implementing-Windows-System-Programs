#pragma once

#include "set"
#include "map"

typedef PVOID WP_ITEM;
typedef PVOID WP_SET; 
typedef void(WINAPI *PFN_WICB)(PVOID pParma);

class WaitPool
{
	typedef std::map<HANDLE, WP_ITEM> ITEM_MAP;
	ITEM_MAP m_waits;
	HANDLE	 m_hmuMap;

	typedef std::set<WP_SET> ITEM_SET;
	ITEM_SET m_sets;
	HANDLE	 m_hmuSet;

public:
	WP_ITEM Register(HANDLE hSync, PFN_WICB pfnCB, PVOID pPrm);
	bool Unregister(WP_ITEM wpi);
	void Start(int nMaxCnt = INT_MAX);
	void Stop();
};
