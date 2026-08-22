#include <xruntime.h>

#include <stdio.h>



/* 展示固定容量 SPSC 类型队列的复制入队和移动出队。 */
int main(void)
{
	xtypedspscqueue Queue;
	int32 iInput = 42;
	int32 iOutput = 0;

	if ( !xrtTypedSPSCQueueInit(&Queue, xrtTypeInt32(), 16u) ||
		 (xrtTypedSPSCQueueTryPush(&Queue, &iInput) != XQUEUE_OK) ||
		 (xrtTypedSPSCQueueTryPop(&Queue, &iOutput) != XQUEUE_OK) ) {
		xrtTypedSPSCQueueUnit(&Queue);
		return 1;
	}
	printf("value=%d capacity=%zu\n", iOutput, Queue.Core.Capacity);
	xrtTypedSPSCQueueUnit(&Queue);
	return 0;
}
