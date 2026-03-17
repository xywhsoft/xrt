#include "../xrt.h"

static int32 __Test_FutureCore_ThreadTask(ptr pArg, xfuture_result* pOut)
{
	int* pValue = (int*)pArg;

	if ( pOut == NULL || pValue == NULL ) {
		return XRT_NET_ERROR;
	}

	pOut->pValue = pValue;
	return XRT_NET_OK;
}

typedef struct {
	xpromise* pPromise;
	int* pValue;
	uint32 iDelayMs;
	int32 iStatus;
	str sError;
} __test_future_resolve_ctx;

static int32 __Test_FutureCore_ResolveThreadProc(ptr pArg, xfuture_result* pOut)
{
	__test_future_resolve_ctx* pCtx = (__test_future_resolve_ctx*)pArg;

	if ( pOut == NULL || pCtx == NULL || pCtx->pPromise == NULL ) {
		return XRT_NET_ERROR;
	}
	if ( pCtx->iDelayMs > 0 ) {
		xrtSleep(pCtx->iDelayMs);
	}
	if ( pCtx->iStatus == XRT_NET_OK ) {
		(void)xPromiseResolve(pCtx->pPromise, pCtx->pValue);
	} else if ( pCtx->iStatus == XRT_NET_CANCELLED ) {
		(void)xPromiseCancel(pCtx->pPromise, pCtx->sError);
	} else if ( pCtx->iStatus == XRT_NET_CLOSED ) {
		(void)xPromiseClose(pCtx->pPromise, pCtx->sError);
	} else {
		(void)xPromiseReject(pCtx->pPromise, pCtx->iStatus, pCtx->sError);
	}
	return XRT_NET_OK;
}

static int32 __Test_FutureCore_ThenProc(const xfuture_result* pIn, ptr pArg, xfuture_result* pOut)
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

static int32 __Test_FutureCore_CatchProc(const xfuture_result* pIn, ptr pArg, xfuture_result* pOut)
{
	int* pDst = (int*)pArg;

	if ( pIn == NULL || pOut == NULL || pDst == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( pIn->iStatus == XRT_NET_OK ) {
		return XRT_NET_ERROR;
	}

	*pDst = 7788;
	pOut->pValue = pDst;
	return XRT_NET_OK;
}

static void __Test_FutureCore_FinallyProc(const xfuture_result* pIn, ptr pArg)
{
	int* pHit = (int*)pArg;

	(void)pIn;
	if ( pHit ) {
		(*pHit)++;
	}
}

#if !defined(XRT_NO_COROUTINE)
static int32 __Test_FutureCore_CoTask(ptr pArg, xfuture_result* pOut)
{
	int* pValue = (int*)pArg;

	if ( pOut == NULL || pValue == NULL ) {
		return XRT_NET_ERROR;
	}

	pOut->pValue = pValue;
	return XRT_NET_OK;
}
#endif

static int __Test_FutureCore_Run(void)
{
	xfuture* pFuture = NULL;
	xfuture* pFutureRef = NULL;
	xpromise* pPromise = NULL;
	xwaitsrc tSrc;
	xfuture_result tResult;
	int iValue = 20260317;
	ptr pWaitValue = NULL;
	ptr pWaitValue2 = NULL;
	bool bOk = true;

	memset(&tSrc, 0, sizeof(tSrc));
	memset(&tResult, 0, sizeof(tResult));

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		return 10;
	}

	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return 11;
	}

	if ( xFutureState(pFuture) != XFUTURE_PENDING ) {
		bOk = false;
	}
	if ( xFutureWaitTimeout(pFuture, 5) ) {
		bOk = false;
	}
	if ( xFutureStatus(pFuture) != XRT_NET_AGAIN ) {
		bOk = false;
	}

	if ( !xPromiseResolve(pPromise, &iValue) ) {
		bOk = false;
	}
	if ( xPromiseResolve(pPromise, NULL) ) {
		bOk = false;
	}

	if ( xFutureState(pFuture) != XFUTURE_RESOLVED ) {
		bOk = false;
	}
	if ( !xFutureWait(pFuture) ) {
		bOk = false;
	}
	if ( xFutureValue(pFuture) != &iValue ) {
		bOk = false;
	}
	if ( !xFutureGetResult(pFuture, &tResult) ) {
		bOk = false;
	}
	if ( tResult.iStatus != XRT_NET_OK || tResult.pValue != &iValue ) {
		bOk = false;
	}
	if ( xFutureGetCreateTimeMs(pFuture) == 0 || xFutureGetCompleteTimeMs(pFuture) < xFutureGetCreateTimeMs(pFuture) ) {
		bOk = false;
	}
	if ( !xFutureSetDebugName(pFuture, (str)"future-core-run") ) {
		bOk = false;
	}
	if ( strcmp((char*)xFutureGetDebugName(pFuture), "future-core-run") != 0 ) {
		bOk = false;
	}
	pFutureRef = xPromiseGetFuture(pPromise);
	if ( pFutureRef != pFuture || xPromisePeekFuture(pPromise) != pFuture ) {
		bOk = false;
	}

	tSrc = xWaitSourceFromFuture(pFuture);
	pWaitValue = xWaitSourceWaitValue(&tSrc);
	pWaitValue2 = xFutureWaitValue(pFuture);
	if ( pWaitValue != &iValue || pWaitValue2 != &iValue ) {
		bOk = false;
	}

	if ( pFutureRef ) {
		xFutureRelease(pFutureRef);
	}
	xPromiseDestroy(pPromise);
	xFutureRelease(pFuture);
	return bOk ? 0 : 12;
}

static int __Test_FutureCore_TaskRunThread(void)
{
	xfuture* pFuture = NULL;
	int iValue = 3101;
	bool bOk = true;

	pFuture = xTaskRunThread(__Test_FutureCore_ThreadTask, &iValue, 0);
	if ( pFuture == NULL ) {
		return 20;
	}

	if ( !xFutureWaitTimeout(pFuture, 1000) ) {
		bOk = false;
	}
	if ( xFutureValue(pFuture) != &iValue ) {
		bOk = false;
	}
	if ( xFutureStatus(pFuture) != XRT_NET_OK ) {
		bOk = false;
	}

	xFutureRelease(pFuture);
	return bOk ? 0 : 21;
}

static int __Test_FutureCore_Continuation(void)
{
	xfuture* pFuture = NULL;
	xfuture* pNext = NULL;
	xpromise* pPromise = NULL;
	int iBase = 100;
	int iThenValue = 0;
	int iCatchValue = 0;
	int iFinallyHit = 0;
	bool bOk = true;

	pFuture = xFutureCreate();
	pPromise = xPromiseCreate(pFuture);
	if ( pFuture == NULL || pPromise == NULL ) {
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pFuture ) xFutureRelease(pFuture);
		return 40;
	}
	(void)xPromiseResolve(pPromise, &iBase);
	pNext = xFutureThenInline(pFuture, __Test_FutureCore_ThenProc, &iThenValue);
	if ( pNext == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 41;
	}
	if ( !xFutureWait(pNext) || xFutureValue(pNext) != &iThenValue || iThenValue != 105 ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 411;
	}
	xFutureRelease(pNext);
	xPromiseDestroy(pPromise);
	xFutureRelease(pFuture);

	pFuture = xFutureCreate();
	pPromise = xPromiseCreate(pFuture);
	if ( pFuture == NULL || pPromise == NULL ) {
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pFuture ) xFutureRelease(pFuture);
		return 42;
	}
	(void)xPromiseReject(pPromise, XRT_NET_ERROR, (str)"reject");
	pNext = xFutureCatchInline(pFuture, __Test_FutureCore_CatchProc, &iCatchValue);
	if ( pNext == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 43;
	}
	if ( !xFutureWait(pNext) || xFutureValue(pNext) != &iCatchValue || iCatchValue != 7788 ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 431;
	}
	xFutureRelease(pNext);
	xPromiseDestroy(pPromise);
	xFutureRelease(pFuture);

	pFuture = xFutureCreate();
	pPromise = xPromiseCreate(pFuture);
	if ( pFuture == NULL || pPromise == NULL ) {
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pFuture ) xFutureRelease(pFuture);
		return 44;
	}
	iThenValue = 0;
	pNext = xFutureThenEngine(pFuture, NULL, 0, __Test_FutureCore_ThenProc, &iThenValue);
	if ( pNext == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 45;
	}
	(void)xPromiseResolve(pPromise, &iBase);
	if ( !xFutureWaitTimeout(pNext, 1000) ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 4511;
	}
	if ( xFutureValue(pNext) != &iThenValue || iThenValue != 105 ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 4512;
	}
	xFutureRelease(pNext);
	xPromiseDestroy(pPromise);
	xFutureRelease(pFuture);

	pFuture = xFutureCreate();
	pPromise = xPromiseCreate(pFuture);
	if ( pFuture == NULL || pPromise == NULL ) {
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pFuture ) xFutureRelease(pFuture);
		return 452;
	}
	iThenValue = 0;
	pNext = xFutureThenCurrent(pFuture, __Test_FutureCore_ThenProc, &iThenValue);
	if ( pNext == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 453;
	}
	(void)xPromiseResolve(pPromise, &iBase);
	if ( xFutureState(pNext) != XFUTURE_PENDING ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 454;
	}
	if ( xFuturePumpCurrentContinuations(1) != 1 ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 455;
	}
	if ( !xFutureWait(pNext) || xFutureValue(pNext) != &iThenValue || iThenValue != 105 ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 456;
	}
	xFutureRelease(pNext);
	xPromiseDestroy(pPromise);
	xFutureRelease(pFuture);

	pFuture = xFutureCreate();
	pPromise = xPromiseCreate(pFuture);
	if ( pFuture == NULL || pPromise == NULL ) {
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pFuture ) xFutureRelease(pFuture);
		return 457;
	}
	iThenValue = 0;
	pNext = xFutureThenCurrent(pFuture, __Test_FutureCore_ThenProc, &iThenValue);
	if ( pNext == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 458;
	}
	(void)xPromiseResolve(pPromise, &iBase);
	if ( !xFutureWaitTimeout(pNext, 1000) || xFutureValue(pNext) != &iThenValue || iThenValue != 105 ) {
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return 459;
	}
	xFutureRelease(pNext);
	xPromiseDestroy(pPromise);
	xFutureRelease(pFuture);

	#if !defined(XRT_NO_COROUTINE)
	{
		xcosched* pSched = xrtCoSchedCreate();
		if ( pSched == NULL ) {
			return 46;
		}
		pFuture = xFutureCreate();
		pPromise = xPromiseCreate(pFuture);
		if ( pFuture == NULL || pPromise == NULL ) {
			if ( pPromise ) xPromiseDestroy(pPromise);
			if ( pFuture ) xFutureRelease(pFuture);
			xrtCoSchedDestroy(pSched);
			return 47;
		}
		pNext = xFutureFinallyCo(pFuture, pSched, __Test_FutureCore_FinallyProc, &iFinallyHit, 0);
		if ( pNext == NULL ) {
			xPromiseDestroy(pPromise);
			xFutureRelease(pFuture);
			xrtCoSchedDestroy(pSched);
			return 48;
		}
		(void)xPromiseResolve(pPromise, &iBase);
		xrtCoSchedRun(pSched);
		if ( !xFutureWait(pNext) || xFutureValue(pNext) != &iBase || iFinallyHit != 1 ) {
			xFutureRelease(pNext);
			xPromiseDestroy(pPromise);
			xFutureRelease(pFuture);
			xrtCoSchedDestroy(pSched);
			return 481;
		}
		xFutureRelease(pNext);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		xrtCoSchedDestroy(pSched);
	}
	#endif

	return bOk ? 0 : 49;
}

typedef struct {
	xfuture* pFuture;
	int iStatus;
} __test_future_waiter_ctx;

static int32 __Test_FutureCore_WaiterProc(ptr pArg, xfuture_result* pOut)
{
	__test_future_waiter_ctx* pCtx = (__test_future_waiter_ctx*)pArg;

	if ( pCtx == NULL || pCtx->pFuture == NULL || pOut == NULL ) {
		return XRT_NET_ERROR;
	}

	pCtx->iStatus = xFutureWait(pCtx->pFuture) ? XRT_NET_OK : xFutureStatus(pCtx->pFuture);
	pOut->pValue = pCtx;
	return pCtx->iStatus;
}

static int __Test_FutureCore_MultiContinuationAndWaiter(void)
{
	xfuture* pSource = NULL;
	xpromise* pPromise = NULL;
	xfuture* pThen1 = NULL;
	xfuture* pThen2 = NULL;
	xfuture* pFinally = NULL;
	xfuture* pWaiter1 = NULL;
	xfuture* pWaiter2 = NULL;
	__test_future_waiter_ctx tWaiter1;
	__test_future_waiter_ctx tWaiter2;
	int iBase = 200;
	int iValue1 = 0;
	int iValue2 = 0;
	int iFinallyHit = 0;

	memset(&tWaiter1, 0, sizeof(tWaiter1));
	memset(&tWaiter2, 0, sizeof(tWaiter2));

	pSource = xFutureCreate();
	pPromise = xPromiseCreate(pSource);
	if ( pSource == NULL || pPromise == NULL ) {
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pSource ) xFutureRelease(pSource);
		return 57;
	}

	pThen1 = xFutureThenCurrent(pSource, __Test_FutureCore_ThenProc, &iValue1);
	pThen2 = xFutureThenCurrent(pSource, __Test_FutureCore_ThenProc, &iValue2);
	pFinally = xFutureFinallyCurrent(pSource, __Test_FutureCore_FinallyProc, &iFinallyHit);
	if ( pThen1 == NULL || pThen2 == NULL || pFinally == NULL ) {
		if ( pThen1 ) xFutureRelease(pThen1);
		if ( pThen2 ) xFutureRelease(pThen2);
		if ( pFinally ) xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 58;
	}
	if ( xFutureGetPendingContinuationCount(pSource) != 3 ) {
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 59;
	}

	tWaiter1.pFuture = pSource;
	tWaiter2.pFuture = pSource;
	pWaiter1 = xTaskRunThread(__Test_FutureCore_WaiterProc, &tWaiter1, 0);
	pWaiter2 = xTaskRunThread(__Test_FutureCore_WaiterProc, &tWaiter2, 0);
	if ( pWaiter1 == NULL || pWaiter2 == NULL ) {
		if ( pWaiter1 ) xFutureRelease(pWaiter1);
		if ( pWaiter2 ) xFutureRelease(pWaiter2);
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 60;
	}

	(void)xPromiseResolve(pPromise, &iBase);
	if ( xFuturePumpCurrentContinuations(0) != 3 ) {
		xFutureRelease(pWaiter1);
		xFutureRelease(pWaiter2);
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 61;
	}
	if ( xFutureGetPendingContinuationCount(pSource) != 0 ) {
		xFutureRelease(pWaiter1);
		xFutureRelease(pWaiter2);
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 62;
	}
	if ( !xFutureWaitTimeout(pWaiter1, 1000) || !xFutureWaitTimeout(pWaiter2, 1000) ) {
		xFutureRelease(pWaiter1);
		xFutureRelease(pWaiter2);
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 63;
	}
	if ( tWaiter1.iStatus != XRT_NET_OK || tWaiter2.iStatus != XRT_NET_OK ) {
		xFutureRelease(pWaiter1);
		xFutureRelease(pWaiter2);
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 64;
	}
	if ( !xFutureWait(pThen1) || !xFutureWait(pThen2) || !xFutureWait(pFinally) ) {
		xFutureRelease(pWaiter1);
		xFutureRelease(pWaiter2);
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 65;
	}
	if ( iValue1 != 205 || iValue2 != 205 || iFinallyHit != 1 ) {
		xFutureRelease(pWaiter1);
		xFutureRelease(pWaiter2);
		xFutureRelease(pThen1);
		xFutureRelease(pThen2);
		xFutureRelease(pFinally);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 66;
	}

	xFutureRelease(pWaiter1);
	xFutureRelease(pWaiter2);
	xFutureRelease(pThen1);
	xFutureRelease(pThen2);
	xFutureRelease(pFinally);
	xPromiseDestroy(pPromise);
	xFutureRelease(pSource);
	return 0;
}

static int __Test_FutureCore_Combinators(void)
{
	xfuture* pA = NULL;
	xfuture* pB = NULL;
	xfuture* pGroup = NULL;
	xpromise* pPromiseA = NULL;
	xpromise* pPromiseB = NULL;
	xfuture* arrFuture[2];
	int iValueA = 501;
	int iValueB = 502;

	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		return 60;
	}
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pGroup = xFutureWhenAny(arrFuture, 2);
	if ( pGroup == NULL ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 61;
	}
	(void)xPromiseResolve(pPromiseB, &iValueB);
	if ( !xFutureWaitTimeout(pGroup, 1000) || xFutureValue(pGroup) != &iValueB || xFutureStatus(pGroup) != XRT_NET_OK ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 611;
	}
	{
		xfuture* pHeldSource = xFutureGetGroupSource(pGroup);
		if ( xFutureGetGroupSourceIndex(pGroup) != 1 || pHeldSource != pB || xFuturePeekGroupSource(pGroup) != pB ) {
			if ( pHeldSource ) xFutureRelease(pHeldSource);
			xFutureRelease(pGroup);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 612;
		}
		xFutureRelease(pHeldSource);
	}
	xFutureRelease(pGroup);
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);

	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		return 62;
	}
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 63;
	}
	(void)xPromiseResolve(pPromiseA, &iValueA);
	(void)xPromiseResolve(pPromiseB, &iValueB);
	if ( !xFutureWaitTimeout(pGroup, 1000) || xFutureStatus(pGroup) != XRT_NET_OK ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 631;
	}
	if ( xFutureGetGroupSourceIndex(pGroup) != -1 || xFuturePeekGroupSource(pGroup) != NULL ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 632;
	}
	{
		const xfuture_all_value* pAll = xFuturePeekAllValue(pGroup);
		if ( pAll == NULL || xFutureGetAllValueCount(pGroup) != 2 || xFutureGetAllValueItem(pGroup, 0) != &iValueA || xFutureGetAllValueItem(pGroup, 1) != &iValueB ) {
			xFutureRelease(pGroup);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 633;
		}
		if ( pAll->iCount != 2 || pAll->arrValue == NULL || pAll->arrValue[0] != &iValueA || pAll->arrValue[1] != &iValueB ) {
			xFutureRelease(pGroup);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 634;
		}
	}
	xFutureRelease(pGroup);
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);

	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		return 64;
	}
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 65;
	}
	(void)xPromiseResolve(pPromiseA, &iValueA);
	(void)xPromiseReject(pPromiseB, XRT_NET_ERROR, (str)"all failed");
	{
		int iSpin = 0;
		while ( xFutureState(pGroup) == XFUTURE_PENDING && iSpin < 200 ) {
			xrtThreadYield();
			iSpin++;
		}
	}
	if ( xFutureState(pGroup) == XFUTURE_PENDING || xFutureStatus(pGroup) != XRT_NET_ERROR ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 651;
	}
	{
		xfuture* pHeldSource = xFutureGetGroupSource(pGroup);
		if ( xFutureGetGroupSourceIndex(pGroup) != 1 || pHeldSource != pB || xFuturePeekGroupSource(pGroup) != pB ) {
			if ( pHeldSource ) xFutureRelease(pHeldSource);
			xFutureRelease(pGroup);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 652;
		}
		xFutureRelease(pHeldSource);
	}
	xFutureRelease(pGroup);
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);

	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		return 64;
	}
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 65;
	}
	(void)xPromiseResolve(pPromiseA, &iValueA);
	if ( !xFutureWaitTimeout(pGroup, 1000) || xFutureValue(pGroup) != &iValueA || xFutureStatus(pGroup) != XRT_NET_OK ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 651;
	}
	{
		xfuture* pHeldSource = xFutureGetGroupSource(pGroup);
		if ( xFutureGetGroupSourceIndex(pGroup) != 0 || pHeldSource != pA || xFuturePeekGroupSource(pGroup) != pA ) {
			if ( pHeldSource ) xFutureRelease(pHeldSource);
			xFutureRelease(pGroup);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 653;
		}
		xFutureRelease(pHeldSource);
	}
	{
		int iSpin = 0;
		while ( xFutureState(pB) == XFUTURE_PENDING && iSpin < 200 ) {
			xrtThreadYield();
			iSpin++;
		}
	}
	if ( xFutureState(pB) != XFUTURE_CANCELLED ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 652;
	}
	xFutureRelease(pGroup);
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);

	return 0;
}

static int __Test_FutureCore_TaskGroup(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pA = NULL;
	xfuture* pB = NULL;
	xfuture* pPending = NULL;
	xpromise* pPromiseA = NULL;
	xpromise* pPromiseB = NULL;
	xpromise* pPendingPromise = NULL;
	int iValueA = 901;
	int iValueB = 902;
	int iSpin = 0;

	pGroup = xTaskGroupCreate();
	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	if ( pGroup == NULL || pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 70;
	}

	if ( !xTaskGroupAddFuture(pGroup, pA) || !xTaskGroupAddFuture(pGroup, pB) || xTaskGroupCount(pGroup) != 2 ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		return 71;
	}

	(void)xPromiseResolve(pPromiseA, &iValueA);
	(void)xPromiseResolve(pPromiseB, &iValueB);
	if ( !xTaskGroupWaitTimeout(pGroup, 1000) ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		return 72;
	}

	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);
	xTaskGroupDestroy(pGroup);

	pGroup = xTaskGroupCreate();
	pPending = xFutureCreate();
	pPendingPromise = xPromiseCreate(pPending);
	if ( pGroup == NULL || pPending == NULL || pPendingPromise == NULL ) {
		if ( pPendingPromise ) xPromiseDestroy(pPendingPromise);
		if ( pPending ) xFutureRelease(pPending);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 73;
	}

	if ( !xTaskGroupAddFuture(pGroup, pPending) ) {
		xPromiseDestroy(pPendingPromise);
		xFutureRelease(pPending);
		xTaskGroupDestroy(pGroup);
		return 74;
	}

	xTaskGroupCancel(pGroup);

	while ( xFutureState(pPending) == XFUTURE_PENDING && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pPending) != XFUTURE_CANCELLED ) {
		xPromiseDestroy(pPendingPromise);
		xFutureRelease(pPending);
		xTaskGroupDestroy(pGroup);
		return 76;
	}

	xPromiseDestroy(pPendingPromise);
	xFutureRelease(pPending);
	xTaskGroupDestroy(pGroup);
	return 0;
}

static int32 __Test_FutureCore_EngineTask(xnetworker* pWorker, ptr pArg, xfuture_result* pOut)
{
	int* pValue = (int*)pArg;

	(void)pWorker;
	if ( pOut == NULL || pValue == NULL ) {
		return XRT_NET_ERROR;
	}

	pOut->pValue = pValue;
	return XRT_NET_OK;
}

static int __Test_FutureCore_TaskGroupRun(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pThreadFuture = NULL;
	xfuture* pEngineFuture = NULL;
	int iThreadValue = 1101;
	int iEngineValue = 1102;

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		return 80;
	}

	pThreadFuture = xTaskGroupRunThread(pGroup, __Test_FutureCore_ThreadTask, &iThreadValue, 0);
	pEngineFuture = xTaskGroupRunEngine(pGroup, NULL, 0, __Test_FutureCore_EngineTask, &iEngineValue);
	if ( pThreadFuture == NULL || pEngineFuture == NULL ) {
		if ( pThreadFuture ) xFutureRelease(pThreadFuture);
		if ( pEngineFuture ) xFutureRelease(pEngineFuture);
		xTaskGroupDestroy(pGroup);
		return 81;
	}
	if ( xTaskGroupCount(pGroup) != 2 ) {
		xFutureRelease(pThreadFuture);
		xFutureRelease(pEngineFuture);
		xTaskGroupDestroy(pGroup);
		return 82;
	}
	if ( !xTaskGroupWaitTimeout(pGroup, 1000) ) {
		xFutureRelease(pThreadFuture);
		xFutureRelease(pEngineFuture);
		xTaskGroupDestroy(pGroup);
		return 83;
	}
	if ( xFutureValue(pThreadFuture) != &iThreadValue || xFutureValue(pEngineFuture) != &iEngineValue ) {
		xFutureRelease(pThreadFuture);
		xFutureRelease(pEngineFuture);
		xTaskGroupDestroy(pGroup);
		return 84;
	}

	xFutureRelease(pThreadFuture);
	xFutureRelease(pEngineFuture);
	xTaskGroupDestroy(pGroup);
	return 0;
}

static int __Test_FutureCore_TaskGroupScope(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pDone = NULL;
	xfuture* pPending = NULL;
	xfuture* pDestroyPending = NULL;
	xpromise* pDonePromise = NULL;
	xpromise* pPendingPromise = NULL;
	xpromise* pDestroyPendingPromise = NULL;
	int iValue = 1201;
	int iSpin = 0;

	pGroup = xTaskGroupCreate();
	pDone = xFutureCreate();
	pPending = xFutureCreate();
	pDonePromise = xPromiseCreate(pDone);
	pPendingPromise = xPromiseCreate(pPending);
	if ( pGroup == NULL || pDone == NULL || pPending == NULL || pDonePromise == NULL || pPendingPromise == NULL ) {
		if ( pDonePromise ) xPromiseDestroy(pDonePromise);
		if ( pPendingPromise ) xPromiseDestroy(pPendingPromise);
		if ( pDone ) xFutureRelease(pDone);
		if ( pPending ) xFutureRelease(pPending);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 90;
	}

	if ( !xTaskGroupAddFuture(pGroup, pDone) || !xTaskGroupAddFuture(pGroup, pPending) ) {
		xPromiseDestroy(pDonePromise);
		xPromiseDestroy(pPendingPromise);
		xFutureRelease(pDone);
		xFutureRelease(pPending);
		xTaskGroupDestroy(pGroup);
		return 91;
	}

	(void)xPromiseResolve(pDonePromise, &iValue);
	if ( xTaskGroupReapCompleted(pGroup) != 1 || xTaskGroupCount(pGroup) != 1 ) {
		xPromiseDestroy(pDonePromise);
		xPromiseDestroy(pPendingPromise);
		xFutureRelease(pDone);
		xFutureRelease(pPending);
		xTaskGroupDestroy(pGroup);
		return 92;
	}

	xTaskGroupClose(pGroup);
	if ( xTaskGroupAddFuture(pGroup, pDone) ) {
		xPromiseDestroy(pDonePromise);
		xPromiseDestroy(pPendingPromise);
		xFutureRelease(pDone);
		xFutureRelease(pPending);
		xTaskGroupDestroy(pGroup);
		return 93;
	}

	xPromiseDestroy(pDonePromise);
	xPromiseDestroy(pPendingPromise);
	xFutureRelease(pDone);
	xFutureRelease(pPending);
	xTaskGroupDestroy(pGroup);

	pGroup = xTaskGroupCreate();
	pDestroyPending = xFutureCreate();
	pDestroyPendingPromise = xPromiseCreate(pDestroyPending);
	if ( pGroup == NULL || pDestroyPending == NULL || pDestroyPendingPromise == NULL ) {
		if ( pDestroyPendingPromise ) xPromiseDestroy(pDestroyPendingPromise);
		if ( pDestroyPending ) xFutureRelease(pDestroyPending);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 94;
	}

	if ( !xTaskGroupAddFuture(pGroup, pDestroyPending) ) {
		xPromiseDestroy(pDestroyPendingPromise);
		xFutureRelease(pDestroyPending);
		xTaskGroupDestroy(pGroup);
		return 95;
	}

	xTaskGroupDestroy(pGroup);
	while ( xFutureState(pDestroyPending) == XFUTURE_PENDING && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pDestroyPending) != XFUTURE_CANCELLED ) {
		xPromiseDestroy(pDestroyPendingPromise);
		xFutureRelease(pDestroyPending);
		return 96;
	}

	xPromiseDestroy(pDestroyPendingPromise);
	xFutureRelease(pDestroyPending);
	return 0;
}

static int __Test_FutureCore_TaskGroupJoin(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pA = NULL;
	xfuture* pB = NULL;
	xfuture* pJoin = NULL;
	xpromise* pPromiseA = NULL;
	xpromise* pPromiseB = NULL;
	int iValueA = 1301;
	int iValueB = 1302;
	int iSpin = 0;

	pGroup = xTaskGroupCreate();
	pA = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	if ( pGroup == NULL || pA == NULL || pPromiseA == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pA ) xFutureRelease(pA);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 100;
	}

	if ( !xTaskGroupAddFuture(pGroup, pA) ) {
		xPromiseDestroy(pPromiseA);
		xFutureRelease(pA);
		xTaskGroupDestroy(pGroup);
		return 101;
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xPromiseDestroy(pPromiseA);
		xFutureRelease(pA);
		xTaskGroupDestroy(pGroup);
		return 102;
	}
	if ( xFutureState(pJoin) != XFUTURE_PENDING ) {
		xFutureRelease(pJoin);
		xPromiseDestroy(pPromiseA);
		xFutureRelease(pA);
		xTaskGroupDestroy(pGroup);
		return 103;
	}

	pB = xFutureCreate();
	pPromiseB = xPromiseCreate(pB);
	if ( pB == NULL || pPromiseB == NULL || !xTaskGroupAddFuture(pGroup, pB) || xTaskGroupCount(pGroup) != 2 ) {
		xFutureRelease(pJoin);
		xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		return 104;
	}

	(void)xPromiseResolve(pPromiseA, &iValueA);
	(void)xPromiseResolve(pPromiseB, &iValueB);
	while ( xFutureState(pJoin) == XFUTURE_PENDING && iSpin < 50 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pJoin) != XFUTURE_PENDING ) {
		xFutureRelease(pJoin);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		return 105;
	}

	xTaskGroupClose(pGroup);
	if ( !xFutureWaitTimeout(pJoin, 1000) ) {
		xFutureRelease(pJoin);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		return 106;
	}
	xFutureRelease(pJoin);
	if ( !xTaskGroupJoinTimeout(pGroup, 1000) || xTaskGroupCount(pGroup) != 0 ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		return 107;
	}

	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);
	xTaskGroupDestroy(pGroup);
	return 0;
}

static int __Test_FutureCore_TaskGroupParentCancel(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pParentA = NULL;
	xfuture* pParentB = NULL;
	xfuture* pChild = NULL;
	xpromise* pParentPromiseA = NULL;
	xpromise* pParentPromiseB = NULL;
	xpromise* pChildPromise = NULL;
	int iSpin = 0;

	pGroup = xTaskGroupCreate();
	pParentA = xFutureCreate();
	pParentB = xFutureCreate();
	pChild = xFutureCreate();
	pParentPromiseA = xPromiseCreate(pParentA);
	pParentPromiseB = xPromiseCreate(pParentB);
	pChildPromise = xPromiseCreate(pChild);
	if ( pGroup == NULL || pParentA == NULL || pParentB == NULL || pChild == NULL || pParentPromiseA == NULL || pParentPromiseB == NULL || pChildPromise == NULL ) {
		if ( pParentPromiseA ) xPromiseDestroy(pParentPromiseA);
		if ( pParentPromiseB ) xPromiseDestroy(pParentPromiseB);
		if ( pChildPromise ) xPromiseDestroy(pChildPromise);
		if ( pParentA ) xFutureRelease(pParentA);
		if ( pParentB ) xFutureRelease(pParentB);
		if ( pChild ) xFutureRelease(pChild);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 110;
	}

	if ( !xTaskGroupAddFuture(pGroup, pChild) ) {
		xPromiseDestroy(pParentPromiseA);
		xPromiseDestroy(pParentPromiseB);
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pParentA);
		xFutureRelease(pParentB);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
		return 111;
	}
	if ( !xTaskGroupBindParent(pGroup, pParentA) ) {
		xPromiseDestroy(pParentPromiseA);
		xPromiseDestroy(pParentPromiseB);
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pParentA);
		xFutureRelease(pParentB);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
		return 112;
	}
	if ( !xTaskGroupBindParent(pGroup, pParentB) ) {
		xPromiseDestroy(pParentPromiseA);
		xPromiseDestroy(pParentPromiseB);
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pParentA);
		xFutureRelease(pParentB);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
		return 113;
	}

	(void)xPromiseCancel(pParentPromiseB, (str)"parent cancel");
	while ( xFutureState(pChild) == XFUTURE_PENDING && iSpin < 400 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pChild) != XFUTURE_CANCELLED ) {
		xPromiseDestroy(pParentPromiseA);
		xPromiseDestroy(pParentPromiseB);
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pParentA);
		xFutureRelease(pParentB);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
		return 114;
	}

	xPromiseDestroy(pParentPromiseA);
	xPromiseDestroy(pParentPromiseB);
	xPromiseDestroy(pChildPromise);
	xFutureRelease(pParentA);
	xFutureRelease(pParentB);
	xFutureRelease(pChild);
	xTaskGroupDestroy(pGroup);
	return 0;
}

static int __Test_FutureCore_TaskGroupNestedScope(void)
{
	xtaskgroup* pParent = NULL;
	xtaskgroup* pChild = NULL;
	xfuture* pLeaf = NULL;
	xfuture* pParentJoin = NULL;
	xpromise* pLeafPromise = NULL;
	int iSpin = 0;

	pParent = xTaskGroupCreate();
	if ( pParent == NULL ) {
		return 115;
	}

	pChild = xTaskGroupCreateChild(pParent);
	if ( pChild == NULL ) {
		xTaskGroupDestroy(pParent);
		return 116;
	}

	pLeaf = xFutureCreate();
	pLeafPromise = xPromiseCreate(pLeaf);
	if ( pLeaf == NULL || pLeafPromise == NULL ) {
		if ( pLeafPromise ) xPromiseDestroy(pLeafPromise);
		if ( pLeaf ) xFutureRelease(pLeaf);
		xTaskGroupDestroy(pChild);
		xTaskGroupDestroy(pParent);
		return 117;
	}

	if ( !xTaskGroupAddFuture(pChild, pLeaf) ) {
		xPromiseDestroy(pLeafPromise);
		xFutureRelease(pLeaf);
		xTaskGroupDestroy(pChild);
		xTaskGroupDestroy(pParent);
		return 118;
	}

	pParentJoin = xTaskGroupJoinFuture(pParent);
	if ( pParentJoin == NULL ) {
		xPromiseDestroy(pLeafPromise);
		xFutureRelease(pLeaf);
		xTaskGroupDestroy(pChild);
		xTaskGroupDestroy(pParent);
		return 119;
	}

	xTaskGroupClose(pParent);
	while ( xFutureState(pLeaf) == XFUTURE_PENDING && iSpin < 400 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pLeaf) != XFUTURE_CANCELLED ) {
		xFutureRelease(pParentJoin);
		xPromiseDestroy(pLeafPromise);
		xFutureRelease(pLeaf);
		xTaskGroupDestroy(pChild);
		xTaskGroupDestroy(pParent);
		return 120;
	}

	if ( !xFutureWaitTimeout(pParentJoin, 1000) ) {
		xFutureRelease(pParentJoin);
		xPromiseDestroy(pLeafPromise);
		xFutureRelease(pLeaf);
		xTaskGroupDestroy(pChild);
		xTaskGroupDestroy(pParent);
		return 121;
	}

	xFutureRelease(pParentJoin);
	xPromiseDestroy(pLeafPromise);
	xFutureRelease(pLeaf);
	xTaskGroupDestroy(pChild);
	xTaskGroupDestroy(pParent);
	return 0;
}

static int __Test_FutureCore_ContinuationStress(void)
{
	int i;

	for ( i = 0; i < 64; ++i ) {
		xfuture* pSource = NULL;
		xfuture* pThen = NULL;
		xpromise* pPromise = NULL;
		int iBase = 1000 + i;
		int iOut = 0;

		pSource = xFutureCreate();
		pPromise = xPromiseCreate(pSource);
		if ( pSource == NULL || pPromise == NULL ) {
			if ( pPromise ) xPromiseDestroy(pPromise);
			if ( pSource ) xFutureRelease(pSource);
			return 122;
		}

		pThen = xFutureThenCurrent(pSource, __Test_FutureCore_ThenProc, &iOut);
		if ( pThen == NULL ) {
			xPromiseDestroy(pPromise);
			xFutureRelease(pSource);
			return 123;
		}
		if ( !xPromiseResolve(pPromise, &iBase) ) {
			xFutureRelease(pThen);
			xPromiseDestroy(pPromise);
			xFutureRelease(pSource);
			return 124;
		}
		if ( !xFutureWaitTimeout(pThen, 1000) ) {
			xFutureRelease(pThen);
			xPromiseDestroy(pPromise);
			xFutureRelease(pSource);
			return 125;
		}
		if ( iOut != (iBase + 5) ) {
			xFutureRelease(pThen);
			xPromiseDestroy(pPromise);
			xFutureRelease(pSource);
			return 126;
		}
		if ( xFutureGetPendingContinuationCount(pSource) != 0 ) {
			xFutureRelease(pThen);
			xPromiseDestroy(pPromise);
			xFutureRelease(pSource);
			return 127;
		}

		xFutureRelease(pThen);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
	}

	return 0;
}

static int __Test_FutureCore_TaskGroupBatch(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pJoin = NULL;
	int arrValue[16];
	xfuture* arrFuture[16];
	int i;

	memset(arrValue, 0, sizeof(arrValue));
	memset(arrFuture, 0, sizeof(arrFuture));

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		return 128;
	}

	for ( i = 0; i < 16; ++i ) {
		arrValue[i] = 3000 + i;
		arrFuture[i] = xTaskGroupRunThread(pGroup, __Test_FutureCore_ThreadTask, &arrValue[i], 0);
		if ( arrFuture[i] == NULL ) {
			for ( i = 0; i < 16; ++i ) {
				if ( arrFuture[i] ) xFutureRelease(arrFuture[i]);
			}
			xTaskGroupDestroy(pGroup);
			return 129;
		}
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		for ( i = 0; i < 16; ++i ) {
			xFutureRelease(arrFuture[i]);
		}
		xTaskGroupDestroy(pGroup);
		return 130;
	}

	xTaskGroupClose(pGroup);
	if ( !xFutureWaitTimeout(pJoin, 2000) ) {
		xFutureRelease(pJoin);
		for ( i = 0; i < 16; ++i ) {
			xFutureRelease(arrFuture[i]);
		}
		xTaskGroupDestroy(pGroup);
		return 131;
	}
	if ( !xTaskGroupJoinTimeout(pGroup, 2000) ) {
		xFutureRelease(pJoin);
		for ( i = 0; i < 16; ++i ) {
			xFutureRelease(arrFuture[i]);
		}
		xTaskGroupDestroy(pGroup);
		return 132;
	}
	if ( xTaskGroupCount(pGroup) != 0 ) {
		xFutureRelease(pJoin);
		for ( i = 0; i < 16; ++i ) {
			xFutureRelease(arrFuture[i]);
		}
		xTaskGroupDestroy(pGroup);
		return 133;
	}

	xFutureRelease(pJoin);
	for ( i = 0; i < 16; ++i ) {
		xFutureRelease(arrFuture[i]);
	}
	xTaskGroupDestroy(pGroup);
	return 0;
}

static int __Test_FutureCore_CombinatorRaceStress(void)
{
	int i;

	for ( i = 0; i < 32; ++i ) {
		xfuture* pA = NULL;
		xfuture* pB = NULL;
		xpromise* pPromiseA = NULL;
		xpromise* pPromiseB = NULL;
		xfuture* pAny = NULL;
		xfuture* pRace = NULL;
		xfuture* pTaskA = NULL;
		xfuture* pTaskB = NULL;
		xfuture* arrFuture[2];
		__test_future_resolve_ctx tCtxA;
		__test_future_resolve_ctx tCtxB;
		int iValueA = 4000 + i;
		int iValueB = 5000 + i;
		int iIndex = (i & 1);

		memset(&tCtxA, 0, sizeof(tCtxA));
		memset(&tCtxB, 0, sizeof(tCtxB));

		pA = xFutureCreate();
		pB = xFutureCreate();
		pPromiseA = xPromiseCreate(pA);
		pPromiseB = xPromiseCreate(pB);
		if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
			if ( pPromiseA ) xPromiseDestroy(pPromiseA);
			if ( pPromiseB ) xPromiseDestroy(pPromiseB);
			if ( pA ) xFutureRelease(pA);
			if ( pB ) xFutureRelease(pB);
			return 134;
		}

		arrFuture[0] = pA;
		arrFuture[1] = pB;
		pAny = xFutureWhenAny(arrFuture, 2);
		pRace = xFutureRace(arrFuture, 2);
		if ( pAny == NULL || pRace == NULL ) {
			if ( pAny ) xFutureRelease(pAny);
			if ( pRace ) xFutureRelease(pRace);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 135;
		}

		tCtxA.pPromise = pPromiseA;
		tCtxA.pValue = &iValueA;
		tCtxA.iDelayMs = (iIndex == 0) ? 0 : 20;
		tCtxA.iStatus = XRT_NET_OK;
		tCtxB.pPromise = pPromiseB;
		tCtxB.pValue = &iValueB;
		tCtxB.iDelayMs = (iIndex == 0) ? 20 : 0;
		tCtxB.iStatus = XRT_NET_OK;

		pTaskA = xTaskRunThread(__Test_FutureCore_ResolveThreadProc, &tCtxA, 0);
		pTaskB = xTaskRunThread(__Test_FutureCore_ResolveThreadProc, &tCtxB, 0);
		if ( pTaskA == NULL || pTaskB == NULL ) {
			if ( pTaskA ) xFutureRelease(pTaskA);
			if ( pTaskB ) xFutureRelease(pTaskB);
			xFutureRelease(pAny);
			xFutureRelease(pRace);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 136;
		}

		if ( !xFutureWaitTimeout(pAny, 2000) || !xFutureWaitTimeout(pRace, 2000) ) {
			xFutureRelease(pTaskA);
			xFutureRelease(pTaskB);
			xFutureRelease(pAny);
			xFutureRelease(pRace);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 137;
		}
		if ( xFutureGetGroupSourceIndex(pAny) != iIndex || xFutureGetGroupSourceIndex(pRace) != iIndex ) {
			xFutureRelease(pTaskA);
			xFutureRelease(pTaskB);
			xFutureRelease(pAny);
			xFutureRelease(pRace);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 138;
		}
		if ( (iIndex == 0 && (xFuturePeekGroupSource(pAny) != pA || xFuturePeekGroupSource(pRace) != pA)) ||
			(iIndex == 1 && (xFuturePeekGroupSource(pAny) != pB || xFuturePeekGroupSource(pRace) != pB)) ) {
			xFutureRelease(pTaskA);
			xFutureRelease(pTaskB);
			xFutureRelease(pAny);
			xFutureRelease(pRace);
			xPromiseDestroy(pPromiseA);
			xPromiseDestroy(pPromiseB);
			xFutureRelease(pA);
			xFutureRelease(pB);
			return 139;
		}

		xFutureRelease(pTaskA);
		xFutureRelease(pTaskB);
		xFutureRelease(pAny);
		xFutureRelease(pRace);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
	}

	return 0;
}

static int __Test_FutureCore_TaskGroupParentMultiCancelStress(void)
{
	int i;

	for ( i = 0; i < 16; ++i ) {
		xtaskgroup* pGroup = NULL;
		xfuture* pParentA = NULL;
		xfuture* pParentB = NULL;
		xfuture* pChild = NULL;
		xpromise* pParentPromiseA = NULL;
		xpromise* pParentPromiseB = NULL;
		xpromise* pChildPromise = NULL;
		int iSpin = 0;

		pGroup = xTaskGroupCreate();
		pParentA = xFutureCreate();
		pParentB = xFutureCreate();
		pChild = xFutureCreate();
		pParentPromiseA = xPromiseCreate(pParentA);
		pParentPromiseB = xPromiseCreate(pParentB);
		pChildPromise = xPromiseCreate(pChild);
		if ( pGroup == NULL || pParentA == NULL || pParentB == NULL || pChild == NULL || pParentPromiseA == NULL || pParentPromiseB == NULL || pChildPromise == NULL ) {
			if ( pParentPromiseA ) xPromiseDestroy(pParentPromiseA);
			if ( pParentPromiseB ) xPromiseDestroy(pParentPromiseB);
			if ( pChildPromise ) xPromiseDestroy(pChildPromise);
			if ( pParentA ) xFutureRelease(pParentA);
			if ( pParentB ) xFutureRelease(pParentB);
			if ( pChild ) xFutureRelease(pChild);
			if ( pGroup ) xTaskGroupDestroy(pGroup);
			return 140;
		}
		if ( !xTaskGroupAddFuture(pGroup, pChild) || !xTaskGroupBindParent(pGroup, pParentA) || !xTaskGroupBindParent(pGroup, pParentB) ) {
			xPromiseDestroy(pParentPromiseA);
			xPromiseDestroy(pParentPromiseB);
			xPromiseDestroy(pChildPromise);
			xFutureRelease(pParentA);
			xFutureRelease(pParentB);
			xFutureRelease(pChild);
			xTaskGroupDestroy(pGroup);
			return 141;
		}

		if ( (i & 1) == 0 ) {
			(void)xPromiseCancel(pParentPromiseA, (str)"parent a cancel");
			(void)xPromiseCancel(pParentPromiseB, (str)"parent b cancel");
		} else {
			(void)xPromiseCancel(pParentPromiseB, (str)"parent b cancel");
			(void)xPromiseCancel(pParentPromiseA, (str)"parent a cancel");
		}
		while ( xFutureState(pChild) == XFUTURE_PENDING && iSpin < 2000 ) {
			xrtThreadYield();
			iSpin++;
		}
		if ( xFutureState(pChild) != XFUTURE_CANCELLED ) {
			xPromiseDestroy(pParentPromiseA);
			xPromiseDestroy(pParentPromiseB);
			xPromiseDestroy(pChildPromise);
			xFutureRelease(pParentA);
			xFutureRelease(pParentB);
			xFutureRelease(pChild);
			xTaskGroupDestroy(pGroup);
			return 142;
		}

		xPromiseDestroy(pParentPromiseA);
		xPromiseDestroy(pParentPromiseB);
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pParentA);
		xFutureRelease(pParentB);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
	}

	return 0;
}

static int __Test_FutureCore_TaskGroupDestroyPending(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pChild = NULL;
	xpromise* pChildPromise = NULL;
	int iSpin = 0;

	pGroup = xTaskGroupCreate();
	pChild = xFutureCreate();
	pChildPromise = xPromiseCreate(pChild);
	if ( pGroup == NULL || pChild == NULL || pChildPromise == NULL ) {
		if ( pChildPromise ) xPromiseDestroy(pChildPromise);
		if ( pChild ) xFutureRelease(pChild);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 145;
	}
	if ( !xTaskGroupAddFuture(pGroup, pChild) ) {
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
		return 146;
	}

	xTaskGroupDestroy(pGroup);
	while ( xFutureState(pChild) == XFUTURE_PENDING && iSpin < 2000 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pChild) != XFUTURE_CANCELLED ) {
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pChild);
		return 147;
	}

	xPromiseDestroy(pChildPromise);
	xFutureRelease(pChild);
	return 0;
}

static int __Test_FutureCore_TaskGroupAddAfterClose(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pChild = NULL;
	xpromise* pChildPromise = NULL;
	bool bAddOk = false;

	pGroup = xTaskGroupCreate();
	pChild = xFutureCreate();
	pChildPromise = xPromiseCreate(pChild);
	if ( pGroup == NULL || pChild == NULL || pChildPromise == NULL ) {
		if ( pChildPromise ) xPromiseDestroy(pChildPromise);
		if ( pChild ) xFutureRelease(pChild);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		return 148;
	}

	xTaskGroupClose(pGroup);
	bAddOk = xTaskGroupAddFuture(pGroup, pChild);
	if ( bAddOk ) {
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
		return 149;
	}
	if ( xTaskGroupCount(pGroup) != 0 ) {
		xPromiseDestroy(pChildPromise);
		xFutureRelease(pChild);
		xTaskGroupDestroy(pGroup);
		return 150;
	}

	xPromiseDestroy(pChildPromise);
	xFutureRelease(pChild);
	xTaskGroupDestroy(pGroup);
	return 0;
}

static int __Test_FutureCore_OwnershipLifetime(void)
{
	xfuture* pSource = NULL;
	xfuture* pHeldFromPromise = NULL;
	xfuture* pA = NULL;
	xfuture* pB = NULL;
	xfuture* pAny = NULL;
	xfuture* pHeldSource = NULL;
	xpromise* pPromise = NULL;
	xpromise* pPromiseA = NULL;
	xpromise* pPromiseB = NULL;
	xfuture* arrFuture[2];
	int iValue = 6101;
	int iValueA = 6201;
	int iValueB = 6202;

	pSource = xFutureCreate();
	pPromise = xPromiseCreate(pSource);
	if ( pSource == NULL || pPromise == NULL ) {
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pSource ) xFutureRelease(pSource);
		return 151;
	}

	pHeldFromPromise = xPromiseGetFuture(pPromise);
	if ( pHeldFromPromise == NULL || xPromisePeekFuture(pPromise) != pSource ) {
		if ( pHeldFromPromise ) xFutureRelease(pHeldFromPromise);
		xPromiseDestroy(pPromise);
		xFutureRelease(pSource);
		return 152;
	}

	(void)xPromiseResolve(pPromise, &iValue);
	xFutureRelease(pSource);
	if ( !xFutureWaitTimeout(pHeldFromPromise, 1000) || xFutureValue(pHeldFromPromise) != &iValue ) {
		xFutureRelease(pHeldFromPromise);
		xPromiseDestroy(pPromise);
		return 153;
	}
	xFutureRelease(pHeldFromPromise);
	xPromiseDestroy(pPromise);

	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		return 154;
	}

	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pAny = xFutureWhenAny(arrFuture, 2);
	if ( pAny == NULL ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 155;
	}

	(void)xPromiseResolve(pPromiseB, &iValueB);
	(void)xPromiseResolve(pPromiseA, &iValueA);
	if ( !xFutureWaitTimeout(pAny, 1000) ) {
		xFutureRelease(pAny);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 156;
	}

	pHeldSource = xFutureGetGroupSource(pAny);
	if ( pHeldSource == NULL || xFuturePeekGroupSource(pAny) != pB || xFutureGetGroupSourceIndex(pAny) != 1 ) {
		if ( pHeldSource ) xFutureRelease(pHeldSource);
		xFutureRelease(pAny);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 157;
	}

	xFutureRelease(pAny);
	xFutureRelease(pA);
	xFutureRelease(pB);
	if ( !xFutureWaitTimeout(pHeldSource, 1000) || xFutureValue(pHeldSource) != &iValueB ) {
		xFutureRelease(pHeldSource);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		return 158;
	}

	xFutureRelease(pHeldSource);
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	return 0;
}

#if !defined(XRT_NO_COROUTINE)
static int __Test_FutureCore_TaskRunCo(void)
{
	xfuture* pFuture = NULL;
	xcosched* pSched = NULL;
	int iValue = 3102;
	bool bOk = true;

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		return 30;
	}

	pFuture = xTaskRunCo(pSched, __Test_FutureCore_CoTask, &iValue, 0);
	if ( pFuture == NULL ) {
		xrtCoSchedDestroy(pSched);
		return 31;
	}

	xrtCoSchedRun(pSched);
	if ( !xFutureWait(pFuture) ) {
		bOk = false;
	}
	if ( xFutureValue(pFuture) != &iValue ) {
		bOk = false;
	}
	if ( xFutureStatus(pFuture) != XRT_NET_OK ) {
		bOk = false;
	}

	xFutureRelease(pFuture);
	xrtCoSchedDestroy(pSched);
	return bOk ? 0 : 32;
}
#endif

static int __Test_FutureCore_RunAll(void)
{
	int iRet = 0;

	iRet = __Test_FutureCore_Run();
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskRunThread();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_Continuation();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_MultiContinuationAndWaiter();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_Combinators();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroup();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupRun();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupScope();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupJoin();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupParentCancel();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupNestedScope();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_ContinuationStress();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupBatch();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_CombinatorRaceStress();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupParentMultiCancelStress();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupDestroyPending();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskGroupAddAfterClose();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_OwnershipLifetime();
	}
	#if !defined(XRT_NO_COROUTINE)
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskRunCo();
	}
	#endif

	return iRet;
}

#if defined(XRT_FUTURE_CORE_EMBEDDED)
void Test_FutureCore(xrtGlobalData* xCore)
{
	int iRet;

	(void)xCore;
	iRet = __Test_FutureCore_RunAll();
	if ( iRet == 0 ) {
		printf("Future Core Test : PASS\n");
	} else {
		printf("Future Core Test : FAIL (%d)\n", iRet);
	}
}
#else
int main(void)
{
	xrtGlobalData* pCore;
	int iRet = 0;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	iRet = __Test_FutureCore_RunAll();

	xrtUnit();
	return iRet;
}
#endif
