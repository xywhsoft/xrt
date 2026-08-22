#include <stdio.h>
#include <xruntime.h>



/* 返回一个整数。 */
static bool answer(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	xvalue* pValue = xrtValueInt((int64)(intptr_t)pEnvironment);

	(void)pFrame;
	if ( pValue == NULL ) {
		return false;
	}
	if ( !xrtCallResultPushTake(pResult, &pValue) ) {
		xrtValueRelease(pValue);
		return false;
	}
	return true;
}



int main(void)
{
	xrtcallable* pCallable = xrtCallableCreate(
		NULL, answer, (ptr)(intptr_t)42, NULL);
	xvalue* pFunction = xrtValueCallableTake(&pCallable);
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	int64 iValue;
	int iResult = 0;

	if (
		(pFunction == NULL) ||
		!xrtValueInvoke(pFunction, NULL, &Result) ||
		!xrtValueGetInt(xrtCallResultGet(&Result, 0u), &iValue)
	) {
		iResult = 1;
	} else {
		printf("answer=%lld\n", (long long)iValue);
	}
	xrtCallResultUnit(&Result);
	xrtValueRelease(pFunction);
	return iResult;
}
