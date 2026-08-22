/*
 * XRT Example - Future Continuation Chain
 * XRT 范例 - Future 延续链
 *
 * Description / 说明:
 *   EN: Demonstrates Then/Catch/Finally continuations in the current-thread pipeline.
 *   CN: 演示当前线程上下文中的 Then/Catch/Finally 延续链。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/future_continuation_chain.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/future_continuation_chain -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static int32 ThenAddFive(const xfuture_result* pIn, ptr pArg, xfuture_result* pOut)
{
	int* pBase = NULL;
	int* pDst = (int*)pArg;

	if ( pIn == NULL || pOut == NULL || pDst == NULL ) {
		return XRT_NET_ERROR;
	}

	pBase = (int*)pIn->pValue;
	if ( pBase == NULL ) {
		return XRT_NET_ERROR;
	}

	*pDst = *pBase + 5;
	pOut->pValue = pDst;
	return XRT_NET_OK;
}


static int32 CatchRecover(const xfuture_result* pIn, ptr pArg, xfuture_result* pOut)
{
	int* pDst = (int*)pArg;

	if ( pIn == NULL || pOut == NULL || pDst == NULL ) {
		return XRT_NET_ERROR;
	}

	*pDst = 777;
	pOut->pValue = pDst;
	return XRT_NET_OK;
}


static void FinallyMark(const xfuture_result* pIn, ptr pArg)
{
	int* pHit = (int*)pArg;

	(void)pIn;
	if ( pHit != NULL ) {
		(*pHit)++;
	}
}


static void PumpCurrentAll(void)
{
	while ( xFuturePumpCurrentContinuations(16) > 0 ) {
	}
}


int main(void)
{
	xfuture* pSource = NULL;
	xpromise* pPromise = NULL;
	xfuture* pThen = NULL;
	xfuture* pFinally = NULL;
	xfuture* pErrorSource = NULL;
	xpromise* pErrorPromise = NULL;
	xfuture* pCatch = NULL;
	int iBase = 100;
	int iThenValue = 0;
	int iRecovered = 0;
	int iFinallyHit = 0;
	int iRet = 0;

	xrtInit();

	pSource = xFutureCreate();
	pPromise = xPromiseCreate(pSource);
	pThen = xFutureThenCurrent(pSource, ThenAddFive, &iThenValue);
	pFinally = xFutureFinallyCurrent(pSource, FinallyMark, &iFinallyHit);
	if ( pSource == NULL || pPromise == NULL || pThen == NULL || pFinally == NULL ) {
		printf("success_chain = failed\n");
		iRet = 1;
		goto cleanup;
	}

	(void)xPromiseResolve(pPromise, &iBase);
	printf("pending_before_pump = %d\n", xFutureGetPendingContinuationCount(pSource));
	PumpCurrentAll();

	if ( !xFutureWaitTimeout(pThen, 1000) || !xFutureWaitTimeout(pFinally, 1000) ) {
		printf("success_chain_wait = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	printf("then_status = %d\n", (int)xFutureStatus(pThen));
	printf("then_value = %d\n", *(int*)xFutureValue(pThen));
	printf("finally_hits = %d\n", iFinallyHit);

	pErrorSource = xFutureCreate();
	pErrorPromise = xPromiseCreate(pErrorSource);
	pCatch = xFutureCatchCurrent(pErrorSource, CatchRecover, &iRecovered);
	if ( pErrorSource == NULL || pErrorPromise == NULL || pCatch == NULL ) {
		printf("error_chain = failed\n");
		iRet = 1;
		goto cleanup;
	}

	(void)xPromiseReject(pErrorPromise, XRT_NET_ERROR, (str)"demo reject");
	PumpCurrentAll();
	if ( !xFutureWaitTimeout(pCatch, 1000) ) {
		printf("catch_wait = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	printf("source_error = %s\n", (const char*)xFutureError(pErrorSource));
	printf("catch_status = %d\n", (int)xFutureStatus(pCatch));
	printf("catch_value = %d\n", *(int*)xFutureValue(pCatch));

cleanup:
	if ( pCatch != NULL ) {
		xFutureRelease(pCatch);
	}
	if ( pErrorPromise != NULL ) {
		xPromiseDestroy(pErrorPromise);
	}
	if ( pErrorSource != NULL ) {
		xFutureRelease(pErrorSource);
	}
	if ( pFinally != NULL ) {
		xFutureRelease(pFinally);
	}
	if ( pThen != NULL ) {
		xFutureRelease(pThen);
	}
	if ( pPromise != NULL ) {
		xPromiseDestroy(pPromise);
	}
	if ( pSource != NULL ) {
		xFutureRelease(pSource);
	}
	xrtUnit();
	return iRet;
}
