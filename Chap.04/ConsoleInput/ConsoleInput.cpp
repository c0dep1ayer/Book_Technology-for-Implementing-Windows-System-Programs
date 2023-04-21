#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


void _tmain()
{
	HANDLE hStdOut = CreateFile(L"CONOUT$",
		GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	HANDLE hStdIn = CreateFile(L"CONIN$",
		GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
	//HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	//HANDLE hStdIn  = GetStdHandle(STD_INPUT_HANDLE);

	char szIn[256], szOut[256];
	while (true)
	{
		DWORD dwTrBytes = 0;
		ReadFile(hStdIn, szIn, sizeof(szIn), &dwTrBytes, NULL);
		szIn[dwTrBytes] = 0;
		if (_stricmp(szIn, "quit\r\n") == 0)
			break;

		int len = sprintf(szOut, "<== Echo : %s", szIn);
		WriteFile(hStdOut, szOut, (DWORD)len, &dwTrBytes, NULL);
	}
	CloseHandle(hStdIn);
	CloseHandle(hStdOut);
}
