#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define SHARE_FILE	_T("c:\\temp\\__$share.tmp")
#define EVENT_RESP	_T("Global\\EVENT_RESP")

void _tmain(int argc, TCHAR* argv[])
{
	if (argc < 3)
		return;

	cout << "======= START Child Process ========" << endl;
	DWORD	 dwSvcThrId = (DWORD)_ttoi(argv[1]);
#if _WIN64
	PAPCFUNC pfnApcProc = (PAPCFUNC)_ttoi64(argv[2]);
#else
	PAPCFUNC pfnApcProc = (PAPCFUNC)_ttoi(argv[2]);
#endif 

	HANDLE hSvcThrd = NULL, hevResp = NULL;
	try
	{
		hevResp = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_RESP);
		if (hevResp == NULL)
			throw GetLastError();

		hSvcThrd = OpenThread(THREAD_SET_CONTEXT, FALSE, dwSvcThrId);
		if (hSvcThrd == NULL)
			throw GetLastError();

		char szIn[256];
		while (true)
		{
			cin >> szIn;
			if (stricmp(szIn, "quit") == 0)
				break;

			LONG lSize = strlen(szIn);
			HANDLE hFile = CreateFile
			(
				SHARE_FILE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL
			);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				DWORD dwTrans = 0;
				WriteFile(hFile, szIn, lSize, &dwTrans, NULL);
				CloseHandle(hFile);

				if (!QueueUserAPC(pfnApcProc, hSvcThrd, (ULONG_PTR)lSize))
					throw GetLastError();
				WaitForSingleObject(hevResp, INFINITE);

				hFile = CreateFile
				(
					SHARE_FILE, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL
				);
				ReadFile(hFile, szIn, sizeof(szIn), &dwTrans, NULL);
				CloseHandle(hFile);
				szIn[dwTrans] = 0;
				cout << " ==> RESP : " << szIn << endl;
			}
		}
	}
	catch (DWORD hr)
	{
		cout << "Error occurred, code=" << hr << endl;
	}
	if (hSvcThrd)	CloseHandle(hSvcThrd);
	if (hevResp)	CloseHandle(hevResp);
	cout << "======= END Child Process ========" << endl;
}
