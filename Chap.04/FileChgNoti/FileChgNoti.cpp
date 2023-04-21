#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


void _tmain()
{
	DWORD dwNotFlg = FILE_NOTIFY_CHANGE_FILE_NAME |
					 FILE_NOTIFY_CHANGE_DIR_NAME |
					 FILE_NOTIFY_CHANGE_ATTRIBUTES |
					 FILE_NOTIFY_CHANGE_SIZE;

	HANDLE hChgNoti = FindFirstChangeNotification(_T("C:\\Temp"), FALSE, dwNotFlg);
	if (hChgNoti == INVALID_HANDLE_VALUE)
	{
		cout << " ~~~ FindFirstChangeNotification error : " << GetLastError() << endl;
		return;
	}

	while (true)
	{
		DWORD dwWaitCode = WaitForSingleObject(hChgNoti, INFINITE);
		if (dwWaitCode == WAIT_FAILED)
			break;

		printf("=> Dir entry changed...\n");
		WIN32_FIND_DATA fd;
		HANDLE hFindFile = ::FindFirstFile(_T("C:\\Temp\\*"), &fd);
		if (hFindFile != INVALID_HANDLE_VALUE)
		{
			do
			{
				FILETIME ft;
				FileTimeToLocalFileTime(&fd.ftLastAccessTime, &ft);
				SYSTEMTIME st;
				FileTimeToSystemTime(&ft, &st);
				
				_tprintf(_T("  %04d-%02d-%02d %02d:%02d:%02d+%03d : %s\n"),
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, 
					st.wSecond, st.wMilliseconds, fd.cFileName);
			}
			while (FindNextFile(hFindFile, &fd));
			FindClose(hFindFile);
		}

		if (!FindNextChangeNotification(hChgNoti))
		{
			cout << " ~~~ FindNextChangeNotification error : " << GetLastError() << endl;
			break;
		}
	}
	FindCloseChangeNotification(hChgNoti);
}
