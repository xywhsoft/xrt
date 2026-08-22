#include "../test.h"
#include "../test_thread.h"
#include <xrt/future_bridge.h>



/* 等待线程只观察桥的最终装配状态。 */
typedef struct testfuturebridgewait {
	xfuturebridge* Bridge;
	bool Ready;
} testfuturebridgewait;



/* 记录桥转发的协作取消次数。 */
static void testFutureBridgeCancel(ptr pData)
{
	xatomic32* pCount = (xatomic32*)pData;

	(void)xrtAtomic32FetchAdd(pCount, 1, XMEMORY_ACQ_REL);
}



/* 在线程中等待创建方发布装配结果。 */
static int testFutureBridgeWaitThread(ptr pData)
{
	testfuturebridgewait* pWait = (testfuturebridgewait*)pData;

	pWait->Ready = xrtFutureBridgeWait(pWait->Bridge);
	return 0;
}



/* 释放一个没有交给异步适配器托管的测试桥。 */
static void testFutureBridgeDestroy(
	xfuturebridge* pBridge,
	xfuture* pFuture
)
{
	xpromise* pPromise = xrtFutureBridgePromise(pBridge);

	xrtFutureBridgeUnwatch(pBridge);
	(void)xrtPromiseCancel(pPromise);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
}



/* 验证正常发布后，Future 取消只转发一次且桥可以安全注销。 */
static void testFutureBridgeReady(void)
{
	xfuturebridge tBridge;
	xatomic32 CancelCount = XRT_ATOMIC32_INIT(0);
	xfuture* pFuture = xrtFutureBridgeCreate(&tBridge, NULL);

	testRequire(pFuture != NULL, "future bridge create failed");
	testRequire(xrtFutureBridgeWatch(
		&tBridge,
		testFutureBridgeCancel,
		&CancelCount
	), "future bridge watch failed");
	testRequire(xrtFutureBridgeReady(&tBridge),
		"future bridge ready publish failed");
	testRequire(xrtFutureBridgeWait(&tBridge),
		"ready future bridge was not publishable");
	testRequire(xrtFutureCancel(pFuture),
		"future bridge cancellation request failed");
	testRequire(!xrtFutureCancel(pFuture),
		"future bridge accepted repeated cancellation");
	testRequire(xrtAtomic32Load(
		&CancelCount,
		XMEMORY_ACQUIRE
	) == 1, "future bridge forwarded cancellation more than once");
	testFutureBridgeDestroy(&tBridge, pFuture);
}



/* 验证已取消父令牌会在监听装配时立即且仅触发一次。 */
static void testFutureBridgeCancelledParent(void)
{
	xfuturebridge tBridge;
	xatomic32 CancelCount = XRT_ATOMIC32_INIT(0);
	xcancel* pParent = xrtCancelCreate();
	xfuture* pFuture;

	testRequire(pParent != NULL, "future bridge parent create failed");
	testRequire(xrtCancelRequest(pParent),
		"future bridge parent cancellation failed");
	pFuture = xrtFutureBridgeCreate(&tBridge, pParent);
	testRequire(pFuture != NULL,
		"future bridge cancelled-parent create failed");
	testRequire(xrtFutureBridgeWatch(
		&tBridge,
		testFutureBridgeCancel,
		&CancelCount
	), "future bridge cancelled-parent watch failed");
	testRequire(xrtAtomic32Load(
		&CancelCount,
		XMEMORY_ACQUIRE
	) == 1, "future bridge missed inherited cancellation");
	testRequire(xrtFutureBridgeReady(&tBridge),
		"future bridge parent ready publish failed");
	testFutureBridgeDestroy(&tBridge, pFuture);
	xrtCancelDestroy(pParent);
}



/* 反复竞争等待线程与成功、失败发布，压实 acquire/release 状态交接。 */
static void testFutureBridgePublishRace(void)
{
	for ( size_t iRound = 0; iRound < 200; iRound++ ) {
		xfuturebridge tBridge;
		testfuturebridgewait tWait = { &tBridge, false };
		testthread tThread;
		xfuture* pFuture = xrtFutureBridgeCreate(
			&tBridge,
			NULL
		);
		bool bReady = (iRound & 1) == 0;

		testRequire(pFuture != NULL,
			"future bridge race create failed");
		memset(&tThread, 0, sizeof(tThread));
		tThread.Proc = testFutureBridgeWaitThread;
		tThread.Data = &tWait;
		testThreadsStart(&tThread, 1);
		if ( bReady ) {
			testRequire(xrtFutureBridgeReady(&tBridge),
				"future bridge race ready publish failed");
		} else {
			testRequire(xrtFutureBridgeFail(&tBridge),
				"future bridge race failure publish failed");
		}
		testThreadsJoin(&tThread, 1);
		testRequire((tThread.Result == 0) &&
			(tWait.Ready == bReady),
			"future bridge publish race state mismatch");
		testFutureBridgeDestroy(&tBridge, pFuture);
	}
}



/* 覆盖公开桥的参数检查、显式 Init 与一次性发布契约。 */
static void testFutureBridgeContract(void)
{
	xfuturebridge tBridge;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	memset(&tBridge, 0, sizeof(tBridge));
	xrtClearError();
	testRequire(!xrtFutureBridgeInit(NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"future bridge null init error mismatch");
	xrtClearError();
	testRequire(!xrtFutureBridgeInit(&tBridge, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"future bridge null promise error mismatch");

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire((pPromise != NULL) && (pFuture != NULL),
		"future bridge explicit pair create failed");
	testRequire(xrtFutureBridgeInit(&tBridge, pPromise),
		"future bridge explicit init failed");
	testRequire(xrtFutureBridgePromise(&tBridge) == pPromise,
		"future bridge explicit promise mismatch");

	xrtClearError();
	testRequire(!xrtFutureBridgeWatch(&tBridge, NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"future bridge null callback error mismatch");
	xrtClearError();
	testRequire(xrtFutureBridgeFail(&tBridge),
		"future bridge explicit failure publish failed");
	testRequire(!xrtFutureBridgeWait(&tBridge),
		"future bridge explicit failure remained ready");
	xrtClearError();
	testRequire(!xrtFutureBridgeReady(&tBridge) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"future bridge repeated publish error mismatch");

	xrtFutureBridgeUnwatch(&tBridge);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
}



/* 覆盖异步桥装配、继承取消和跨线程状态发布。 */
int main(void)
{
	testFutureBridgeReady();
	testFutureBridgeCancelledParent();
	testFutureBridgePublishRace();
	testFutureBridgeContract();
	printf("[PASS] future bridge\n");
	return 0;
}
