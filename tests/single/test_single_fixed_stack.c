#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供外部缓冲固定栈。 */
int main(void)
{
	xfixedstack tStack;
	int pStorage[2];
	int iValue = 7;
	int iOutput = 0;

	if ( !xrtFixedStackInit(&tStack, pStorage, sizeof(pStorage), sizeof(int)) ) {
		return 1;
	}
	if ( !xrtFixedStackPush(&tStack, &iValue) ) {
		return 2;
	}
	if ( !xrtFixedStackPop(&tStack, &iOutput) || (iOutput != 7) ) {
		return 3;
	}
	xrtFixedStackUnit(&tStack);
	return 0;
}
