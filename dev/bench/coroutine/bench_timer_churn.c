#include "../xnet2/bench_common.h"
#include "../../../xrt.h"

typedef struct {
	uint64_t iWaitTarget;
	uint64_t iWaitCount;
} __bench_co_timer_ctx;

static int64 __benchCoMonoMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64)GetTickCount64();
	#else
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return ((int64)tNow.tv_sec * 1000LL) + ((int64)tNow.tv_nsec / 1000000LL);
	#endif
}

static void __benchCoTimerMain(ptr pArg)
{
	__bench_co_timer_ctx* pCtx = (__bench_co_timer_ctx*)pArg;

	if ( !pCtx ) return;

	while ( pCtx->iWaitCount < pCtx->iWaitTarget ) {
		pCtx->iWaitCount++;
		xrtCoSleepUntil(__benchCoMonoMs());
	}
}

int main(int argc, char** argv)
{
	uint64_t iWaitTarget = xbenchArgU64(argc, argv, 1, 200000u);
	__bench_co_timer_ctx tCtx;
	xcosched* pSched = NULL;
	xbenchtimer tTimer;
	uint64_t iElapsedNs = 0u;

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.iWaitTarget = iWaitTarget;

	printf("xrt coroutine bench_timer_churn\n");
	printf("backend=%s\n", xrtCoGetBackendName());
	printf("timer_wait_target=%" PRIu64 "\n", iWaitTarget);

	if ( !xrtInit() ) return 1;

	pSched = xrtCoSchedCreate();
	if ( !pSched ) {
		xrtUnit();
		return 2;
	}

	if ( !xrtCoSchedSpawn(pSched, __benchCoTimerMain, &tCtx, 0u) ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 3;
	}

	xbenchTimerStart(&tTimer);
	xrtCoSchedRun(pSched);
	xbenchTimerStop(&tTimer);

	iElapsedNs = xbenchTimerElapsedNs(&tTimer);

	xbenchPrintMetricU64("timer_waits", tCtx.iWaitCount);
	xbenchPrintMetricU64("elapsed_ns", iElapsedNs);
	xbenchPrintMetricDouble("timer_waits_per_sec", xbenchSafeRate(tCtx.iWaitCount, iElapsedNs));
	xbenchPrintMetricDouble("ns_per_timer_wait", tCtx.iWaitCount ? ((double)iElapsedNs / (double)tCtx.iWaitCount) : 0.0);

	xrtCoSchedDestroy(pSched);
	xrtUnit();
	return 0;
}
