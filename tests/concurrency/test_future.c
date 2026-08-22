#include "../test.h"



/* Owned 值析构只记录次数，测试值本身保存在栈上。 */
static void testFutureDestroyValue(ptr pValue, ptr pData)
{
	int* pDestroyed = (int*)pData;

	(void)pValue;
	(*pDestroyed)++;
}



/* 验证 Promise 透传覆盖全部终态，并正确保留成功值的源所有者。 */
static void testFutureForward(void)
{
	xfuture* pSource;
	xfuture* pOutput;
	xpromise* pSourcePromise;
	xpromise* pOutputPromise;
	xerror* pError;
	int iValue = 73;
	int iDestroyed = 0;

	/* 成功值使用源 Future 保活，避免借用值在输出销毁前失效。 */
	pSourcePromise = xrtPromiseCreate(&pSource, NULL);
	pOutputPromise = xrtPromiseCreate(&pOutput, NULL);
	testRequire((pSourcePromise != NULL) && (pOutputPromise != NULL),
		"future forward resolve setup failed");
	testRequire(xrtPromiseResolveOwned(
		pSourcePromise,
		&iValue,
		testFutureDestroyValue,
		&iDestroyed
	), "future forward owned resolve failed");
	xrtPromiseDestroy(pSourcePromise);
	testRequire(xrtPromiseForward(pOutputPromise, pSource),
		"future resolved forward failed");
	xrtFutureDestroy(pSource);
	testRequire(iDestroyed == 0, "forwarded source value was released early");
	testRequire((xrtFutureState(pOutput) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pOutput) == &iValue), "forwarded success mismatch");
	xrtPromiseDestroy(pOutputPromise);
	xrtFutureDestroy(pOutput);
	testRequire(iDestroyed == 1, "forwarded source value destructor mismatch");

	/* 失败终态增加错误引用，不依赖源 Future 的后续生命周期。 */
	pSourcePromise = xrtPromiseCreate(&pSource, NULL);
	pOutputPromise = xrtPromiseCreate(&pOutput, NULL);
	pError = xrtErrorCreate(XERR_PROTOCOL, "test.forward", 9, "forward failed");
	testRequire((pSourcePromise != NULL) && (pOutputPromise != NULL) &&
		(pError != NULL), "future forward failure setup failed");
	testRequire(xrtPromiseReject(pSourcePromise, pError),
		"future forward source reject failed");
	xrtErrorFree(pError);
	testRequire(xrtPromiseForward(pOutputPromise, pSource),
		"future failed forward failed");
	xrtPromiseDestroy(pSourcePromise);
	xrtFutureDestroy(pSource);
	testRequire((xrtFutureState(pOutput) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pOutput)) == XERR_PROTOCOL),
		"forwarded failure mismatch");
	xrtPromiseDestroy(pOutputPromise);
	xrtFutureDestroy(pOutput);

	/* 取消与关闭保持原终态，不把控制状态降级成普通失败。 */
	pSourcePromise = xrtPromiseCreate(&pSource, NULL);
	pOutputPromise = xrtPromiseCreate(&pOutput, NULL);
	testRequire((pSourcePromise != NULL) && (pOutputPromise != NULL) &&
		xrtPromiseCancel(pSourcePromise) &&
		xrtPromiseForward(pOutputPromise, pSource),
		"future cancelled forward failed");
	testRequire(xrtFutureState(pOutput) == XFUTURE_CANCELLED,
		"forwarded cancellation mismatch");
	xrtPromiseDestroy(pSourcePromise);
	xrtPromiseDestroy(pOutputPromise);
	xrtFutureDestroy(pSource);
	xrtFutureDestroy(pOutput);

	pSourcePromise = xrtPromiseCreate(&pSource, NULL);
	pOutputPromise = xrtPromiseCreate(&pOutput, NULL);
	testRequire((pSourcePromise != NULL) && (pOutputPromise != NULL) &&
		xrtPromiseClose(pSourcePromise) &&
		xrtPromiseForward(pOutputPromise, pSource),
		"future closed forward failed");
	testRequire(xrtFutureState(pOutput) == XFUTURE_CLOSED,
		"forwarded close mismatch");
	xrtPromiseDestroy(pSourcePromise);
	xrtPromiseDestroy(pOutputPromise);
	xrtFutureDestroy(pSource);
	xrtFutureDestroy(pOutput);

	/* Pending 源与自透传都必须失败，且不能改变目标终态。 */
	pSourcePromise = xrtPromiseCreate(&pSource, NULL);
	pOutputPromise = xrtPromiseCreate(&pOutput, NULL);
	testRequire((pSourcePromise != NULL) && (pOutputPromise != NULL),
		"future invalid forward setup failed");
	xrtClearError();
	testRequire(!xrtPromiseForward(pOutputPromise, pSource),
		"future forwarded a pending source");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_AGAIN) &&
		(xrtFutureState(pOutput) == XFUTURE_PENDING),
		"pending forward contract mismatch");
	xrtClearError();
	testRequire(!xrtPromiseForward(pSourcePromise, pSource),
		"future accepted self forward");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtFutureState(pSource) == XFUTURE_PENDING),
		"self forward contract mismatch");
	xrtPromiseDestroy(pSourcePromise);
	xrtPromiseDestroy(pOutputPromise);
	xrtFutureDestroy(pSource);
	xrtFutureDestroy(pOutput);
}



/* Watch 状态分别记录通知和所有权释放次数。 */
typedef struct test_future_watch_state {
	int Notified;
	int Released;
} test_future_watch_state;



/* Future 完成时记录一次无分配通知。 */
static void testFutureWatchNotify(ptr pData)
{
	test_future_watch_state* pState =
		(test_future_watch_state*)pData;

	pState->Notified++;
}



/* Watch 离开等待链后记录一次所有权释放。 */
static void testFutureWatchRelease(ptr pData)
{
	test_future_watch_state* pState =
		(test_future_watch_state*)pData;

	pState->Released++;
}



/* 验证 Watch 的完成、已就绪和主动摘除契约。 */
static void testFutureWatch(void)
{
	test_future_watch_state State = { 0, 0 };
	xfuturewatch Watch;
	xfuture* pFuture;
	xpromise* pPromise;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(
		(pPromise != NULL) &&
		xrtFutureWatchInit(
			&Watch,
			testFutureWatchNotify,
			testFutureWatchRelease,
			&State
		) &&
		(xrtFutureWatchAdd(pFuture, &Watch) ==
		 XFUTURE_WATCH_PENDING) &&
		xrtPromiseResolve(pPromise, NULL) &&
		(State.Notified == 1) &&
		(State.Released == 1),
		"Future Watch completion mismatch"
	);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);

	State.Notified = 0;
	State.Released = 0;
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(
		(pPromise != NULL) &&
		xrtPromiseResolve(pPromise, NULL) &&
		xrtFutureWatchInit(
			&Watch,
			testFutureWatchNotify,
			testFutureWatchRelease,
			&State
		) &&
		(xrtFutureWatchAdd(pFuture, &Watch) ==
		 XFUTURE_WATCH_READY) &&
		(State.Notified == 0) &&
		(State.Released == 0),
		"ready Future Watch acquired ownership"
	);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(
		(pPromise != NULL) &&
		xrtFutureWatchInit(
			&Watch,
			testFutureWatchNotify,
			testFutureWatchRelease,
			&State
		) &&
		(xrtFutureWatchAdd(pFuture, &Watch) ==
		 XFUTURE_WATCH_PENDING) &&
		xrtFutureWatchDetach(pFuture, &Watch) &&
		(State.Notified == 0) &&
		(State.Released == 1) &&
		xrtPromiseResolve(pPromise, NULL) &&
		(State.Notified == 0),
		"detached Future Watch was notified"
	);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
}



/* 验证创建、结果、所有权、错误、取消、关闭和生产端引用。 */
int main(void)
{
	xfuture* pFuture;
	xpromise* pPromise;
	xcancel* pToken;
	xcancel* pParent;
	xerror* pError;
	xfutureresult tResult;
	int iValue = 42;
	int iDestroyed = 0;
	int iRejectedDestroyed = 0;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire((pPromise != NULL) && (pFuture != NULL), "future pair create failed");
	testRequire(xrtFutureState(pFuture) == XFUTURE_PENDING, "new future state mismatch");
	testRequire(!xrtFutureDone(pFuture), "new future was already done");
	xrtClearError();
	testRequire(!xrtFutureResult(pFuture, &tResult), "pending future exposed a result");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "pending result error mismatch");
	pToken = xrtFutureCancelToken(pFuture);
	testRequire(pToken != NULL, "future cancel token failed");
	testRequire(xrtFutureCancel(pFuture), "future cancel request failed");
	testRequire(xrtCancelRequested(pToken), "future cancel request did not reach token");
	testRequire(!xrtFutureCancel(pFuture), "duplicate future cancel request succeeded");
	testRequire(xrtFutureState(pFuture) == XFUTURE_PENDING,
		"cancel request forged a terminal state");
	testRequire(xrtPromiseResolve(pPromise, &iValue), "future resolve failed");
	testRequire(xrtFutureWait(pFuture) == XWAIT_OK, "resolved future wait failed");
	testRequire(xrtFutureState(pFuture) == XFUTURE_RESOLVED, "resolved state mismatch");
	testRequire(xrtFutureValue(pFuture) == &iValue, "resolved value mismatch");
	testRequire(xrtFutureResult(pFuture, &tResult), "resolved result copy failed");
	testRequire((tResult.State == XFUTURE_RESOLVED) && (tResult.Value == &iValue) &&
		(tResult.Error == NULL), "resolved result fields mismatch");
	testRequire(!xrtPromiseResolve(pPromise, NULL), "future accepted a second terminal result");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "duplicate completion error mismatch");
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	xrtCancelDestroy(pToken);

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire((pPromise != NULL) && xrtPromiseResolveOwned(
		pPromise, &iValue, testFutureDestroyValue, &iDestroyed),
		"owned future resolve failed");
	testRequire(
		!xrtPromiseResolveOwned(
			pPromise,
			&iValue,
			testFutureDestroyValue,
			&iRejectedDestroyed
		),
		"owned future accepted a duplicate terminal result"
	);
	testRequire(
		(iRejectedDestroyed == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"failed owned resolve consumed caller ownership"
	);
	testFutureDestroyValue(&iValue, &iRejectedDestroyed);
	testRequire(
		iRejectedDestroyed == 1,
		"caller could not release rejected owned value"
	);
	xrtPromiseDestroy(pPromise);
	testRequire(iDestroyed == 0, "owned value destroyed while future remained alive");
	xrtFutureDestroy(pFuture);
	testRequire(iDestroyed == 1, "owned value destructor count mismatch");

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	pError = xrtErrorCreate(XERR_PROTOCOL, "test.future", 7, "future failed");
	testRequire((pPromise != NULL) && (pError != NULL), "failed future setup failed");
	testRequire(xrtPromiseReject(pPromise, pError), "future reject failed");
	xrtErrorFree(pError);
	testRequire(xrtFutureState(pFuture) == XFUTURE_FAILED, "failed future state mismatch");
	testRequire(xrtErrorKind(xrtFutureError(pFuture)) == XERR_PROTOCOL,
		"future error borrow mismatch");
	xrtClearError();
	testRequire(xrtFutureValue(pFuture) == NULL, "failed future exposed a value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL, "failed future raise mismatch");
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire((pPromise != NULL) && xrtPromiseCancel(pPromise), "future terminal cancel failed");
	testRequire(xrtFutureState(pFuture) == XFUTURE_CANCELLED, "cancelled state mismatch");
	xrtClearError();
	(void)xrtFutureValue(pFuture);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CANCELLED,
		"cancelled future value error mismatch");
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(pPromise != NULL, "close-on-drop pair create failed");
	pToken = xrtPromiseCancelToken(pPromise);
	testRequire(pToken != NULL, "promise cancel token failed");
	xrtPromiseDestroy(pPromise);
	testRequire(xrtFutureState(pFuture) == XFUTURE_CLOSED, "promise drop did not close future");
	testRequire(xrtCancelRequested(pToken), "promise drop did not request cancellation");
	xrtClearError();
	(void)xrtFutureValue(pFuture);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CLOSED, "closed future error mismatch");
	xrtCancelDestroy(pToken);
	xrtFutureDestroy(pFuture);

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(pPromise != NULL, "promise ref pair create failed");
	testRequire(xrtPromiseRef(pPromise) == pPromise, "promise ref failed");
	xrtPromiseDestroy(pPromise);
	testRequire(xrtFutureState(pFuture) == XFUTURE_PENDING,
		"non-final promise release closed future");
	xrtPromiseDestroy(pPromise);
	testRequire(xrtFutureState(pFuture) == XFUTURE_CLOSED,
		"final promise release did not close future");
	xrtFutureDestroy(pFuture);

	pParent = xrtCancelCreate();
	testRequire(pParent != NULL, "future parent cancel create failed");
	pPromise = xrtPromiseCreate(&pFuture, pParent);
	testRequire(pPromise != NULL, "parented future create failed");
	pToken = xrtFutureCancelToken(pFuture);
	testRequire(pToken != NULL, "parented future token failed");
	testRequire(xrtCancelRequest(pParent), "parent cancel request failed");
	testRequire(xrtCancelRequested(pToken), "future token did not inherit parent cancel");
	testRequire(xrtFutureState(pFuture) == XFUTURE_PENDING,
		"parent cancellation forged future terminal state");
	testRequire(xrtPromiseCancel(pPromise), "parented future cancel completion failed");
	xrtCancelDestroy(pToken);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	xrtCancelDestroy(pParent);

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	pToken = xrtCancelCreate();
	testRequire((pPromise != NULL) && (pToken != NULL), "future wait setup failed");
	testRequire(xrtFutureWaitFor(pFuture, 0) == XWAIT_TIMEOUT, "future zero wait mismatch");
	testRequire(xrtCancelRequest(pToken), "future waiter cancel request failed");
	testRequire(xrtFutureWaitUntilCancel(pFuture, XRT_DEADLINE_NEVER, pToken) ==
		XWAIT_CANCELLED, "future cancelled wait mismatch");
	testRequire(xrtPromiseClose(pPromise), "future explicit close failed");
	testRequire(xrtFutureWait(pFuture) == XWAIT_OK, "closed future wait mismatch");
	xrtCancelDestroy(pToken);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);

	testFutureWatch();
	testFutureForward();

	printf("[PASS] future\n");
	return 0;
}
