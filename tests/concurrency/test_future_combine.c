#include "../test.h"



/* 创建一个独立 Future/Promise 对并检查测试前置条件。 */
static xpromise* testFutureCombinePair(xfuture** ppFuture)
{
	xpromise* pPromise = xrtPromiseCreate(ppFuture, NULL);

	testRequire((pPromise != NULL) && (*ppFuture != NULL),
		"future combine pair create failed");
	return pPromise;
}



/* 释放两个输入端点，保持每个场景的所有权边界清晰。 */
static void testFutureCombineDestroyPair(
	xpromise* pPromise,
	xfuture* pFuture
)
{
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
}



/* 验证 Any 选择首个终态，并由源 Future 保留真实失败结果。 */
static void testFutureCombineAny(void)
{
	xfuture* pA;
	xfuture* pB;
	xfuture* pAny;
	xfuture* arrFuture[2];
	xpromise* pPromiseA = testFutureCombinePair(&pA);
	xpromise* pPromiseB = testFutureCombinePair(&pB);
	xerror* pError = xrtErrorCreate(
		XERR_PROTOCOL,
		"test.future.combine",
		17,
		"selected source failed"
	);
	const xfuturepick* pPick;

	testRequire(pError != NULL, "future Any error create failed");
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pAny = xrtFutureAny(arrFuture, 2);
	testRequire(pAny != NULL, "future Any create failed");
	testRequire(xrtPromiseReject(pPromiseB, pError),
		"future Any source reject failed");
	xrtErrorFree(pError);
	testRequire(xrtFutureWait(pAny) == XWAIT_OK, "future Any wait failed");
	testRequire(xrtFutureState(pAny) == XFUTURE_RESOLVED,
		"future Any coordination state mismatch");
	pPick = (const xfuturepick*)xrtFutureValue(pAny);
	testRequire((pPick != NULL) && (pPick->Index == 1) &&
		(pPick->Future == pB), "future Any selection mismatch");
	testRequire((xrtFutureState(pPick->Future) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pPick->Future)) == XERR_PROTOCOL),
		"future Any hid selected source failure");

	xrtFutureDestroy(pAny);
	testFutureCombineDestroyPair(pPromiseA, pA);
	testFutureCombineDestroyPair(pPromiseB, pB);
}



/* 验证 All 等待每个槽位，并按输入顺序保存全部源引用。 */
static void testFutureCombineAll(void)
{
	xfuture* pA;
	xfuture* pB;
	xfuture* pAllFuture;
	xfuture* arrFuture[3];
	xpromise* pPromiseA = testFutureCombinePair(&pA);
	xpromise* pPromiseB = testFutureCombinePair(&pB);
	const xfutureall* pAll;
	int iValue = 31;

	arrFuture[0] = pB;
	arrFuture[1] = pA;
	arrFuture[2] = pA;
	pAllFuture = xrtFutureAll(arrFuture, 3);
	testRequire(pAllFuture != NULL, "future All create failed");
	testRequire(xrtPromiseResolve(pPromiseA, &iValue),
		"future All first source resolve failed");
	testRequire(xrtFutureState(pAllFuture) == XFUTURE_PENDING,
		"future All completed before every slot");
	testRequire(xrtPromiseClose(pPromiseB),
		"future All second source close failed");
	testRequire(xrtFutureWait(pAllFuture) == XWAIT_OK, "future All wait failed");
	pAll = (const xfutureall*)xrtFutureValue(pAllFuture);
	testRequire((pAll != NULL) && (pAll->Count == 3),
		"future All result count mismatch");
	testRequire((pAll->Futures[0] == pB) && (pAll->Futures[1] == pA) &&
		(pAll->Futures[2] == pA), "future All input order mismatch");
	testRequire((xrtFutureState(pAll->Futures[0]) == XFUTURE_CLOSED) &&
		(xrtFutureValue(pAll->Futures[1]) == &iValue),
		"future All source outcomes mismatch");

	xrtFutureDestroy(pAllFuture);
	testFutureCombineDestroyPair(pPromiseA, pA);
	testFutureCombineDestroyPair(pPromiseB, pB);
}



/* 验证空 All 恒等元和预完成 Any 的稳定输入顺序。 */
static void testFutureCombineImmediate(void)
{
	xfuture* pA;
	xfuture* pB;
	xfuture* pAny;
	xfuture* pAllFuture;
	xfuture* arrFuture[2];
	xpromise* pPromiseA = testFutureCombinePair(&pA);
	xpromise* pPromiseB = testFutureCombinePair(&pB);
	const xfuturepick* pPick;
	const xfutureall* pAll;

	testRequire(xrtPromiseResolve(pPromiseA, NULL),
		"future immediate first resolve failed");
	testRequire(xrtPromiseResolve(pPromiseB, NULL),
		"future immediate second resolve failed");
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pAny = xrtFutureAny(arrFuture, 2);
	testRequire(pAny != NULL, "precompleted future Any create failed");
	pPick = (const xfuturepick*)xrtFutureValue(pAny);
	testRequire((pPick != NULL) && (pPick->Index == 0) &&
		(pPick->Future == pA), "precompleted Any did not prefer input order");

	pAllFuture = xrtFutureAll(NULL, 0);
	testRequire(pAllFuture != NULL, "empty future All create failed");
	pAll = (const xfutureall*)xrtFutureValue(pAllFuture);
	testRequire((pAll != NULL) && (pAll->Count == 0) &&
		(pAll->Futures == NULL), "empty future All result mismatch");

	xrtFutureDestroy(pAllFuture);
	xrtFutureDestroy(pAny);
	testFutureCombineDestroyPair(pPromiseA, pA);
	testFutureCombineDestroyPair(pPromiseB, pB);
}



/* 验证同一源重复出现时，完成批次不会因互相摘除而自等待。 */
static void testFutureCombineDuplicate(void)
{
	xfuture* pSource;
	xfuture* pAny;
	xfuture* arrFuture[2];
	xpromise* pPromise = testFutureCombinePair(&pSource);
	const xfuturepick* pPick;

	arrFuture[0] = pSource;
	arrFuture[1] = pSource;
	pAny = xrtFutureAny(arrFuture, 2);
	testRequire(pAny != NULL, "duplicate future Any create failed");
	testRequire(xrtPromiseResolve(pPromise, NULL),
		"duplicate future Any source resolve failed");
	pPick = (const xfuturepick*)xrtFutureValue(pAny);
	testRequire((pPick != NULL) && (pPick->Index == 0) &&
		(pPick->Future == pSource), "duplicate future Any selection mismatch");

	xrtFutureDestroy(pAny);
	testFutureCombineDestroyPair(pPromise, pSource);
}



/* 验证 Race 只请求败者取消，败者生产端仍决定最终终态。 */
static void testFutureCombineRace(void)
{
	xfuture* pA;
	xfuture* pB;
	xfuture* pRace;
	xfuture* arrFuture[2];
	xpromise* pPromiseA = testFutureCombinePair(&pA);
	xpromise* pPromiseB = testFutureCombinePair(&pB);
	xcancel* pLoserCancel = xrtFutureCancelToken(pB);
	const xfuturepick* pPick;
	int iValueA = 41;
	int iValueB = 42;

	testRequire(pLoserCancel != NULL, "future Race loser token failed");
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pRace = xrtFutureRace(arrFuture, 2);
	testRequire(pRace != NULL, "future Race create failed");
	testRequire(xrtPromiseResolve(pPromiseA, &iValueA),
		"future Race winner resolve failed");
	pPick = (const xfuturepick*)xrtFutureValue(pRace);
	testRequire((pPick != NULL) && (pPick->Index == 0) &&
		(pPick->Future == pA), "future Race winner mismatch");
	testRequire(xrtCancelRequested(pLoserCancel),
		"future Race did not request loser cancellation");
	testRequire(xrtFutureState(pB) == XFUTURE_PENDING,
		"future Race forged loser terminal state");
	testRequire(xrtPromiseResolve(pPromiseB, &iValueB),
		"future Race loser could not handle cancellation");
	testRequire(xrtFutureState(pB) == XFUTURE_RESOLVED,
		"future Race loser producer outcome was lost");

	xrtCancelDestroy(pLoserCancel);
	xrtFutureDestroy(pRace);
	testFutureCombineDestroyPair(pPromiseA, pA);
	testFutureCombineDestroyPair(pPromiseB, pB);
}



/* 验证取消组合器会摘除监听、确认自身取消并传播请求。 */
static void testFutureCombineCancel(void)
{
	xfuture* pA;
	xfuture* pB;
	xfuture* pAll;
	xfuture* arrFuture[2];
	xpromise* pPromiseA = testFutureCombinePair(&pA);
	xpromise* pPromiseB = testFutureCombinePair(&pB);
	xcancel* pCancelA = xrtFutureCancelToken(pA);
	xcancel* pCancelB = xrtFutureCancelToken(pB);

	testRequire((pCancelA != NULL) && (pCancelB != NULL),
		"future combine source token failed");
	arrFuture[0] = pA;
	arrFuture[1] = pB;
	pAll = xrtFutureAll(arrFuture, 2);
	testRequire(pAll != NULL, "cancelled future All create failed");
	testRequire(xrtFutureCancel(pAll), "future All cancel request failed");
	testRequire(xrtFutureState(pAll) == XFUTURE_CANCELLED,
		"future All did not confirm its own cancellation");
	testRequire(xrtCancelRequested(pCancelA) &&
		xrtCancelRequested(pCancelB),
		"future All cancellation did not reach sources");
	testRequire((xrtFutureState(pA) == XFUTURE_PENDING) &&
		(xrtFutureState(pB) == XFUTURE_PENDING),
		"future All cancellation forged source state");
	testRequire(xrtPromiseCancel(pPromiseA) && xrtPromiseCancel(pPromiseB),
		"future combine sources could not confirm cancellation");

	xrtCancelDestroy(pCancelA);
	xrtCancelDestroy(pCancelB);
	xrtFutureDestroy(pAll);
	testFutureCombineDestroyPair(pPromiseA, pA);
	testFutureCombineDestroyPair(pPromiseB, pB);
}



/* 验证参数拒绝不会留下半构造组合器。 */
static void testFutureCombineInvalid(void)
{
	xfuture* pFuture;
	xfuture* arrFuture[1] = { NULL };

	xrtClearError();
	testRequire(xrtFutureAny(NULL, 0) == NULL,
		"empty future Any unexpectedly succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"empty future Any error mismatch");
	xrtClearError();
	pFuture = xrtFutureAll(arrFuture, 1);
	testRequire(pFuture == NULL, "null future All source unexpectedly succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null future All source error mismatch");
	xrtClearError();
	pFuture = xrtFutureAll(arrFuture, (size_t)INT32_MAX);
	testRequire(pFuture == NULL,
		"future All reference-count overflow unexpectedly succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"future All reference-count overflow error mismatch");
}



/* 用深层单输入 All 链验证完成通知按迭代队列派发而不是递归消耗线程栈。 */
static void testFutureCombineDeepCascade(void)
{
	const size_t iCount = 16384;
	xfuture** pChain = (xfuture**)xrtMalloc(iCount * sizeof(xfuture*));
	xpromise* pPromise;
	int iValue = 73;

	testRequire(pChain != NULL, "deep future chain allocation failed");
	memset(pChain, 0, iCount * sizeof(xfuture*));
	pPromise = testFutureCombinePair(&pChain[0]);
	for ( size_t i = 1; i < iCount; i++ ) {
		pChain[i] = xrtFutureAll(&pChain[i - 1], 1);
		testRequire(pChain[i] != NULL, "deep future All create failed");
	}
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"deep future source resolve failed");
	testRequire(xrtFutureState(pChain[iCount - 1]) == XFUTURE_RESOLVED,
		"deep future cascade did not reach the final node");

	xrtPromiseDestroy(pPromise);
	for ( size_t i = iCount; i != 0; i-- ) {
		xrtFutureDestroy(pChain[i - 1]);
	}
	xrtFree(pChain);
}



/* 覆盖组合器的完成、取消、所有权和边界契约。 */
int main(void)
{
	testFutureCombineAny();
	testFutureCombineAll();
	testFutureCombineImmediate();
	testFutureCombineDuplicate();
	testFutureCombineRace();
	testFutureCombineCancel();
	testFutureCombineInvalid();
	testFutureCombineDeepCascade();
	printf("[PASS] future combine\n");
	return 0;
}
