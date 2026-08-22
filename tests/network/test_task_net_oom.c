#include "../test.h"



typedef struct testtasknetoom {
	xatomic32 Calls;
	xatomic32 Destroyed;
} testtasknetoom;



/* OOM 成功边界上的任务只记录执行次数。 */
static xtaskoutcome testTaskNetOomRun(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtasknetoom* pContext = (testtasknetoom*)pData;

	(void)pWorker;
	(void)pCancel;
	(void)pResult;
	(void)xrtAtomic32FetchAdd(&pContext->Calls, 1, XMEMORY_RELEASE);
	return XTASK_SUCCESS;
}



/* 只有真正受理的任务可以取得数据析构权。 */
static void testTaskNetOomDestroy(ptr pValue, ptr pData)
{
	testtasknetoom* pContext = (testtasknetoom*)pValue;

	(void)pData;
	(void)xrtAtomic32FetchAdd(
		&pContext->Destroyed,
		1,
		XMEMORY_RELEASE
	);
}



/* 逐个验证任务包装、Promise、Timer 和取消监听分配失败都完整回滚。 */
int main(void)
{
	xnetengineconfig tConfig;
	testtasknetoom Context;
	xtaskargs tArgs;
	xnetengine* pEngine;
	uint32 iFailures = 0;
	bool bSucceeded = false;

	memset(&Context, 0, sizeof(Context));
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Destroy = testTaskNetOomDestroy;
	xrtNetEngineConfigInit(&tConfig);
	tConfig.Backend = XNET_PORT_SELECT;
	tConfig.Workers = 1;
	tConfig.CommandCapacity = 64;
	tConfig.TimerLimit = 16;
	pEngine = xrtNetEngineCreate(&tConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"network task OOM engine start failed");

	for ( uint64 iOffset = 0; iOffset < 32u; iOffset++ ) {
		xfuture* pFuture;
		uint32 iDestroyed;
		bool bTriggered;

		iDestroyed = xrtAtomic32Load(
			&Context.Destroyed,
			XMEMORY_ACQUIRE
		);
		testRequire(xrtMemDebugFailAfter(iOffset),
			"network task OOM fault setup failed");
		pFuture = xrtTaskNetAfter(
			pEngine,
			0,
			testTaskNetOomRun,
			&Context,
			&tArgs,
			60000000u
		);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pFuture != NULL ) {
			testRequire(!bTriggered && xrtFutureCancel(pFuture),
				"network task OOM success cleanup failed");
			testRequire(xrtFutureWaitFor(pFuture, 3000000u) == XWAIT_OK,
				"network task OOM success did not cancel");
			testRequire(xrtFutureState(pFuture) == XFUTURE_CANCELLED,
				"network task OOM success state mismatch");
			xrtFutureDestroy(pFuture);
			bSucceeded = true;
			break;
		}
		iFailures++;
		testRequire(bTriggered &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"network task OOM error mismatch");
		xrtClearError();
		testRequire(xrtNetEngineStop(pEngine),
			"network task OOM cleanup stop failed");
		testRequire(
			xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE) ==
				iDestroyed,
			"network task OOM consumed caller data"
		);
		testRequire(xrtNetEngineStart(pEngine),
			"network task OOM cleanup restart failed");
	}
	testRequire(bSucceeded && (iFailures >= 4u),
		"network task OOM sweep missed an allocation boundary");
	testRequire(xrtAtomic32Load(&Context.Calls, XMEMORY_ACQUIRE) == 0,
		"network task OOM entered a cancelled callback");
	testRequire(xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE) == 1,
		"network task OOM accepted data destroy mismatch");
	testRequire(xrtNetEngineDestroy(pEngine),
		"network task OOM engine destroy failed");
	printf("[PASS] network task OOM\n");
	return 0;
}
