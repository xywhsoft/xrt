#include "../test.h"
#include "../test_thread.h"



#define TEST_SPIN_THREADS 4u
#define TEST_SPIN_ITERATIONS 50000u



typedef struct testspinstate {
	xspinlock Lock;
	uint64 Counter;
} testspinstate;



/* 在短临界区内递增共享计数。 */
static int testSpinWorker(ptr pData)
{
	testspinstate* pState = (testspinstate*)pData;

	for ( uint32 i = 0u; i < TEST_SPIN_ITERATIONS; i++ ) {
		if ( !xrtSpinLock(&pState->Lock) ) {
			return 1;
		}
		pState->Counter++;
		if ( !xrtSpinUnlock(&pState->Lock) ) {
			return 2;
		}
	}
	return 0;
}



/* 验证竞争下不丢失临界区更新。 */
int main(void)
{
	testspinstate State;
	testthread arrThreads[TEST_SPIN_THREADS];

	memset(&State, 0, sizeof(State));
	memset(arrThreads, 0, sizeof(arrThreads));
	testRequire(xrtSpinInit(&State.Lock), "threaded spin init failed");
	for ( size_t i = 0u; i < TEST_SPIN_THREADS; i++ ) {
		arrThreads[i].Proc = testSpinWorker;
		arrThreads[i].Data = &State;
	}
	testThreadsStart(arrThreads, TEST_SPIN_THREADS);
	testThreadsJoin(arrThreads, TEST_SPIN_THREADS);
	for ( size_t i = 0u; i < TEST_SPIN_THREADS; i++ ) {
		testRequire(arrThreads[i].Result == 0, "spin worker failed");
	}
	testRequire(
		State.Counter == (uint64)TEST_SPIN_THREADS * TEST_SPIN_ITERATIONS,
		"spin protected counter mismatch"
	);
	testRequire(xrtSpinUnit(&State.Lock), "threaded spin unit failed");
	printf("[PASS] spin threads\n");
	return 0;
}
