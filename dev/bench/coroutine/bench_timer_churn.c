#include "../bench_common.h"

#define XRT_MODULE_COROUTINE_SCHEDULER
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 即时定时器状态记录计划执行次数和实际完成次数。 */
typedef struct benchcoroutinetimer {
	uint64 Target;
	uint64 Completed;
} benchcoroutinetimer;



/* 每轮注册一个立即到期的截止时间，覆盖定时器插入、摘除和恢复。 */
static ptr benchCoroutineTimerProc(ptr pData)
{
	benchcoroutinetimer* pState = (benchcoroutinetimer*)pData;

	while ( pState->Completed < pState->Target ) {
		if ( xrtCoSleepUntil(xrtDeadlineAfter(0)) != XWAIT_OK ) {
			return NULL;
		}
		pState->Completed++;
	}
	return pState;
}



/* 测量调度器即时定时器的完整周转吞吐。 */
int main(int argc, char** argv)
{
	uint64 iTarget = xbenchArgU64(argc, argv, 1, 200000u);
	benchcoroutinetimer State;
	xbenchtimer Timer;
	xcosched* pScheduler;
	xcoro* pCoroutine;
	uint64 iElapsed;
	int iResult = 1;

	if ( iTarget == 0 ) {
		return 1;
	}
	memset(&State, 0, sizeof(State));
	State.Target = iTarget;
	pScheduler = xrtCoSchedCreate();
	if ( pScheduler == NULL ) {
		return 2;
	}
	pCoroutine = xrtCoSpawn(
		pScheduler,
		benchCoroutineTimerProc,
		&State,
		NULL
	);
	if ( pCoroutine == NULL ) {
		goto Exit;
	}

	xbenchTimerStart(&Timer);
	if ( !xrtCoSchedRun(pScheduler) ) {
		goto DestroyCoroutine;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	if (
		(State.Completed != iTarget) ||
		(xrtCoResult(pCoroutine) != &State)
	) {
		goto DestroyCoroutine;
	}
	xbenchPrintMetricDouble(
		"coroutine_timers_per_sec",
		xbenchSafeRate(iTarget, iElapsed)
	);
	iResult = 0;

DestroyCoroutine:
	if ( !xrtCoDestroy(pCoroutine) ) {
		iResult = 3;
	}

Exit:
	if ( !xrtCoSchedDestroy(pScheduler) ) {
		iResult = 4;
	}
	return iResult;
}
