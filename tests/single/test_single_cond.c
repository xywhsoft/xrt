#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的条件变量零超时。 */
int main(void)
{
	xmutex tMutex;
	xcond tCond;
	int iResult = 1;

	if ( xrtMutexInit(&tMutex) && xrtCondInit(&tCond) &&
		 xrtMutexLock(&tMutex) &&
		 (xrtCondWaitFor(&tCond, &tMutex, 0) == XWAIT_TIMEOUT) &&
		 xrtMutexUnlock(&tMutex) && xrtCondUnit(&tCond) &&
		 xrtMutexUnit(&tMutex) ) {
		iResult = 0;
	}
	return iResult;
}
