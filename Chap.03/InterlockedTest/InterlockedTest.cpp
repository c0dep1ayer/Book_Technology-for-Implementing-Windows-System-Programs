#include "stdafx.h"
#include "Windows.h"
#include "iostream"
using namespace std;


#define IN_VALUE	0x01234567
void _tmain()
{
	LONG target = 12;
	LONG orgVal, retVal;

	target = orgVal = 12;
	retVal = InterlockedExchange(&target, 5);
	printf("InterlockedExchange => "
		"orgVal=%d, retVal=%d, target=%d\n", orgVal, retVal, target);

	target = orgVal = 125;
	retVal = InterlockedCompareExchange(&target, 384, 125);
	printf("InterlockedCompareExchange cmp 125 => "
		"orgVal=%d, retVal=%d, target=%d\n", orgVal, retVal, target);

	target = orgVal = 8;
	retVal = InterlockedCompareExchange(&target, 384, 125);
	printf("InterlockedCompareExchange cmp 125 => "
		"orgVal=%d, retVal=%d, target=%d\n", orgVal, retVal, target);

	target = orgVal = 12;
	retVal = InterlockedExchangeAdd(&target, 5);
	printf("InterlockedExchangeAdd with 5 => "
		"orgVal=%d, retVal=%d, target=%d\n", orgVal, retVal, target);
	target = orgVal = 12;
	retVal = InterlockedExchangeAdd(&target, -25);
	printf("InterlockedExchangeAdd with -25 => "
		"orgVal=%d, retVal=%d, target=%d\n", orgVal, retVal, target);

	target = orgVal = 12;
	retVal = InterlockedIncrement(&target);
	printf("InterlockedIncrement => "
		"orgVal=%d, retVal=%d, target=%d\n", orgVal, retVal, target);
	target = orgVal = 12;
	retVal = InterlockedDecrement(&target);
	printf("InterlockedDecrement => "
		"orgVal=%d, retVal=%d, target=%d\n", orgVal, retVal, target);

	target = orgVal = 0x01234567;
	retVal = InterlockedAnd(&target, 0xFF);
	printf("InterlockedAnd => "
		"orgVal=0x%08x, retVal=0x%08x, target=0x%08x\n", orgVal, retVal, target);
	target = orgVal = 0x01234567;
	retVal = InterlockedOr(&target, 0xFF);
	printf("InterlockedOr => "
		"orgVal=0x%08x, retVal=0x%08x, target=0x%08x\n", orgVal, retVal, target);
	target = orgVal = 0x01234567;
	retVal = InterlockedXor(&target, 0xFF);
	printf("InterlockedXor => "
		"orgVal=0x%08x, retVal=0x%08x, target=0x%08x\n", orgVal, retVal, target);


	char bitBuff[33] = { 0, };

	target = orgVal = 0x01234567;
	for (int i = 31; i >= 0; i--)
	{
		BOOL val = InterlockedBitTestAndSet(&target, i);
		bitBuff[31 - i] = (val ? '1' : '0') ;
	}
	printf("InterlockedBitTestAndSet => %s\n\t"
		"orgVal=0x%08x, retVal=0x%08x, target=0x%08x\n", bitBuff, orgVal, retVal, target);
	target = orgVal = 0x01234567;
	for (int i = 31; i >= 0; i--)
	{
		BOOL val = InterlockedBitTestAndReset(&target, i);
		bitBuff[31 - i] = (val ? '1' : '0');
	}
	printf("InterlockedBitTestAndReset => %s\n\t"
		"orgVal=0x%08x, retVal=0x%08x, target=0x%08x\n", bitBuff, orgVal, retVal, target);
}
