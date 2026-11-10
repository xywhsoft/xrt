#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的嵌入式互斥锁。 */
int main(void)
{
	xmutex tMutex;

	return xrtMutexInit(&tMutex) &&
        xrtMutexTryLockResult(&tMutex)==XMUTEX_TRY_OK &&
        xrtMutexTryLockResult(&tMutex)==XMUTEX_TRY_BUSY &&
        !xrtMutexTryLock(&tMutex) &&
		xrtMutexUnlock(&tMutex) && xrtMutexUnit(&tMutex) ? 0 : 1;
}
