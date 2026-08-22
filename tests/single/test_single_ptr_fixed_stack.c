#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供固定容量指针栈薄层。 */
int main(void)
{
	xptrfixedstack Stack;
	ptr pStorage[2];
	int Value = 7;
	ptr pOutput = NULL;

	if ( !xrtPtrFixedStackInit(&Stack, pStorage, 2) ) {
		return 1;
	}
	if ( !xrtPtrFixedStackPush(&Stack, &Value) ) {
		xrtPtrFixedStackUnit(&Stack);
		return 2;
	}
	if ( !xrtPtrFixedStackPop(&Stack, &pOutput) || (pOutput != &Value) ) {
		xrtPtrFixedStackUnit(&Stack);
		return 3;
	}
	xrtPtrFixedStackUnit(&Stack);
	return 0;
}
