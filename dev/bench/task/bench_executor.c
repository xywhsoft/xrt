#include "../bench_common.h"

#define XRT_MODULE_EXECUTOR
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 最小 detached 工作只执行一次原子计数。 */
static void benchExecutorRun(ptr pData)
{
	xatomic64* pCount = (xatomic64*)pData;

	(void)xrtAtomic64FetchAdd(pCount, 1, XMEMORY_RELAXED);
}



int main(int argc, char** argv)
{
	uint32 iCount = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iThreads = xbenchArgU32(argc, argv, 2, 4u);
	uint32 iBatch = xbenchArgU32(argc, argv, 3, 32u);
	xexecutorconfig Config;
	xexecutoritem* pItems;
	xexecutorstats Stats;
	xatomic64 Count;
	xexecutor* pExecutor;
	xbenchtimer Timer;
	uint64 iElapsed;

	if ( (iCount == 0) || (iThreads == 0) || (iBatch == 0) ) {
		return 1;
	}
	memset(&Config, 0, sizeof(Config));
	memset(&Stats, 0, sizeof(Stats));
	Config.Threads = iThreads;
	Config.QueueLimit =
		((size_t)iCount / iThreads) + iBatch + 1u;
	xrtAtomic64Init(&Count, 0);
	pItems = (xexecutoritem*)calloc(iBatch, sizeof(*pItems));
	if ( pItems == NULL ) {
		return 2;
	}
	for ( uint32 i = 0; i < iBatch; i++ ) {
		pItems[i].Proc = benchExecutorRun;
		pItems[i].Data = &Count;
	}
	pExecutor = xrtExecutorCreate(&Config);
	if ( pExecutor == NULL ) {
		free(pItems);
		return 3;
	}
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iCount; ) {
		uint32 iCurrent = (iCount - i) < iBatch ? (iCount - i) : iBatch;

		if ( !xrtExecutorSubmitBatch(pExecutor, pItems, iCurrent) ) {
			return 4;
		}
		i += iCurrent;
	}
	if ( !xrtExecutorClose(pExecutor) ||
		(xrtExecutorWait(pExecutor) != XWAIT_OK) ) {
		return 5;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	if ( !xrtExecutorGet(pExecutor, &Stats) ||
		(xrtAtomic64Load(&Count, XMEMORY_ACQUIRE) != iCount) ) {
		return 6;
	}
	printf("xrt executor benchmark\n");
	printf("executor_threads: %u\n", iThreads);
	printf("executor_batch: %u\n", iBatch);
	printf("executor_tasks: %u\n", iCount);
	printf("executor_elapsed_ns: %" PRIu64 "\n", iElapsed);
	printf(
		"executor_tasks_per_sec: %.3f\n",
		xbenchSafeRate(iCount, iElapsed)
	);
	printf("executor_stolen: %" PRIu64 "\n", Stats.Stolen);
	free(pItems);
	return xrtExecutorDestroy(pExecutor) ? 0 : 7;
}
