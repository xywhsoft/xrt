#include "../test.h"
#include "../test_thread.h"



#define TEST_MEMDEBUG_THREAD_COUNT 8
#define TEST_MEMDEBUG_ITERATIONS 8000



/* 每个原生线程独立记录调试压力测试失败。 */
typedef struct test_memdebug_thread_context {
	int Index;
} test_memdebug_thread_context;



/* 反复跨越池化与 backing 隔离边界，压测并发淘汰和统计快照。 */
static int testMemDebugThreadRun(ptr pData)
{
	test_memdebug_thread_context* pContext = (test_memdebug_thread_context*)pData;

	for ( size_t i = 0; i < TEST_MEMDEBUG_ITERATIONS; i++ ) {
		size_t iSize = (((i * 29u) + ((size_t)pContext->Index * 31u)) % 1536u) + 1u;
		size_t iNewSize = (((i * 43u) + ((size_t)pContext->Index * 7u)) % 2304u) + 1u;
		unsigned char iValue = (unsigned char)(i + (size_t)pContext->Index);
		unsigned char* pMemory = (unsigned char*)xrtMalloc(iSize);

		if ( pMemory == NULL ) {
			return 1;
		}
		pMemory[0] = iValue;
		pMemory = (unsigned char*)xrtRealloc(pMemory, iNewSize);
		if ( pMemory == NULL ) {
			return 2;
		}
		if ( pMemory[0] != iValue ) {
			xrtFree(pMemory);
			return 3;
		}
		xrtFree(pMemory);
	}

	return 0;
}



/* 验证其他线程不会继承或消费调用线程的分配故障。 */
static int testMemDebugFaultThreadRun(ptr pData)
{
	ptr pMemory;

	(void)pData;
	pMemory = xrtMalloc(32);
	if ( pMemory == NULL ) {
		return 1;
	}
	if ( xrtMemDebugFailTriggered() ) {
		xrtFree(pMemory);
		return 2;
	}
	xrtFree(pMemory);
	return 0;
}



/* 验证多线程调试记录最终没有活动分配和计数漂移。 */
int main(void)
{
	test_memdebug_thread_context arrContext[TEST_MEMDEBUG_THREAD_COUNT];
	testthread arrThread[TEST_MEMDEBUG_THREAD_COUNT];
	xmemdebugsnapshot tSnapshot;
	ptr pProbe;

	memset(arrContext, 0, sizeof(arrContext));
	testRequire(xrtMemDebugReset(), "initial debug reset failed");
	for ( int i = 0; i < TEST_MEMDEBUG_THREAD_COUNT; i++ ) {
		arrContext[i].Index = i;
		arrThread[i].Proc = testMemDebugThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_MEMDEBUG_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_MEMDEBUG_THREAD_COUNT);
	for ( int i = 0; i < TEST_MEMDEBUG_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "memory debug worker failed");
	}
	xrtMemDebugSnapshot(&tSnapshot);
	testRequire(tSnapshot.LiveCount == 0, "thread stress leaked tracked allocations");
	testRequire(tSnapshot.AllocCount >= (TEST_MEMDEBUG_THREAD_COUNT * TEST_MEMDEBUG_ITERATIONS),
		"thread stress allocation count mismatch");
	testRequire(tSnapshot.ReallocCount >= (TEST_MEMDEBUG_THREAD_COUNT * TEST_MEMDEBUG_ITERATIONS),
		"thread stress realloc count mismatch");
	testRequire((tSnapshot.OverflowCount == 0) && (tSnapshot.UnderflowCount == 0),
		"thread stress reported false boundary corruption");
	testRequire(tSnapshot.UseAfterFreeCount == 0, "thread stress reported false use-after-free");

	testRequire(
		xrtMemDebugFailAfter(0),
		"thread-local allocation fault setup failed"
	);
	arrThread[0].Proc = testMemDebugFaultThreadRun;
	arrThread[0].Data = NULL;
	testThreadsStart(arrThread, 1);
	testThreadsJoin(arrThread, 1);
	testRequire(
		arrThread[0].Result == 0,
		"allocation fault leaked into another thread"
	);
	pProbe = xrtMalloc(32);
	testRequire(
		(pProbe == NULL) && xrtMemDebugFailTriggered(),
		"another thread consumed the caller allocation fault"
	);
	xrtClearError();
	xrtMemDebugFailClear();
	testRequire(xrtMemDebugReset(), "final debug reset failed");
	printf("[PASS] memory_debug_threads\n");
	return 0;
}
