using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;


namespace Lock_Account
{
	class Account
	{
		Object  _thisLock;
		int     _balance;
		Random  _rand = new Random();

		public Account(int initial)
		{
			_thisLock = new Object();
			_balance = initial;
		}

		int Withdraw(int amount)
		{

			if (_balance < 0)
				throw new Exception("Negative Balance");

			lock (_thisLock)
			{
				if (_balance >= amount)
				{
					Console.WriteLine("Balance before Withdrawal :  " + _balance);
					Console.WriteLine("Amount to Withdraw        : -" + amount);
					_balance = _balance - amount;
					Console.WriteLine("Balance after Withdrawal  :  " + _balance);
					return amount;
				}
				return 0;
			}
		}

		public void DoTransactions()
		{
			for (int i = 0; i < 100; i++)
			{
				Withdraw(_rand.Next(1, 100));
			}
		}
	}


	class Program
	{
		static void Main(string[] args)
		{
			Thread[] threads = new Thread[10];
			Account acc = new Account(1000);
			for (int i = 0; i < 10; i++)
			{
				Thread t = new Thread(new ThreadStart(acc.DoTransactions));
				threads[i] = t;
			}
			for (int i = 0; i < 10; i++)
			{
				threads[i].Start();
			}
		}
	}
}
