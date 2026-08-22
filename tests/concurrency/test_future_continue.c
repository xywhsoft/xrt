#include "../test.h"



/* 基础延续测试记录回调次数、顺序、输入终态和输出值。 */
typedef struct testfuturecontinue {
	int Value;
	int Hits;
	int Freed;
	int Marker;
	int* Order;
	xfuturestate State;
	xpromise* Held;
} testfuturecontinue;



/* 创建一对独立 Future/Promise 端点。 */
static xpromise* testFutureContinuePair(xfuture** ppFuture)
{
	xpromise* pPromise = xrtPromiseCreate(ppFuture, NULL);

	testRequire((pPromise != NULL) && (*ppFuture != NULL),
		"future continuation pair create failed");
	return pPromise;
}



/* 释放一对调用方端点。 */
static void testFutureContinuePairDestroy(
	xpromise* pPromise,
	xfuture* pFuture
)
{
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
}



/* 成功延续读取输入值并写入一个新结果。 */
static void testFutureThenProc(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	testfuturecontinue* pContext = (testfuturecontinue*)pData;

	pContext->State = pInput->State;
	pContext->Hits++;
	if ( pContext->Order != NULL ) {
		*pContext->Order = pContext->Marker;
	}
	pContext->Value = *(int*)pInput->Value + 5;
	testRequire(xrtPromiseResolve(pOutput, &pContext->Value),
		"future Then output resolve failed");
}



/* 失败延续把结构化错误恢复成普通成功值。 */
static void testFutureCatchProc(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	testfuturecontinue* pContext = (testfuturecontinue*)pData;

	pContext->State = pInput->State;
	pContext->Hits++;
	pContext->Value = 7788;
	testRequire((pInput->Error != NULL) &&
		(xrtErrorKind(pInput->Error) == XERR_PROTOCOL),
		"future Catch input error mismatch");
	testRequire(xrtPromiseResolve(pOutput, &pContext->Value),
		"future Catch output resolve failed");
}



/* 全状态延续显式记录输入，并创建自己的成功结果。 */
static void testFutureContinueProc(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	testfuturecontinue* pContext = (testfuturecontinue*)pData;

	pContext->State = pInput->State;
	pContext->Hits++;
	if ( pContext->Order != NULL ) {
		*pContext->Order = pContext->Marker;
	}
	pContext->Value = 9001;
	testRequire(xrtPromiseResolve(pOutput, &pContext->Value),
		"future Continue output resolve failed");
}



/* 全状态延续也可以显式发布结构化失败结果。 */
static void testFutureContinueReject(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	testfuturecontinue* pContext = (testfuturecontinue*)pData;
	xerror* pError;

	pContext->State = pInput->State;
	pContext->Hits++;
	pError = xrtErrorCreate(
		XERR_VALUE,
		"test.future.continue",
		23,
		"continuation rejected"
	);
	testRequire((pError != NULL) && xrtPromiseReject(pOutput, pError),
		"future Continue output reject failed");
	xrtErrorFree(pError);
}



/* Finally 只观察输入，输出由运行库自动透传。 */
static void testFutureFinallyProc(
	const xfutureresult* pInput,
	ptr pData
)
{
	testfuturecontinue* pContext = (testfuturecontinue*)pData;

	pContext->State = pInput->State;
	pContext->Hits++;
	if ( pContext->Order != NULL ) {
		*pContext->Order = pContext->Marker;
	}
}



/* Owned 延续释放过程记录恰好一次的数据释放。 */
static void testFutureContinueDestroy(ptr pValue, ptr pData)
{
	testfuturecontinue* pContext = (testfuturecontinue*)pValue;

	(void)pData;
	pContext->Freed++;
}



/* 延迟完成回调保留输出 Promise，验证异步逃生路径。 */
static void testFutureContinueHold(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	testfuturecontinue* pContext = (testfuturecontinue*)pData;

	pContext->State = pInput->State;
	pContext->Hits++;
	pContext->Held = xrtPromiseRef(pOutput);
	testRequire(pContext->Held != NULL,
		"future continuation output promise retain failed");
}



/* 验证成功路径、注册顺序、跳过透传和全状态延续。 */
static void testFutureContinueSuccess(void)
{
	testfuturecontinue tThen = { 0 };
	testfuturecontinue tCatch = { 0 };
	testfuturecontinue tFinally = { 0 };
	testfuturecontinue tContinue = { 0 };
	int arrOrder[3] = { 0 };
	int iValue = 100;
	xfuture* pSource;
	xfuture* pThen;
	xfuture* pCatch;
	xfuture* pFinally;
	xfuture* pContinue;
	xpromise* pPromise = testFutureContinuePair(&pSource);

	tThen.Order = &arrOrder[0];
	tThen.Marker = 1;
	tFinally.Order = &arrOrder[1];
	tFinally.Marker = 2;
	tContinue.Order = &arrOrder[2];
	tContinue.Marker = 3;
	pThen = xrtFutureThen(pSource, testFutureThenProc, &tThen);
	pCatch = xrtFutureCatch(pSource, testFutureCatchProc, &tCatch);
	pFinally = xrtFutureFinally(pSource, testFutureFinallyProc, &tFinally);
	pContinue = xrtFutureContinue(
		pSource,
		testFutureContinueProc,
		&tContinue
	);
	testRequire((pThen != NULL) && (pCatch != NULL) &&
		(pFinally != NULL) && (pContinue != NULL),
		"future success continuations create failed");
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"future continuation source resolve failed");
	testRequire((tThen.Hits == 1) && (tCatch.Hits == 0) &&
		(tFinally.Hits == 1) && (tContinue.Hits == 1),
		"future success callback selection mismatch");
	testRequire((arrOrder[0] == 1) && (arrOrder[1] == 2) &&
		(arrOrder[2] == 3), "future continuation order mismatch");
	testRequire((xrtFutureValue(pThen) == &tThen.Value) &&
		(tThen.Value == 105), "future Then result mismatch");
	testRequire(xrtFutureValue(pCatch) == &iValue,
		"future Catch success pass-through mismatch");
	testRequire(xrtFutureValue(pFinally) == &iValue,
		"future Finally result pass-through mismatch");
	testRequire(xrtFutureValue(pContinue) == &tContinue.Value,
		"future Continue result mismatch");

	xrtFutureDestroy(pContinue);
	xrtFutureDestroy(pFinally);
	xrtFutureDestroy(pCatch);
	xrtFutureDestroy(pThen);
	testFutureContinuePairDestroy(pPromise, pSource);
}



/* 验证失败只进入 Catch，其他条件延续安全透传结构化错误。 */
static void testFutureContinueFailure(void)
{
	testfuturecontinue tThen = { 0 };
	testfuturecontinue tCatch = { 0 };
	testfuturecontinue tFinally = { 0 };
	xfuture* pSource;
	xfuture* pThen;
	xfuture* pCatch;
	xfuture* pFinally;
	xpromise* pPromise = testFutureContinuePair(&pSource);
	xerror* pError = xrtErrorCreate(
		XERR_PROTOCOL,
		"test.future.continue",
		17,
		"source failed"
	);

	testRequire(pError != NULL, "future continuation error create failed");
	pThen = xrtFutureThen(pSource, testFutureThenProc, &tThen);
	pCatch = xrtFutureCatch(pSource, testFutureCatchProc, &tCatch);
	pFinally = xrtFutureFinally(pSource, testFutureFinallyProc, &tFinally);
	testRequire((pThen != NULL) && (pCatch != NULL) && (pFinally != NULL),
		"future failure continuations create failed");
	testRequire(xrtPromiseReject(pPromise, pError),
		"future continuation source reject failed");
	xrtErrorFree(pError);
	testRequire((tThen.Hits == 0) && (tCatch.Hits == 1) &&
		(tFinally.Hits == 1), "future failure callback selection mismatch");
	testRequire((xrtFutureState(pThen) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pThen)) == XERR_PROTOCOL),
		"future Then failure pass-through mismatch");
	testRequire((xrtFutureState(pCatch) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pCatch) == &tCatch.Value),
		"future Catch recovery mismatch");
	testRequire((xrtFutureState(pFinally) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pFinally)) == XERR_PROTOCOL),
		"future Finally failure pass-through mismatch");

	xrtFutureDestroy(pFinally);
	xrtFutureDestroy(pCatch);
	xrtFutureDestroy(pThen);
	testFutureContinuePairDestroy(pPromise, pSource);
}



/* 验证 Catch 不吞掉取消和关闭这两类独立终态。 */
static void testFutureContinueTerminalPass(void)
{
	testfuturecontinue tCancel = { 0 };
	testfuturecontinue tClose = { 0 };
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise = testFutureContinuePair(&pSource);

	pNext = xrtFutureCatch(pSource, testFutureCatchProc, &tCancel);
	testRequire(pNext != NULL, "future cancelled Catch create failed");
	testRequire(xrtPromiseCancel(pPromise),
		"future cancelled source complete failed");
	testRequire((tCancel.Hits == 0) &&
		(xrtFutureState(pNext) == XFUTURE_CANCELLED),
		"future Catch swallowed cancellation");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureCatch(pSource, testFutureCatchProc, &tClose);
	testRequire(pNext != NULL, "future closed Catch create failed");
	testRequire(xrtPromiseClose(pPromise),
		"future closed source complete failed");
	testRequire((tClose.Hits == 0) &&
		(xrtFutureState(pNext) == XFUTURE_CLOSED),
		"future Catch swallowed close state");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);
}



/* 验证 Owned 数据在运行、跳过和输出取消路径都恰好释放一次。 */
static void testFutureContinueOwned(void)
{
	testfuturecontinue tRun = { 0 };
	testfuturecontinue tSkip = { 0 };
	testfuturecontinue tCancel = { 0 };
	testfuturecontinue tContinue = { 0 };
	testfuturecontinue tReject = { 0 };
	testfuturecontinue tCatch = { 0 };
	testfuturecontinue tFinally = { 0 };
	testfuturecontinue tInvalid = { 0 };
	int iValue = 7;
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise;
	xcancel* pSourceCancel;
	xerror* pError;

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureThenOwned(
		pSource,
		testFutureThenProc,
		&tRun,
		testFutureContinueDestroy,
		NULL
	);
	testRequire(pNext != NULL, "future owned Then create failed");
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"future owned Then source resolve failed");
	testRequire((tRun.Hits == 1) && (tRun.Freed == 1),
		"future owned Then lifetime mismatch");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureContinueOwned(
		pSource,
		testFutureContinueReject,
		&tReject,
		testFutureContinueDestroy,
		NULL
	);
	testRequire(pNext != NULL, "future owned rejecting Continue create failed");
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"future owned rejecting Continue source resolve failed");
	testRequire((tReject.Hits == 1) && (tReject.Freed == 1) &&
		(xrtFutureState(pNext) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pNext)) == XERR_VALUE),
		"future owned rejecting Continue contract mismatch");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureThenOwned(
		pSource,
		testFutureThenProc,
		&tSkip,
		testFutureContinueDestroy,
		NULL
	);
	pError = xrtErrorCreate(XERR_PROTOCOL, "test", 1, "skip");
	testRequire((pNext != NULL) && (pError != NULL),
		"future owned skipped Then setup failed");
	testRequire(xrtPromiseReject(pPromise, pError),
		"future owned skipped Then reject failed");
	xrtErrorFree(pError);
	testRequire((tSkip.Hits == 0) && (tSkip.Freed == 1),
		"future owned skipped data lifetime mismatch");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureThenOwned(
		pSource,
		testFutureThenProc,
		&tCancel,
		testFutureContinueDestroy,
		NULL
	);
	pSourceCancel = xrtFutureCancelToken(pSource);
	testRequire(
		(pNext != NULL) && (pSourceCancel != NULL),
		"future owned cancelled Then create failed"
	);
	testRequire(xrtFutureCancel(pNext),
		"future continuation output cancel request failed");
	testRequire(
		!xrtCancelRequested(pSourceCancel),
		"public future continuation cancelled its shared source"
	);
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"future cancelled continuation source resolve failed");
	testRequire((tCancel.Hits == 0) && (tCancel.Freed == 1) &&
		(xrtFutureState(pNext) == XFUTURE_CANCELLED),
		"future cancelled continuation contract mismatch");
	xrtCancelDestroy(pSourceCancel);
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureContinueOwned(
		pSource,
		testFutureContinueProc,
		&tContinue,
		testFutureContinueDestroy,
		NULL
	);
	pError = xrtErrorCreate(XERR_PROTOCOL, "test", 2, "continue");
	testRequire((pNext != NULL) && (pError != NULL),
		"future owned Continue setup failed");
	testRequire(xrtPromiseReject(pPromise, pError),
		"future owned Continue source reject failed");
	xrtErrorFree(pError);
	testRequire((tContinue.Hits == 1) && (tContinue.Freed == 1) &&
		(tContinue.State == XFUTURE_FAILED) &&
		(xrtFutureValue(pNext) == &tContinue.Value),
		"future owned Continue contract mismatch");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureCatchOwned(
		pSource,
		testFutureCatchProc,
		&tCatch,
		testFutureContinueDestroy,
		NULL
	);
	pError = xrtErrorCreate(XERR_PROTOCOL, "test", 3, "catch");
	testRequire((pNext != NULL) && (pError != NULL),
		"future owned Catch setup failed");
	testRequire(xrtPromiseReject(pPromise, pError),
		"future owned Catch source reject failed");
	xrtErrorFree(pError);
	testRequire((tCatch.Hits == 1) && (tCatch.Freed == 1) &&
		(xrtFutureValue(pNext) == &tCatch.Value),
		"future owned Catch contract mismatch");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pNext = xrtFutureFinallyOwned(
		pSource,
		testFutureFinallyProc,
		&tFinally,
		testFutureContinueDestroy,
		NULL
	);
	testRequire(pNext != NULL, "future owned Finally create failed");
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"future owned Finally source resolve failed");
	testRequire((tFinally.Hits == 1) && (tFinally.Freed == 1) &&
		(xrtFutureValue(pNext) == &iValue),
		"future owned Finally contract mismatch");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	xrtClearError();
	testRequire(xrtFutureThenOwned(
		NULL,
		testFutureThenProc,
		&tInvalid,
		testFutureContinueDestroy,
		NULL
	) == NULL, "invalid owned continuation unexpectedly succeeded");
	testRequire((tInvalid.Freed == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"failed continuation consumed caller data");
}



/* 验证独占延续只在显式入口中把输出取消传播给源生产链。 */
static void testFutureContinueOwnedCancelSource(void)
{
	testfuturecontinue tThen = { 0 };
	testfuturecontinue tContinue = { 0 };
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise;
	xcancel* pSourceCancel;

	pPromise = testFutureContinuePair(&pSource);
	pSourceCancel = xrtFutureCancelToken(pSource);
	pNext = xrtFutureThenOwnedCancelSource(
		pSource,
		testFutureThenProc,
		&tThen,
		testFutureContinueDestroy,
		NULL
	);
	testRequire((pSourceCancel != NULL) && (pNext != NULL),
		"exclusive future Then create failed");
	testRequire(xrtFutureCancel(pNext) &&
		xrtCancelRequested(pSourceCancel),
		"exclusive future Then did not cancel source");
	testRequire(xrtPromiseCancel(pPromise),
		"exclusive future Then source cancel failed");
	testRequire((tThen.Hits == 0) && (tThen.Freed == 1) &&
		(xrtFutureState(pNext) == XFUTURE_CANCELLED),
		"exclusive future Then terminal contract mismatch");
	xrtCancelDestroy(pSourceCancel);
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);

	pPromise = testFutureContinuePair(&pSource);
	pSourceCancel = xrtFutureCancelToken(pSource);
	pNext = xrtFutureContinueOwnedCancelSource(
		pSource,
		testFutureContinueProc,
		&tContinue,
		testFutureContinueDestroy,
		NULL
	);
	testRequire((pSourceCancel != NULL) && (pNext != NULL),
		"exclusive future Continue create failed");
	testRequire(xrtFutureCancel(pNext) &&
		xrtCancelRequested(pSourceCancel),
		"exclusive future Continue did not cancel source");
	testRequire(xrtPromiseClose(pPromise),
		"exclusive future Continue source close failed");
	testRequire((tContinue.Hits == 0) && (tContinue.Freed == 1) &&
		(xrtFutureState(pNext) == XFUTURE_CANCELLED),
		"exclusive future Continue terminal contract mismatch");
	xrtCancelDestroy(pSourceCancel);
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);
}



/* 验证回调可显式保留输出 Promise，并在任意后续异步路径完成。 */
static void testFutureContinueRetain(void)
{
	testfuturecontinue tContext = { 0 };
	int iSource = 5;
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise = testFutureContinuePair(&pSource);

	pNext = xrtFutureContinue(pSource, testFutureContinueHold, &tContext);
	testRequire(pNext != NULL, "retained continuation create failed");
	testRequire(xrtPromiseResolve(pPromise, &iSource),
		"retained continuation source resolve failed");
	testRequire((tContext.Hits == 1) && (tContext.Held != NULL) &&
		(xrtFutureState(pNext) == XFUTURE_PENDING),
		"retained continuation closed output too early");
	tContext.Value = 64;
	testRequire(xrtPromiseResolve(tContext.Held, &tContext.Value),
		"retained continuation late resolve failed");
	xrtPromiseDestroy(tContext.Held);
	testRequire(xrtFutureValue(pNext) == &tContext.Value,
		"retained continuation result mismatch");

	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);
}



/* Owned 成功值析构用于验证 Promise 透传对源结果的保活。 */
static void testFutureForwardValueDestroy(ptr pValue, ptr pData)
{
	int* pFreed = (int*)pData;

	(*pFreed)++;
	free(pValue);
}



/* 验证输出 Future 在源端点离开后仍安全拥有透传结果。 */
static void testFutureForwardLifetime(void)
{
	xfuture* pSource;
	xfuture* pOutput;
	xpromise* pSourcePromise = testFutureContinuePair(&pSource);
	xpromise* pOutputPromise = testFutureContinuePair(&pOutput);
	int* pValue = (int*)malloc(sizeof(int));
	int iFreed = 0;

	testRequire(pValue != NULL, "future forward owned value allocate failed");
	*pValue = 77;
	testRequire(xrtPromiseResolveOwned(
		pSourcePromise,
		pValue,
		testFutureForwardValueDestroy,
		&iFreed
	), "future forward source resolve failed");
	testRequire(xrtPromiseForward(pOutputPromise, pSource),
		"future result forward failed");
	testFutureContinuePairDestroy(pSourcePromise, pSource);
	testRequire((iFreed == 0) && (*(int*)xrtFutureValue(pOutput) == 77),
		"future forward did not retain source result");
	testFutureContinuePairDestroy(pOutputPromise, pOutput);
	testRequire(iFreed == 1, "future forwarded value destructor mismatch");
}



/* 记录终态观察回调看到的状态与生产端取消令牌。 */
typedef struct testfuturecancelpublish {
	xcancel* Cancel;
	xfuturestate State;
	bool Requested;
} testfuturecancelpublish;



/* 终态回调必须看到与 CANCELLED/CLOSED 状态一致的取消请求。 */
static void testFutureCancelPublishProc(
	const xfutureresult* pInput,
	ptr pData
)
{
	testfuturecancelpublish* pContext =
		(testfuturecancelpublish*)pData;

	pContext->State = pInput->State;
	pContext->Requested = xrtCancelRequested(pContext->Cancel);
}



/* 验证取消请求先于终态通知发布，避免观察者读取到矛盾快照。 */
static void testFutureCancelPublish(void)
{
	const xfuturestate arrState[] = {
		XFUTURE_CANCELLED,
		XFUTURE_CLOSED,
		XFUTURE_CLOSED
	};

	for ( size_t i = 0; i < (sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		testfuturecancelpublish tContext = { 0 };
		xfuture* pSource;
		xfuture* pObserve;
		xpromise* pPromise = testFutureContinuePair(&pSource);

		tContext.Cancel = xrtFutureCancelToken(pSource);
		testRequire(tContext.Cancel != NULL,
			"future terminal publish cancel token failed");
		pObserve = xrtFutureFinally(
			pSource,
			testFutureCancelPublishProc,
			&tContext
		);
		testRequire(pObserve != NULL,
			"future terminal publish observer failed");
		if ( arrState[i] == XFUTURE_CANCELLED ) {
			testRequire(xrtPromiseCancel(pPromise),
				"future cancel terminal publish failed");
		} else if ( i == 1 ) {
			testRequire(xrtPromiseClose(pPromise),
				"future close terminal publish failed");
		} else {
			xrtPromiseDestroy(pPromise);
			pPromise = NULL;
		}
		testRequire(
			(tContext.State == arrState[i]) && tContext.Requested,
			"future terminal callback observed cancellation after state"
		);
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pObserve);
		xrtCancelDestroy(tContext.Cancel);
		xrtFutureDestroy(pSource);
	}
}



/* 深链观察回调只计数，用于同时验证完成和释放路径的迭代实现。 */
static void testFutureContinueDeepProc(
	const xfutureresult* pInput,
	ptr pData
)
{
	size_t* pHits = (size_t*)pData;

	testRequire(pInput->State == XFUTURE_RESOLVED,
		"deep continuation input state mismatch");
	(*pHits)++;
}



/* 验证一万六千层透传不会把完成或所有者释放深度映射到 C 栈。 */
static void testFutureContinueDeep(void)
{
	const size_t iDepth = 16384;
	xfuture* pRoot;
	xfuture* pCurrent;
	xpromise* pPromise = testFutureContinuePair(&pRoot);
	size_t iHits = 0;
	int iValue = 123;

	pCurrent = pRoot;
	for ( size_t i = 0; i < iDepth; i++ ) {
		xfuture* pNext = xrtFutureFinally(
			pCurrent,
			testFutureContinueDeepProc,
			&iHits
		);

		testRequire(pNext != NULL, "deep future continuation create failed");
		xrtFutureDestroy(pCurrent);
		pCurrent = pNext;
	}
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"deep future continuation source resolve failed");
	testRequire((iHits == iDepth) &&
		(xrtFutureValue(pCurrent) == &iValue),
		"deep future continuation result mismatch");
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pCurrent);
}



/* 验证预完成源在注册线程同步执行，语义不依赖额外 pump。 */
static void testFutureContinueImmediate(void)
{
	testfuturecontinue tContext = { 0 };
	int iValue = 9;
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise = testFutureContinuePair(&pSource);

	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"immediate continuation source resolve failed");
	pNext = xrtFutureThen(pSource, testFutureThenProc, &tContext);
	testRequire((pNext != NULL) && (tContext.Hits == 1) &&
		(xrtFutureValue(pNext) == &tContext.Value),
		"immediate future continuation mismatch");
	xrtFutureDestroy(pNext);
	testFutureContinuePairDestroy(pPromise, pSource);
}



/* 运行 Future continuation 的功能、所有权和深链回归。 */
int main(void)
{
	testFutureContinueSuccess();
	testFutureContinueFailure();
	testFutureContinueTerminalPass();
	testFutureContinueOwned();
	testFutureContinueOwnedCancelSource();
	testFutureContinueRetain();
	testFutureForwardLifetime();
	testFutureCancelPublish();
	testFutureContinueImmediate();
	testFutureContinueDeep();

	printf("[PASS] future continuation\n");
	return 0;
}
