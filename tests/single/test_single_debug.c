#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证裁剪宏开启时单头文件包含完整内存调试实现。 */
int main(void)
{
	xmemdebugsnapshot tSnapshot;
	ptr pMemory;

	pMemory = xrtMalloc(32);
	if ( pMemory == NULL ) {
		return 1;
	}
	xrtFree(pMemory);
	xrtMemDebugSnapshot(&tSnapshot);
	if ( (tSnapshot.AllocCount != 1) || (tSnapshot.FreeCount != 1) ) {
		return 2;
	}
	if ( !xrtMemDebugReset() ) {
		return 3;
	}
	return 0;
}
