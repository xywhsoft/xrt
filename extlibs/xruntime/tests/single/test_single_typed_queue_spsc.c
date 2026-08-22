#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的 SPSC 类型值所有权路径。 */
int main(void)
{
	xtypedspscqueue Queue;
	int32 iInput = 7;
	int32 iOutput = 0;
	int iResult = 0;

	if ( !xrtTypedSPSCQueueInit(&Queue, xrtTypeInt32(), 2u) ) {
		return 1;
	}
	if ( (xrtTypedSPSCQueueTryPush(&Queue, &iInput) != XQUEUE_OK) ||
		 (xrtTypedSPSCQueueTryPop(&Queue, &iOutput) != XQUEUE_OK) ||
		 (iOutput != 7) ) {
		iResult = 2;
	}
	xrtTypedSPSCQueueUnit(&Queue);
	return iResult;
}
