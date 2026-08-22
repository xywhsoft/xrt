#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供外部槽环 MPSC 队列。 */
int main(void)
{
	xmpscqueue tQueue;
	xqueueslot pStorage[4];
	int iValue = 7;
	ptr pOutput = NULL;

	if ( !xrtMPSCQueueInitBuffer(&tQueue, pStorage, 4u) ) {
		return 1;
	}
	if ( xrtMPSCQueueTryPush(&tQueue, &iValue) != XQUEUE_OK ) {
		return 2;
	}
	if (
		(xrtMPSCQueueTryPop(&tQueue, &pOutput) != XQUEUE_OK) ||
		(pOutput != &iValue)
	) {
		return 3;
	}
	xrtMPSCQueueUnit(&tQueue);
	return 0;
}
