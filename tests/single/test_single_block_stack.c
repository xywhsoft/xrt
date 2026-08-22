#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须保留跨块增长和稳定活动元素地址。 */
int main(void)
{
	xblockstack tStack;
	int* pFirst;
	int iOutput;

	if ( !xrtBlockStackInitLayout(&tStack, sizeof(int), sizeof(int), 2) ) {
		return 1;
	}
	pFirst = (int*)xrtBlockStackAdd(&tStack);
	if ( pFirst == NULL ) {
		xrtBlockStackUnit(&tStack);
		return 2;
	}
	*pFirst = 10;
	for ( int i = 1; i < 8; i++ ) {
		if ( !xrtBlockStackPush(&tStack, &i) ) {
			xrtBlockStackUnit(&tStack);
			return 3;
		}
	}
	if ( (xrtBlockStackGet(&tStack, 0) != pFirst) || (*pFirst != 10) ) {
		xrtBlockStackUnit(&tStack);
		return 4;
	}
	if ( !xrtBlockStackPop(&tStack, &iOutput) || (iOutput != 7) ) {
		xrtBlockStackUnit(&tStack);
		return 5;
	}
	xrtBlockStackUnit(&tStack);
	return 0;
}
