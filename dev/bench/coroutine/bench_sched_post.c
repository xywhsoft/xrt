#include "../bench_common.h"

#define XRT_MODULE_COROUTINE_SCHEDULER
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 跨线程投递状态只使用原子计数协调生产者和调度线程。 */
typedef struct benchcoroutinepost {
	xcosched* Scheduler;
	uint64 Target;
	volatile long Submitted;
	volatile long Processed;
	volatile long Failed;
} benchcoroutinepost;



/* 调度线程消费一次已投递过程。 */
static void benchCoroutinePostProc(xcosched* pScheduler, ptr pData)
{
	benchcoroutinepost* pState = (benchcoroutinepost*)pData;

	(void)pScheduler;
	(void)xbenchAtomicInc(&pState->Processed);
}



/* 外部线程连续向单线程调度器投递最小过程。 */
static int32 benchCoroutinePostThread(ptr pData)
{
	benchcoroutinepost* pState = (benchcoroutinepost*)pData;

	for ( uint64 i = 0; i < pState->Target; i++ ) {
		if ( !xrtCoSchedPost(
			pState->Scheduler,
			benchCoroutinePostProc,
			pState
		) ) {
			(void)xbenchAtomicInc(&pState->Failed);
			return 1;
		}
		(void)xbenchAtomicInc(&pState->Submitted);
	}
	return 0;
}



/* 测量跨线程入队、唤醒和调度线程消费组成的完整 post 路径。 */
int main(int argc, char** argv)
{
	uint64 iTarget = xbenchArgU64(argc, argv, 1, 200000u);
	benchcoroutinepost State;
	xbenchtimer Timer;
	xthread* pThread;
	uint64 iElapsed = 0;
	int iResult = 1;

	if ( iTarget == 0 ) {
		return 1;
	}
	memset(&State, 0, sizeof(State));
	State.Target = iTarget;
	State.Scheduler = xrtCoSchedCreate();
	if ( State.Scheduler == NULL ) {
		return 2;
	}
	pThread = xrtThreadCreate(benchCoroutinePostThread, &State, 0);
	if ( pThread == NULL ) {
		goto Exit;
	}

	xbenchTimerStart(&Timer);
	while ( (uint64)xbenchAtomicLoad(&State.Processed) < iTarget ) {
		xwaitresult Result = xrtCoSchedPollFor(State.Scheduler, 1000000u);

		if (
			(Result == XWAIT_ERROR) ||
			(xbenchAtomicLoad(&State.Failed) != 0)
		) {
			goto Join;
		}
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	iResult = 0;

Join:
	if ( xrtThreadWait(pThread) != XWAIT_OK ) {
		iResult = 3;
	}
	if ( xrtThreadExitCode(pThread) != 0 ) {
		iResult = 4;
	}
	xrtThreadDestroy(pThread);
	if ( iResult == 0 ) {
		xbenchPrintMetricU64(
			"coroutine_post_processed",
			(uint64)xbenchAtomicLoad(&State.Processed)
		);
		xbenchPrintMetricDouble(
			"coroutine_posts_per_sec",
			xbenchSafeRate(iTarget, iElapsed)
		);
	}

Exit:
	if ( !xrtCoSchedDestroy(State.Scheduler) ) {
		iResult = 5;
	}
	return iResult;
}
