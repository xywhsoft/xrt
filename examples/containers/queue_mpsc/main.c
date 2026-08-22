#include <stdio.h>
#include <xrt.h>



/* 模拟两个生产者批量发布，并由唯一消费者按 FIFO 排空。 */
int main(void)
{
	xmpscqueue Queue;
	xqueueslot Storage[8];
	int pValues[] = { 10, 20, 30, 40 };
	ptr pFirstBatch[] = { &pValues[0], &pValues[1] };
	ptr pSecondBatch[] = { &pValues[2], &pValues[3] };
	ptr pOutput[4];
	xqueuebatchresult Batch;

	if ( !xrtMPSCQueueInitBuffer(&Queue, Storage, 8u) ) {
		return 1;
	}
	if ( xrtMPSCQueuePushBatch(&Queue, pFirstBatch, 2u).Count != 2u ) {
		xrtMPSCQueueUnit(&Queue);
		return 2;
	}
	if ( xrtMPSCQueuePushBatch(&Queue, pSecondBatch, 2u).Count != 2u ) {
		xrtMPSCQueueUnit(&Queue);
		return 3;
	}

	/* 实际并发程序必须等待全部生产者返回后再关闭。 */
	xrtMPSCQueueClose(&Queue);
	Batch = xrtMPSCQueuePopBatch(&Queue, pOutput, 4u);
	for ( size_t i = 0; i < Batch.Count; i++ ) {
		printf("%d\n", *(int*)pOutput[i]);
	}
	xrtMPSCQueueUnit(&Queue);
	return Batch.Count == 4u ? 0 : 4;
}
