#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;

#define TEST_MODE 3

#if TEST_MODE == 1
volatile int g_x, g_y;
#else
int g_x, g_y;
#endif
int g_a, g_b;

DWORD WINAPI TP25(PVOID)
{
	g_x = 25;
#if TEST_MODE == 2
	_ReadWriteBarrier();
#elif TEST_MODE == 3
	MemoryBarrier();
#endif
	g_a = g_y;
	return 0;
}

DWORD WINAPI TP99(PVOID)
{
	g_y = 99;
#if TEST_MODE == 2
	_ReadWriteBarrier();
#elif TEST_MODE == 3
	MemoryBarrier();
#endif
	g_b = g_x;
	return 0;
}

void _tmain()
{
	DWORD dwTheaID = 0;
	CloseHandle(CreateThread(NULL, 0, TP25, NULL, 0, &dwTheaID));
	CloseHandle(CreateThread(NULL, 0, TP99, NULL, 0, &dwTheaID));

	getchar();

	cout << "A= " << g_a << ", B=" << g_b << endl;
}
