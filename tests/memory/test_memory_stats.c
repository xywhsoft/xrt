#include "../test.h"



/* 验证 API 请求、实际堆通道和尺寸类统计口径。 */
int main(void)
{
	const unsigned char arrSource[] = { 1, 2, 3, 4 };
	xmemstats tStats;
	ptr pMallocSmall;
	ptr pCalloc;
	ptr pMallocLarge;
	ptr pRealloc;
	ptr pCopy;

	xrtMemStatsEnable(false);
	xrtMemStatsReset();
	pMallocSmall = xrtMalloc(16);
	testRequire(pMallocSmall != NULL, "disabled allocation failed");
	xrtFree(pMallocSmall);
	xrtMemStatsGet(&tStats);
	testRequire((tStats.MallocCalls == 0) && (tStats.BlockAllocCalls == 0), "disabled statistics changed");

	xrtMemStatsEnable(true);
	xrtMemStatsReset();
	pMallocSmall = xrtMalloc(8);
	pCalloc = xrtCalloc(2, 12);
	pMallocLarge = xrtMalloc(2048);
	pRealloc = xrtRealloc(NULL, 40);
	pCopy = xrtMemDup(arrSource, sizeof(arrSource));
	testRequire((pMallocSmall != NULL) && (pCalloc != NULL) && (pMallocLarge != NULL), "allocation failed");
	testRequire((pRealloc != NULL) && (pCopy != NULL), "realloc or duplicate failed");

	xrtFree(pMallocSmall);
	xrtFree(pCalloc);
	xrtFree(pMallocLarge);
	xrtFree(pRealloc);
	xrtFree(pCopy);
	xrtMemStatsGet(&tStats);

	testRequire(tStats.Enabled, "statistics should be enabled");
	testRequire((tStats.ClassStep == 16) && (tStats.ClassCutoff == 1024) && (tStats.ClassCount == 64),
		"size class metadata mismatch");
	testRequire((tStats.MallocCalls == 2) && (tStats.MallocBytes == 2056), "malloc statistics mismatch");
	testRequire((tStats.CallocCalls == 1) && (tStats.CallocBytes == 24), "calloc statistics mismatch");
	testRequire((tStats.ReallocCalls == 1) && (tStats.ReallocBytes == 40), "realloc statistics mismatch");
	testRequire((tStats.MemDupCalls == 1) && (tStats.MemDupBytes == 4), "duplicate statistics mismatch");
	testRequire(tStats.FreeCalls == 5, "free statistics mismatch");
	testRequire((tStats.BlockAllocCalls == 5) && (tStats.BlockAllocBytes == 2124), "block allocation mismatch");
	testRequire((tStats.BlockFreeCalls == 5) && (tStats.BlockFreeBytes == 2124), "block release mismatch");
	testRequire((tStats.PooledAllocCalls == 4) && (tStats.PooledAllocBytes == 76), "pooled statistics mismatch");
	testRequire((tStats.DirectAllocCalls == 1) && (tStats.DirectAllocBytes == 2048),
		"direct allocation statistics mismatch");
	testRequire((tStats.BackingAllocCalls >= tStats.DirectAllocCalls) &&
		(tStats.BackingAllocBytes > tStats.DirectAllocBytes), "backing allocation statistics mismatch");
	testRequire(tStats.BackingReallocCalls == 0, "backing resize statistics mismatch");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		testRequire(tStats.BackingFreeCalls == 0,
			"debug quarantine must defer the direct backing release");
		testRequire(xrtMemDebugReset(), "debug quarantine reset failed");
		xrtMemStatsGet(&tStats);
		testRequire(tStats.BackingFreeCalls == 1,
			"debug quarantine reset did not release the direct backing block");
	#else
		testRequire(tStats.BackingFreeCalls >= 1, "direct backing release statistics mismatch");
	#endif
	testRequire((tStats.ClassCalls[0] == 2) && (tStats.ClassBytes[0] == 12), "16-byte class mismatch");
	testRequire((tStats.ClassCalls[1] == 1) && (tStats.ClassBytes[1] == 24), "32-byte class mismatch");
	testRequire((tStats.ClassCalls[2] == 1) && (tStats.ClassBytes[2] == 40), "48-byte class mismatch");

	xrtMemStatsReset();
	xrtMemStatsGet(&tStats);
	testRequire((tStats.MallocCalls == 0) && (tStats.BlockAllocCalls == 0), "reset did not clear statistics");
	testRequire(xrtCalloc(SIZE_MAX, 2) == NULL, "overflowing calloc must fail");
	pCalloc = xrtCalloc(1, 1);
	testRequire(pCalloc != NULL, "post-overflow calloc failed");
	xrtMemStatsGet(&tStats);
	testRequire(tStats.CallocCalls == 2, "overflow call count mismatch");
	if ( SIZE_MAX == UINT64_MAX ) {
		testRequire(tStats.CallocBytes == UINT64_MAX, "64-bit byte counters must saturate");
	} else {
		testRequire(tStats.CallocBytes == ((uint64)SIZE_MAX + 1), "32-bit byte counters mismatch");
	}
	xrtFree(pCalloc);
	xrtClearError();
	xrtMemStatsReset();
	xrtMemStatsGet(NULL);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null snapshot must report argument error");
	xrtClearError();
	xrtMemStatsEnable(false);
	printf("[PASS] memory_stats\n");
	return 0;
}
