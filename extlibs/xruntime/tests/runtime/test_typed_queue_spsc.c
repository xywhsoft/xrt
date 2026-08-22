#include "../test.h"



/* 验证 SPSC 类型队列的容量、批量、移动、关闭和重置合同。 */
int main(void)
{
	xtypedspscqueue Queue;
	int32 pInput[] = { 10, 20, 30, 40, 50 };
	int32 pOutput[4] = { 0 };
	int32 iTaken = 60;
	xqueuebatchresult Batch;

	testRequire(
		xrtTypedSPSCQueueInit(&Queue, xrtTypeInt32(), 3u),
		"typed SPSC init failed"
	);
	testRequire(
		(xrtTypedSPSCQueueItemType(&Queue) == xrtTypeInt32()) &&
		(xrtTypedSPSCQueueCapacity(&Queue) == 4u) &&
		(xrtTypedSPSCQueueCount(&Queue) == 0u),
		"typed SPSC metadata mismatch"
	);
	Batch = xrtTypedSPSCQueuePushBatch(&Queue, pInput, 5u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 4u) &&
		(xrtTypedSPSCQueueCount(&Queue) == 4u),
		"typed SPSC partial push mismatch"
	);
	testRequire(
		xrtTypedSPSCQueueTryPush(&Queue, &pInput[4]) == XQUEUE_FULL,
		"typed SPSC full result mismatch"
	);
	Batch = xrtTypedSPSCQueuePopBatch(&Queue, pOutput, 2u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 2u) &&
		(pOutput[0] == 10) && (pOutput[1] == 20),
		"typed SPSC pop batch mismatch"
	);
	testRequire(
		xrtTypedSPSCQueueTryPushTake(&Queue, &iTaken) == XQUEUE_OK &&
		(iTaken == 0),
		"typed SPSC take push mismatch"
	);
	xrtTypedSPSCQueueClose(&Queue);
	testRequire(
		xrtTypedSPSCQueueIsClosed(&Queue) &&
		(xrtTypedSPSCQueueTryPush(&Queue, &pInput[0]) == XQUEUE_CLOSED),
		"typed SPSC close mismatch"
	);
	Batch = xrtTypedSPSCQueuePopBatch(&Queue, pOutput, 4u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u) &&
		(pOutput[0] == 30) && (pOutput[1] == 40) &&
		(pOutput[2] == 60),
		"typed SPSC drain order mismatch"
	);
	testRequire(
		(xrtTypedSPSCQueueTryPop(&Queue, &pOutput[0]) == XQUEUE_CLOSED) &&
		xrtTypedSPSCQueueIsDrained(&Queue) &&
		xrtTypedSPSCQueueReset(&Queue) &&
		!xrtTypedSPSCQueueIsClosed(&Queue),
		"typed SPSC terminal or reset mismatch"
	);
	xrtTypedSPSCQueueUnit(&Queue);
	xrtClearError();
	printf("[PASS] typed SPSC queue\n");
	return 0;
}
