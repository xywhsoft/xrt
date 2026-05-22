#include "../xnet2/bench_common.h"
#include "../../../xrt.h"

typedef struct {
	xcosched* pSched;
	xcoro pTarget;
	uint64_t iWakeTarget;
	volatile long iProcessedCount;
	volatile long iSleepSeq;
	volatile long iPostAttempts;
	volatile long iPostSuccess;
	volatile long iPostErrors;
	bool bDone;
} __bench_co_post_ctx;

static bool __benchCoSpinWaitMin(volatile long* pValue, long iExpectMin, uint32_t iTimeoutMs)
{
	uint64_t iDeadlineNs = xbenchNowNs() + ((uint64_t)iTimeoutMs * 1000000ULL);

	while ( xbenchAtomicLoad(pValue) < iExpectMin ) {
		if ( xbenchNowNs() >= iDeadlineNs ) {
			return xbenchAtomicLoad(pValue) >= iExpectMin;
		}

		#if defined(_WIN32) || defined(_WIN64)
			SwitchToThread();
		#else
			usleep(0);
		#endif
	}

	return true;
}

static void __benchCoPostMain(ptr pArg)
{
	__bench_co_post_ctx* pCtx = (__bench_co_post_ctx*)pArg;

	if ( !pCtx ) return;

	while ( (uint64_t)xbenchAtomicLoad(&pCtx->iProcessedCount) < pCtx->iWakeTarget ) {
		long iProcessed = xbenchAtomicInc(&pCtx->iProcessedCount);
		if ( (uint64_t)iProcessed >= pCtx->iWakeTarget ) {
			break;
		}

		xbenchAtomicInc(&pCtx->iSleepSeq);
		xrtCoSleep(3600000u);
	}

	pCtx->bDone = true;
}

static uint32 __benchCoPostWorker(ptr pArg)
{
	__bench_co_post_ctx* pCtx = (__bench_co_post_ctx*)pArg;
	uint64_t iNeedPosts = 0u;

	if ( !pCtx || !pCtx->pSched || !pCtx->pTarget ) return 0;

	iNeedPosts = (pCtx->iWakeTarget > 0u) ? (pCtx->iWakeTarget - 1u) : 0u;
	for ( uint64_t i = 0; i < iNeedPosts; ++i ) {
		if ( !__benchCoSpinWaitMin(&pCtx->iSleepSeq, (long)(i + 1u), 10000u) ) {
			xbenchAtomicInc(&pCtx->iPostErrors);
			(void)xrtCoSchedPost(pCtx->pSched, NULL);
			break;
		}

		xbenchAtomicInc(&pCtx->iPostAttempts);
		if ( xrtCoSchedPost(pCtx->pSched, pCtx->pTarget) ) {
			xbenchAtomicInc(&pCtx->iPostSuccess);
		}
		else {
			xbenchAtomicInc(&pCtx->iPostErrors);
			(void)xrtCoSchedPost(pCtx->pSched, NULL);
			break;
		}
	}

	return 0;
}

int main(int argc, char** argv)
{
	uint64_t iWakeTarget = xbenchArgU64(argc, argv, 1, 100000u);
	__bench_co_post_ctx tCtx;
	xcosched* pSched = NULL;
	xthread pPoster = NULL;
	xbenchtimer tTimer;
	uint64_t iElapsedNs = 0u;

	if ( iWakeTarget < 2u ) {
		iWakeTarget = 2u;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.iWakeTarget = iWakeTarget;

	printf("xrt coroutine bench_sched_post\n");
	printf("backend=%s\n", xrtCoGetBackendName());
	printf("wake_target=%" PRIu64 "\n", iWakeTarget);

	if ( !xrtInit() ) return 1;

	pSched = xrtCoSchedCreate();
	if ( !pSched ) {
		xrtUnit();
		return 2;
	}

	tCtx.pSched = pSched;
	tCtx.pTarget = xrtCoSchedSpawn(pSched, __benchCoPostMain, &tCtx, 0u);
	if ( !tCtx.pTarget ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 3;
	}

	/* Prime the coroutine so subsequent wakeups are all cross-thread posts. */
	if ( !xrtCoSchedStep(pSched) ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 4;
	}

	pPoster = xrtThreadCreate(__benchCoPostWorker, &tCtx, 0u);
	if ( !pPoster ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 5;
	}

	xbenchTimerStart(&tTimer);
	while ( !tCtx.bDone ) {
		if ( !xrtCoSchedPollOnce(pSched, XRT_CO_WAIT_INFINITE) && !tCtx.bDone ) {
			if ( xbenchAtomicLoad(&tCtx.iPostErrors) > 0 ) {
				break;
			}
		}
	}
	xbenchTimerStop(&tTimer);

	xrtThreadWait(pPoster);
	xrtThreadDestroy(pPoster);
	iElapsedNs = xbenchTimerElapsedNs(&tTimer);

	xbenchPrintMetricU64("processed_wakes", (uint64_t)xbenchAtomicLoad(&tCtx.iProcessedCount));
	xbenchPrintMetricU64("post_attempts", (uint64_t)xbenchAtomicLoad(&tCtx.iPostAttempts));
	xbenchPrintMetricU64("post_success", (uint64_t)xbenchAtomicLoad(&tCtx.iPostSuccess));
	xbenchPrintMetricU64("post_errors", (uint64_t)xbenchAtomicLoad(&tCtx.iPostErrors));
	xbenchPrintMetricU64("elapsed_ns", iElapsedNs);
	xbenchPrintMetricDouble("wakeups_per_sec", xbenchSafeRate((uint64_t)xbenchAtomicLoad(&tCtx.iProcessedCount), iElapsedNs));
	xbenchPrintMetricDouble("ns_per_wakeup", xbenchAtomicLoad(&tCtx.iProcessedCount) > 0 ? ((double)iElapsedNs / (double)xbenchAtomicLoad(&tCtx.iProcessedCount)) : 0.0);

	xrtCoSchedDestroy(pSched);
	xrtUnit();
	return (xbenchAtomicLoad(&tCtx.iPostErrors) == 0 && tCtx.bDone) ? 0 : 6;
}
