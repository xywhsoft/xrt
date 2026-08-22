#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供指针友好动态栈。 */
int main(void)
{
	xptrstack tStack;
	int iValue = 11;
	ptr pOutput = NULL;

	if ( !xrtPtrStackInit(&tStack) ) {
		return 1;
	}
	if ( !xrtPtrStackPush(&tStack, &iValue) ) {
		xrtPtrStackUnit(&tStack);
		return 2;
	}
	if ( !xrtPtrStackPop(&tStack, &pOutput) || (pOutput != &iValue) ) {
		xrtPtrStackUnit(&tStack);
		return 3;
	}
	xrtPtrStackUnit(&tStack);
	return 0;
}
