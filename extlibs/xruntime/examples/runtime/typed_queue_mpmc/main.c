#include <xruntime.h>

#include <stdio.h>



/* 展示固定容量 MPMC 类型队列的复制入队和移动出队。 */
int main(void)
{
	xtypedmpmcqueue Queue;
	uint64 iInput = 62u;
	uint64 iOutput = 0u;

	if ( !xrtTypedMPMCQueueInit(&Queue, xrtTypeUInt64(), 16u) ||
		 (xrtTypedMPMCQueueTryPush(&Queue, &iInput) != XQUEUE_OK) ||
		 (xrtTypedMPMCQueueTryPop(&Queue, &iOutput) != XQUEUE_OK) ) {
		xrtTypedMPMCQueueUnit(&Queue);
		return 1;
	}
	printf("value=%llu capacity=%zu\n", (unsigned long long)iOutput, Queue.Core.Capacity);
	xrtTypedMPMCQueueUnit(&Queue);
	return 0;
}
