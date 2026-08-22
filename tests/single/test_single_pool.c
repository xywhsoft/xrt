#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供可跨页增长的固定对象池。 */
int main(void)
{
	xpool tPool;
	ptr arrObject[300];
	xpoolinfo tInfo;

	if ( !xrtPoolInitLayout(&tPool, 24, 16, 128) ) {
		return 1;
	}
	for ( size_t i = 0; i < 300; i++ ) {
		arrObject[i] = xrtPoolAlloc(&tPool);
		if ( arrObject[i] == NULL ) {
			xrtPoolUnit(&tPool);
			return 2;
		}
	}
	xrtPoolGet(&tPool, &tInfo);
	if (
		(tInfo.PageCapacity != 128) ||
		(tInfo.PageCount != 3) ||
		(xrtPoolReset(&tPool) != 300)
	) {
		xrtPoolUnit(&tPool);
		return 3;
	}
	xrtPoolUnit(&tPool);
	return 0;
}
