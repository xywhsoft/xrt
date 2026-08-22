#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供集合插入顺序和集合运算。 */
int main(void)
{
	xset tLeft;
	xset tRight;
	xset* pUnion;
	xsetiter tIterator;
	int iOne = 1;
	int iTwo = 2;

	if ( !xrtSetInit(&tLeft, sizeof(int)) ||
		!xrtSetInit(&tRight, sizeof(int)) ) {
		return 1;
	}
	if ( !xrtSetAdd(&tLeft, &iOne) || !xrtSetAdd(&tRight, &iTwo) ) {
		return 2;
	}
	if ( !xrtSetIsDisjoint(&tLeft, &tRight) ) {
		return 3;
	}
	pUnion = xrtSetUnion(&tLeft, &tRight);
	if ( (pUnion == NULL) || (xrtSetCount(pUnion) != 2) ) {
		return 4;
	}
	if ( !xrtSetIterBegin(pUnion, &tIterator) ||
		(*(const int*)xrtSetIterNext(&tIterator) != 1) ) {
		return 5;
	}
	xrtSetIterEnd(&tIterator);
	xrtSetDestroy(pUnion);
	xrtSetUnit(&tRight);
	xrtSetUnit(&tLeft);
	return 0;
}
