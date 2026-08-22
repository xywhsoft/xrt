#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供数组的追加、插入和读取能力。 */
int main(void)
{
	xarray tArray;
	int pValues[] = { 1, 3 };
	int iMiddle = 2;

	if ( !xrtArrayInit(&tArray, sizeof(int)) ) {
		return 1;
	}
	if ( !xrtArrayAppend(&tArray, pValues, 2) ) {
		xrtArrayUnit(&tArray);
		return 2;
	}
	if ( !xrtArrayInsert(&tArray, 1, &iMiddle, 1) ) {
		xrtArrayUnit(&tArray);
		return 3;
	}
	if (
		(tArray.Count != 3) ||
		(*(int*)xrtArrayGet(&tArray, 0) != 1) ||
		(*(int*)xrtArrayGet(&tArray, 1) != 2) ||
		(*(int*)xrtArrayGet(&tArray, 2) != 3)
	) {
		xrtArrayUnit(&tArray);
		return 4;
	}
	xrtArrayUnit(&tArray);
	return 0;
}
