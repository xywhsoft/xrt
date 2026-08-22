#include "../test.h"
#include "../test_thread.h"



#define TEST_MPSC_PRODUCERS 4u
#define TEST_MPSC_ITEMS_PER_PRODUCER 100000u
#define TEST_MPSC_TOTAL_ITEMS (TEST_MPSC_PRODUCERS * TEST_MPSC_ITEMS_PER_PRODUCER)
#define TEST_MPSC_PUSH_BATCH 7u
#define TEST_MPSC_POP_BATCH 31u



/* 多生产者共享完成状态，队列仍只由主线程消费。 */
typedef struct testmpscshared {
	xmpscqueue* Queue;
	xatomic32 Failure;
	xatomic32 Completed;
} testmpscshared;



/* 每个生产者保存独立编号，生成互不重叠且可验证顺序的指针值。 */
typedef struct testmpscproducer {
	testmpscshared* Shared;
	uint32 Index;
} testmpscproducer;



/* 批量发布一个生产者的递增序列，满队列时只执行短自旋。 */
static int testMPSCProducer(ptr pData)
{
	testmpscproducer* pProducer = (testmpscproducer*)pData;
	testmpscshared* pShared = pProducer->Shared;
	size_t iSent = 0;
	ptr pItems[TEST_MPSC_PUSH_BATCH];

	while ( iSent < TEST_MPSC_ITEMS_PER_PRODUCER ) {
		size_t iRequest = TEST_MPSC_ITEMS_PER_PRODUCER - iSent;
		xqueuebatchresult Batch;

		if ( iRequest > TEST_MPSC_PUSH_BATCH ) {
			iRequest = TEST_MPSC_PUSH_BATCH;
		}
		for ( size_t i = 0; i < iRequest; i++ ) {
			uintptr_t iValue =
				((uintptr_t)pProducer->Index * TEST_MPSC_ITEMS_PER_PRODUCER) +
				iSent + i + 1u;

			pItems[i] = (ptr)iValue;
		}

		Batch = xrtMPSCQueuePushBatch(pShared->Queue, pItems, iRequest);
		if ( Batch.Result == XQUEUE_OK ) {
			if ( Batch.Count == 0u ) {
				xrtAtomic32Store(&pShared->Failure, 1u, XMEMORY_RELAXED);
				return 1;
			}
			iSent += Batch.Count;
			continue;
		}
		if ( Batch.Result != XQUEUE_FULL ) {
			xrtAtomic32Store(&pShared->Failure, 1u, XMEMORY_RELAXED);
			return 1;
		}
		xrtAtomicPause();
	}

	xrtAtomic32FetchAdd(&pShared->Completed, 1u, XMEMORY_RELEASE);
	return 0;
}



/* 验证四个生产者竞争下无丢失、无重复，并保持每个生产者的 FIFO。 */
int main(void)
{
	xmpscqueue tQueue;
	testmpscshared tShared;
	testmpscproducer pProducer[TEST_MPSC_PRODUCERS];
	testthread pThread[TEST_MPSC_PRODUCERS];
	uint8* pSeen;
	uint32 pLast[TEST_MPSC_PRODUCERS] = { 0 };
	ptr pItems[TEST_MPSC_POP_BATCH];
	size_t iReceived = 0;

	testRequire(xrtMPSCQueueInit(&tQueue, 256u), "threaded MPSC init failed");
	tShared.Queue = &tQueue;
	xrtAtomic32Init(&tShared.Failure, 0u);
	xrtAtomic32Init(&tShared.Completed, 0u);
	memset(pThread, 0, sizeof(pThread));
	for ( uint32 i = 0u; i < TEST_MPSC_PRODUCERS; i++ ) {
		pProducer[i].Shared = &tShared;
		pProducer[i].Index = i;
		pThread[i].Proc = testMPSCProducer;
		pThread[i].Data = &pProducer[i];
	}
	pSeen = (uint8*)xrtCalloc(TEST_MPSC_TOTAL_ITEMS, sizeof(uint8));
	testRequire(pSeen != NULL, "threaded MPSC seen table allocation failed");
	testThreadsStart(pThread, TEST_MPSC_PRODUCERS);

	while ( iReceived < TEST_MPSC_TOTAL_ITEMS ) {
		xqueuebatchresult Batch = xrtMPSCQueuePopBatch(
			&tQueue,
			pItems,
			TEST_MPSC_POP_BATCH
		);

		if ( Batch.Result == XQUEUE_EMPTY ) {
			testRequire(
				xrtAtomic32Load(&tShared.Failure, XMEMORY_RELAXED) == 0u,
				"threaded MPSC producer reported failure"
			);
			testRequire(
				(xrtAtomic32Load(&tShared.Completed, XMEMORY_ACQUIRE) < TEST_MPSC_PRODUCERS) ||
				(iReceived == TEST_MPSC_TOTAL_ITEMS),
				"threaded MPSC lost published items"
			);
			xrtAtomicPause();
			continue;
		}
		testRequire(Batch.Result == XQUEUE_OK, "threaded MPSC pop batch failed");
		testRequire(Batch.Count != 0u, "threaded MPSC returned empty success batch");
		for ( size_t i = 0; i < Batch.Count; i++ ) {
			uintptr_t iValue = (uintptr_t)pItems[i];
			uint32 iProducer;
			uint32 iSequence;

			testRequire(
				(iValue >= 1u) && (iValue <= TEST_MPSC_TOTAL_ITEMS),
				"threaded MPSC value range mismatch"
			);
			iProducer = (uint32)((iValue - 1u) / TEST_MPSC_ITEMS_PER_PRODUCER);
			iSequence = (uint32)(((iValue - 1u) % TEST_MPSC_ITEMS_PER_PRODUCER) + 1u);
			testRequire(pSeen[iValue - 1u] == 0u, "threaded MPSC duplicated item");
			testRequire(
				iSequence == (pLast[iProducer] + 1u),
				"threaded MPSC per-producer FIFO mismatch"
			);
			pSeen[iValue - 1u] = 1u;
			pLast[iProducer] = iSequence;
			iReceived++;
		}
	}

	testThreadsJoin(pThread, TEST_MPSC_PRODUCERS);
	for ( uint32 i = 0u; i < TEST_MPSC_PRODUCERS; i++ ) {
		testRequire(pThread[i].Result == 0, "threaded MPSC producer failed");
		testRequire(
			pLast[i] == TEST_MPSC_ITEMS_PER_PRODUCER,
			"threaded MPSC producer item count mismatch"
		);
	}
	testRequire(
		xrtAtomic32Load(&tShared.Completed, XMEMORY_ACQUIRE) == TEST_MPSC_PRODUCERS,
		"threaded MPSC completion count mismatch"
	);
	testRequire(xrtMPSCQueueCount(&tQueue) == 0u, "threaded MPSC final count mismatch");

	/* 关闭必须发生在全部生产者已经返回之后。 */
	xrtMPSCQueueClose(&tQueue);
	testRequire(
		xrtMPSCQueuePopBatch(&tQueue, pItems, TEST_MPSC_POP_BATCH).Result == XQUEUE_CLOSED,
		"threaded MPSC terminal result mismatch"
	);
	testRequire(xrtMPSCQueueIsDrained(&tQueue), "threaded MPSC did not drain");
	xrtFree(pSeen);
	xrtMPSCQueueUnit(&tQueue);
	printf("[PASS] queue_mpsc_threads\n");
	return 0;
}
