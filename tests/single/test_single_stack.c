#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供可增长动态栈。 */
int main(void)
{
	xstack tStack;
	int iValue = 9;
	int iOutput = 0;

	if ( !xrtStackInit(&tStack, sizeof(int)) ) {
		return 1;
	}
	if ( !xrtStackPush(&tStack, &iValue) ) {
		xrtStackUnit(&tStack);
		return 2;
	}
	if ( !xrtStackPop(&tStack, &iOutput) || (iOutput != 9) ) {
		xrtStackUnit(&tStack);
		return 3;
	}
	xrtStackUnit(&tStack);
	return 0;
}
