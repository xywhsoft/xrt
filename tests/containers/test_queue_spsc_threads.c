#include "../test.h"
#include "../test_thread.h"



#define TEST_SPSC_ITEMS 200000u
#define TEST_SPSC_CLOSE_CYCLES 50000u



/* 生产者状态只由对应测试线程写入。 */
typedef struct testspscstate {
	xspscqueue* Queue;
	xatomic32 Failure;
	xatomic32 Round;
} testspscstate;



/* 持续压入递增标号，满时执行短自旋，完成后关闭队列。 */
static int testSPSCProducer(ptr pData)
{
	testspscstate* pState = (testspscstate*)pData;

	for ( uintptr_t i = 1u; i <= TEST_SPSC_ITEMS; i++ ) {
		for ( ;; ) {
			xqueueresult iResult = xrtSPSCQueueTryPush(
				pState->Queue,
				(ptr)(uintptr_t)i
			);

			if ( iResult == XQUEUE_OK ) {
				break;
			}
			if ( iResult != XQUEUE_FULL ) {
				xrtAtomic32Store(&pState->Failure, 1u, XMEMORY_RELAXED);
				return 1;
			}
			xrtAtomicPause();
		}
	}
	xrtSPSCQueueClose(pState->Queue);
	return 0;
}



/* 反复发布最后一个元素并关闭，等待消费者排空和重置后进入下一轮。 */
static int testSPSCCloseProducer(ptr pData)
{
	testspscstate* pState = (testspscstate*)pData;

	for ( uint32 i = 1u; i <= TEST_SPSC_CLOSE_CYCLES; i++ ) {
		while (
			xrtAtomic32Load(&pState->Round, XMEMORY_ACQUIRE) != (i - 1u)
		) {
			xrtAtomicPause();
		}
		if (
			xrtSPSCQueueTryPush(
				pState->Queue,
				(ptr)(uintptr_t)i
			) != XQUEUE_OK
		) {
			xrtAtomic32Store(&pState->Failure, 1u, XMEMORY_RELAXED);
			return 1;
		}
		xrtSPSCQueueClose(pState->Queue);
	}
	return 0;
}



/* 验证关闭发布不能越过最后一个元素，并覆盖重复关闭与重置边界。 */
static void testSPSCClosePublication(void)
{
	xspscqueue tQueue;
	testspscstate tState;
	testthread tProducer;
	ptr pItem;

	testRequire(xrtSPSCQueueInit(&tQueue, 1u), "close publication SPSC init failed");
	tState.Queue = &tQueue;
	xrtAtomic32Init(&tState.Failure, 0u);
	xrtAtomic32Init(&tState.Round, 0u);
	memset(&tProducer, 0, sizeof(tProducer));
	tProducer.Proc = testSPSCCloseProducer;
	tProducer.Data = &tState;
	testThreadsStart(&tProducer, 1u);

	for ( uint32 i = 1u; i <= TEST_SPSC_CLOSE_CYCLES; i++ ) {
		xqueueresult iTerminal;

		for ( ;; ) {
			xqueueresult iResult = xrtSPSCQueueTryPop(&tQueue, &pItem);

			if ( iResult == XQUEUE_EMPTY ) {
				xrtAtomicPause();
				continue;
			}
			testRequire(iResult == XQUEUE_OK, "SPSC close passed unpublished item");
			testRequire((uintptr_t)pItem == i, "SPSC close publication FIFO mismatch");
			break;
		}
		do {
			iTerminal = xrtSPSCQueueTryPop(&tQueue, &pItem);
			if ( iTerminal == XQUEUE_EMPTY ) {
				xrtAtomicPause();
			}
		} while ( iTerminal == XQUEUE_EMPTY );
		testRequire(iTerminal == XQUEUE_CLOSED, "SPSC close cycle terminal result mismatch");
		testRequire(xrtSPSCQueueIsDrained(&tQueue), "SPSC close cycle did not drain");
		testRequire(xrtSPSCQueueReset(&tQueue), "SPSC close cycle reset failed");
		xrtAtomic32Store(&tState.Round, i, XMEMORY_RELEASE);
	}

	testThreadsJoin(&tProducer, 1u);
	testRequire(tProducer.Result == 0, "SPSC close publication producer failed");
	testRequire(
		xrtAtomic32Load(&tState.Failure, XMEMORY_RELAXED) == 0u,
		"SPSC close publication producer reported failure"
	);
	xrtSPSCQueueUnit(&tQueue);
}



/* 验证跨线程发布不丢失、不重复且严格保持 FIFO。 */
int main(void)
{
	xspscqueue tQueue;
	testspscstate tState;
	testthread tProducer;
	uintptr_t iExpected = 1u;
	ptr pItem;

	testRequire(xrtSPSCQueueInit(&tQueue, 256u), "threaded SPSC init failed");
	tState.Queue = &tQueue;
	xrtAtomic32Init(&tState.Failure, 0u);
	xrtAtomic32Init(&tState.Round, 0u);
	memset(&tProducer, 0, sizeof(tProducer));
	tProducer.Proc = testSPSCProducer;
	tProducer.Data = &tState;
	testThreadsStart(&tProducer, 1u);

	for ( ;; ) {
		xqueueresult iResult = xrtSPSCQueueTryPop(&tQueue, &pItem);

		if ( iResult == XQUEUE_OK ) {
			testRequire((uintptr_t)pItem == iExpected, "threaded SPSC FIFO mismatch");
			iExpected++;
			continue;
		}
		if ( iResult == XQUEUE_EMPTY ) {
			xrtAtomicPause();
			continue;
		}
		testRequire(iResult == XQUEUE_CLOSED, "threaded SPSC terminal result mismatch");
		break;
	}

	testThreadsJoin(&tProducer, 1u);
	testRequire(tProducer.Result == 0, "threaded SPSC producer failed");
	testRequire(
		xrtAtomic32Load(&tState.Failure, XMEMORY_RELAXED) == 0u,
		"threaded SPSC producer reported failure"
	);
	testRequire(iExpected == TEST_SPSC_ITEMS + 1u, "threaded SPSC item count mismatch");
	testRequire(xrtSPSCQueueIsDrained(&tQueue), "threaded SPSC did not drain");
	xrtSPSCQueueUnit(&tQueue);
	testSPSCClosePublication();
	printf("[PASS] queue_spsc_threads\n");
	return 0;
}
