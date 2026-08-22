#include "../test.h"



/* 验证 MPMC 类型队列的容量、批量、移动、关闭和重置合同。 */
int main(void)
{
	xtypedmpmcqueue Queue;
	uint32 pInput[] = { 1u, 2u, 3u, 4u, 5u };
	uint32 pOutput[4] = { 0u };
	uint32 iTaken = 6u;
	xqueuebatchresult Batch;

	testRequire(
		xrtTypedMPMCQueueInit(&Queue, xrtTypeUInt32(), 3u),
		"typed MPMC init failed"
	);
	testRequire(
		(xrtTypedMPMCQueueItemType(&Queue) == xrtTypeUInt32()) &&
		(xrtTypedMPMCQueueCapacity(&Queue) == 4u),
		"typed MPMC metadata mismatch"
	);
	Batch = xrtTypedMPMCQueuePushBatch(&Queue, pInput, 5u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 4u) &&
		(xrtTypedMPMCQueueCount(&Queue) == 4u),
		"typed MPMC partial push mismatch"
	);
	testRequire(
		xrtTypedMPMCQueueTryPush(&Queue, &pInput[4]) == XQUEUE_FULL,
		"typed MPMC full result mismatch"
	);
	Batch = xrtTypedMPMCQueuePopBatch(&Queue, pOutput, 2u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 2u) &&
		(pOutput[0] == 1u) && (pOutput[1] == 2u),
		"typed MPMC pop batch mismatch"
	);
	testRequire(
		xrtTypedMPMCQueueTryPushTake(&Queue, &iTaken) == XQUEUE_OK &&
		(iTaken == 0u),
		"typed MPMC take push mismatch"
	);
	xrtTypedMPMCQueueClose(&Queue);
	Batch = xrtTypedMPMCQueuePopBatch(&Queue, pOutput, 4u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u) &&
		(pOutput[0] == 3u) && (pOutput[1] == 4u) &&
		(pOutput[2] == 6u),
		"typed MPMC drain order mismatch"
	);
	testRequire(
		(xrtTypedMPMCQueueTryPop(&Queue, &pOutput[0]) == XQUEUE_CLOSED) &&
		xrtTypedMPMCQueueIsDrained(&Queue) &&
		xrtTypedMPMCQueueReset(&Queue),
		"typed MPMC terminal or reset mismatch"
	);
	xrtTypedMPMCQueueUnit(&Queue);
	xrtClearError();
	printf("[PASS] typed MPMC queue\n");
	return 0;
}
