/*
 * 范例：network/engine_tour —— Engine/Worker 自省、任务与定时器
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Engine 自省】 xrtNetEngineState / WorkerCount / Worker /
 *                  Current / Stats / Pin / Unpin
 *   【任务】       xrtNetEnginePost（亲和 Worker）
 *                  xrtNetPostPending（嵌入式 Post 队列状态）
 *   【定时器】     __xrtNetEngineSchedule（绝对截止时间）
 *                  xrtNetEngineTimerCancel（异步取消）
 *                  xrtNetEngineTimerCancelCurrent（Worker 内取消）
 *   【Worker】     xrtNetWorkerEngine / Index / IsCurrent / Port /
 *                  BufPool / Alloc / Free / OperationId / Stats
 *   【Completion】 xrtNetCompletionInit（借用过程与数据）
 * 模块宏：XRT_MODULE_NET_ENGINE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/engine_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   engine: create stopped -> start running, 2 workers ok
 *   engine: pin/unpin + operation-id unique ok
 *   engine: worker introspection inside task ok
 *   engine: post-pending true->false ok
 *   engine: schedule fire + async cancel ok
 *   engine: timer-cancel-current inside callback ok
 *   engine: stats aggregate posts>=2 timers>=2 ok
 *
 * 关键约束：Current/BufPool/CancelCurrent 有线程/时序门槛——
 *   Current 只在 Worker 线程内返回自身；BufPool 只能从该
 *   Worker 的回调调用；CancelCurrent 只在 Timer 所属 Worker
 *   上生效（本例用同亲和的两个 Timer 演示"回调内取消另一个"）。
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <xrt.h>

/* Worker 内自省结果：主线程核对。 */
typedef struct exampletask {
	xnetengine* pEngine;
	xatomic32 Done;
	bool bCurrent;
	bool bIsCurrent;
	bool bEngineOk;
	bool bIndexOk;
	bool bPortOk;
	bool bBufPoolOk;
	bool bAllocOk;
	bool bFreeOk;
	bool bIdOk;
	uint64 IdInWorker;
} exampletask;

/* 定时器结果收集：区分到期/取消。 */
typedef struct exampletimers {
	xatomic32 Fired;
	xatomic32 Cancelled;
	xatomic32 CancelCurrentOk;
	uint64 iLongId;
} exampletimers;

/* Hold one Worker so Pending can be observed before the posted task runs. */
typedef struct examplepostbarrier {
	xatomic32 Entered;
	xatomic32 Release;
} examplepostbarrier;

static void examplePostBarrier(xnetworker* pWorker, ptr pData)
{
	examplepostbarrier* pBarrier = (examplepostbarrier*)pData;
	(void)pWorker;
	xrtAtomic32Store(&pBarrier->Entered, 1u, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pBarrier->Release, XMEMORY_ACQUIRE) == 0u ) {
		xrtThreadYield();
	}
}

/* 前置声明：最简任务与 Completion 过程定义在 main 之后。 */
static void exampleSimpleTask(xnetworker* pWorker, ptr pData);
static void exampleCompletionProc(xnetworker* pWorker,
	const xnetportevent* pEvent, ptr pData);

/* Worker 任务：全部 Worker 族自省 + 内存分配。 */
static void exampleWorkerTask(xnetworker* pWorker, ptr pData)
{
	exampletask* pTask = (exampletask*)pData;
	xnetbufpool* pPool;
	xnetenginestats Stats;
	ptr pBlock;
	uint64 IdA;
	uint64 IdB;

	pTask->bCurrent =
		(xrtNetEngineCurrent(pTask->pEngine) == pWorker);
	pTask->bIsCurrent = xrtNetWorkerIsCurrent(pWorker);
	pTask->bEngineOk =
		(xrtNetWorkerEngine(pWorker) == pTask->pEngine);
	pTask->bIndexOk = (xrtNetWorkerIndex(pWorker) == 0u);
	pTask->bPortOk = (xrtNetWorkerPort(pWorker) != NULL);
	/* BufPool 只能从本 Worker 回调内调用。 */
	pPool = xrtNetWorkerBufPool(pWorker);
	pTask->bBufPoolOk = (pPool != NULL);
	/* 分级缓存：分配清零块再归还。 */
	pBlock = xrtNetWorkerAlloc(pWorker, 32u);
	pTask->bAllocOk = (pBlock != NULL) &&
		(((const uint8*)pBlock)[0] == 0u) &&
		(((const uint8*)pBlock)[31] == 0u);
	xrtNetWorkerFree(pWorker, pBlock, 32u);
	pTask->bFreeOk = true;
	/* 操作 ID：Engine 内唯一且非零。 */
	IdA = xrtNetWorkerOperationId(pWorker);
	IdB = xrtNetWorkerOperationId(pWorker);
	pTask->bIdOk = (IdA != 0u) && (IdB != 0u) && (IdA != IdB);
	pTask->IdInWorker = IdB;
	/* Worker 统计快照可从任意线程读取。 */
	xrtAtomic32Store(&pTask->Done, 1u, XMEMORY_RELEASE);
	(void)Stats;
}

/* 短定时器：到期回调。 */
static void exampleFireTimer(xnetworker* pWorker, uint64 Id,
	xnetresult Result, ptr pData)
{
	exampletimers* pTimers = (exampletimers*)pData;

	(void)pWorker;
	(void)Id;
	if ( Result == XNET_RESULT_OK ) {
		(void)xrtAtomic32FetchAdd(&pTimers->Fired, 1u, XMEMORY_RELEASE);
	}
}

/* 长定时器：被取消后以 CANCELLED 终结。 */
static void exampleLongTimer(xnetworker* pWorker, uint64 Id,
	xnetresult Result, ptr pData)
{
	exampletimers* pTimers = (exampletimers*)pData;

	(void)pWorker;
	(void)Id;
	if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtAtomic32FetchAdd(&pTimers->Cancelled, 1u, XMEMORY_RELEASE);
	}
}

/* 载体定时器：到期时在自己的 Worker 上取消长定时器。 */
static void exampleCarrierTimer(xnetworker* pWorker, uint64 Id,
	xnetresult Result, ptr pData)
{
	exampletimers* pTimers = (exampletimers*)pData;

	(void)Id;
	if ( Result != XNET_RESULT_OK ) {
		return;
	}
	/* CancelCurrent 只在本 Worker（同亲和）上立即生效。 */
	xrtAtomic32Store(&pTimers->CancelCurrentOk,
		xrtNetEngineTimerCancelCurrent(xrtNetWorkerEngine(pWorker), pTimers->iLongId)
			? 1u : 0u, XMEMORY_RELEASE);
}

/* 自旋等待辅助：有界轮询完成标志并获取回调结果。 */
static bool exampleSpinUntil(const xatomic32* pFlag, uint32 iTimeoutMs)
{
	for ( uint32 i = 0; i < iTimeoutMs; i++ ) {
		if ( xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) != 0u ) {
			return true;
		}
		xrtSleep(1u);
	}
	return xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) != 0u;
}

int main(void)
{
	xnetengineconfig Config;
	xnetengine* pEngine = NULL;
	xnetworker* pWorker0 = NULL;
	xnetworker* pWorker1 = NULL;
	xnetworkerstats WorkerStats;
	xnetenginestats EngineStats;
	xnetcompletion Completion;
	exampletask Task;
	exampletimers Timers;
	examplepostbarrier Barrier = { XRT_ATOMIC32_INIT(0), XRT_ATOMIC32_INIT(0) };
	xnetpost BarrierPost;
	uint64 IdOpA;
	uint64 IdOpB;
	uint64 IdFire;
	xatomic32 TaskDone = XRT_ATOMIC32_INIT(0);
	int iResult = 1;

	/* ---- 生命周期：创建即 STOPPED，Start 后 RUNNING。 ---- */
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 2;
	pEngine = xrtNetEngineCreate(&Config);
	if ( (pEngine == NULL) ||
		(xrtNetEngineState(pEngine) != XNET_ENGINE_STOPPED) ||
		!xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	while ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
		xrtSleep(1u);
	}
	if ( (xrtNetEngineWorkerCount(pEngine) != 2u) ||
		((pWorker0 = xrtNetEngineWorker(pEngine, 0u)) == NULL) ||
		((pWorker1 = xrtNetEngineWorker(pEngine, 1u)) == NULL) ||
		(xrtNetEngineWorker(pEngine, 99u) != NULL) ||
		/* 主线程不属于任何 Worker → Current 为空。 */
		(xrtNetEngineCurrent(pEngine) != NULL) ) {
		goto Cleanup;
	}
	printf("engine: create stopped -> start running, 2 workers ok\n");

	/* ---- 生命周期占用：Pin/Unpin 成对；操作 ID 全局唯一。 ---- */
	if ( !xrtNetEnginePin(pEngine) ||
		!xrtNetEngineUnpin(pEngine) ) {
		goto Cleanup;
	}
	IdOpA = xrtNetWorkerOperationId(pWorker0);
	IdOpB = xrtNetWorkerOperationId(pWorker1);
	if ( (IdOpA == 0u) || (IdOpB == 0u) || (IdOpA == IdOpB) ) {
		goto Cleanup;
	}
	printf("engine: pin/unpin + operation-id unique ok\n");

	/* ---- Worker 自省：投递到 0 号亲和，回调内逐项核对。 ---- */
	memset(&Task, 0, sizeof(Task));
	Task.pEngine = pEngine;
	if ( !xrtNetEnginePost(pEngine, 0u, exampleWorkerTask,
			(ptr)&Task) ||
		!exampleSpinUntil(&Task.Done, 2000u) ||
		!Task.bCurrent || !Task.bIsCurrent ||
		!Task.bEngineOk || !Task.bIndexOk || !Task.bPortOk ||
		!Task.bBufPoolOk || !Task.bAllocOk || !Task.bFreeOk ||
		!Task.bIdOk ||
		(Task.IdInWorker == IdOpA) ||
		(Task.IdInWorker == IdOpB) ) {
		goto Cleanup;
	}
	/* Worker 统计：至少执行了本任务。 */
	if ( !xrtNetWorkerStats(pWorker0, &WorkerStats) ||
		(WorkerStats.PostsExecuted < 1u) ) {
		goto Cleanup;
	}
	printf("engine: worker introspection inside task ok\n");

	/* ---- 嵌入式 Post：受理后在队列为 Pending，执行后清除。 ---- */
	{
		xnetpost Post;

		if ( !xrtNetPostInit(&BarrierPost) ||
			!xrtNetPost(pWorker0, &BarrierPost, examplePostBarrier, &Barrier) ||
			!exampleSpinUntil(&Barrier.Entered, 2000u) ) {
			goto Cleanup;
		}
		if ( !xrtNetPostInit(&Post) ||
			!xrtNetPost(pWorker0, &Post,
				exampleSimpleTask, (ptr)&TaskDone) ) {
			goto Cleanup;
		}
		/* Worker 在屏障中，Post 此时仍在队列内。 */
		if ( !xrtNetPostPending(&Post) ) {
			goto Cleanup;
		}
		xrtAtomic32Store(&Barrier.Release, 1u, XMEMORY_RELEASE);
		if ( !exampleSpinUntil(&TaskDone, 2000u) ||
			xrtNetPostPending(&Post) ) {
			goto Cleanup;
		}
	}
	printf("engine: post-pending true->false ok\n");

	/* ---- 定时器：绝对截止时间到期 + 异步取消长定时器。 ---- */
	memset(&Timers, 0, sizeof(Timers));
	IdFire = xrtNetEngineSchedule(pEngine, 0u,0, exampleFireTimer, (ptr)&Timers);
	Timers.iLongId = xrtNetEngineSchedule(pEngine, 0u,3600000, exampleLongTimer,
		(ptr)&Timers);
	if ( (IdFire == 0u) || (Timers.iLongId == 0u) ||
		!xrtNetEngineTimerCancel(pEngine, Timers.iLongId) ) {
		goto Cleanup;
	}
	/* 自旋等两个回调都终结：一个 OK 一个 CANCELLED。 */
	for ( uint32 i = 0; i < 3000u; i++ ) {
		if ( (xrtAtomic32Load(&Timers.Fired, XMEMORY_ACQUIRE) >= 1) && (xrtAtomic32Load(&Timers.Cancelled, XMEMORY_ACQUIRE) >= 1) ) {
			break;
		}
		xrtSleep(1u);
	}
	if ( (xrtAtomic32Load(&Timers.Fired, XMEMORY_ACQUIRE) < 1) || (xrtAtomic32Load(&Timers.Cancelled, XMEMORY_ACQUIRE) < 1) ) {
		goto Cleanup;
	}
	printf("engine: schedule fire + async cancel ok\n");

	/* ---- CancelCurrent：同 Worker 的载体定时器内取消长定时器。 ---- */
	memset(&Timers, 0, sizeof(Timers));
	Timers.iLongId = xrtNetEngineSchedule(pEngine, 1u,3600000, exampleLongTimer,
		(ptr)&Timers);
	if ( (Timers.iLongId == 0u) ||
		(xrtNetEngineSchedule(pEngine, 1u,0,
			exampleCarrierTimer, (ptr)&Timers) == 0u) ||
		!exampleSpinUntil(&Timers.CancelCurrentOk, 2000u) ) {
		goto Cleanup;
	}
	/* 长定时器以 CANCELLED 终结。 */
	for ( uint32 i = 0; i < 3000u; i++ ) {
		if ( xrtAtomic32Load(&Timers.Cancelled, XMEMORY_ACQUIRE) >= 1 ) {
			break;
		}
		xrtSleep(1u);
	}
	if ( xrtAtomic32Load(&Timers.CancelCurrentOk, XMEMORY_ACQUIRE) == 0u || (xrtAtomic32Load(&Timers.Cancelled, XMEMORY_ACQUIRE) < 1) ) {
		goto Cleanup;
	}
	printf("engine: timer-cancel-current inside callback ok\n");

	/* ---- 聚合统计：全部 Worker 的 Posts/Timers 计入。 ---- */
	if ( !xrtNetEngineStats(pEngine, &EngineStats) ||
		(EngineStats.PostsExecuted < 2u) ||
		(EngineStats.TimersFired < 2u) ) {
		goto Cleanup;
	}
	/* CompletionInit：借用过程与数据的初始化形态。 */
	xrtNetCompletionInit(&Completion, exampleCompletionProc,
		(ptr)&TaskDone);
	printf("engine: stats aggregate posts>=2 timers>=2 ok\n");
	iResult = 0;

Cleanup:
	xrtAtomic32Store(&Barrier.Release, 1u, XMEMORY_RELEASE);
	if ( (pEngine != NULL) &&
		(xrtNetEngineState(pEngine) != XNET_ENGINE_STOPPED) ) {
		xrtNetEngineStop(pEngine);
	}
	if ( pEngine != NULL ) {
		xrtNetEngineDestroy(pEngine);
	}
	return iResult;
}

/* 最简任务：发布一个完成标志。 */
static void exampleSimpleTask(xnetworker* pWorker, ptr pData)
{
	xatomic32* pFlag = (xatomic32*)pData;

	(void)pWorker;
	xrtAtomic32Store(pFlag, 1u, XMEMORY_RELEASE);
}

/* Completion 过程：本例不触发端口事件，仅演示初始化形态。 */
static void exampleCompletionProc(xnetworker* pWorker,
	const xnetportevent* pEvent, ptr pData)
{
	(void)pWorker;
	(void)pEvent;
	(void)pData;
}
