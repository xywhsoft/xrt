#include "../bench_common.h"

#define XRT_MODULE_TASK_POOL
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 任务过程只执行一个原子计数，用于测量完整提交、调度和 Future 回收成本。 */
typedef struct benchtaskpoolcontext {
	volatile long Executed;
} benchtaskpoolcontext;



/* 执行最小可观察任务，避免编译器删除工作线程中的任务过程。 */
static xtaskoutcome benchTaskPoolProc(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	benchtaskpoolcontext* pContext = (benchtaskpoolcontext*)pData;

	(void)pCancel;
	(void)pResult;
	if ( pContext == NULL ) {
		return XTASK_FAILED;
	}
	xbenchAtomicInc(&pContext->Executed);
	return XTASK_SUCCESS;
}



/* 等待并释放窗口中的全部 Future，同时验证每个任务均成功完成。 */
static bool benchTaskPoolDrain(xfuture** pFutures, size_t iWindow)
{
	bool bResult = true;

	for ( size_t i = 0; i < iWindow; i++ ) {
		if ( pFutures[i] == NULL ) {
			continue;
		}
		if (
			(xrtFutureWait(pFutures[i]) != XWAIT_OK) ||
			(xrtFutureState(pFutures[i]) != XFUTURE_RESOLVED)
		) {
			bResult = false;
		}
		xrtFutureDestroy(pFutures[i]);
		pFutures[i] = NULL;
	}
	return bResult;
}



/*
	在固定 Future 窗口内持续提交任务。
	窗口限制调用方持有的 Future 数量，队列限制负责测量任务池自身的背压路径。
*/
static bool benchTaskPoolRun(
	const char* sPrefix,
	uint32 iThreads,
	size_t iQueueLimit,
	uint32 iCount,
	size_t iWindow
)
{
	benchtaskpoolcontext tContext;
	xtaskpoolconfig tConfig;
	xtaskpoolstats tStats;
	xtaskpool* pPool;
	xfuture** pFutures;
	xbenchtimer tTimer;
	uint64 iElapsed;
	bool bResult = true;

	memset(&tContext, 0, sizeof(tContext));
	memset(&tConfig, 0, sizeof(tConfig));
	memset(&tStats, 0, sizeof(tStats));
	if (
		(sPrefix == NULL) ||
		(iThreads == 0) ||
		(iQueueLimit == 0) ||
		(iCount == 0) ||
		(iWindow == 0)
	) {
		return false;
	}
	if ( iWindow > iCount ) {
		iWindow = iCount;
	}
	pFutures = (xfuture**)calloc(iWindow, sizeof(xfuture*));
	if ( pFutures == NULL ) {
		return false;
	}

	tConfig.Threads = iThreads;
	tConfig.QueueLimit = iQueueLimit;
	pPool = xrtTaskPoolCreate(&tConfig);
	if ( pPool == NULL ) {
		free(pFutures);
		return false;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iCount; i++ ) {
		size_t iSlot = (size_t)i % iWindow;

		if ( pFutures[iSlot] != NULL ) {
			if (
				(xrtFutureWait(pFutures[iSlot]) != XWAIT_OK) ||
				(xrtFutureState(pFutures[iSlot]) != XFUTURE_RESOLVED)
			) {
				bResult = false;
			}
			xrtFutureDestroy(pFutures[iSlot]);
			pFutures[iSlot] = NULL;
			if ( !bResult ) {
				break;
			}
		}

		pFutures[iSlot] = xrtTaskSubmitWait(
			pPool,
			benchTaskPoolProc,
			&tContext,
			NULL
		);
		if ( pFutures[iSlot] == NULL ) {
			bResult = false;
			break;
		}
	}
	if ( !benchTaskPoolDrain(pFutures, iWindow) ) {
		bResult = false;
	}
	xbenchTimerStop(&tTimer);
	iElapsed = xbenchTimerElapsedNs(&tTimer);

	if ( !bResult ) {
		(void)xrtTaskPoolCancel(pPool);
	} else if ( !xrtTaskPoolClose(pPool) ) {
		bResult = false;
	}
	if ( xrtTaskPoolWait(pPool) != XWAIT_OK ) {
		bResult = false;
	}
	if ( !xrtTaskPoolGet(pPool, &tStats) ) {
		bResult = false;
	}
	if (
		(xbenchAtomicLoad(&tContext.Executed) != (long)iCount) ||
		(tStats.Submitted != iCount) ||
		(tStats.Completed != iCount) ||
		(tStats.Succeeded != iCount) ||
		(tStats.Failed != 0) ||
		(tStats.Cancelled != 0) ||
		(tStats.Rejected != 0) ||
		(tStats.Queued != 0) ||
		(tStats.Running != 0)
	) {
		bResult = false;
	}

	printf("%s_threads: %" PRIu32 "\n", sPrefix, iThreads);
	printf("%s_queue_limit: %zu\n", sPrefix, iQueueLimit);
	printf("%s_future_window: %zu\n", sPrefix, iWindow);
	printf("%s_tasks: %" PRIu32 "\n", sPrefix, iCount);
	printf("%s_elapsed_ns: %" PRIu64 "\n", sPrefix, iElapsed);
	printf(
		"%s_tasks_per_sec: %.3f\n",
		sPrefix,
		xbenchSafeRate(iCount, iElapsed)
	);
	printf("%s_rejected: %" PRIu64 "\n", sPrefix, tStats.Rejected);

	free(pFutures);
	if ( !xrtTaskPoolDestroy(pPool) ) {
		bResult = false;
	}
	return bResult;
}



/* 执行常规有界窗口和容量为一的背压吞吐基准。 */
int main(int argc, char** argv)
{
	uint32 iCount = xbenchArgU32(argc, argv, 1, 200000u);
	uint32 iThreads = xbenchArgU32(argc, argv, 2, 4u);
	uint32 iWindow = xbenchArgU32(argc, argv, 3, 4096u);
	uint32 iBackpressureCount = xbenchArgU32(argc, argv, 4, 50000u);
	size_t iBackpressureWindow;

	if (
		(iCount == 0) ||
		(iCount > (uint32)LONG_MAX) ||
		(iThreads == 0) ||
		(iThreads > XRT_TASK_POOL_THREAD_LIMIT) ||
		(iWindow == 0) ||
		(iBackpressureCount == 0) ||
		(iBackpressureCount > (uint32)LONG_MAX)
	) {
		fprintf(stderr, "invalid task pool benchmark arguments.\n");
		return 1;
	}
	iBackpressureWindow = ((size_t)iThreads * 2u) + 1u;

	xbenchApplyCpuPinFromEnv();
	printf("xrt task pool benchmark\n");
	if (
		!benchTaskPoolRun(
			"windowed",
			iThreads,
			iWindow,
			iCount,
			iWindow
		)
	) {
		return 2;
	}
	if (
		!benchTaskPoolRun(
			"queue_one",
			iThreads,
			1u,
			iBackpressureCount,
			iBackpressureWindow
		)
	) {
		return 3;
	}
	return 0;
}
