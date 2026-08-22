#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的嵌入式互斥锁。 */
int main(void)
{
	xmutex tMutex;

	return xrtMutexInit(&tMutex) && xrtMutexLock(&tMutex) &&
		xrtMutexUnlock(&tMutex) && xrtMutexUnit(&tMutex) ? 0 : 1;
}
