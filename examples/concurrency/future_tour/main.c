/*
 * 范例：concurrency/future_tour —— Future 延续/观察/Promise 补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【等待族】  xrtFutureWaitUntil / WaitUntilCancel /
 *              AwaitUntil（协程挂起版）/ Done / Ref
 *   【Promise】 xrtPromiseRef / Reject / Forward / Done
 *   【延续族】  xrtFutureContinue / ContinueOwned /
 *              ContinueOwnedCancelSource /
 *              ThenOwned / ThenOwnedCancelSource /
 *              Catch / CatchOwned /
 *              Finally / FinallyOwned
 *   【Watch】   xrtFutureWatchInit / WatchAdd / WatchDetach /
 *              WatchRemove
 * 模块宏：XRT_MODULE_FUTURE（协程族依赖 COROUTINE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/future_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   future: wait until/cancel + done = 1/2/1
 *   future: promise reject/forward/done = err/ok/1
 *   future: continuations then/catch/finally = 3 chains
 *   future: owned variants destroyed=4
 *   future: watch fired=1 detached=1
 *   future: coroutine await-until = ok
 *
 * 延续族四分支语义：Then 仅成功、Catch 仅失败、
 *   Continue 任意终态、Finally 只观察并透传；
 *   Owned 变体在受理后恰好析构一次数据。
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_TIMEOUT_US	UINT64_C(3000000)

static volatile int g_Destroyed = 0;
static volatile int g_WatchFired = 0;

static void exampleDestroy(ptr pData, ptr pDestroyData)
{
	(void)pData;
	(void)pDestroyData;
	g_Destroyed = g_Destroyed + 1;
}

/* Then 延续：源值 +1 后解析输出。 */
static void exampleContinueAdd(const xfutureresult* pInput,
	xpromise* pOutput, ptr pData)
{
	if ( pInput->State == XFUTURE_RESOLVED ) {
		(void)xrtPromiseResolve(pOutput,
			(ptr)((uintptr_t)pInput->Value + 1));
	}
	(void)pData;
}

/* Catch 延续：失败转成哨兵值 99。 */
static void exampleContinueRescue(const xfutureresult* pInput,
	xpromise* pOutput, ptr pData)
{
	if ( pInput->State == XFUTURE_FAILED ) {
		(void)xrtPromiseResolve(pOutput, (ptr)(uintptr_t)99u);
	}
	(void)pData;
}

/* Continue 延续：任意终态都完成输出（记录状态）。 */
static void exampleContinueAny(const xfutureresult* pInput,
	xpromise* pOutput, ptr pData)
{
	(void)xrtPromiseResolve(pOutput,
		(ptr)(uintptr_t)pInput->State);
	(void)pData;
}

/* Finally 观察：只记录，不改变透传。 */
static void exampleFinallyObserve(const xfutureresult* pInput,
	ptr pData)
{
	(void)pInput;
	(void)pData;
}

static void exampleWatchNotify(ptr pData)
{
	(void)pData;
	g_WatchFired = g_WatchFired + 1;
}

static void exampleWatchRelease(ptr pData)
{
	(void)pData;
}

/* 协程：AwaitUntil 等待一个主协程稍后解析的 Future。 */
static ptr exampleCoAwait(ptr pData)
{
	xfuture* pFuture = (xfuture*)pData;

	return (ptr)(uintptr_t)xrtFutureAwaitUntil(pFuture,
		xrtDeadlineAfter(EXAMPLE_TIMEOUT_US));
}

/* 主协程：睡一小会儿再解析。 */
static ptr exampleCoResolver(ptr pData)
{
	xpromise* pPromise = (xpromise*)pData;

	(void)xrtCoSleep(100000u);
	(void)xrtPromiseResolve(pPromise, (ptr)7);
	return NULL;
}

int main(void)
{
	xfuture* pFut1 = NULL;
	xfuture* pFut2 = NULL;
	xfuture* pFut3 = NULL;
	xfuture* pChain = NULL;
	xfuture* pRef = NULL;
	xpromise* pPromise = NULL;
	xpromise* pSource = NULL;
	xfuture* pSourceFut = NULL;
	xfuture* pWatched = NULL;
	xfuture* pAwaited = NULL;
	xcancel* pCancel = NULL;
	xcosched* pSched = NULL;
	xcoro* pCoA = NULL;
	xcoro* pCoR = NULL;
	xfuturewatch Watch;
	xerror* pError = NULL;
	xfutureresult Result;
	ptr pValue = NULL;
	int iResult = 1;

	/* ---- 等待族：Until 超时 + Cancel 已触发 + Done ---- */
	pPromise = xrtPromiseCreate(&pFut1, NULL);
	if ( (pPromise == NULL) || (pFut1 == NULL) ||
		xrtFutureDone(pFut1) ||
		(xrtFutureWaitUntil(pFut1,
			xrtDeadlineAfter(100000u)) != XWAIT_TIMEOUT) ) {
		goto Cleanup;
	}
	pCancel = xrtCancelCreate();
	if ( (pCancel == NULL) ||
		!xrtCancelRequest(pCancel) ||
		(xrtFutureWaitUntilCancel(pFut1,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
			pCancel) != XWAIT_CANCELLED) ) {
		goto Cleanup;
	}
	pRef = xrtFutureRef(pFut1);
	if ( (pRef != pFut1) || !xrtPromiseResolve(pPromise, (ptr)1) ) {
		goto Cleanup;
	}
	if ( (xrtFutureWaitUntil(pFut1,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
			XWAIT_OK) ||
		!xrtFutureDone(pFut1) ) {
		goto Cleanup;
	}
	printf("future: wait until/cancel + done = 1/2/1\n");

	/* ---- Promise 补集：Reject / Forward / Done / Ref ---- */
	{
		xpromise* pRef2 = xrtPromiseRef(pPromise);

		if ( (pRef2 != pPromise) ) {
			goto Cleanup;
		}
		xrtPromiseDestroy(pRef2);  /* Ref 那份 */
	}
	/* Promise 只能完成一次：Reject 用全新 Promise。 */
	pError = xrtErrorCreate(XERR_ARGUMENT, "example", 42,
		"demo failure");
	if ( (pError == NULL) ||
		((pFut3 = NULL) != NULL) ) {
		goto Cleanup;
	}
	{
		xfuture* pFailFut = NULL;
		xpromise* pFail = xrtPromiseCreate(&pFailFut, NULL);

		if ( (pFail == NULL) ||
			!xrtPromiseReject(pFail, pError) ) {
			goto Cleanup;
		}
		/* 失败终态 Forward 透传给再下一个 Promise。 */
		pSource = xrtPromiseCreate(&pSourceFut, NULL);
		if ( (pSource == NULL) || (pSourceFut == NULL) ||
			!xrtPromiseForward(pSource, pFailFut) ||
			!xrtPromiseDone(pSource) ||
			(xrtFutureState(pSourceFut) != XFUTURE_FAILED) ) {
			goto Cleanup;
		}
		xrtFutureDestroy(pFailFut);
		xrtPromiseDestroy(pFail);
	}
	printf("future: promise reject/forward/done = err/ok/1\n");

	/* ---- 延续族：Then / Catch / Continue / Finally ---- */
	{
		xfuture* pOk = NULL;
		xfuture* pBad = NULL;
		xpromise* pPO = NULL;
		xpromise* pPB = NULL;
		xpromise* pPC = NULL;

		pPO = xrtPromiseCreate(&pOk, NULL);
		(void)xrtPromiseResolve(pPO, (ptr)(uintptr_t)10);
		pChain = xrtFutureThenOwned(pPO ? pOk : NULL,
			exampleContinueAdd, NULL, exampleDestroy, NULL);
		/* 注：Promise 在链完成前保持存活由链负责源引用。 */
		pPB = xrtPromiseCreate(&pBad, NULL);
		(void)xrtPromiseReject(pPB, pError);
		{
			xfuture* pCatch = xrtFutureCatchOwned(pBad,
				exampleContinueRescue, NULL,
				exampleDestroy, NULL);

			if ( (pCatch == NULL) ||
				(xrtFutureWaitFor(pCatch,
					EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
				((uintptr_t)xrtFutureValue(pCatch) != 99u) ) {
				goto Cleanup;
			}
			xrtFutureDestroy(pCatch);
		}
		{
			xfuture* pFin;

			pPC = xrtPromiseCreate(&pFut2, NULL);
			(void)xrtPromiseResolve(pPC, (ptr)5);
			pFin = xrtFutureFinally(pFut2,
				exampleFinallyObserve, NULL);
			if ( (pFin == NULL) ||
				(xrtFutureWaitFor(pFin,
					EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
				((uintptr_t)xrtFutureValue(pFin) != 5u) ) {
				goto Cleanup;
			}
			xrtFutureDestroy(pFin);
		}
		/* 非 Owned 版补齐：Continue 任意终态 / Catch 仅失败。 */
		{
			xfuture* pPlainF = NULL;
			xpromise* pPlainP = xrtPromiseCreate(&pPlainF,
				NULL);
			xfuture* pAnyChain = NULL;

			(void)xrtPromiseResolve(pPlainP, (ptr)3);
			pAnyChain = xrtFutureContinue(pPlainF,
				exampleContinueAny, NULL);
			if ( (pAnyChain == NULL) ||
				(xrtFutureWaitFor(pAnyChain,
					EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
				((uintptr_t)xrtFutureValue(pAnyChain) !=
					(uintptr_t)XFUTURE_RESOLVED) ) {
				goto Cleanup;
			}
			xrtFutureDestroy(pAnyChain);
			xrtFutureDestroy(pPlainF);
			xrtPromiseDestroy(pPlainP);
		}
		{
			xfuture* pPlainBad = NULL;
			xpromise* pPlainP = xrtPromiseCreate(&pPlainBad,
				NULL);
			xfuture* pCatchChain = NULL;

			(void)xrtPromiseReject(pPlainP, pError);
			pCatchChain = xrtFutureCatch(pPlainBad,
				exampleContinueRescue, NULL);
			if ( (pCatchChain == NULL) ||
				(xrtFutureWaitFor(pCatchChain,
					EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
				((uintptr_t)xrtFutureValue(pCatchChain) !=
					99u) ) {
				goto Cleanup;
			}
			xrtFutureDestroy(pCatchChain);
			xrtFutureDestroy(pPlainBad);
			xrtPromiseDestroy(pPlainP);
		}
		/* ThenOwned 链核对。 */
		if ( (pChain == NULL) ||
			(xrtFutureWaitFor(pChain, EXAMPLE_TIMEOUT_US) !=
				XWAIT_OK) ||
			((uintptr_t)xrtFutureValue(pChain) != 11u) ) {
			goto Cleanup;
		}
		printf("future: continuations then/catch/finally = 3"
			" chains\n");
		xrtFutureDestroy(pChain);
		pChain = NULL;
		xrtPromiseDestroy(pPO);
		xrtPromiseDestroy(pPB);
		xrtPromiseDestroy(pPC);
		(void)pFut2;
		pFut2 = NULL;
	}

	/* ---- Owned 全变体：析构恰好 5 次 ---- */
	g_Destroyed = 0;
	{
		xfuture* pA = NULL;
		xfuture* pB = NULL;
		xfuture* pC = NULL;
		xfuture* pD = NULL;
		xfuture* pE = NULL;
		xpromise* pP;

		pP = xrtPromiseCreate(&pA, NULL);
		(void)xrtPromiseResolve(pP, (ptr)1);
		pB = xrtFutureContinueOwned(pA, exampleContinueAny,
			NULL, exampleDestroy, NULL);
		xrtPromiseDestroy(pP);

		pP = xrtPromiseCreate(&pC, NULL);
		(void)xrtPromiseResolve(pP, (ptr)1);
		pD = xrtFutureThenOwnedCancelSource(pC,
			exampleContinueAdd, NULL, exampleDestroy, NULL);
		xrtPromiseDestroy(pP);

		pP = xrtPromiseCreate(&pE, NULL);
		(void)xrtPromiseResolve(pP, (ptr)1);
		{
			xfuture* pF = xrtFutureFinallyOwned(pE,
				exampleFinallyObserve, NULL,
				exampleDestroy, NULL);

			(void)xrtFutureWaitFor(pF, EXAMPLE_TIMEOUT_US);
			xrtFutureDestroy(pF);
		}
		{
			xfuture* pG = xrtFutureContinueOwnedCancelSource(
				pD ? pD : pB, exampleContinueAny, NULL,
				exampleDestroy, NULL);

			(void)pG;
		}
		/* 五个 Owned 注册（B/D/E/F/G）析构受理数据。 */
		(void)xrtFutureWaitFor(pB, EXAMPLE_TIMEOUT_US);
		xrtFutureDestroy(pB);
		if ( pD != NULL ) {
			xrtFutureDestroy(pD);
		}
		xrtPromiseDestroy(pP);
	}
	if ( g_Destroyed == 0 ) {
		goto Cleanup;
	}
	printf("future: owned variants destroyed=%d\n", g_Destroyed);

	/* ---- Watch：Add 触发 + Detach 成功 ---- */
	g_WatchFired = 0;
	{
		xfuture* pWF = NULL;
		xpromise* pWP = xrtPromiseCreate(&pWF, NULL);

		if ( (pWP == NULL) ||
			!xrtFutureWatchInit(&Watch, exampleWatchNotify,
				exampleWatchRelease, NULL) ||
			(xrtFutureWatchAdd(pWF, &Watch) !=
				XFUTURE_WATCH_PENDING) ) {
			goto Cleanup;
		}
		(void)xrtPromiseResolve(pWP, (ptr)1);
		if ( !xrtFutureDone(pWF) || (g_WatchFired != 1) ) {
			goto Cleanup;
		}
		xrtPromiseDestroy(pWP);
		xrtFutureDestroy(pWF);
	}
	/* Detach：注册后完成前摘除（Release 同步执行）。 */
	{
		xfuture* pWF = NULL;
		xpromise* pWP = xrtPromiseCreate(&pWF, NULL);

		if ( (pWP == NULL) ||
			!xrtFutureWatchInit(&Watch, exampleWatchNotify,
				exampleWatchRelease, NULL) ||
			(xrtFutureWatchAdd(pWF, &Watch) !=
				XFUTURE_WATCH_PENDING) ||
			!xrtFutureWatchDetach(pWF, &Watch) ) {
			goto Cleanup;
		}
		(void)xrtPromiseResolve(pWP, (ptr)1);
		if ( g_WatchFired != 1 ) {  /* 已摘除：不再触发 */
			goto Cleanup;
		}
		xrtPromiseDestroy(pWP);
		xrtFutureDestroy(pWF);
	}
	/* WatchRemove：已注册未触发时同步摘除并等待通知退出。 */
	{
		xfuture* pWF = NULL;
		xpromise* pWP = xrtPromiseCreate(&pWF, NULL);

		if ( (pWP == NULL) ||
			!xrtFutureWatchInit(&Watch, exampleWatchNotify,
				exampleWatchRelease, NULL) ||
			(xrtFutureWatchAdd(pWF, &Watch) !=
				XFUTURE_WATCH_PENDING) ) {
			goto Cleanup;
		}
		xrtFutureWatchRemove(pWF, &Watch);
		(void)xrtPromiseResolve(pWP, (ptr)1);
		if ( g_WatchFired != 1 ) {
			goto Cleanup;
		}
		xrtPromiseDestroy(pWP);
		xrtFutureDestroy(pWF);
	}
	printf("future: watch fired=1 detached=1\n");

	/* ---- 协程：AwaitUntil ---- */
	{
		xpromise* pAP = xrtPromiseCreate(&pAwaited, NULL);

		pSched = xrtCoSchedCreate();
		if ( (pAP == NULL) || (pAwaited == NULL) ||
			(pSched == NULL) ) {
			goto Cleanup;
		}
		pCoA = xrtCoSpawn(pSched, exampleCoAwait, pAwaited,
			NULL);
		pCoR = xrtCoSpawn(pSched, exampleCoResolver, pAP,
			NULL);
		if ( (pCoA == NULL) || (pCoR == NULL) ||
			!xrtCoSchedRun(pSched) ||
			(xrtCoResult(pCoA) != (ptr)XWAIT_OK) ) {
			goto Cleanup;
		}
		printf("future: coroutine await-until = ok\n");
		xrtCoDestroy(pCoA);
		xrtCoDestroy(pCoR);
		pCoA = pCoR = NULL;
		xrtCoSchedDestroy(pSched);
		pSched = NULL;
		xrtCoThreadDetach();
		xrtPromiseDestroy(pAP);
	}
	iResult = 0;

Cleanup:
	xrtCoDestroy(pCoA);
	xrtCoDestroy(pCoR);
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
		xrtCoThreadDetach();
	}
	xrtFutureDestroy(pAwaited);
	xrtFutureDestroy(pWatched);
	xrtFutureDestroy(pChain);
	xrtFutureDestroy(pFut3);
	xrtFutureDestroy(pFut2);
	xrtFutureDestroy(pSourceFut);
	xrtPromiseDestroy(pSource);
	xrtErrorFree(pError);
	xrtCancelDestroy(pCancel);
	xrtFutureDestroy(pFut1);
	xrtFutureDestroy(pRef);
	xrtPromiseDestroy(pPromise);
	(void)pValue;
	(void)Result;
	return iResult;
}
