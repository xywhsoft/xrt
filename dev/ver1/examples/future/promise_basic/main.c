/*
 * XRT Example - Promise Basic
 * XRT 范例 - Promise 基础
 *
 * Description / 说明:
 *   EN: Demonstrates manual future/promise creation, resolve, wait, and result inspection.
 *   CN: 演示手动创建 future/promise，以及 resolve、wait 和结果读取。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/future_promise_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/future_promise_basic -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


int main(void)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = NULL;
	xfuture_result tResult;
	int iValue = 42;
	int iRet = 0;

	xrtInit();
	memset(&tResult, 0, sizeof(tResult));

	pFuture = xFutureCreate();
	pPromise = xPromiseCreate(pFuture);
	if ( pFuture == NULL || pPromise == NULL ) {
		printf("create = failed\n");
		iRet = 1;
		goto cleanup;
	}

	(void)xFutureSetDebugName(pFuture, (str)"future.promise_basic");
	(void)xPromiseResolve(pPromise, &iValue);

	if ( !xFutureWaitTimeout(pFuture, 1000) ) {
		printf("wait = timeout\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xFutureGetResult(pFuture, &tResult) ) {
		printf("get_result = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("debug_name = %s\n", (const char*)xFutureGetDebugName(pFuture));
	printf("status = %d\n", (int)tResult.iStatus);
	printf("state = %d\n", (int)xFutureState(pFuture));
	printf("value = %d\n", *(int*)tResult.pValue);
	printf("error = %s\n", (const char*)tResult.sError);

cleanup:
	if ( pPromise != NULL ) {
		xPromiseDestroy(pPromise);
	}
	if ( pFuture != NULL ) {
		xFutureRelease(pFuture);
	}
	xrtUnit();
	return iRet;
}
