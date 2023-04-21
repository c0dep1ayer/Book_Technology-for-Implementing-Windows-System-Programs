#pragma once

enum CMDT
{
	NEW, DEL, EXIT
};

class WaitSet;
struct WAIT_ITEM
{
	HANDLE		_handle;
	PVOID		_pPrm;
	PFN_WICB	_pfnCB;
	WaitSet*	_base;

	WAIT_ITEM(HANDLE handle, PFN_WICB pfnCB, PVOID pPrm, WaitSet* base)
	{
		_handle = handle;
		_pPrm = pPrm;
		_pfnCB = pfnCB;
		_base = base;
	}
};
typedef WAIT_ITEM* PWAIT_ITEM;
