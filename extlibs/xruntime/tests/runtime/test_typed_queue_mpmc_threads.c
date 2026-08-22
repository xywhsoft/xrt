#include "../test.h"
#include "../test_thread.h"



#define TEST_TYPED_MPMC_PRODUCERS 4u
#define TEST_TYPED_MPMC_CONSUMERS 4u
#define TEST_TYPED_MPMC_ITEMS 25000u
#define TEST_TYPED_MPMC_TOTAL \
	(TEST_TYPED_MPMC_PRODUCERS * TEST_TYPED_MPMC_ITEMS)



/* MPMC 竞争测试共享队列、恰好一次标记和完成统计。 */
typedef struct testtypedmpmcshared {
	xtypedmpmcqueue* Queue;
	xatomic32* Seen;
	xatomic32 Consumed;
	xatomic32 Failure;
} testtypedmpmcshared;



/* 每个 MPMC 生产者保存独立编号。 */
typedef struct testtypedmpmcproducer {
	testtypedmpmcshared* Shared;
	uint32 Index;
} testtypedmpmcproducer;



/* 并发复制发布互不重叠的正整数区间。 */
static int testTypedMPMCProducer(ptr pData)
{
	testtypedmpmcproducer* pProducer = (testtypedmpmcproducer*)pData;

	for ( uint32 i = 1u; i <= TEST_TYPED_MPMC_ITEMS; ) {
		uint64 iValue = ((uint64)pProducer->Index * TEST_TYPED_MPMC_ITEMS) + i;
		xqueueresult Result = xrtTypedMPMCQueueTryPush(
			pProducer->Shared->Queue, &iValue
		);

		if ( Result == XQUEUE_OK ) {
			i++;
			continue;
		}
		if ( Result != XQUEUE_FULL ) {
			const xerror* pError = xrtGetError();

			fprintf(
				stderr,
				"typed MPMC producer result=%d state=%u op=%s message=%s\n",
				(int)Result,
				xrtAtomic32Load(
					&pProducer->Shared->Queue->Core.State,
					XMEMORY_ACQUIRE
				),
				pError != NULL ? xrtErrorOperation(pError) : "none",
				pError != NULL ? xrtErrorMessage(pError) : "none"
			);
			xrtAtomic32Store(
				&pProducer->Shared->Failure, 1u, XMEMORY_RELEASE
			);
			return 1;
		}
		xrtAtomicPause();
	}
	return 0;
}



/* 并发移动消费并验证每个值只出现一次。 */
static int testTypedMPMCConsumer(ptr pData)
{
	testtypedmpmcshared* pShared = (testtypedmpmcshared*)pData;

	for ( ;; ) {
		uint64 iValue = 0u;
		xqueueresult Result = xrtTypedMPMCQueueTryPop(
			pShared->Queue, &iValue
		);

		if ( Result == XQUEUE_OK ) {
			uint32 iExpected = 0u;

			if ( (iValue == 0u) || (iValue > TEST_TYPED_MPMC_TOTAL) ||
				 !xrtAtomic32CompareExchange(
					&pShared->Seen[iValue - 1u],
					&iExpected,
					1u,
					XMEMORY_ACQ_REL,
					XMEMORY_RELAXED
				 ) ) {
				xrtAtomic32Store(&pShared->Failure, 2u, XMEMORY_RELEASE);
				return 2;
			}
			xrtAtomic32FetchAdd(&pShared->Consumed, 1u, XMEMORY_RELAXED);
			continue;
		}
		if ( Result == XQUEUE_EMPTY ) {
			if ( xrtAtomic32Load(&pShared->Failure, XMEMORY_ACQUIRE) != 0u ) {
				return 3;
			}
			xrtAtomicPause();
			continue;
		}
		if ( Result == XQUEUE_CLOSED ) {
			return 0;
		}
		{
			const xerror* pError = xrtGetError();

			fprintf(
				stderr,
				"typed MPMC consumer result=%d state=%u op=%s message=%s\n",
				(int)Result,
				xrtAtomic32Load(
					&pShared->Queue->Core.State, XMEMORY_ACQUIRE
				),
				pError != NULL ? xrtErrorOperation(pError) : "none",
				pError != NULL ? xrtErrorMessage(pError) : "none"
			);
		}
		xrtAtomic32Store(&pShared->Failure, 3u, XMEMORY_RELEASE);
		return 4;
	}
}



/* 验证预分配值槽在多生产者多消费者竞争下恰好传递一次。 */
int main(void)
{
	xtypedmpmcqueue Queue;
	testtypedmpmcshared Shared;
	testtypedmpmcproducer Producers[TEST_TYPED_MPMC_PRODUCERS];
	testthread ProducerThreads[TEST_TYPED_MPMC_PRODUCERS] = { 0 };
	testthread ConsumerThreads[TEST_TYPED_MPMC_CONSUMERS] = { 0 };

	testRequire(
		xrtTypedMPMCQueueInit(&Queue, xrtTypeUInt64(), 256u),
		"threaded typed MPMC init failed"
	);
	Shared.Queue = &Queue;
	Shared.Seen = (xatomic32*)xrtMalloc(
		TEST_TYPED_MPMC_TOTAL * sizeof(xatomic32)
	);
	testRequire(Shared.Seen != NULL, "typed MPMC seen table allocation failed");
	for ( size_t i = 0u; i < TEST_TYPED_MPMC_TOTAL; i++ ) {
		xrtAtomic32Init(&Shared.Seen[i], 0u);
	}
	xrtAtomic32Init(&Shared.Consumed, 0u);
	xrtAtomic32Init(&Shared.Failure, 0u);
	for ( uint32 i = 0u; i < TEST_TYPED_MPMC_CONSUMERS; i++ ) {
		ConsumerThreads[i].Proc = testTypedMPMCConsumer;
		ConsumerThreads[i].Data = &Shared;
	}
	testThreadsStart(ConsumerThreads, TEST_TYPED_MPMC_CONSUMERS);
	for ( uint32 i = 0u; i < TEST_TYPED_MPMC_PRODUCERS; i++ ) {
		Producers[i].Shared = &Shared;
		Producers[i].Index = i;
		ProducerThreads[i].Proc = testTypedMPMCProducer;
		ProducerThreads[i].Data = &Producers[i];
	}
	testThreadsStart(ProducerThreads, TEST_TYPED_MPMC_PRODUCERS);
	testThreadsJoin(ProducerThreads, TEST_TYPED_MPMC_PRODUCERS);
	for ( uint32 i = 0u; i < TEST_TYPED_MPMC_PRODUCERS; i++ ) {
		testRequire(
			ProducerThreads[i].Result == 0,
			"threaded typed MPMC producer failed"
		);
	}
	xrtTypedMPMCQueueClose(&Queue);
	testThreadsJoin(ConsumerThreads, TEST_TYPED_MPMC_CONSUMERS);
	for ( uint32 i = 0u; i < TEST_TYPED_MPMC_CONSUMERS; i++ ) {
		testRequire(
			ConsumerThreads[i].Result == 0,
			"threaded typed MPMC consumer failed"
		);
	}
	testRequire(
		(xrtAtomic32Load(&Shared.Failure, XMEMORY_ACQUIRE) == 0u) &&
		(xrtAtomic32Load(&Shared.Consumed, XMEMORY_ACQUIRE) ==
		 TEST_TYPED_MPMC_TOTAL) &&
		xrtTypedMPMCQueueIsDrained(&Queue),
		"threaded typed MPMC final state mismatch"
	);
	xrtFree(Shared.Seen);
	xrtTypedMPMCQueueUnit(&Queue);
	printf("[PASS] typed MPMC queue threads\n");
	return 0;
}
