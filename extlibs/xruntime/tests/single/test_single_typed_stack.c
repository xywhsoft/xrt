#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"



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
	if ( !xrtTypedStackInit(&Stack, xrtTypeInt64()) ||
		 !xrtTypedStackPush(&Stack, &iInput) ||
		 !xrtTypedStackPushBatch(&Stack, &Stack) ) return 3;
	xtypedarray* pItems = xrtTypedStackPeekBatch(&Stack, 0u, SIZE_MAX);
	if ( !pItems || xrtTypedArrayCount(pItems) != 2u ) return 4;
	xrtTypedArrayDestroy(pItems);
	pItems = xrtTypedStackPopBatch(&Stack, SIZE_MAX);
	if ( !pItems || xrtTypedArrayCount(pItems) != 2u ||
		 xrtTypedStackCount(&Stack) != 0u ||
		 *(const int64*)xrtTypedArrayConstGet(pItems, 0u) != iInput ) return 5;
	xrtTypedArrayDestroy(pItems);
	xrtTypedStackUnit(&Stack);
	return iResult;
}
