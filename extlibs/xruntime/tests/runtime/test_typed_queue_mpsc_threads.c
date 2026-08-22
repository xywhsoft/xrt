#include "../test.h"
#include "../test_thread.h"



#define TEST_TYPED_MPSC_PRODUCERS 4u
#define TEST_TYPED_MPSC_ITEMS 50000u
#define TEST_TYPED_MPSC_TOTAL \
	(TEST_TYPED_MPSC_PRODUCERS * TEST_TYPED_MPSC_ITEMS)



/* MPSC 竞争测试共享队列、生产完成数和失败码。 */
typedef struct testtypedmpscshared {
	xtypedmpscqueue* Queue;
	xatomic32 Completed;
	xatomic32 Failure;
} testtypedmpscshared;



/* 每个生产者保存独立编号以验证各自 FIFO。 */
typedef struct testtypedmpscproducer {
	testtypedmpscshared* Shared;
	uint32 Index;
} testtypedmpscproducer;



/* 并发复制发布一个生产者的递增编号区间。 */
static int testTypedMPSCProducer(ptr pData)
{
	testtypedmpscproducer* pProducer = (testtypedmpscproducer*)pData;

	for ( uint32 i = 1u; i <= TEST_TYPED_MPSC_ITEMS; ) {
		uint64 iValue = ((uint64)pProducer->Index << 32) | i;
		xqueueresult Result = xrtTypedMPSCQueueTryPush(
			pProducer->Shared->Queue, &iValue
		);

		if ( Result == XQUEUE_OK ) {
			i++;
			continue;
		}
		if ( Result != XQUEUE_FULL ) {
			xrtAtomic32Store(
				&pProducer->Shared->Failure, 1u, XMEMORY_RELEASE
			);
			return 1;
		}
		xrtAtomicPause();
	}
	xrtAtomic32FetchAdd(
		&pProducer->Shared->Completed, 1u, XMEMORY_RELEASE
	);
	return 0;
}



/* 验证多生产者竞争下无丢失、重复且各生产者保持 FIFO。 */
int main(void)
{
	xtypedmpscqueue Queue;
	testtypedmpscshared Shared;
	testtypedmpscproducer Producers[TEST_TYPED_MPSC_PRODUCERS];
	testthread Threads[TEST_TYPED_MPSC_PRODUCERS] = { 0 };
	uint32 pLast[TEST_TYPED_MPSC_PRODUCERS] = { 0u };
	uint8* pSeen;
	size_t iReceived = 0u;

	testRequire(
		xrtTypedMPSCQueueInit(&Queue, xrtTypeUInt64(), 256u),
		"threaded typed MPSC init failed"
	);
	Shared.Queue = &Queue;
	xrtAtomic32Init(&Shared.Completed, 0u);
	xrtAtomic32Init(&Shared.Failure, 0u);
	pSeen = (uint8*)xrtCalloc(TEST_TYPED_MPSC_TOTAL, sizeof(uint8));
	testRequire(pSeen != NULL, "typed MPSC seen table allocation failed");
	for ( uint32 i = 0u; i < TEST_TYPED_MPSC_PRODUCERS; i++ ) {
		Producers[i].Shared = &Shared;
		Producers[i].Index = i;
		Threads[i].Proc = testTypedMPSCProducer;
		Threads[i].Data = &Producers[i];
	}
	testThreadsStart(Threads, TEST_TYPED_MPSC_PRODUCERS);
	while ( iReceived < TEST_TYPED_MPSC_TOTAL ) {
		uint64 iValue = 0u;
		xqueueresult Result = xrtTypedMPSCQueueTryPop(&Queue, &iValue);

		if ( Result == XQUEUE_EMPTY ) {
			testRequire(
				xrtAtomic32Load(&Shared.Failure, XMEMORY_ACQUIRE) == 0u,
				"threaded typed MPSC producer failed"
			);
			xrtAtomicPause();
			continue;
		}
		testRequire(Result == XQUEUE_OK, "threaded typed MPSC pop failed");
		{
			uint32 iProducer = (uint32)(iValue >> 32);
			uint32 iSequence = (uint32)iValue;
			size_t iSeen = ((size_t)iProducer * TEST_TYPED_MPSC_ITEMS) +
				iSequence - 1u;

			testRequire(
				(iProducer < TEST_TYPED_MPSC_PRODUCERS) &&
				(iSequence == (pLast[iProducer] + 1u)) &&
				(pSeen[iSeen] == 0u),
				"threaded typed MPSC order or uniqueness mismatch"
			);
			pLast[iProducer] = iSequence;
			pSeen[iSeen] = 1u;
		}
		iReceived++;
	}
	testThreadsJoin(Threads, TEST_TYPED_MPSC_PRODUCERS);
	for ( uint32 i = 0u; i < TEST_TYPED_MPSC_PRODUCERS; i++ ) {
		testRequire(
			(Threads[i].Result == 0) &&
			(pLast[i] == TEST_TYPED_MPSC_ITEMS),
			"threaded typed MPSC producer result mismatch"
		);
	}
	testRequire(
		xrtAtomic32Load(&Shared.Completed, XMEMORY_ACQUIRE) ==
		TEST_TYPED_MPSC_PRODUCERS,
		"threaded typed MPSC completion mismatch"
	);
	xrtTypedMPSCQueueClose(&Queue);
	xrtFree(pSeen);
	xrtTypedMPSCQueueUnit(&Queue);
	printf("[PASS] typed MPSC queue threads\n");
	return 0;
}
