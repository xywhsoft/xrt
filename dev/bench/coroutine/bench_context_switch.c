#include "../xnet2/bench_common.h"
#include "../../../xrt.h"

typedef struct {
	uint64_t iYieldTarget;
	uint64_t iYieldCount;
} __bench_co_switch_ctx;

static void __benchCoSwitchMain(ptr pArg)
{
	__bench_co_switch_ctx* pCtx = (__bench_co_switch_ctx*)pArg;

	if ( !pCtx ) return;

	while ( pCtx->iYieldCount < pCtx->iYieldTarget ) {
		pCtx->iYieldCount++;
		xrtCoYield();
	}
}

int main(int argc, char** argv)
{
	uint64_t iYieldTarget = xbenchArgU64(argc, argv, 1, 500000u);
	__bench_co_switch_ctx tCtx;
	xbenchtimer tTimer;
	xcoro pCo = NULL;
	uint64_t iResumeCount = 0u;
	uint64_t iSwitchCount = 0u;
	uint64_t iElapsedNs = 0u;

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.iYieldTarget = iYieldTarget;

	printf("xrt coroutine bench_context_switch\n");
	printf("backend=%s\n", xrtCoGetBackendName());
	printf("yield_target=%" PRIu64 "\n", iYieldTarget);

	if ( !xrtInit() ) return 1;

	pCo = xrtCoCreate(__benchCoSwitchMain, &tCtx, 0u);
	if ( !pCo ) {
		xrtUnit();
		return 2;
	}

	xbenchTimerStart(&tTimer);
	while ( xrtCoGetState(pCo) != XRT_CO_DEAD ) {
		if ( !xrtCoResume(pCo) ) {
			xrtCoDestroy(pCo);
			xrtUnit();
			return 3;
		}
		iResumeCount++;
	}
	xbenchTimerStop(&tTimer);

	iElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iSwitchCount = iResumeCount + tCtx.iYieldCount;

	xbenchPrintMetricU64("resume_calls", iResumeCount);
	xbenchPrintMetricU64("yield_calls", tCtx.iYieldCount);
	xbenchPrintMetricU64("context_switches", iSwitchCount);
	xbenchPrintMetricU64("elapsed_ns", iElapsedNs);
	xbenchPrintMetricDouble("switches_per_sec", xbenchSafeRate(iSwitchCount, iElapsedNs));
	xbenchPrintMetricDouble("ns_per_switch", iSwitchCount ? ((double)iElapsedNs / (double)iSwitchCount) : 0.0);

	xrtCoDestroy(pCo);
	xrtUnit();
	return 0;
}
