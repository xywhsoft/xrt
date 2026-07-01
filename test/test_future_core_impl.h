#include "../xrt.h"

// 内部函数：__Test_FutureCore_ThreadTask
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


// 内部函数：__Test_FutureCore_ResolveThreadProc
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


// 内部函数：__Test_FutureCore_ThenProc
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


// 内部函数：__Test_FutureCore_CatchProc
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


// 内部函数：__Test_FutureCore_FinallyProc
static void __Test_FutureCore_FinallyProc(const xfuture_result* pIn, ptr pArg)
{
	int* pHit = (int*)pArg;

	(void)pIn;
	if ( pHit ) {
		(*pHit)++;
	}
}

static int __Test_FutureCore_RaceOwnedResultBorrow(void)
{
	xfuture* pSource = NULL;
	xfuture* pLoser = NULL;
	xfuture* pRace = NULL;
	xpromise* pSourcePromise = NULL;
	xpromise* pLoserPromise = NULL;
	xfuture* arrFuture[2];
	char* sOwned = NULL;
	char* sSourceValue = NULL;
	int iRet = 0;

	pSource = xFutureCreate();
	pLoser = xFutureCreate();
	pSourcePromise = xPromiseCreate(pSource);
	pLoserPromise = xPromiseCreate(pLoser);
	if ( pSource == NULL || pLoser == NULL || pSourcePromise == NULL || pLoserPromise == NULL ) {
		iRet = 860;
		goto cleanup;
	}

	sOwned = (char*)malloc(32);
	if ( sOwned == NULL ) {
		iRet = 861;
		goto cleanup;
	}
	strcpy(sOwned, "race-owned-source");

	arrFuture[0] = pSource;
	arrFuture[1] = pLoser;
	pRace = xFutureRace(arrFuture, 2);
	if ( pRace == NULL ) {
		iRet = 862;
		goto cleanup;
	}
	if ( !xPromiseResolveOwned(pSourcePromise, sOwned) ) {
		sOwned = NULL;
		iRet = 863;
		goto cleanup;
	}
	sOwned = NULL;
	if ( !xFutureWaitTimeout(pRace, 1000) ) {
		iRet = 864;
		goto cleanup;
	}
	if ( xFutureStatus(pRace) != XRT_NET_OK || xFutureValue(pRace) != xFutureValue(pSource) ) {
		iRet = 865;
		goto cleanup;
	}
	if ( xFutureValueIsOwned(pRace) ) {
		// 回归保护：race 结果只能借用源值。若这里失败，先清掉 owned 标记避免测试清理时重复释放。
		pRace->iResultFlags &= ~XFUTURE_RESULT_F_OWN_VALUE;
		pRace->pfnFreeValue = NULL;
		iRet = 866;
		goto cleanup;
	}

	sSourceValue = (char*)xFutureValue(pSource);
	if ( sSourceValue == NULL || strcmp(sSourceValue, "race-owned-source") != 0 ) {
		iRet = 867;
		goto cleanup;
	}

	xFutureRelease(pRace);
	pRace = NULL;

	sSourceValue = (char*)xFutureValue(pSource);
	if ( sSourceValue == NULL || strcmp(sSourceValue, "race-owned-source") != 0 ) {
		iRet = 868;
		goto cleanup;
	}

cleanup:
	if ( sOwned ) { free(sOwned); }
	if ( pRace ) { xFutureRelease(pRace); }
	if ( pSourcePromise ) { xPromiseDestroy(pSourcePromise); }
	if ( pLoserPromise ) { xPromiseDestroy(pLoserPromise); }
	if ( pSource ) { xFutureRelease(pSource); }
	if ( pLoser ) { xFutureRelease(pLoser); }
	return iRet;
}

#if !defined(XRT_NO_COROUTINE)
// 内部函数：__Test_FutureCore_CoTask
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

// 内部函数：__Test_FutureCore_Run
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


// 内部函数：__Test_FutureCore_TaskRunThread
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


static int __Test_FutureCore_TypeDesc(void)
{
	xfuture* pFuture = NULL;
	xfuture* pStored = NULL;
	xfuture* pUnboxed = NULL;
	xtarray pArray = NULL;
	xvalue pBox = NULL;
	xpromise* pPromise = NULL;
	int iValue = 20260628;
	bool bOk = true;

	pFuture = xFutureCreate();
	pPromise = xPromiseCreate(pFuture);
	pArray = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);
	if ( pFuture == NULL || pPromise == NULL || pArray == NULL ) {
		if ( pBox ) xvoUnref(pBox);
		if ( pArray ) xrtTypedArrayDestroy(pArray);
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pFuture ) xFutureRelease(pFuture);
		return 30;
	}

	if ( xrtTypeFuture()->Kind != XRT_TYPE_KIND_FUTURE || strcmp(xrtTypeName(xrtTypeFuture()), "future") != 0 ) {
		bOk = false;
	}
	if ( xrtTypedArrayAppend(pArray, &pFuture) == NULL ) {
		bOk = false;
	}

	// 验证 typed container 通过 future 类型描述持有引用。
	xFutureRelease(pFuture);
	pFuture = NULL;
	pStored = (xfuture*)*(xfuture**)xrtTypedArrayGet(pArray, 1);
	if ( pStored == NULL ) {
		bOk = false;
	} else {
		(void)xPromiseResolve(pPromise, &iValue);
		if ( !xFutureWait(pStored) || xFutureValue(pStored) != &iValue ) {
			bOk = false;
		}
	}

	if ( pStored != NULL ) {
		pBox = xrtTypeBoxValue(xrtTypeFuture(), &pStored);
		if ( pBox == NULL || !xrtTypeSame(xvoTypeDesc(pBox), xrtTypeFuture()) ) {
			bOk = false;
		}
		if ( !xrtTypeUnboxValue(xrtTypeFuture(), pBox, &pUnboxed) || pUnboxed != pStored ) {
			bOk = false;
		}
		if ( pUnboxed != NULL ) {
			xFutureRelease(pUnboxed);
		}
	}

	if ( pBox ) xvoUnref(pBox);
	xrtTypedArrayDestroy(pArray);
	xPromiseDestroy(pPromise);
	return bOk ? 0 : 31;
}


// 内部函数：__Test_FutureCore_Continuation
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


// 内部函数：__Test_FutureCore_WaiterProc
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


// 内部函数：__Test_FutureCore_MultiContinuationAndWaiter
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


// 内部函数：__Test_FutureCore_Combinators
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

	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		return 66;
	}
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 661;
	}
	if ( !xFutureRequestCancel(pGroup) ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 662;
	}
	{
		int iSpin = 0;
		while ( (xFutureState(pA) == XFUTURE_PENDING || xFutureState(pB) == XFUTURE_PENDING) && iSpin < 200 ) {
			xrtThreadYield();
			iSpin++;
		}
	}
	if ( xFutureState(pGroup) != XFUTURE_CANCELLED || xFutureState(pA) != XFUTURE_CANCELLED || xFutureState(pB) != XFUTURE_CANCELLED ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 663;
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
		return 67;
	}
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 671;
	}
	if ( !xFutureRequestCancel(pGroup) ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 672;
	}
	{
		int iSpin = 0;
		while ( (xFutureState(pA) == XFUTURE_PENDING || xFutureState(pB) == XFUTURE_PENDING) && iSpin < 200 ) {
			xrtThreadYield();
			iSpin++;
		}
	}
	if ( xFutureState(pGroup) != XFUTURE_CANCELLED || xFutureState(pA) != XFUTURE_CANCELLED || xFutureState(pB) != XFUTURE_CANCELLED ) {
		xFutureRelease(pGroup);
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 673;
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
		return 68;
	}
	if ( !xFutureForwardCancelTo(pA, pB) ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 681;
	}
	if ( !xFutureRequestCancel(pA) ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 682;
	}
	{
		int iSpin = 0;
		while ( xFutureState(pB) == XFUTURE_PENDING && iSpin < 200 ) {
			xrtThreadYield();
			iSpin++;
		}
	}
	if ( xFutureState(pA) != XFUTURE_CANCELLED || xFutureState(pB) != XFUTURE_CANCELLED ) {
		xPromiseDestroy(pPromiseA);
		xPromiseDestroy(pPromiseB);
		xFutureRelease(pA);
		xFutureRelease(pB);
		return 683;
	}
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);

	return 0;
}


// 内部函数：__Test_FutureCore_TaskGroup
static int __Test_FutureCore_RaceCompletedFirstMultiLoser(void)
{
	xfuture* pA = NULL;
	xfuture* pB = NULL;
	xfuture* pC = NULL;
	xfuture* pRace = NULL;
	xpromise* pPromiseA = NULL;
	xpromise* pPromiseB = NULL;
	xpromise* pPromiseC = NULL;
	xfuture* arrFuture[3];
	int iValueA = 6901;
	int iValueB = 6902;
	int iValueC = 6903;
	int iRet = 0;
	int iSpin = 0;

	pA = xFutureCreate();
	pB = xFutureCreate();
	pC = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	pPromiseC = xPromiseCreate(pC);
	if ( pA == NULL || pB == NULL || pC == NULL || pPromiseA == NULL || pPromiseB == NULL || pPromiseC == NULL ) {
		iRet = 690;
		goto cleanup;
	}

	arrFuture[0] = pA;
	arrFuture[1] = pB;
	arrFuture[2] = pC;
	pRace = xFutureRace(arrFuture, 3);
	if ( pRace == NULL ) {
		iRet = 692;
		goto cleanup;
	}
	if ( !xPromiseResolve(pPromiseA, &iValueA) ) {
		iRet = 691;
		goto cleanup;
	}
	if ( xPromiseResolve(pPromiseB, &iValueB) || xPromiseResolve(pPromiseC, &iValueC) ) {
		iRet = 696;
		goto cleanup;
	}
	if ( !xFutureWaitTimeout(pRace, 1000) ) {
		iRet = 693;
		goto cleanup;
	}
	if ( xFutureStatus(pRace) != XRT_NET_OK || xFutureValue(pRace) != &iValueA ) {
		iRet = 694;
		goto cleanup;
	}

	while ( (xFutureState(pB) == XFUTURE_PENDING || xFutureState(pC) == XFUTURE_PENDING) && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pB) != XFUTURE_CANCELLED || xFutureState(pC) != XFUTURE_CANCELLED ) {
		iRet = 695;
		goto cleanup;
	}

cleanup:
	if ( pRace ) xFutureRelease(pRace);
	if ( pPromiseA ) xPromiseDestroy(pPromiseA);
	if ( pPromiseB ) xPromiseDestroy(pPromiseB);
	if ( pPromiseC ) xPromiseDestroy(pPromiseC);
	if ( pA ) xFutureRelease(pA);
	if ( pB ) xFutureRelease(pB);
	if ( pC ) xFutureRelease(pC);
	return iRet;
}


// 内部函数：__Test_FutureCore_RaceFailureCancelLoser
static int __Test_FutureCore_RaceFailureCancelLoser(void)
{
	xfuture* pWinner = NULL;
	xfuture* pLoser = NULL;
	xfuture* pRace = NULL;
	xpromise* pWinnerPromise = NULL;
	xpromise* pLoserPromise = NULL;
	xfuture* arrFuture[2];
	int iWinnerValue = 8101;
	int iLoserValue = 8102;
	int iRet = 0;
	int iSpin = 0;

	// 已失败 source 先完成，race 必须失败并取消仍 pending 的 loser。
	pWinner = xFutureCreate();
	pLoser = xFutureCreate();
	pWinnerPromise = xPromiseCreate(pWinner);
	pLoserPromise = xPromiseCreate(pLoser);
	if ( pWinner == NULL || pLoser == NULL || pWinnerPromise == NULL || pLoserPromise == NULL ) {
		iRet = 810;
		goto cleanup;
	}
	if ( !xPromiseReject(pWinnerPromise, XRT_NET_ERROR, (str)"race completed reject") ) {
		iRet = 811;
		goto cleanup;
	}
	arrFuture[0] = pWinner;
	arrFuture[1] = pLoser;
	pRace = xFutureRace(arrFuture, 2);
	if ( pRace == NULL ) {
		iRet = 812;
		goto cleanup;
	}
	iSpin = 0;
	while ( xFutureState(pRace) == XFUTURE_PENDING && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pRace) == XFUTURE_PENDING ) {
		iRet = 813;
		goto cleanup;
	}
	if ( xFutureStatus(pRace) != XRT_NET_ERROR ) {
		iRet = 815;
		goto cleanup;
	}
	if ( xFutureGetGroupSourceIndex(pRace) != 0 ) {
		iRet = 816;
		goto cleanup;
	}
	if ( xFuturePeekGroupSource(pRace) != pWinner ) {
		iRet = 817;
		goto cleanup;
	}
	while ( xFutureState(pLoser) == XFUTURE_PENDING && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pLoser) != XFUTURE_CANCELLED || xPromiseResolve(pLoserPromise, &iLoserValue) ) {
		iRet = 814;
		goto cleanup;
	}

	if ( pRace ) { xFutureRelease(pRace); pRace = NULL; }
	if ( pWinnerPromise ) { xPromiseDestroy(pWinnerPromise); pWinnerPromise = NULL; }
	if ( pLoserPromise ) { xPromiseDestroy(pLoserPromise); pLoserPromise = NULL; }
	if ( pWinner ) { xFutureRelease(pWinner); pWinner = NULL; }
	if ( pLoser ) { xFutureRelease(pLoser); pLoser = NULL; }

	// pending source 失败时，source-inline race 回调必须立即取消 loser。
	pWinner = xFutureCreate();
	pLoser = xFutureCreate();
	pWinnerPromise = xPromiseCreate(pWinner);
	pLoserPromise = xPromiseCreate(pLoser);
	if ( pWinner == NULL || pLoser == NULL || pWinnerPromise == NULL || pLoserPromise == NULL ) {
		iRet = 820;
		goto cleanup;
	}
	arrFuture[0] = pWinner;
	arrFuture[1] = pLoser;
	pRace = xFutureRace(arrFuture, 2);
	if ( pRace == NULL ) {
		iRet = 821;
		goto cleanup;
	}
	if ( !xPromiseReject(pWinnerPromise, XRT_NET_ERROR, (str)"race pending reject") ) {
		iRet = 822;
		goto cleanup;
	}
	if ( xPromiseResolve(pLoserPromise, &iLoserValue) ) {
		iRet = 823;
		goto cleanup;
	}
	iSpin = 0;
	while ( xFutureState(pRace) == XFUTURE_PENDING && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pRace) == XFUTURE_PENDING || xFutureStatus(pRace) != XRT_NET_ERROR || xFutureGetGroupSourceIndex(pRace) != 0 || xFuturePeekGroupSource(pRace) != pWinner ) {
		iRet = 824;
		goto cleanup;
	}
	if ( xFutureState(pLoser) != XFUTURE_CANCELLED ) {
		iRet = 825;
		goto cleanup;
	}

	if ( pRace ) { xFutureRelease(pRace); pRace = NULL; }
	if ( pWinnerPromise ) { xPromiseDestroy(pWinnerPromise); pWinnerPromise = NULL; }
	if ( pLoserPromise ) { xPromiseDestroy(pLoserPromise); pLoserPromise = NULL; }
	if ( pWinner ) { xFutureRelease(pWinner); pWinner = NULL; }
	if ( pLoser ) { xFutureRelease(pLoser); pLoser = NULL; }

	// 已取消 source 先完成，race 必须取消并取消仍 pending 的 loser。
	pWinner = xFutureCreate();
	pLoser = xFutureCreate();
	pWinnerPromise = xPromiseCreate(pWinner);
	pLoserPromise = xPromiseCreate(pLoser);
	if ( pWinner == NULL || pLoser == NULL || pWinnerPromise == NULL || pLoserPromise == NULL ) {
		iRet = 830;
		goto cleanup;
	}
	if ( !xPromiseCancel(pWinnerPromise, (str)"race completed cancel") ) {
		iRet = 831;
		goto cleanup;
	}
	arrFuture[0] = pWinner;
	arrFuture[1] = pLoser;
	pRace = xFutureRace(arrFuture, 2);
	if ( pRace == NULL ) {
		iRet = 832;
		goto cleanup;
	}
	iSpin = 0;
	while ( xFutureState(pLoser) == XFUTURE_PENDING && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pRace) == XFUTURE_PENDING || xFutureStatus(pRace) != XRT_NET_CANCELLED || xFutureGetGroupSourceIndex(pRace) != 0 || xFuturePeekGroupSource(pRace) != pWinner ) {
		iRet = 833;
		goto cleanup;
	}
	if ( xFutureState(pLoser) != XFUTURE_CANCELLED || xPromiseResolve(pLoserPromise, &iLoserValue) ) {
		iRet = 834;
		goto cleanup;
	}

	if ( pRace ) { xFutureRelease(pRace); pRace = NULL; }
	if ( pWinnerPromise ) { xPromiseDestroy(pWinnerPromise); pWinnerPromise = NULL; }
	if ( pLoserPromise ) { xPromiseDestroy(pLoserPromise); pLoserPromise = NULL; }
	if ( pWinner ) { xFutureRelease(pWinner); pWinner = NULL; }
	if ( pLoser ) { xFutureRelease(pLoser); pLoser = NULL; }

	// pending source 被取消时，race group 与 loser 都必须立即进入取消态。
	pWinner = xFutureCreate();
	pLoser = xFutureCreate();
	pWinnerPromise = xPromiseCreate(pWinner);
	pLoserPromise = xPromiseCreate(pLoser);
	if ( pWinner == NULL || pLoser == NULL || pWinnerPromise == NULL || pLoserPromise == NULL ) {
		iRet = 840;
		goto cleanup;
	}
	arrFuture[0] = pWinner;
	arrFuture[1] = pLoser;
	pRace = xFutureRace(arrFuture, 2);
	if ( pRace == NULL ) {
		iRet = 841;
		goto cleanup;
	}
	if ( !xPromiseCancel(pWinnerPromise, (str)"race pending cancel") ) {
		iRet = 842;
		goto cleanup;
	}
	if ( xPromiseResolve(pLoserPromise, &iLoserValue) ) {
		iRet = 843;
		goto cleanup;
	}
	iSpin = 0;
	while ( xFutureState(pRace) == XFUTURE_PENDING && iSpin < 200 ) {
		xrtThreadYield();
		iSpin++;
	}
	if ( xFutureState(pRace) == XFUTURE_PENDING || xFutureStatus(pRace) != XRT_NET_CANCELLED || xFutureGetGroupSourceIndex(pRace) != 0 || xFuturePeekGroupSource(pRace) != pWinner ) {
		iRet = 844;
		goto cleanup;
	}
	if ( xFutureState(pLoser) != XFUTURE_CANCELLED ) {
		iRet = 845;
		goto cleanup;
	}

cleanup:
	if ( pRace ) xFutureRelease(pRace);
	if ( pWinnerPromise ) xPromiseDestroy(pWinnerPromise);
	if ( pLoserPromise ) xPromiseDestroy(pLoserPromise);
	if ( pWinner ) xFutureRelease(pWinner);
	if ( pLoser ) xFutureRelease(pLoser);
	(void)iWinnerValue;
	return iRet;
}


static int __Test_FutureCore_RepeatedCancelState(void)
{
	xfuture* pPending = NULL;
	xfuture* pResolved = NULL;
	xfuture* pRejected = NULL;
	xpromise* pPendingPromise = NULL;
	xpromise* pResolvedPromise = NULL;
	xpromise* pRejectedPromise = NULL;
	int iValue = 7001;
	int iRet = 0;

	pPending = xFutureCreate();
	pPendingPromise = xPromiseCreate(pPending);
	if ( pPending == NULL || pPendingPromise == NULL ) {
		iRet = 700;
		goto cleanup;
	}
	if ( !xFutureRequestCancel(pPending) ) {
		iRet = 701;
		goto cleanup;
	}
	if ( !xFutureRequestCancel(pPending) ) {
		iRet = 702;
		goto cleanup;
	}
	if ( xFutureState(pPending) != XFUTURE_CANCELLED || xFutureStatus(pPending) != XRT_NET_CANCELLED ) {
		iRet = 703;
		goto cleanup;
	}
	if ( xPromiseResolve(pPendingPromise, &iValue) ) {
		iRet = 704;
		goto cleanup;
	}

	pResolved = xFutureCreate();
	pResolvedPromise = xPromiseCreate(pResolved);
	if ( pResolved == NULL || pResolvedPromise == NULL ) {
		iRet = 710;
		goto cleanup;
	}
	if ( !xPromiseResolve(pResolvedPromise, &iValue) ) {
		iRet = 711;
		goto cleanup;
	}
	if ( xFutureState(pResolved) != XFUTURE_RESOLVED || xFutureStatus(pResolved) != XRT_NET_OK ) {
		iRet = 712;
		goto cleanup;
	}
	if ( xFutureRequestCancel(pResolved) ) {
		iRet = 713;
		goto cleanup;
	}

	pRejected = xFutureCreate();
	pRejectedPromise = xPromiseCreate(pRejected);
	if ( pRejected == NULL || pRejectedPromise == NULL ) {
		iRet = 720;
		goto cleanup;
	}
	if ( !xPromiseReject(pRejectedPromise, XRT_NET_ERROR, (str)"reject") ) {
		iRet = 721;
		goto cleanup;
	}
	if ( xFutureState(pRejected) != XFUTURE_REJECTED || xFutureStatus(pRejected) != XRT_NET_ERROR ) {
		iRet = 722;
		goto cleanup;
	}
	if ( xFutureRequestCancel(pRejected) ) {
		iRet = 723;
		goto cleanup;
	}

cleanup:
	if ( pPendingPromise ) xPromiseDestroy(pPendingPromise);
	if ( pResolvedPromise ) xPromiseDestroy(pResolvedPromise);
	if ( pRejectedPromise ) xPromiseDestroy(pRejectedPromise);
	if ( pPending ) xFutureRelease(pPending);
	if ( pResolved ) xFutureRelease(pResolved);
	if ( pRejected ) xFutureRelease(pRejected);
	return iRet;
}


static int __Test_FutureCore_GroupDuplicateSource(void)
{
	xfuture* pSource = NULL;
	xfuture* pPendingAll = NULL;
	xfuture* pPendingRace = NULL;
	xfuture* pRejected = NULL;
	xfuture* pPendingRejectAll = NULL;
	xfuture* pPendingRejectRace = NULL;
	xfuture* pCancelled = NULL;
	xfuture* pPendingCancelAll = NULL;
	xfuture* pPendingCancelRace = NULL;
	xfuture* pGroup = NULL;
	xpromise* pPromise = NULL;
	xpromise* pPendingAllPromise = NULL;
	xpromise* pPendingRacePromise = NULL;
	xpromise* pRejectedPromise = NULL;
	xpromise* pPendingRejectAllPromise = NULL;
	xpromise* pPendingRejectRacePromise = NULL;
	xpromise* pCancelledPromise = NULL;
	xpromise* pPendingCancelAllPromise = NULL;
	xpromise* pPendingCancelRacePromise = NULL;
	xfuture* arrFuture[2];
	int iValue = 7301;
	int iPendingAllValue = 7302;
	int iPendingRaceValue = 7303;
	int iRet = 0;
	xfuture* pHeldSource = NULL;

	pSource = xFutureCreate();
	pPromise = xPromiseCreate(pSource);
	if ( pSource == NULL || pPromise == NULL ) {
		iRet = 730;
		goto cleanup;
	}
	if ( !xPromiseResolve(pPromise, &iValue) ) {
		iRet = 731;
		goto cleanup;
	}

	arrFuture[0] = pSource;
	arrFuture[1] = pSource;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 732;
		goto cleanup;
	}
	if ( !xFutureWaitTimeout(pGroup, 1000) ) {
		iRet = 733;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_OK || xFutureGetAllValueCount(pGroup) != 2 ) {
		iRet = 734;
		goto cleanup;
	}
	if ( xFutureGetAllValueItem(pGroup, 0) != &iValue || xFutureGetAllValueItem(pGroup, 1) != &iValue ) {
		iRet = 735;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 736;
		goto cleanup;
	}
	if ( !xFutureWaitTimeout(pGroup, 1000) ) {
		iRet = 737;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_OK || xFutureValue(pGroup) != &iValue ) {
		iRet = 738;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pSource || xFuturePeekGroupSource(pGroup) != pSource ) {
		iRet = 739;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pSource) != XFUTURE_RESOLVED || xFutureRequestCancel(pSource) ) {
		iRet = 740;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pPendingAll = xFutureCreate();
	pPendingAllPromise = xPromiseCreate(pPendingAll);
	if ( pPendingAll == NULL || pPendingAllPromise == NULL ) {
		iRet = 741;
		goto cleanup;
	}
	arrFuture[0] = pPendingAll;
	arrFuture[1] = pPendingAll;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 742;
		goto cleanup;
	}
	if ( !xPromiseResolve(pPendingAllPromise, &iPendingAllValue) ) {
		iRet = 743;
		goto cleanup;
	}
	if ( !xFutureWaitTimeout(pGroup, 1000) ) {
		iRet = 744;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_OK || xFutureGetAllValueCount(pGroup) != 2 ) {
		iRet = 745;
		goto cleanup;
	}
	if ( xFutureGetAllValueItem(pGroup, 0) != &iPendingAllValue || xFutureGetAllValueItem(pGroup, 1) != &iPendingAllValue ) {
		iRet = 746;
		goto cleanup;
	}
	if ( xFutureState(pPendingAll) != XFUTURE_RESOLVED || xFutureRequestCancel(pPendingAll) ) {
		iRet = 747;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pPendingRace = xFutureCreate();
	pPendingRacePromise = xPromiseCreate(pPendingRace);
	if ( pPendingRace == NULL || pPendingRacePromise == NULL ) {
		iRet = 748;
		goto cleanup;
	}
	arrFuture[0] = pPendingRace;
	arrFuture[1] = pPendingRace;
	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 749;
		goto cleanup;
	}
	if ( !xPromiseResolve(pPendingRacePromise, &iPendingRaceValue) ) {
		iRet = 750;
		goto cleanup;
	}
	if ( !xFutureWaitTimeout(pGroup, 1000) ) {
		iRet = 751;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_OK || xFutureValue(pGroup) != &iPendingRaceValue ) {
		iRet = 752;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pPendingRace || xFuturePeekGroupSource(pGroup) != pPendingRace ) {
		iRet = 753;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pPendingRace) != XFUTURE_RESOLVED || xFutureRequestCancel(pPendingRace) ) {
		iRet = 754;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pRejected = xFutureCreate();
	pRejectedPromise = xPromiseCreate(pRejected);
	if ( pRejected == NULL || pRejectedPromise == NULL ) {
		iRet = 755;
		goto cleanup;
	}
	if ( !xPromiseReject(pRejectedPromise, XRT_NET_ERROR, (str)"duplicate reject") ) {
		iRet = 756;
		goto cleanup;
	}
	arrFuture[0] = pRejected;
	arrFuture[1] = pRejected;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 757;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 758;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_ERROR || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 759;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pRejected || xFuturePeekGroupSource(pGroup) != pRejected ) {
		iRet = 760;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pRejected) != XFUTURE_REJECTED || xFutureRequestCancel(pRejected) ) {
		iRet = 761;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 762;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 763;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_ERROR || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 764;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pRejected || xFuturePeekGroupSource(pGroup) != pRejected ) {
		iRet = 765;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pRejected) != XFUTURE_REJECTED || xFutureRequestCancel(pRejected) ) {
		iRet = 766;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pPendingRejectAll = xFutureCreate();
	pPendingRejectAllPromise = xPromiseCreate(pPendingRejectAll);
	if ( pPendingRejectAll == NULL || pPendingRejectAllPromise == NULL ) {
		iRet = 767;
		goto cleanup;
	}
	arrFuture[0] = pPendingRejectAll;
	arrFuture[1] = pPendingRejectAll;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 768;
		goto cleanup;
	}
	if ( !xPromiseReject(pPendingRejectAllPromise, XRT_NET_ERROR, (str)"pending duplicate reject all") ) {
		iRet = 769;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 770;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_ERROR || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 771;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pPendingRejectAll || xFuturePeekGroupSource(pGroup) != pPendingRejectAll ) {
		iRet = 772;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pPendingRejectAll) != XFUTURE_REJECTED || xFutureRequestCancel(pPendingRejectAll) ) {
		iRet = 773;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pPendingRejectRace = xFutureCreate();
	pPendingRejectRacePromise = xPromiseCreate(pPendingRejectRace);
	if ( pPendingRejectRace == NULL || pPendingRejectRacePromise == NULL ) {
		iRet = 774;
		goto cleanup;
	}
	arrFuture[0] = pPendingRejectRace;
	arrFuture[1] = pPendingRejectRace;
	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 775;
		goto cleanup;
	}
	if ( !xPromiseReject(pPendingRejectRacePromise, XRT_NET_ERROR, (str)"pending duplicate reject race") ) {
		iRet = 776;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 777;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_ERROR || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 778;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pPendingRejectRace || xFuturePeekGroupSource(pGroup) != pPendingRejectRace ) {
		iRet = 779;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pPendingRejectRace) != XFUTURE_REJECTED || xFutureRequestCancel(pPendingRejectRace) ) {
		iRet = 780;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pCancelled = xFutureCreate();
	pCancelledPromise = xPromiseCreate(pCancelled);
	if ( pCancelled == NULL || pCancelledPromise == NULL ) {
		iRet = 781;
		goto cleanup;
	}
	if ( !xPromiseCancel(pCancelledPromise, (str)"duplicate cancel") ) {
		iRet = 782;
		goto cleanup;
	}
	arrFuture[0] = pCancelled;
	arrFuture[1] = pCancelled;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 783;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 784;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_CANCELLED || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 785;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pCancelled || xFuturePeekGroupSource(pGroup) != pCancelled ) {
		iRet = 786;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pCancelled) != XFUTURE_CANCELLED || !xFutureRequestCancel(pCancelled) ) {
		iRet = 787;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 788;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 789;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_CANCELLED || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 790;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pCancelled || xFuturePeekGroupSource(pGroup) != pCancelled ) {
		iRet = 791;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pCancelled) != XFUTURE_CANCELLED || !xFutureRequestCancel(pCancelled) ) {
		iRet = 792;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pPendingCancelAll = xFutureCreate();
	pPendingCancelAllPromise = xPromiseCreate(pPendingCancelAll);
	if ( pPendingCancelAll == NULL || pPendingCancelAllPromise == NULL ) {
		iRet = 793;
		goto cleanup;
	}
	arrFuture[0] = pPendingCancelAll;
	arrFuture[1] = pPendingCancelAll;
	pGroup = xFutureWhenAll(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 794;
		goto cleanup;
	}
	if ( !xPromiseCancel(pPendingCancelAllPromise, (str)"pending duplicate cancel all") ) {
		iRet = 795;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 796;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_CANCELLED || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 797;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pPendingCancelAll || xFuturePeekGroupSource(pGroup) != pPendingCancelAll ) {
		iRet = 798;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pPendingCancelAll) != XFUTURE_CANCELLED || !xFutureRequestCancel(pPendingCancelAll) ) {
		iRet = 799;
		goto cleanup;
	}
	xFutureRelease(pGroup);
	pGroup = NULL;

	pPendingCancelRace = xFutureCreate();
	pPendingCancelRacePromise = xPromiseCreate(pPendingCancelRace);
	if ( pPendingCancelRace == NULL || pPendingCancelRacePromise == NULL ) {
		iRet = 800;
		goto cleanup;
	}
	arrFuture[0] = pPendingCancelRace;
	arrFuture[1] = pPendingCancelRace;
	pGroup = xFutureRace(arrFuture, 2);
	if ( pGroup == NULL ) {
		iRet = 801;
		goto cleanup;
	}
	if ( !xPromiseCancel(pPendingCancelRacePromise, (str)"pending duplicate cancel race") ) {
		iRet = 802;
		goto cleanup;
	}
	if ( xrtNetFutureWait(pGroup, 1000) == XRT_NET_TIMEOUT || xFutureState(pGroup) == XFUTURE_PENDING ) {
		iRet = 803;
		goto cleanup;
	}
	if ( xFutureStatus(pGroup) != XRT_NET_CANCELLED || xFutureGetGroupSourceIndex(pGroup) != 0 ) {
		iRet = 804;
		goto cleanup;
	}
	pHeldSource = xFutureGetGroupSource(pGroup);
	if ( pHeldSource != pPendingCancelRace || xFuturePeekGroupSource(pGroup) != pPendingCancelRace ) {
		iRet = 805;
		goto cleanup;
	}
	xFutureRelease(pHeldSource);
	pHeldSource = NULL;
	if ( xFutureState(pPendingCancelRace) != XFUTURE_CANCELLED || !xFutureRequestCancel(pPendingCancelRace) ) {
		iRet = 806;
		goto cleanup;
	}

cleanup:
	if ( pHeldSource ) xFutureRelease(pHeldSource);
	if ( pGroup ) xFutureRelease(pGroup);
	if ( pPromise ) xPromiseDestroy(pPromise);
	if ( pPendingAllPromise ) xPromiseDestroy(pPendingAllPromise);
	if ( pPendingRacePromise ) xPromiseDestroy(pPendingRacePromise);
	if ( pRejectedPromise ) xPromiseDestroy(pRejectedPromise);
	if ( pPendingRejectAllPromise ) xPromiseDestroy(pPendingRejectAllPromise);
	if ( pPendingRejectRacePromise ) xPromiseDestroy(pPendingRejectRacePromise);
	if ( pCancelledPromise ) xPromiseDestroy(pCancelledPromise);
	if ( pPendingCancelAllPromise ) xPromiseDestroy(pPendingCancelAllPromise);
	if ( pPendingCancelRacePromise ) xPromiseDestroy(pPendingCancelRacePromise);
	if ( pSource ) xFutureRelease(pSource);
	if ( pPendingAll ) xFutureRelease(pPendingAll);
	if ( pPendingRace ) xFutureRelease(pPendingRace);
	if ( pRejected ) xFutureRelease(pRejected);
	if ( pPendingRejectAll ) xFutureRelease(pPendingRejectAll);
	if ( pPendingRejectRace ) xFutureRelease(pPendingRejectRace);
	if ( pCancelled ) xFutureRelease(pCancelled);
	if ( pPendingCancelAll ) xFutureRelease(pPendingCancelAll);
	if ( pPendingCancelRace ) xFutureRelease(pPendingCancelRace);
	return iRet;
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


// 内部函数：__Test_FutureCore_EngineTask
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


// 内部函数：__Test_FutureCore_TaskGroupRun
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


// 内部函数：__Test_FutureCore_TaskGroupScope
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


// 内部函数：__Test_FutureCore_TaskGroupJoin
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


// 内部函数：__Test_FutureCore_TaskGroupParentCancel
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


// 内部函数：__Test_FutureCore_TaskGroupNestedScope
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


// 内部函数：__Test_FutureCore_ContinuationStress
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


// 内部函数：__Test_FutureCore_TaskGroupBatch
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


// 内部函数：__Test_FutureCore_CombinatorRaceStress
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


// 内部函数：__Test_FutureCore_TaskGroupParentMultiCancelStress
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


// 内部函数：__Test_FutureCore_TaskGroupDestroyPending
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


// 内部函数：__Test_FutureCore_TaskGroupAddAfterClose
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


// 内部函数：__Test_FutureCore_OwnershipLifetime
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
// 内部函数：__Test_FutureCore_TaskRunCo
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

// 内部函数：__Test_FutureCore_RunAll
static int __Test_FutureCore_RunAll(void)
{
	int iRet = 0;

	iRet = __Test_FutureCore_Run();
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskRunThread();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TypeDesc();
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
		iRet = __Test_FutureCore_RaceCompletedFirstMultiLoser();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_RaceFailureCancelLoser();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_RepeatedCancelState();
	}
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_GroupDuplicateSource();
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
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_RaceOwnedResultBorrow();
	}
	#if !defined(XRT_NO_COROUTINE)
	if ( iRet == 0 ) {
		iRet = __Test_FutureCore_TaskRunCo();
	}
	#endif

	return iRet;
}

#if defined(XRT_FUTURE_CORE_EMBEDDED)
// Future核心测试
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
// MAIN相关处理
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
