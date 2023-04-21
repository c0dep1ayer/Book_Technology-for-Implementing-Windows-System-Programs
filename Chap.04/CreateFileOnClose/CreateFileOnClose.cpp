#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;


void _tmain(void)
{
	HANDLE hFile = CreateFile
	(
		_T("C:\\temp\\tempfile.tmp"),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
		NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		cout << "CreateFile failed, code : " << GetLastError() << endl;
		return;
	}

	Sleep(5000);

	CloseHandle(hFile);
}
