#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供外部槽环 MPMC 队列。 */
int main(void)
{
	xmpmcqueue tQueue;
	xqueueslot pStorage[4];
	int iValue = 7;
	ptr pOutput = NULL;

	if ( !xrtMPMCQueueInitBuffer(&tQueue, pStorage, 4u) ) {
		return 1;
	}
	if ( xrtMPMCQueueTryPush(&tQueue, &iValue) != XQUEUE_OK ) {
		return 2;
	}
	if (
		(xrtMPMCQueueTryPop(&tQueue, &pOutput) != XQUEUE_OK) ||
		(pOutput != &iValue)
	) {
		return 3;
	}
	xrtMPMCQueueUnit(&tQueue);
	return 0;
}
