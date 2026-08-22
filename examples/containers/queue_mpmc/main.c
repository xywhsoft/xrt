#include <stdio.h>
#include <xrt.h>



/* 模拟多个生产者发布，并由多个消费者通过同一组 API 领取。 */
int main(void)
{
	xmpmcqueue Queue;
	xqueueslot Storage[8];
	int pValues[] = { 10, 20, 30, 40 };
	ptr pFirstBatch[] = { &pValues[0], &pValues[1] };
	ptr pSecondBatch[] = { &pValues[2], &pValues[3] };
	ptr pOutput[4];
	xqueuebatchresult Batch;

	if ( !xrtMPMCQueueInitBuffer(&Queue, Storage, 8u) ) {
		return 1;
	}
	if ( xrtMPMCQueuePushBatch(&Queue, pFirstBatch, 2u).Count != 2u ) {
		xrtMPMCQueueUnit(&Queue);
		return 2;
	}
	if ( xrtMPMCQueuePushBatch(&Queue, pSecondBatch, 2u).Count != 2u ) {
		xrtMPMCQueueUnit(&Queue);
		return 3;
	}
	xrtMPMCQueueClose(&Queue);

	/* 实际程序可让任意消费者并发执行同一个弹出接口。 */
	Batch = xrtMPMCQueuePopBatch(&Queue, pOutput, 4u);
	for ( size_t i = 0; i < Batch.Count; i++ ) {
		printf("%d\n", *(int*)pOutput[i]);
	}
	xrtMPMCQueueUnit(&Queue);
	return Batch.Count == 4u ? 0 : 4;
}
