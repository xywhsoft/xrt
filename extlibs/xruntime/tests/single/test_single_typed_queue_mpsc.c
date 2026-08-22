#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的 MPSC 类型值所有权路径。 */
int main(void)
{
	xtypedmpscqueue Queue;
	int64 iInput = 17;
	int64 iOutput = 0;
	int iResult = 0;

	if ( !xrtTypedMPSCQueueInit(&Queue, xrtTypeInt64(), 2u) ) {
		return 1;
	}
	if ( (xrtTypedMPSCQueueTryPush(&Queue, &iInput) != XQUEUE_OK) ||
		 (xrtTypedMPSCQueueTryPop(&Queue, &iOutput) != XQUEUE_OK) ||
		 (iOutput != 17) ) {
		iResult = 2;
	}
	xrtTypedMPSCQueueUnit(&Queue);
	return iResult;
}
