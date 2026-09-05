#include <stdio.h>
#include <string.h>
#include <xrt.h>



typedef struct exampleengine {
	xatomic32 Done;
} exampleengine;



/* 普通任务在亲和 Worker 上执行。 */
static void examplePost(xnetworker* pWorker, ptr pData)
{
	exampleengine* pState = (exampleengine*)pData;

	printf("worker %u handled a posted task\n", xrtNetWorkerIndex(pWorker));
	(void)xrtAtomic32FetchAdd(&pState->Done, 1, XMEMORY_RELEASE);
}



/* Timer 以明确终态回到同一个 Worker。 */
static void exampleTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	exampleengine* pState = (exampleengine*)pData;

	printf("worker %u completed timer %llu with result %d\n",
		xrtNetWorkerIndex(pWorker),
		(unsigned long long)Id,
		(int)Result);
	(void)xrtAtomic32FetchAdd(&pState->Done, 1, XMEMORY_RELEASE);
}



/*
 * 范例：network/engine —— 网络引擎：Worker 亲和的任务与定时器
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNetEngineConfigInit + Workers   引擎配置
 *   xrtNetEngineCreate / Start / Destroy   生命周期
 *   xrtNetEnginePost        向指定 Worker 投递任务
 *   xrtNetEngineAfter       延迟任务（微秒定时器）
 *   xrtNetWorkerIndex       回调里查自己跑在哪个 Worker
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/engine/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   worker 0 handled a posted task
 *   worker 1 completed timer 2 with result 0
 *
 * 引擎 = 事件后端（IOCP/epoll/kqueue/io_uring/select）
 *   + Worker 线程池。Worker 亲和是核心设计：连接绑定
 *   Worker，其上的回调/任务/定时器全部同线程执行——
 *   回调内无需加锁。主线程用原子计数 + deadline 等待，
 *   Destroy 前确保全部回调完成。
 */


/* 建立两个 Worker，分别演示立即任务和延迟任务。 */
int main(void)
{
	xnetengineconfig Config;
	exampleengine State;
	xnetengine* pEngine;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 2;
	pEngine = xrtNetEngineCreate(&Config);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	if ( !xrtNetEnginePost(pEngine, 0, examplePost, &State) ||
		 (xrtNetEngineAfter(
			pEngine,
			1,
			100000u,
			exampleTimer,
			&State
		) == 0) ) {
		(void)xrtNetEngineDestroy(pEngine);
		return 2;
	}
	iDeadline = xrtDeadlineAfter(2000000u);
	while ( xrtAtomic32Load(&State.Done, XMEMORY_ACQUIRE) != 2 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			(void)xrtNetEngineDestroy(pEngine);
			return 3;
		}
		xrtThreadYield();
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 4;
}
