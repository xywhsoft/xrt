#include "../test.h"
#include "../test_thread.h"



#define TEST_MPMC_PRODUCERS 4u
#define TEST_MPMC_CONSUMERS 4u
#define TEST_MPMC_PUSH_BATCH 7u
#define TEST_MPMC_POP_BATCH 31u



/* 所有线程共享队列、恰好一次标记和完成统计。 */
typedef struct testmpmcshared {
	xmpmcqueue* Queue;
	xatomic32* Seen;
	xatomic32 Consumed;
	xatomic32 Failure;
	size_t ItemsPerProducer;
	size_t TotalItems;
} testmpmcshared;



/* 每个生产者保存独立编号，生成互不重叠的指针值。 */
typedef struct testmpmcproducer {
	testmpmcshared* Shared;
	uint32 Index;
} testmpmcproducer;



/* 批量发布一个生产者的递增序列，满队列时只执行短自旋。 */
static int testMPMCProducer(ptr pData)
{
	testmpmcproducer* pProducer = (testmpmcproducer*)pData;
	testmpmcshared* pShared = pProducer->Shared;
	size_t iSent = 0;
	ptr pItems[TEST_MPMC_PUSH_BATCH];

	while ( iSent < pShared->ItemsPerProducer ) {
		size_t iRequest = pShared->ItemsPerProducer - iSent;
		xqueuebatchresult Batch;

		if ( iRequest > TEST_MPMC_PUSH_BATCH ) {
			iRequest = TEST_MPMC_PUSH_BATCH;
		}
		for ( size_t i = 0; i < iRequest; i++ ) {
			uintptr_t iValue =
				((uintptr_t)pProducer->Index * pShared->ItemsPerProducer) +
				iSent + i + 1u;

			pItems[i] = (ptr)iValue;
		}
		Batch = xrtMPMCQueuePushBatch(pShared->Queue, pItems, iRequest);
		if ( Batch.Result == XQUEUE_OK ) {
			if ( Batch.Count == 0u ) {
				xrtAtomic32Store(&pShared->Failure, 1u, XMEMORY_RELAXED);
				return 1;
			}
			iSent += Batch.Count;
			continue;
		}
		if ( Batch.Result != XQUEUE_FULL ) {
			xrtAtomic32Store(&pShared->Failure, 2u, XMEMORY_RELAXED);
			return 2;
		}
		xrtAtomicPause();
	}
	return 0;
}



/* 批量竞争消费并以原子标记验证每个值恰好出现一次。 */
static int testMPMCConsumer(ptr pData)
{
	testmpmcshared* pShared = (testmpmcshared*)pData;
	ptr pItems[TEST_MPMC_POP_BATCH];

	for ( ;; ) {
		xqueuebatchresult Batch = xrtMPMCQueuePopBatch(
			pShared->Queue,
			pItems,
			TEST_MPMC_POP_BATCH
		);

		if ( Batch.Result == XQUEUE_OK ) {
			if ( Batch.Count == 0u ) {
				xrtAtomic32Store(&pShared->Failure, 3u, XMEMORY_RELAXED);
				return 3;
			}
			for ( size_t i = 0; i < Batch.Count; i++ ) {
				uintptr_t iValue = (uintptr_t)pItems[i];
				uint32 iExpected = 0u;

				if ( (iValue < 1u) || (iValue > pShared->TotalItems) ) {
					xrtAtomic32Store(&pShared->Failure, 4u, XMEMORY_RELAXED);
					return 4;
				}
				if (
					!xrtAtomic32CompareExchange(
						&pShared->Seen[iValue - 1u],
						&iExpected,
						1u,
						XMEMORY_ACQ_REL,
						XMEMORY_RELAXED
					)
				) {
					xrtAtomic32Store(&pShared->Failure, 5u, XMEMORY_RELAXED);
					return 5;
				}
				xrtAtomic32FetchAdd(&pShared->Consumed, 1u, XMEMORY_RELAXED);
			}
			continue;
		}
		if ( Batch.Result == XQUEUE_EMPTY ) {
			if ( xrtAtomic32Load(&pShared->Failure, XMEMORY_RELAXED) != 0u ) {
				return 6;
			}
			xrtAtomicPause();
			continue;
		}
		if ( Batch.Result == XQUEUE_CLOSED ) {
			return xrtAtomic32Load(&pShared->Failure, XMEMORY_RELAXED) == 0u ? 0 : 7;
		}

		xrtAtomic32Store(&pShared->Failure, 6u, XMEMORY_RELAXED);
		return 8;
	}
}



/* 运行一次指定容量和规模的四生产者四消费者竞争测试。 */
static void testMPMCRun(size_t iCapacity, size_t iItemsPerProducer)
{
	xmpmcqueue tQueue;
	testmpmcshared tShared;
	testmpmcproducer pProducer[TEST_MPMC_PRODUCERS];
	testthread pProducerThread[TEST_MPMC_PRODUCERS];
	testthread pConsumerThread[TEST_MPMC_CONSUMERS];

	testRequire(xrtMPMCQueueInit(&tQueue, iCapacity), "threaded MPMC init failed");
	tShared.Queue = &tQueue;
	tShared.ItemsPerProducer = iItemsPerProducer;
	tShared.TotalItems = TEST_MPMC_PRODUCERS * iItemsPerProducer;
	tShared.Seen = (xatomic32*)xrtMalloc(tShared.TotalItems * sizeof(xatomic32));
	testRequire(tShared.Seen != NULL, "threaded MPMC seen table allocation failed");
	for ( size_t i = 0; i < tShared.TotalItems; i++ ) {
		xrtAtomic32Init(&tShared.Seen[i], 0u);
	}
	xrtAtomic32Init(&tShared.Consumed, 0u);
	xrtAtomic32Init(&tShared.Failure, 0u);
	memset(pProducerThread, 0, sizeof(pProducerThread));
	memset(pConsumerThread, 0, sizeof(pConsumerThread));

	for ( uint32 i = 0u; i < TEST_MPMC_CONSUMERS; i++ ) {
		pConsumerThread[i].Proc = testMPMCConsumer;
		pConsumerThread[i].Data = &tShared;
	}
	testThreadsStart(pConsumerThread, TEST_MPMC_CONSUMERS);
	for ( uint32 i = 0u; i < TEST_MPMC_PRODUCERS; i++ ) {
		pProducer[i].Shared = &tShared;
		pProducer[i].Index = i;
		pProducerThread[i].Proc = testMPMCProducer;
		pProducerThread[i].Data = &pProducer[i];
	}
	testThreadsStart(pProducerThread, TEST_MPMC_PRODUCERS);
	testThreadsJoin(pProducerThread, TEST_MPMC_PRODUCERS);
	for ( uint32 i = 0u; i < TEST_MPMC_PRODUCERS; i++ ) {
		testRequire(pProducerThread[i].Result == 0, "threaded MPMC producer failed");
	}
	testRequire(
		xrtAtomic32Load(&tShared.Failure, XMEMORY_RELAXED) == 0u,
		"threaded MPMC producer phase reported failure"
	);

	/* 所有生产者返回后关闭，消费者继续领取剩余元素并退出。 */
	xrtMPMCQueueClose(&tQueue);
	testThreadsJoin(pConsumerThread, TEST_MPMC_CONSUMERS);
	for ( uint32 i = 0u; i < TEST_MPMC_CONSUMERS; i++ ) {
		testRequire(pConsumerThread[i].Result == 0, "threaded MPMC consumer failed");
	}
	testRequire(
		xrtAtomic32Load(&tShared.Consumed, XMEMORY_RELAXED) == tShared.TotalItems,
		"threaded MPMC consumed count mismatch"
	);
	for ( size_t i = 0; i < tShared.TotalItems; i++ ) {
		testRequire(
			xrtAtomic32Load(&tShared.Seen[i], XMEMORY_RELAXED) == 1u,
			"threaded MPMC lost item"
		);
	}
	testRequire(xrtMPMCQueueIsDrained(&tQueue), "threaded MPMC did not drain");
	testRequire(xrtMPMCQueueCount(&tQueue) == 0u, "threaded MPMC final count mismatch");
	xrtFree(tShared.Seen);
	xrtMPMCQueueUnit(&tQueue);
}



/* 验证常规批量吞吐和容量为二时的极端槽位复用竞争。 */
int main(void)
{
	testMPMCRun(64u, 100000u);
	testMPMCRun(2u, 25000u);
	printf("[PASS] queue_mpmc_threads\n");
	return 0;
}
