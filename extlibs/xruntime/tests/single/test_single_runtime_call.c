#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 单头文件入口返回一个静态布尔值。 */
static bool testSingleCallEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	(void)pFrame;
	return xrtCallResultPush(pResult, xrtValueBool(true));
}



int main(void)
{
	xrtcallable* pCallable = xrtCallableCreate(
		NULL, testSingleCallEntry, NULL, NULL);
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	bool bValue = false;
	int iResult = 0;

	if (
		(pCallable == NULL) ||
		!xrtTypeValidate(xrtTypeCallable()) ||
		(xrtTypeCallable()->Kind != XRT_TYPE_CALLABLE) ||
		!xrtCallableInvoke(pCallable, NULL, &Result) ||
		(xrtCallResultCount(&Result) != 1u) ||
		!xrtValueGetBool(xrtCallResultGet(&Result, 0u), &bValue) ||
		!bValue
	) {
		iResult = 1;
	}
	xrtCallResultUnit(&Result);
	xrtCallableUnref(pCallable);
	return iResult;
}
