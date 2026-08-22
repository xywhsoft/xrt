#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的 MPMC 类型值所有权路径。 */
int main(void)
{
	xtypedmpmcqueue Queue;
	uint64 iInput = 27u;
	uint64 iOutput = 0u;
	int iResult = 0;

	if ( !xrtTypedMPMCQueueInit(&Queue, xrtTypeUInt64(), 2u) ) {
		return 1;
	}
	if ( (xrtTypedMPMCQueueTryPush(&Queue, &iInput) != XQUEUE_OK) ||
		 (xrtTypedMPMCQueueTryPop(&Queue, &iOutput) != XQUEUE_OK) ||
		 (iOutput != 27u) ) {
		iResult = 2;
	}
	xrtTypedMPMCQueueUnit(&Queue);
	return iResult;
}
