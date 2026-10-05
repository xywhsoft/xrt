#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"



/* 单头文件 callable 不返回值。 */
static bool testSingleValueCall(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	(void)pFrame;
	(void)pResult;
	return true;
}



int main(void)
{
	xrtcallable* pCallable = xrtCallableCreate(
		NULL, testSingleValueCall, NULL, NULL);
	xvalue* pValue = xrtValueCallableTake(&pCallable);
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	int iResult = (pValue == NULL) || (pCallable != NULL) ||
		!xrtValueInvoke(pValue, NULL, &Result);

	xrtCallResultUnit(&Result);
	xrtValueRelease(pValue);
	return iResult;
}
