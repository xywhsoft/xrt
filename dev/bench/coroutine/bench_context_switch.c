#include "../bench_common.h"

#define XRT_MODULE_COROUTINE
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 上下文切换状态只保存目标次数和已完成次数，避免测量路径引入额外同步。 */
typedef struct benchcoroutineswitch {
	uint64 Target;
	uint64 Completed;
} benchcoroutineswitch;



/* 每次循环执行一次协程到调用方再返回的完整让出路径。 */
static ptr benchCoroutineSwitchProc(ptr pData)
{
	benchcoroutineswitch* pState = (benchcoroutineswitch*)pData;

	if ( pState == NULL ) {
		return NULL;
	}
	while ( pState->Completed < pState->Target ) {
		pState->Completed++;
		if ( xrtCoYield() != XWAIT_OK ) {
			return NULL;
		}
	}
	return pState;
}



/* 测量当前平台后端的直接 resume/yield 切换吞吐与单次成本。 */
int main(int argc, char** argv)
{
	uint64 iTarget = xbenchArgU64(argc, argv, 1, 1000000u);
	benchcoroutineswitch State;
	xbenchtimer Timer;
	xcoro* pCoroutine;
	uint64 iResume = 0;
	uint64 iSwitches;
	uint64 iElapsed;
	int iResult = 1;

	if ( iTarget == 0 ) {
		return 1;
	}
	memset(&State, 0, sizeof(State));
	State.Target = iTarget;
	pCoroutine = xrtCoCreate(benchCoroutineSwitchProc, &State, NULL);
	if ( pCoroutine == NULL ) {
		return 2;
	}

	xbenchTimerStart(&Timer);
	while ( xrtCoState(pCoroutine) != XCORO_DONE ) {
		if ( !xrtCoResume(pCoroutine) ) {
			goto Exit;
		}
		iResume++;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	iSwitches = iResume + State.Completed;

	printf("coroutine_backend: %s\n", xrtCoBackend());
	xbenchPrintMetricU64("coroutine_switches", iSwitches);
	xbenchPrintMetricU64("coroutine_switch_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"coroutine_switches_per_sec",
		xbenchSafeRate(iSwitches, iElapsed)
	);
	xbenchPrintMetricDouble(
		"coroutine_ns_per_switch",
		iSwitches != 0 ? ((double)iElapsed / (double)iSwitches) : 0.0
	);
	iResult = 0;

Exit:
	if ( !xrtCoDestroy(pCoroutine) ) {
		iResult = 3;
	}
	if ( !xrtCoThreadDetach() ) {
		iResult = 4;
	}
	return iResult;
}
