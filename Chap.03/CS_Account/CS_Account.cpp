#include "stdafx.h"
#include "Windows.h"
#include "time.h"
#include "iostream"
using namespace std;


class Account
{
	CRITICAL_SECTION _thisLock;
	int _balance;

public:
	Account(int initial)
	{
		InitializeCriticalSection(&_thisLock);

		srand((unsigned int)time(0));
		_balance = initial;
	}

	~Account()
	{
		DeleteCriticalSection(&_thisLock);
	}

	int Withdraw(int amount)
	{
		if (_balance < 0)
		{
			cout << "Negative Balance : " << _balance << endl;
			return _balance;
		}

		EnterCriticalSection(&_thisLock);
		{
			if (_balance >= amount)
			{
				cout << "Balance before Withdrawal :  " << _balance << endl;
				cout << "Amount to Withdraw        : -" << amount << endl;
				_balance -= amount;
				cout << "Balance after Withdrawal  :  " << _balance << endl;
			}
			else
				amount = 0;
		}
		LeaveCriticalSection(&_thisLock);
		return amount;
	}

	void DoTransactions()
	{
		for (int i = 0; i < 100; i++)
		{
			Withdraw((int)(rand() % 100) + 1);
		}
	}
};


DWORD WINAPI ThreadProc(LPVOID pParam)
{
	Account* pAcc = (Account*)pParam;
	pAcc->DoTransactions();

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE threads[10];
	Account acc(1000);

	for (int i = 0; i < 10; i++)
	{
		DWORD dwTheaID = 0;
		threads[i] = CreateThread(NULL, 0, ThreadProc, &acc, CREATE_SUSPENDED, &dwTheaID);
	}
	for (int i = 0; i < 10; i++)
	{
		ResumeThread(threads[i]);
	}

	WaitForMultipleObjects(10, threads, TRUE, INFINITE);

	return 0;
}
