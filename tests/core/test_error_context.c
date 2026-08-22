#include "../../src/internal/xrt_internal.h"

#include "../test.h"



/* 验证线程错误和调度器绑定的执行上下文互不污染。 */
int main(void)
{
	xrt_error_context tFirst;
	xrt_error_context tSecond;
	xrt_error_context* pPrevious;
	xerror* pThreadError;
	xerror* pFirstError;
	xerror* pSecondError;

	memset(&tFirst, 0, sizeof(tFirst));
	memset(&tSecond, 0, sizeof(tSecond));
	pThreadError = xrtErrorCreate(XERR_STATE, "test.thread", 1, "thread error");
	pFirstError = xrtErrorCreate(XERR_IO, "test.first", 2, "first error");
	pSecondError = xrtErrorCreate(XERR_TIMEOUT, "test.second", 3, "second error");
	testRequire((pThreadError != NULL) && (pFirstError != NULL) && (pSecondError != NULL), "error allocation failed");

	xrtSetError(pThreadError);
	pPrevious = __xrtErrorContextSwap(&tFirst);
	testRequire((pPrevious == NULL) && (xrtGetError() == NULL), "first context initial state mismatch");
	xrtSetError(pFirstError);
	testRequire(xrtGetError() == pFirstError, "first context error mismatch");

	pPrevious = __xrtErrorContextSwap(&tSecond);
	testRequire((pPrevious == &tFirst) && (xrtGetError() == NULL), "second context initial state mismatch");
	xrtSetError(pSecondError);
	testRequire(xrtGetError() == pSecondError, "second context error mismatch");

	pPrevious = __xrtErrorContextSwap(&tFirst);
	testRequire((pPrevious == &tSecond) && (xrtGetError() == pFirstError), "first context restore mismatch");
	pPrevious = __xrtErrorContextSwap(NULL);
	testRequire((pPrevious == &tFirst) && (xrtGetError() == pThreadError), "thread context restore mismatch");

	xrtErrorFree(pThreadError);
	xrtErrorFree(pFirstError);
	xrtErrorFree(pSecondError);
	xrtClearError();
	__xrtErrorContextSwap(&tFirst);
	xrtClearError();
	__xrtErrorContextSwap(&tSecond);
	xrtClearError();
	__xrtErrorContextSwap(NULL);
	printf("[PASS] error-context\n");
	return 0;
}
