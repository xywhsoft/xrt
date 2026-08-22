#include <xruntime.h>

#include <stdio.h>



/* 展示固定容量 MPSC 类型队列的复制入队和移动出队。 */
int main(void)
{
	xtypedmpscqueue Queue;
	int64 iInput = 52;
	int64 iOutput = 0;

	if ( !xrtTypedMPSCQueueInit(&Queue, xrtTypeInt64(), 16u) ||
		 (xrtTypedMPSCQueueTryPush(&Queue, &iInput) != XQUEUE_OK) ||
		 (xrtTypedMPSCQueueTryPop(&Queue, &iOutput) != XQUEUE_OK) ) {
		xrtTypedMPSCQueueUnit(&Queue);
		return 1;
	}
	printf("value=%lld capacity=%zu\n", (long long)iOutput, Queue.Core.Capacity);
	xrtTypedMPSCQueueUnit(&Queue);
	return 0;
}
