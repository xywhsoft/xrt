#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的读写锁基本路径。 */
int main(void)
{
	xrwlock tLock;

	return xrtRWLockInit(&tLock) && xrtRWLockRead(&tLock) &&
		xrtRWLockReadUnlock(&tLock) && xrtRWLockWrite(&tLock) &&
		xrtRWLockWriteUnlock(&tLock) && xrtRWLockUnit(&tLock) ? 0 : 1;
}
