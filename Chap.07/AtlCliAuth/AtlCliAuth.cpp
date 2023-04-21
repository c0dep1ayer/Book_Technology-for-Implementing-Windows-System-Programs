#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define EVENT_BY_APP	_T("Global\\EVENT_BY_APP")
#define EVENT_BY_SVC	_T("Global\\EVENT_BY_SVC")

void _tmain(void)
{
	cout << "======= Start AtlCliAuth Test ========" << endl;

	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_BY_APP);
	if (hEvent == NULL)
	{
		cout << "EVENT_BY_APP Creation failed : " << GetLastError() << endl;
		return;
	}

	WaitForSingleObject(hEvent, INFINITE);
	CloseHandle(hEvent);


	hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, EVENT_BY_SVC);
	if (hEvent == NULL)
	{
		cout << "EVENT_BY_SVC Open failed : " << GetLastError() << endl;
		return;
	}

	getchar();
	SetEvent(hEvent);
	CloseHandle(hEvent);

	cout << "======= End AtlCliAuth Test ==========" << endl;
}

