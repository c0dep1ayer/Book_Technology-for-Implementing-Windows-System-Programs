#include "stdafx.h"
#include "Windows.h"
#include "Iphlpapi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Iphlpapi.lib")

#define STATUS_CANCELLED	((NTSTATUS)0xC0000120L)

void PrintIpList()
{
	PIP_ADAPTER_INFO pAdapterInfo = NULL;
	ULONG			 ulOutBufLen = 0;
	DWORD			 dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	if (dwRetVal != NO_ERROR)
	{
		if (dwRetVal == ERROR_BUFFER_OVERFLOW)
		{
			pAdapterInfo = (IP_ADAPTER_INFO*)new BYTE[ulOutBufLen];
			dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
			if (dwRetVal != NO_ERROR)
			{
				delete[]((PBYTE)pAdapterInfo);
				cout << " ~~~NotifyAddrChange error : " << GetLastError() << endl;
			}
		}
		else
		{
			if (dwRetVal == ERROR_NO_DATA)	// 232(0xE8)
				return;
			cout << " ~~~NotifyAddrChange error : " << GetLastError() << endl;
		}
	}

	bool bFound = false;
	PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
	while (pAdapter)
	{
		IP_ADDR_STRING* pIPAddr = &pAdapter->IpAddressList;
		while (pIPAddr)
		{
			_tprintf(_T(" ...IP Address : %s\n"), pIPAddr->IpAddress.String);
			pIPAddr = pIPAddr->Next;
		}

		pAdapter = pAdapter->Next;
	}
	if (pAdapterInfo)
		delete[]((PBYTE)pAdapterInfo);
}

OVERLAPPED g_ov;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	CancelIPChangeNotify(&g_ov);
	return TRUE;
}

void _tmain()
{
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		return;
	}

	g_ov.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	while (true)
	{
		HANDLE hevIPChg = NULL;
		if (NotifyAddrChange(&hevIPChg, &g_ov) != NO_ERROR)
		{
			DWORD dwErrCode = GetLastError();
			if (dwErrCode != ERROR_IO_PENDING)
				cout << " ~~~NotifyAddrChange error : " << GetLastError() << endl;
		}

		DWORD dwErrCode = WaitForSingleObject(g_ov.hEvent, INFINITE);
		if (dwErrCode == WAIT_FAILED)
			break;

		if (g_ov.Internal != 0)
		{
			if (g_ov.Internal == STATUS_CANCELLED)
				cout << " ==> IP Changed Noti Canceled!!!" << endl;
			break;
		}
		PrintIpList();
	}
}
