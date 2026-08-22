#define XRT_MODULE_MEMORY_POOL
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头组合构建必须无损拒绝误交给全局堆的显式池对象。 */
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
	if ( (pSmall == NULL) || (pLarge == NULL) ) {
		xrtMemPoolUnit(&tPool);
		return 2;
	}

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

	if ( xrtMemPoolReset(&tPool) != 2 ) {
		xrtMemPoolUnit(&tPool);
		return 5;
	}
	xrtMemPoolUnit(&tPool);
	return 0;
}
