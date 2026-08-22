#include "../test.h"
#include "../test_thread.h"



#define TEST_ATOMIC_THREAD_COUNT 4
#define TEST_ATOMIC_ITERATIONS 100000



/* 两种宽度放在同一共享状态中，但各自保持声明对齐。 */
typedef struct testatomicstate {
	xatomic32 Counter32;
	xatomic64 Counter64;
	xatomic64 CounterCAS;
} testatomicstate;



/* 每个执行流同时验证 FetchAdd 和受竞争的强比较交换。 */
static int testAtomicWorker(ptr pData)
{
	testatomicstate* pState = (testatomicstate*)pData;

	for ( size_t i = 0; i < TEST_ATOMIC_ITERATIONS; i++ ) {
		uint64 iExpected;

		(void)xrtAtomic32FetchAdd(&pState->Counter32, 1u, XMEMORY_RELAXED);
		(void)xrtAtomic64FetchAdd(&pState->Counter64, 1u, XMEMORY_RELAXED);
		iExpected = xrtAtomic64Load(&pState->CounterCAS, XMEMORY_RELAXED);
		while (
			!xrtAtomic64CompareExchange(
				&pState->CounterCAS,
				&iExpected,
				iExpected + 1u,
				XMEMORY_ACQ_REL,
				XMEMORY_RELAXED
			)
		) {
			xrtAtomicPause();
		}
	}
	return 0;
}



/* 发布状态用于验证普通数据受 Release/Acquire 保护。 */
typedef struct testpublishstate {
	int Data;
	xatomic32 Ready;
} testpublishstate;



/* 写入普通数据后以 Release 顺序发布就绪标记。 */
static int testAtomicPublisher(ptr pData)
{
	testpublishstate* pState = (testpublishstate*)pData;

	pState->Data = 1234567;
	xrtAtomic32Store(&pState->Ready, 1u, XMEMORY_RELEASE);
	return 0;
}



/* 验证多线程读改写不丢更新。 */
int main(void)
{
	testatomicstate tState;
	testpublishstate tPublish;
	testthread arrThreads[TEST_ATOMIC_THREAD_COUNT];
	testthread tPublisher;
	uint64 iExpected = TEST_ATOMIC_THREAD_COUNT * TEST_ATOMIC_ITERATIONS;

	xrtAtomic32Init(&tState.Counter32, 0u);
	xrtAtomic64Init(&tState.Counter64, 0u);
	xrtAtomic64Init(&tState.CounterCAS, 0u);
	memset(arrThreads, 0, sizeof(arrThreads));
	for ( size_t i = 0; i < TEST_ATOMIC_THREAD_COUNT; i++ ) {
		arrThreads[i].Proc = testAtomicWorker;
		arrThreads[i].Data = &tState;
	}
	testThreadsStart(arrThreads, TEST_ATOMIC_THREAD_COUNT);
	testThreadsJoin(arrThreads, TEST_ATOMIC_THREAD_COUNT);
	for ( size_t i = 0; i < TEST_ATOMIC_THREAD_COUNT; i++ ) {
		testRequire(arrThreads[i].Result == 0, "atomic worker failed");
	}
	testRequire(
		xrtAtomic32Load(&tState.Counter32, XMEMORY_RELAXED) == (uint32)iExpected,
		"atomic32 concurrent updates were lost"
	);
	testRequire(
		xrtAtomic64Load(&tState.Counter64, XMEMORY_RELAXED) == iExpected,
		"atomic64 concurrent updates were lost"
	);
	testRequire(
		xrtAtomic64Load(&tState.CounterCAS, XMEMORY_RELAXED) == iExpected,
		"atomic64 concurrent CAS updates were lost"
	);

	/* Acquire 读取就绪后必须看到 Release 之前写入的普通数据。 */
	tPublish.Data = 0;
	xrtAtomic32Init(&tPublish.Ready, 0u);
	memset(&tPublisher, 0, sizeof(tPublisher));
	tPublisher.Proc = testAtomicPublisher;
	tPublisher.Data = &tPublish;
	testThreadsStart(&tPublisher, 1u);
	while ( xrtAtomic32Load(&tPublish.Ready, XMEMORY_ACQUIRE) == 0u ) {
		xrtAtomicPause();
	}
	testRequire(tPublish.Data == 1234567, "release/acquire publication mismatch");
	testThreadsJoin(&tPublisher, 1u);
	testRequire(tPublisher.Result == 0, "atomic publisher failed");
	printf("[PASS] atomic_threads\n");
	return 0;
}
