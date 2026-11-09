#include "../test.h"
#include "../test_thread.h"



#define TEST_MEM_STATS_THREAD_COUNT 4
#define TEST_MEM_STATS_ITERATIONS 10000



/* Cold first allocations must race: do not warm the heap on the main thread.
 * The native atomic barrier does not use XRT allocation or preinitialize it. */
#if defined(_WIN32) || defined(_WIN64)
static volatile LONG testMemColdEntered;
#else
static int testMemColdEntered;
#endif
static void testMemColdStart(void)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)InterlockedIncrement(&testMemColdEntered);
    while(InterlockedCompareExchange(&testMemColdEntered,0,0)!=TEST_MEM_STATS_THREAD_COUNT)testThreadYield();
#else
    (void)__sync_fetch_and_add(&testMemColdEntered,1);
    while(__sync_val_compare_and_swap(&testMemColdEntered,0,0)!=TEST_MEM_STATS_THREAD_COUNT)testThreadYield();
#endif
}

/* 并发执行固定数量的公开内存 API 调用。 */
static int testMemStatsThreadRun(ptr pData)
{
	(void)pData;
	testMemColdStart();

	for ( size_t i = 0; i < TEST_MEM_STATS_ITERATIONS; i++ ) {
		size_t iSize = (i % 1536u) + 1u;
		unsigned char* pMemory = (unsigned char*)xrtMalloc(iSize);

		if ( pMemory == NULL ) {
			return 1;
		}
		pMemory[0] = (unsigned char)i;
		pMemory = (unsigned char*)xrtRealloc(pMemory, iSize + 17u);
		if ( pMemory == NULL ) {
			return 2;
		}
		xrtFree(pMemory);
	}
	return 0;
}



/* 验证并发槽聚合不会丢失公开 API 计数。 */
int main(void)
{
	testthread arrThread[TEST_MEM_STATS_THREAD_COUNT];
	xmemstats tStats;
	uint64 iExpected = TEST_MEM_STATS_THREAD_COUNT * TEST_MEM_STATS_ITERATIONS;

	memset(arrThread, 0, sizeof(arrThread));
	xrtMemStatsEnable(true);
	xrtMemStatsReset();
	for ( int i = 0; i < TEST_MEM_STATS_THREAD_COUNT; i++ ) {
		arrThread[i].Proc = testMemStatsThreadRun;
		arrThread[i].Data = NULL;
	}
	testThreadsStart(arrThread, TEST_MEM_STATS_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_MEM_STATS_THREAD_COUNT);
	for ( int i = 0; i < TEST_MEM_STATS_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "memory statistics worker failed");
	}
	xrtMemStatsGet(&tStats);
	testRequire(tStats.MallocCalls == iExpected, "concurrent malloc calls were lost");
	testRequire(tStats.ReallocCalls == iExpected, "concurrent realloc calls were lost");
	testRequire(tStats.FreeCalls == iExpected, "concurrent free calls were lost");
	testRequire(tStats.BlockAllocCalls >= iExpected, "concurrent block allocations were lost");
	testRequire(tStats.BlockFreeCalls >= iExpected, "concurrent block releases were lost");
	xrtMemStatsEnable(false);
	printf("[PASS] memory_stats_threads\n");
	return 0;
}
