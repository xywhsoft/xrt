#include "../test.h"



typedef struct testtasknet {
	xnetengine* Engine;
	xatomic32 Calls;
	xatomic32 Destroyed;
	int Value;
} testtasknet;



/* 记录每个已经受理任务的数据析构。 */
static void testTaskNetDestroy(ptr pValue, ptr pData)
{
	testtasknet* pContext = (testtasknet*)pValue;

	(void)pData;
	(void)xrtAtomic32FetchAdd(
		&pContext->Destroyed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证任务在指定 Engine Worker 上运行并返回借用值。 */
static xtaskoutcome testTaskNetRun(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtasknet* pContext = (testtasknet*)pData;

	testRequire(pWorker != NULL, "network task worker is null");
	testRequire(xrtNetWorkerEngine(pWorker) == pContext->Engine,
		"network task ran on the wrong engine");
	testRequire(xrtNetEngineCurrent(pContext->Engine) == pWorker,
		"network task did not expose its current worker");
	testRequire(!xrtCancelRequested(pCancel),
		"cancelled network task entered the user callback");
	(void)xrtAtomic32FetchAdd(&pContext->Calls, 1, XMEMORY_RELEASE);
	pResult->Value = &pContext->Value;
	return XTASK_SUCCESS;
}



/* 等待并验证一个成功网络任务。 */
static void testTaskNetResolved(xfuture* pFuture, testtasknet* pContext)
{
	testRequire(pFuture != NULL, "network task submission failed");
	testRequire(xrtFutureWaitFor(pFuture, 3000000u) == XWAIT_OK,
		"network task did not complete");
	testRequire(xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		"network task did not resolve");
	testRequire(xrtFutureValue(pFuture) == &pContext->Value,
		"network task value mismatch");
}



/* 覆盖立即、延迟、取消、Engine 停止和拒绝后的所有权合同。 */
int main(void)
{
	xnetengineconfig tConfig;
	xtaskargs tArgs;
	testtasknet Context;
	xcancel* pParent;
	xfuture* pFuture;
	xnetengine* pEngine;
	uint32 iDestroyed;

	memset(&Context, 0, sizeof(Context));
	memset(&tArgs, 0, sizeof(tArgs));
	xrtNetEngineConfigInit(&tConfig);
	tConfig.Backend = XNET_PORT_SELECT;
	tConfig.Workers = 1;
	tConfig.CommandCapacity = 64;
	tConfig.TimerLimit = 32;
	pEngine = xrtNetEngineCreate(&tConfig);
	testRequire(pEngine != NULL, "network task engine create failed");
	testRequire(xrtNetEngineStart(pEngine),
		"network task engine start failed");
	Context.Engine = pEngine;
	Context.Value = 73;
	tArgs.Destroy = testTaskNetDestroy;

	/* 三条成功路径必须使用同一任务结果与所有权语义。 */
	pFuture = xrtTaskNet(pEngine, 0, testTaskNetRun, &Context, &tArgs);
	testTaskNetResolved(pFuture, &Context);
	xrtFutureDestroy(pFuture);
	pFuture = xrtTaskNetAfter(
		pEngine,
		0,
		testTaskNetRun,
		&Context,
		&tArgs,
		1000u
	);
	testTaskNetResolved(pFuture, &Context);
	xrtFutureDestroy(pFuture);
	pFuture = xrtTaskNetUntil(
		pEngine,
		0,
		testTaskNetRun,
		&Context,
		&tArgs,
		xrtDeadlineAfter(1000u)
	);
	testTaskNetResolved(pFuture, &Context);
	xrtFutureDestroy(pFuture);

	/* Future 取消必须摘除长 Timer，且不能进入用户过程。 */
	pFuture = xrtTaskNetAfter(
		pEngine,
		0,
		testTaskNetRun,
		&Context,
		&tArgs,
		5000000u
	);
	testRequire(pFuture != NULL, "cancelled network task submit failed");
	testRequire(xrtFutureCancel(pFuture),
		"network task cancel request failed");
	testRequire(xrtFutureWaitFor(pFuture, 3000000u) == XWAIT_OK,
		"cancelled network task did not finish promptly");
	testRequire(xrtFutureState(pFuture) == XFUTURE_CANCELLED,
		"cancelled network task terminal state mismatch");
	xrtFutureDestroy(pFuture);

	/* 已取消父作用域仍返回一个受理后的取消 Future。 */
	pParent = xrtCancelCreate();
	testRequire((pParent != NULL) && xrtCancelRequest(pParent),
		"parent cancel setup failed");
	tArgs.Cancel = pParent;
	pFuture = xrtTaskNetAfter(
		pEngine,
		0,
		testTaskNetRun,
		&Context,
		&tArgs,
		5000000u
	);
	testRequire(pFuture != NULL, "pre-cancelled network task submit failed");
	testRequire(xrtFutureWaitFor(pFuture, 3000000u) == XWAIT_OK,
		"pre-cancelled network task did not finish");
	testRequire(xrtFutureState(pFuture) == XFUTURE_CANCELLED,
		"pre-cancelled network task state mismatch");
	xrtFutureDestroy(pFuture);
	xrtCancelDestroy(pParent);
	tArgs.Cancel = NULL;

	/* Engine 停止必须以结构化 CLOSED 错误终结已受理 Timer。 */
	pFuture = xrtTaskNetAfter(
		pEngine,
		0,
		testTaskNetRun,
		&Context,
		&tArgs,
		5000000u
	);
	testRequire(pFuture != NULL, "closing network task submit failed");
	testRequire(xrtNetEngineStop(pEngine),
		"network task engine stop failed");
	testRequire(xrtFutureWaitFor(pFuture, 3000000u) == XWAIT_OK,
		"closed network task did not finish");
	testRequire(xrtFutureState(pFuture) == XFUTURE_FAILED,
		"closed network task did not fail");
	testRequire((xrtFutureError(pFuture) != NULL) &&
		(xrtErrorKind(xrtFutureError(pFuture)) == XERR_CLOSED),
		"closed network task error mismatch");
	xrtFutureDestroy(pFuture);

	/* 拒绝路径不能接管或析构调用方数据。 */
	iDestroyed = xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE);
	pFuture = xrtTaskNet(pEngine, 0, testTaskNetRun, &Context, &tArgs);
	testRequire(pFuture == NULL, "stopped engine accepted network task");
	testRequire(
		xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE) == iDestroyed,
		"rejected network task destroyed caller data"
	);
	xrtClearError();

	testRequire(xrtAtomic32Load(&Context.Calls, XMEMORY_ACQUIRE) == 3,
		"network task callback count mismatch");
	testRequire(xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE) == 6,
		"accepted network task destroy count mismatch");
	testRequire(xrtNetEngineDestroy(pEngine),
		"network task engine destroy failed");
	printf("[PASS] network task Future bridge\n");
	return 0;
}
