#include "../test.h"



/* 验证 MPSC 类型队列的容量、批量、移动、关闭和重置合同。 */
int main(void)
{
	xtypedmpscqueue Queue;
	int64 pInput[] = { 11, 22, 33, 44, 55 };
	int64 pOutput[4] = { 0 };
	int64 iTaken = 66;
	xqueuebatchresult Batch;

	testRequire(
		xrtTypedMPSCQueueInit(&Queue, xrtTypeInt64(), 3u),
		"typed MPSC init failed"
	);
	testRequire(
		(xrtTypedMPSCQueueItemType(&Queue) == xrtTypeInt64()) &&
		(xrtTypedMPSCQueueCapacity(&Queue) == 4u),
		"typed MPSC metadata mismatch"
	);
	Batch = xrtTypedMPSCQueuePushBatch(&Queue, pInput, 5u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 4u) &&
		(xrtTypedMPSCQueueCount(&Queue) == 4u),
		"typed MPSC partial push mismatch"
	);
	testRequire(
		xrtTypedMPSCQueueTryPush(&Queue, &pInput[4]) == XQUEUE_FULL,
		"typed MPSC full result mismatch"
	);
	Batch = xrtTypedMPSCQueuePopBatch(&Queue, pOutput, 2u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 2u) &&
		(pOutput[0] == 11) && (pOutput[1] == 22),
		"typed MPSC pop batch mismatch"
	);
	testRequire(
		xrtTypedMPSCQueueTryPushTake(&Queue, &iTaken) == XQUEUE_OK &&
		(iTaken == 0),
		"typed MPSC take push mismatch"
	);
	xrtTypedMPSCQueueClose(&Queue);
	Batch = xrtTypedMPSCQueuePopBatch(&Queue, pOutput, 4u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u) &&
		(pOutput[0] == 33) && (pOutput[1] == 44) &&
		(pOutput[2] == 66),
		"typed MPSC drain order mismatch"
	);
	testRequire(
		(xrtTypedMPSCQueueTryPop(&Queue, &pOutput[0]) == XQUEUE_CLOSED) &&
		xrtTypedMPSCQueueIsDrained(&Queue) &&
		xrtTypedMPSCQueueReset(&Queue),
		"typed MPSC terminal or reset mismatch"
	);
	xrtTypedMPSCQueueUnit(&Queue);
	xrtClearError();
	printf("[PASS] typed MPSC queue\n");
	return 0;
}
