#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供池化小块和登记大块。 */
int main(void)
{
	xmempool tPool;
	ptr pSmall;
	ptr pLarge;

	if ( !xrtMemPoolInit(&tPool, 128) ) {
		return 1;
	}
	pSmall = xrtMemPoolAlloc(&tPool, 17);
	pLarge = xrtMemPoolAlloc(&tPool, 1024);
	if (
		(pSmall == NULL) ||
		(pLarge == NULL) ||
		(xrtMemPoolSize(&tPool, pSmall) != 32) ||
		(xrtMemPoolSize(&tPool, pLarge) != 1024)
	) {
		xrtMemPoolUnit(&tPool);
		return 2;
	}
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		/* 单头调试堆同样必须无损拒绝显式池对象。 */
		xrtFree(pSmall);
		if (
			(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ||
			!xrtMemPoolOwns(&tPool, pSmall)
		) {
			xrtMemPoolUnit(&tPool);
			return 3;
		}
		xrtClearError();
		xrtFree(pLarge);
		if (
			(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ||
			!xrtMemPoolOwns(&tPool, pLarge)
		) {
			xrtMemPoolUnit(&tPool);
			return 4;
		}
		xrtClearError();
	#endif
	if ( xrtMemPoolReset(&tPool) != 2 ) {
		xrtMemPoolUnit(&tPool);
		return 5;
	}
	xrtMemPoolUnit(&tPool);
	return 0;
}
