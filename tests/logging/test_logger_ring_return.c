#include <xrt/detail/wait.h>
#include "../test.h"

/* Keep the real Ring implementation and its private slot-return path in this
 * translation unit, with distinct public names from the linked module. */
static xatomic32 ReturnWaiting;
static void testRingReturnYield(void)
{
	xrtAtomic32Store(&ReturnWaiting, 1u, XMEMORY_RELEASE);
	xrtThreadYield();
}
#define xrtThreadYield testRingReturnYield
#define xrtLogRingConfigInit testRingConfigInit
#define xrtLogRing testRingCreate
#define xrtLogAddRing testRingAdd
#define xrtLogRingTarget testRingTarget
#define xrtLogRingStop testRingStop
#define xrtLogRingStats testRingStats
#define xrtLogRingLastError testRingLastError
#include "../../src/logging/logger_ring.c"
#undef xrtThreadYield
#undef xrtLogRingConfigInit
#undef xrtLogRing
#undef xrtLogAddRing
#undef xrtLogRingTarget
#undef xrtLogRingStop
#undef xrtLogRingStats
#undef xrtLogRingLastError

typedef struct test_ring_return {
	xlogringstate* State;
	xlogringslot* Slot;
	xqueueresult Result;
	xatomic32 Done;
} test_ring_return;

static int32 testRingReturnWorker(ptr pData)
{
	test_ring_return* pTest = (test_ring_return*)pData;
	pTest->Result = __xrtLogRingSlotReturn(pTest->State, pTest->Slot);
	xrtAtomic32Store(&pTest->Done, 1u, XMEMORY_RELEASE);
	return 0;
}

/* Reproduce a paused Free-queue consumer after its head reservation. A later
 * consumer can publish a record while that first sequence is still pending. */
int main(void)
{
	xlogringstate State;
	xlogringslot First, Second;
	test_ring_return Test;
	xthread* pThread;
	ptr pItem = NULL;
	double Deadline;

	memset(&State, 0, sizeof(State));
	memset(&First, 0, sizeof(First));
	memset(&Second, 0, sizeof(Second));
	memset(&Test, 0, sizeof(Test));
	testRequire(xrtMPMCQueueInit(&State.Free, 2u), "Ring return queue init failed");
	testRequire(xrtMPMCQueueTryPush(&State.Free, &First) == XQUEUE_OK &&
		xrtMPMCQueueTryPush(&State.Free, &Second) == XQUEUE_OK,
		"Ring return queue setup failed");
	/* This is the exact intermediate state in MPMC TryPop: Head has advanced
	 * but the reserved slot's release sequence has not yet been stored. */
	xrtAtomic32Store(&State.Free.Head.Position, 1u, XMEMORY_RELAXED);
	testRequire(xrtMPMCQueueTryPop(&State.Free, &pItem) == XQUEUE_OK &&
		pItem == &Second, "Ring return second consumer failed");
	Test.State = &State;
	Test.Slot = &Second;
	pThread = xrtThreadCreate(testRingReturnWorker, &Test, 0u);
	testRequire(pThread != NULL, "Ring return thread create failed");
	Deadline = __xrtWaitAfter(5000);
	while ( xrtAtomic32Load(&ReturnWaiting, XMEMORY_ACQUIRE) == 0u ) {
		testRequire(xrtAtomic32Load(&Test.Done, XMEMORY_ACQUIRE) == 0u,
			"Ring returned a slot before the pending consumer released its sequence");
		testRequire(!__xrtWaitExpired(Deadline), "Ring return did not wait for reservation");
		xrtThreadYield();
	}
	testRequire(xrtAtomic32Load(&Test.Done, XMEMORY_ACQUIRE) == 0u,
		"Ring return discarded the temporarily blocked slot");
	State.Free.Slots[0].Item = NULL;
	xrtAtomic32Store(&State.Free.Slots[0].Sequence, 2u, XMEMORY_RELEASE);
	testRequire(xrtThreadWait(pThread) == XWAIT_OK && Test.Result == XQUEUE_OK,
		"Ring return did not finish after sequence release");
	xrtThreadDestroy(pThread);
	testRequire(__xrtLogRingSlotReturn(&State, &First) == XQUEUE_OK,
		"Ring return lost the first consumer's slot");
	testRequire(xrtMPMCQueueTryPop(&State.Free, &pItem) == XQUEUE_OK &&
		pItem == &Second, "Ring return lost or duplicated the second slot");
	testRequire(xrtMPMCQueueTryPop(&State.Free, &pItem) == XQUEUE_OK &&
		pItem == &First, "Ring return lost or duplicated the first slot");
	testRequire(xrtMPMCQueueTryPop(&State.Free, &pItem) == XQUEUE_EMPTY,
		"Ring return queue has a duplicate slot");
	xrtMPMCQueueUnit(&State.Free);
	puts("[PASS] Logger ring delayed slot return");
	return 0;
}
