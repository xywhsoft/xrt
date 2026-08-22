#include "../test.h"
#include "../test_thread.h"



#define TEST_TYPED_SPSC_ITEMS 200000u



/* SPSC 竞争测试共享固定队列和失败码。 */
typedef struct testtypedspscshared {
	xtypedspscqueue* Queue;
	xatomic32 Failure;
} testtypedspscshared;



/* 唯一生产者按严格递增顺序复制发布全部标量值。 */
static int testTypedSPSCProducer(ptr pData)
{
	testtypedspscshared* pShared = (testtypedspscshared*)pData;

	for ( uint64 i = 1u; i <= TEST_TYPED_SPSC_ITEMS; ) {
		xqueueresult Result = xrtTypedSPSCQueueTryPush(
			pShared->Queue, &i
		);

		if ( Result == XQUEUE_OK ) {
			i++;
			continue;
		}
		if ( Result != XQUEUE_FULL ) {
			xrtAtomic32Store(&pShared->Failure, 1u, XMEMORY_RELEASE);
			return 1;
		}
		xrtAtomicPause();
	}
	return 0;
}



/* 唯一消费者验证每个类型值恰好按 FIFO 顺序到达。 */
static int testTypedSPSCConsumer(ptr pData)
{
	testtypedspscshared* pShared = (testtypedspscshared*)pData;

	for ( uint64 i = 1u; i <= TEST_TYPED_SPSC_ITEMS; ) {
		uint64 iValue = 0u;
		xqueueresult Result = xrtTypedSPSCQueueTryPop(
			pShared->Queue, &iValue
		);

		if ( Result == XQUEUE_OK ) {
			if ( iValue != i ) {
				xrtAtomic32Store(&pShared->Failure, 2u, XMEMORY_RELEASE);
				return 2;
			}
			i++;
			continue;
		}
		if ( Result != XQUEUE_EMPTY ) {
			xrtAtomic32Store(&pShared->Failure, 3u, XMEMORY_RELEASE);
			return 3;
		}
		xrtAtomicPause();
	}
	return 0;
}



/* 验证预分配值槽在真实 SPSC 竞争下无丢失、重复或乱序。 */
int main(void)
{
	xtypedspscqueue Queue;
	testtypedspscshared Shared;
	testthread Threads[2] = { 0 };
	uint64 iValue = 0u;

	testRequire(
		xrtTypedSPSCQueueInit(&Queue, xrtTypeUInt64(), 256u),
		"threaded typed SPSC init failed"
	);
	Shared.Queue = &Queue;
	xrtAtomic32Init(&Shared.Failure, 0u);
	Threads[0].Proc = testTypedSPSCProducer;
	Threads[0].Data = &Shared;
	Threads[1].Proc = testTypedSPSCConsumer;
	Threads[1].Data = &Shared;
	testThreadsStart(Threads, 2u);
	testThreadsJoin(Threads, 2u);
	testRequire(
		(Threads[0].Result == 0) && (Threads[1].Result == 0) &&
		(xrtAtomic32Load(&Shared.Failure, XMEMORY_ACQUIRE) == 0u),
		"threaded typed SPSC worker failed"
	);
	xrtTypedSPSCQueueClose(&Queue);
	testRequire(
		(xrtTypedSPSCQueueTryPop(&Queue, &iValue) == XQUEUE_CLOSED) &&
		xrtTypedSPSCQueueIsDrained(&Queue),
		"threaded typed SPSC terminal mismatch"
	);
	xrtTypedSPSCQueueUnit(&Queue);
	printf("[PASS] typed SPSC queue threads\n");
	return 0;
}
