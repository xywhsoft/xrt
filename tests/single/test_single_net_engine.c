#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件中的 Engine 必须真实启动 Worker 并执行任务和 Timer。 */
typedef struct testsingleengine {
	xatomic32 Posts;
	xatomic32 Timers;
} testsingleengine;



/* 记录单头文件 Worker 任务。 */
static void testSingleEnginePost(xnetworker* pWorker, ptr pData)
{
	testsingleengine* pState = (testsingleengine*)pData;

	if ( xrtNetWorkerIsCurrent(pWorker) ) {
		(void)xrtAtomic32FetchAdd(&pState->Posts, 1, XMEMORY_RELEASE);
	}
}



/* 记录单头文件 Timer 到期。 */
static void testSingleEngineTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testsingleengine* pState = (testsingleengine*)pData;

	if ( xrtNetWorkerIsCurrent(pWorker) && (Id != 0) &&
		 (Result == XNET_RESULT_OK) ) {
		(void)xrtAtomic32FetchAdd(&pState->Timers, 1, XMEMORY_RELEASE);
	}
}



/* 验证单头文件 Engine 的完整生命周期。 */
int main(void)
{
	xnetengineconfig Config;
	testsingleengine State;
	xnetengine* pEngine;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 1;
	Config.CommandCapacity = 16;
	Config.TimerLimit = 16;
	Config.EventBatch = 4;
	pEngine = xrtNetEngineCreate(&Config);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ||
		 !xrtNetEnginePost(pEngine, 0, testSingleEnginePost, &State) ||
		 (xrtNetEngineAfter(
			pEngine,
			0,
			0,
			testSingleEngineTimer,
			&State
		) == 0) ) {
		return 1;
	}
	iDeadline = xrtDeadlineAfter(2000000u);
	while ( (xrtAtomic32Load(&State.Posts, XMEMORY_ACQUIRE) != 1) ||
		 (xrtAtomic32Load(&State.Timers, XMEMORY_ACQUIRE) != 1) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 2;
		}
		xrtThreadYield();
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 3;
}
