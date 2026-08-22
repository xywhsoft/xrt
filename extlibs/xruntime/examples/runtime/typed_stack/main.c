#include <xruntime.h>

#include <stdio.h>



/* 展示类型栈的复制压入、栈顶借用和移动弹出。 */
int main(void)
{
	xtypedstack* pStack = xrtTypedStackCreate(xrtTypeInt64());
	int64 iFirst = 11;
	int64 iSecond = 22;
	int64 iOutput = 0;

	if ( (pStack == NULL) ||
		 !xrtTypedStackPush(pStack, &iFirst) ||
		 !xrtTypedStackPush(pStack, &iSecond) ||
		 !xrtTypedStackPop(pStack, &iOutput) ) {
		xrtTypedStackDestroy(pStack);
		return 1;
	}
	printf(
		"top=%lld count=%zu\n",
		(long long)iOutput,
		xrtTypedStackCount(pStack)
	);
	xrtTypedStackDestroy(pStack);
	return 0;
}
