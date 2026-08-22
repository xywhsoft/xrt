#include "../test.h"



typedef struct testtaskgroupnet {
	xnetengine* Engine;
	xatomic32 Calls;
	xatomic32 Destroyed;
	int Value;
} testtaskgroupnet;



/* 记录任务组已经受理的数据析构。 */
static void testTaskGroupNetDestroy(ptr pValue, ptr pData)
{
	testtaskgroupnet* pContext = (testtaskgroupnet*)pValue;

	(void)pData;
	(void)xrtAtomic32FetchAdd(
		&pContext->Destroyed,
		1,
		XMEMORY_RELEASE
	);
}



/* 返回当前 Worker 上产生的借用结果。 */
static xtaskoutcome testTaskGroupNetRun(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskgroupnet* pContext = (testtaskgroupnet*)pData;

	testRequire(xrtNetEngineCurrent(pContext->Engine) == pWorker,
		"group network task ran on the wrong worker");
	testRequire(!xrtCancelRequested(pCancel),
		"cancelled group network task entered its callback");
	(void)xrtAtomic32FetchAdd(&pContext->Calls, 1, XMEMORY_RELEASE);
	pResult->Value = &pContext->Value;
	return XTASK_SUCCESS;
}



/* 覆盖任务组原子预留、正常排空、取消传播和拒绝所有权。 */
int main(void)
{
	xnetengineconfig tConfig;
	xtaskgroupstats tStats;
	xtaskargs tArgs;
	testtaskgroupnet Context;
	xtaskgroup* pGroup;
	xnetengine* pEngine;
	xfuture* pFirst;
	xfuture* pSecond;
	uint32 iDestroyed;

	memset(&Context, 0, sizeof(Context));
	memset(&tArgs, 0, sizeof(tArgs));
	xrtNetEngineConfigInit(&tConfig);
	tConfig.Workers = 1;
	tConfig.CommandCapacity = 64;
	tConfig.TimerLimit = 32;
	pEngine = xrtNetEngineCreate(&tConfig);
	testRequire(pEngine != NULL, "group network task engine create failed");
	testRequire(xrtNetEngineStart(pEngine),
		"group network task engine start failed");
	Context.Engine = pEngine;
	Context.Value = 41;
	tArgs.Destroy = testTaskGroupNetDestroy;

	/* 立即与延迟任务必须共同进入同一个动态作用域。 */
	pGroup = xrtTaskGroupCreate(NULL);
	testRequire(pGroup != NULL, "network task group create failed");
	pFirst = xrtTaskGroupNet(
		pGroup,
		pEngine,
		0,
		testTaskGroupNetRun,
		&Context,
		&tArgs
	);
	pSecond = xrtTaskGroupNetAfter(
		pGroup,
		pEngine,
		0,
		testTaskGroupNetRun,
		&Context,
		&tArgs,
		1000u
	);
	testRequire((pFirst != NULL) && (pSecond != NULL),
		"network task group submission failed");
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"network task group wait failed");
	testRequire((xrtFutureState(pFirst) == XFUTURE_RESOLVED) &&
		(xrtFutureState(pSecond) == XFUTURE_RESOLVED),
		"network task group child state mismatch");
	testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
		(tStats.Added == 2) && (tStats.Completed == 2) &&
		(tStats.Succeeded == 2) && (tStats.Active == 0),
		"network task group statistics mismatch");
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pSecond);

	/* 已关闭组必须在启动 Engine 操作前拒绝，并保留数据所有权。 */
	iDestroyed = xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE);
	pFirst = xrtTaskGroupNet(
		pGroup,
		pEngine,
		0,
		testTaskGroupNetRun,
		&Context,
		&tArgs
	);
	testRequire(pFirst == NULL, "closed group accepted a network task");
	testRequire(
		xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE) == iDestroyed,
		"closed group destroyed rejected network task data"
	);
	xrtClearError();
	xrtTaskGroupDestroy(pGroup);

	/* 组取消必须传播到长 Timer 并等待其真实取消终态。 */
	pGroup = xrtTaskGroupCreate(NULL);
	testRequire(pGroup != NULL, "cancel network task group create failed");
	pFirst = xrtTaskGroupNetUntil(
		pGroup,
		pEngine,
		0,
		testTaskGroupNetRun,
		&Context,
		&tArgs,
		xrtDeadlineAfter(5000000u)
	);
	testRequire(pFirst != NULL, "cancel network task group submit failed");
	testRequire(xrtTaskGroupCancel(pGroup),
		"network task group cancel failed");
	testRequire(xrtTaskGroupWaitFor(pGroup, 3000000u) == XWAIT_OK,
		"cancelled network task group did not drain");
	testRequire(xrtFutureState(pFirst) == XFUTURE_CANCELLED,
		"cancelled group network task state mismatch");
	xrtFutureDestroy(pFirst);
	xrtTaskGroupDestroy(pGroup);

	testRequire(xrtAtomic32Load(&Context.Calls, XMEMORY_ACQUIRE) == 2,
		"group network task callback count mismatch");
	testRequire(xrtAtomic32Load(&Context.Destroyed, XMEMORY_ACQUIRE) == 3,
		"group network task destroy count mismatch");
	testRequire(xrtNetEngineDestroy(pEngine),
		"group network task engine destroy failed");
	printf("[PASS] network task group Future bridge\n");
	return 0;
}
