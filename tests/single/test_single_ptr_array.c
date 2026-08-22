#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供类型友好的指针数组接口。 */
int main(void)
{
	xptrarray tArray;
	int iLeft = 10;
	int iRight = 20;
	ptr pValue;

	if ( !xrtPtrArrayInit(&tArray) ) {
		return 1;
	}
	if (
		!xrtPtrArrayPush(&tArray, &iLeft) ||
		!xrtPtrArrayPush(&tArray, &iRight)
	) {
		xrtPtrArrayUnit(&tArray);
		return 2;
	}
	if ( !xrtPtrArrayPop(&tArray, &pValue) || (pValue != &iRight) ) {
		xrtPtrArrayUnit(&tArray);
		return 3;
	}
	if ( xrtPtrArrayGet(&tArray, 0) != &iLeft ) {
		xrtPtrArrayUnit(&tArray);
		return 4;
	}
	xrtPtrArrayUnit(&tArray);
	return 0;
}
