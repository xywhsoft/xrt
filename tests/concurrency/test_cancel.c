#include "../test.h"
#include "../test_thread.h"



/* 普通监听回调记录触发次数。 */
static void testCancelCount(ptr pData)
{
	int* pCount = (int*)pData;

	(*pCount)++;
}



/* 回调内注销用例保存已经返回给调用方的监听。 */
typedef struct testcancelself {
	xcancelwatch* Watch;
	int Count;
} testcancelself;



/* 从回调自身注销监听，验证实现不会等待自己。 */
static void testCancelSelfUnwatch(ptr pData)
{
	testcancelself* pState = (testcancelself*)pData;

	pState->Count++;
	xrtCancelUnwatch(pState->Watch);
}



/* 并发注销用例用条件变量精确控制回调退出时机。 */
typedef struct testcancelrace {
	xmutex Lock;
	xcond Cond;
	xcancel* Cancel;
	xcancelwatch* Watch;
	bool Started;
	bool Release;
	bool UnwatchStarted;
	bool Unwatched;
	int Count;
} testcancelrace;



/* 阻塞回调直到主线程允许返回。 */
static void testCancelBlockingCallback(ptr pData)
{
	testcancelrace* pState = (testcancelrace*)pData;

	testRequire(xrtMutexLock(&pState->Lock), "cancel callback lock failed");
	pState->Started = true;
	pState->Count++;
	testRequire(xrtCondBroadcast(&pState->Cond), "cancel callback signal failed");
	while ( !pState->Release ) {
		testRequire(
			xrtCondWait(&pState->Cond, &pState->Lock) == XWAIT_OK,
			"cancel callback wait failed"
		);
	}
	testRequire(xrtMutexUnlock(&pState->Lock), "cancel callback unlock failed");
}



/* 在线程中请求取消。 */
static int testCancelRequestWorker(ptr pData)
{
	testcancelrace* pState = (testcancelrace*)pData;

	return xrtCancelRequest(pState->Cancel) ? 0 : 1;
}



/* 直接请求传入的取消令牌，供父子并发竞争测试使用。 */
static int testCancelDirectRequest(ptr pData)
{
	return xrtCancelRequest((xcancel*)pData) ? 0 : 1;
}



/* 在线程中注销监听，并在返回后发布完成状态。 */
static int testCancelUnwatchWorker(ptr pData)
{
	testcancelrace* pState = (testcancelrace*)pData;

	if ( !xrtMutexLock(&pState->Lock) ) {
		return 1;
	}
	pState->UnwatchStarted = true;
	(void)xrtCondBroadcast(&pState->Cond);
	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return 2;
	}
	xrtCancelUnwatch(pState->Watch);
	if ( !xrtMutexLock(&pState->Lock) ) {
		return 3;
	}
	pState->Unwatched = true;
	(void)xrtCondBroadcast(&pState->Cond);
	return xrtMutexUnlock(&pState->Lock) ? 0 : 4;
}



/* 验证父子传播、幂等请求、迟注册和回调内注销。 */
static void testCancelSemantics(void)
{
	xcancel* pParent = xrtCancelCreate();
	xcancel* pChild;
	xcancel* pRootChild;
	xcancelwatch* pWatch;
	int iCount = 0;
	int iLateCount = 0;
	int iChildCount = 0;
	testcancelself tSelf = { NULL, 0 };

	testRequire(pParent != NULL, "cancel parent create failed");
	pChild = xrtCancelChild(pParent);
	testRequire(pChild != NULL, "cancel child create failed");
	pWatch = xrtCancelWatch(pChild, testCancelCount, &iCount);
	testRequire(pWatch != NULL, "cancel child watch failed");
	testRequire(!xrtCancelTriggered(pWatch), "cancel watch started triggered");
	testRequire(xrtCancelRequest(pParent), "cancel parent request failed");
	testRequire(iCount == 1, "parent cancel did not invoke child callback once");
	testRequire(xrtCancelTriggered(pWatch), "parent cancel did not trigger child watch");
	testRequire(xrtCancelRequested(pChild), "child did not inherit parent cancellation");
	testRequire(!xrtCancelRequest(pParent), "repeated cancel request succeeded");
	testRequire(iCount == 1, "repeated cancel invoked callback again");
	testRequire(xrtCancelRequest(pChild), "first local child request failed");
	testRequire(iCount == 1, "child request invoked inherited callback again");
	xrtCancelUnwatch(pWatch);

	pWatch = xrtCancelWatch(pChild, testCancelCount, &iLateCount);
	testRequire(pWatch != NULL, "late cancel watch failed");
	testRequire(iLateCount == 1, "late cancel watch did not invoke immediately");
	testRequire(xrtCancelTriggered(pWatch), "late cancel watch was not triggered");
	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pParent);
	testRequire(xrtCancelRequested(pChild), "child lost retained cancelled parent");
	xrtCancelDestroy(pChild);

	pParent = xrtCancelCreate();
	testRequire(pParent != NULL, "independent cancel parent create failed");
	pChild = xrtCancelChild(pParent);
	testRequire(pChild != NULL, "independent cancel child create failed");
	pWatch = xrtCancelWatch(pChild, testCancelCount, &iChildCount);
	testRequire(pWatch != NULL, "independent child watch failed");
	testRequire(xrtCancelRequest(pChild), "independent child request failed");
	testRequire(iChildCount == 1, "child request did not invoke child callback");
	testRequire(!xrtCancelRequested(pParent), "child request propagated to parent");
	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pChild);
	xrtCancelDestroy(pParent);

	pParent = xrtCancelCreate();
	testRequire(pParent != NULL, "self unwatch cancel create failed");
	tSelf.Watch = xrtCancelWatch(pParent, testCancelSelfUnwatch, &tSelf);
	testRequire(tSelf.Watch != NULL, "self unwatch registration failed");
	testRequire(xrtCancelRequest(pParent), "self unwatch request failed");
	testRequire(tSelf.Count == 1, "self unwatch callback count mismatch");
	xrtCancelDestroy(pParent);

	pRootChild = xrtCancelChild(NULL);
	testRequire(pRootChild != NULL, "null-parent child creation failed");
	testRequire(!xrtCancelRequested(NULL), "null cancellation query returned true");
	testRequire(xrtCancelRef(pRootChild) == pRootChild, "cancel ref failed");
	xrtCancelDestroy(pRootChild);
	xrtCancelDestroy(pRootChild);
}



/* 验证祖先与子令牌同时请求取消时同一监听仍只回调一次。 */
static void testCancelAncestorRace(void)
{
	for ( size_t i = 0; i < 128; i++ ) {
		xcancel* pParent = xrtCancelCreate();
		xcancel* pChild;
		xcancelwatch* pWatch;
		testthread arrThread[2];
		int iCount = 0;

		testRequire(pParent != NULL, "cancel ancestor race parent create failed");
		pChild = xrtCancelChild(pParent);
		testRequire(pChild != NULL, "cancel ancestor race child create failed");
		pWatch = xrtCancelWatch(pChild, testCancelCount, &iCount);
		testRequire(pWatch != NULL, "cancel ancestor race watch failed");
		arrThread[0].Proc = testCancelDirectRequest;
		arrThread[0].Data = pParent;
		arrThread[1].Proc = testCancelDirectRequest;
		arrThread[1].Data = pChild;
		testThreadsStart(arrThread, 2);
		testThreadsJoin(arrThread, 2);
		testRequire(
			(arrThread[0].Result == 0) && (arrThread[1].Result == 0),
			"cancel ancestor race request failed"
		);
		testRequire(iCount == 1, "cancel ancestor race invoked callback more than once");
		xrtCancelUnwatch(pWatch);
		xrtCancelDestroy(pChild);
		xrtCancelDestroy(pParent);
	}
}



/* 验证注销会完整移除仍未触发的父链节点。 */
static void testCancelUnwatchBeforeRequest(void)
{
	xcancel* pParent = xrtCancelCreate();
	xcancel* pChild;
	xcancelwatch* pWatch;
	int iCount = 0;

	testRequire(pParent != NULL, "cancel unlink parent create failed");
	pChild = xrtCancelChild(pParent);
	testRequire(pChild != NULL, "cancel unlink child create failed");
	pWatch = xrtCancelWatch(pChild, testCancelCount, &iCount);
	testRequire(pWatch != NULL, "cancel unlink watch failed");
	xrtCancelUnwatch(pWatch);
	testRequire(xrtCancelRequest(pParent), "cancel unlink parent request failed");
	testRequire(xrtCancelRequest(pChild), "cancel unlink child request failed");
	testRequire(iCount == 0, "unwatched callback remained linked");
	xrtCancelDestroy(pChild);
	xrtCancelDestroy(pParent);
}



/* 验证其他线程注销会等待正在执行的回调完成。 */
static void testCancelConcurrentUnwatch(void)
{
	testcancelrace tState;
	testthread tRequest;
	testthread tUnwatch;
	xwaitresult tWait;

	memset(&tState, 0, sizeof(tState));
	testRequire(xrtMutexInit(&tState.Lock), "cancel race mutex init failed");
	testRequire(xrtCondInit(&tState.Cond), "cancel race cond init failed");
	tState.Cancel = xrtCancelCreate();
	testRequire(tState.Cancel != NULL, "cancel race token create failed");
	tState.Watch = xrtCancelWatch(
		tState.Cancel,
		testCancelBlockingCallback,
		&tState
	);
	testRequire(tState.Watch != NULL, "cancel race watch failed");

	tRequest.Proc = testCancelRequestWorker;
	tRequest.Data = &tState;
	testThreadsStart(&tRequest, 1);
	testRequire(xrtMutexLock(&tState.Lock), "cancel race main lock failed");
	while ( !tState.Started ) {
		testRequire(
			xrtCondWait(&tState.Cond, &tState.Lock) == XWAIT_OK,
			"cancel race start wait failed"
		);
	}
	testRequire(xrtMutexUnlock(&tState.Lock), "cancel race start unlock failed");

	tUnwatch.Proc = testCancelUnwatchWorker;
	tUnwatch.Data = &tState;
	testThreadsStart(&tUnwatch, 1);
	testRequire(xrtMutexLock(&tState.Lock), "cancel race probe lock failed");
	while ( !tState.UnwatchStarted ) {
		testRequire(
			xrtCondWait(&tState.Cond, &tState.Lock) == XWAIT_OK,
			"cancel unwatch start wait failed"
		);
	}
	if ( !tState.Unwatched ) {
		tWait = xrtCondWaitFor(&tState.Cond, &tState.Lock, 20000);
		testRequire(tWait == XWAIT_TIMEOUT, "unwatch returned before callback completion");
	}
	tState.Release = true;
	testRequire(xrtCondBroadcast(&tState.Cond), "cancel race release signal failed");
	testRequire(xrtMutexUnlock(&tState.Lock), "cancel race release unlock failed");

	testThreadsJoin(&tUnwatch, 1);
	testThreadsJoin(&tRequest, 1);
	testRequire(tUnwatch.Result == 0, "cancel unwatch worker failed");
	testRequire(tRequest.Result == 0, "cancel request worker failed");
	testRequire(tState.Unwatched, "cancel unwatch did not finish");
	testRequire(tState.Count == 1, "cancel race callback count mismatch");
	xrtCancelDestroy(tState.Cancel);
	testRequire(xrtCondUnit(&tState.Cond), "cancel race cond unit failed");
	testRequire(xrtMutexUnit(&tState.Lock), "cancel race mutex unit failed");
}



/* 验证空参数错误和允许为空的便捷操作。 */
static void testCancelErrors(void)
{
	xcancel* pCancel = xrtCancelCreate();

	testRequire(pCancel != NULL, "cancel error token create failed");
	xrtClearError();
	testRequire(!xrtCancelRequest(NULL), "null cancel request succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null cancel request error mismatch");
	xrtClearError();
	testRequire(xrtCancelWatch(NULL, testCancelCount, NULL) == NULL, "null token watch succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null token watch error mismatch");
	xrtClearError();
	testRequire(xrtCancelWatch(pCancel, NULL, NULL) == NULL, "null callback watch succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null callback error mismatch");
	xrtClearError();
	testRequire(!xrtCancelTriggered(NULL), "null watch query succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null watch query error mismatch");
	xrtCancelUnwatch(NULL);
	xrtCancelDestroy(NULL);
	xrtCancelDestroy(pCancel);
}



/* 运行通用取消模型的语义和并发边界测试。 */
int main(void)
{
	testCancelSemantics();
	testCancelAncestorRace();
	testCancelUnwatchBeforeRequest();
	testCancelConcurrentUnwatch();
	testCancelErrors();

	printf("[PASS] cancel\n");
	return 0;
}
