#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供外部缓冲 SPSC 队列。 */
int main(void)
{
	xspscqueue tQueue;
	ptr pStorage[4];
	int iValue = 7;
	ptr pOutput = NULL;

	if ( !xrtSPSCQueueInitBuffer(&tQueue, pStorage, 4u) ) {
		return 1;
	}
	if ( xrtSPSCQueueTryPush(&tQueue, &iValue) != XQUEUE_OK ) {
		return 2;
	}
	if (
		(xrtSPSCQueueTryPop(&tQueue, &pOutput) != XQUEUE_OK) ||
		(pOutput != &iValue)
	) {
		return 3;
	}
	xrtSPSCQueueUnit(&tQueue);
	return 0;
}
