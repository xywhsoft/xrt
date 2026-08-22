#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的类型栈公开入口。 */
int main(void)
{
	xtypedstack Stack;
	int64 iInput = 17;
	int64 iOutput = 0;
	int iResult = 0;

	if ( !xrtTypedStackInit(&Stack, xrtTypeInt64()) ) {
		return 1;
	}
	if ( !xrtTypedStackPush(&Stack, &iInput) ||
		 (*(const int64*)xrtTypedStackConstTop(&Stack) != iInput) ||
		 !xrtTypedStackPop(&Stack, &iOutput) || (iOutput != iInput) ) {
		iResult = 2;
	}
	xrtTypedStackUnit(&Stack);
	return iResult;
}
