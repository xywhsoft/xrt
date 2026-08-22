#include "../internal/xrt_stack.h"



#if defined(XRT_FEATURE_PTR_FIXED_STACK)

/* 只检查固定指针栈类型摘要，完整状态由 FixedStack 入口验证。 */
static bool __xrtPtrFixedStackTypeValid(const xptrfixedstack* pStack)
{
	if ( pStack == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pStack->ItemSize != sizeof(ptr) ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 在调用方指针数组上初始化固定容量指针栈。 */
XRT_API bool xrtPtrFixedStackInit(
	xptrfixedstack* pStack,
	ptr* pMemory,
	size_t iCapacity
)
{
	if ( (pStack == NULL) || (pMemory == NULL) || (iCapacity == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(ptr)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	return xrtFixedStackInit(
		pStack,
		pMemory,
		iCapacity * sizeof(ptr),
		sizeof(ptr)
	);
}



/* 创建拥有指定固定容量的指针栈。 */
XRT_API xptrfixedstack* xrtPtrFixedStackCreate(size_t iCapacity)
{
	return xrtFixedStackCreate(iCapacity, sizeof(ptr));
}



/* 释放拥有的指针存储区，但不释放任何指针目标或栈结构。 */
XRT_API void xrtPtrFixedStackUnit(xptrfixedstack* pStack)
{
	xrtFixedStackUnit(pStack);
}



/* 释放创建的指针栈结构，但不释放任何指针目标。 */
XRT_API void xrtPtrFixedStackDestroy(xptrfixedstack* pStack)
{
	xrtFixedStackDestroy(pStack);
}



/* 清空固定指针栈，但不释放任何指针目标。 */
XRT_API void xrtPtrFixedStackClear(xptrfixedstack* pStack)
{
	if ( !__xrtPtrFixedStackTypeValid(pStack) ) {
		return;
	}

	xrtFixedStackClear(pStack);
}



/* 返回固定指针栈剩余容量。 */
XRT_API size_t xrtPtrFixedStackSpace(const xptrfixedstack* pStack)
{
	if ( !__xrtPtrFixedStackTypeValid(pStack) ) {
		return 0;
	}

	return xrtFixedStackSpace(pStack);
}



/* 返回指定 0 基位置的指针值。 */
XRT_API ptr xrtPtrFixedStackGet(const xptrfixedstack* pStack, size_t iIndex)
{
	ptr const* pValue;

	if ( !__xrtPtrFixedStackTypeValid(pStack) ) {
		return NULL;
	}
	pValue = (ptr const*)xrtFixedStackConstGet(pStack, iIndex);

	return pValue != NULL ? *pValue : NULL;
}



/* 压入一个可为空的指针值。 */
XRT_API bool xrtPtrFixedStackPush(xptrfixedstack* pStack, ptr pValue)
{
	if ( !__xrtPtrFixedStackTypeValid(pStack) ) {
		return false;
	}

	return xrtFixedStackPush(pStack, &pValue);
}



/* 弹出指针值；输出为空表示只删除栈顶。 */
XRT_API bool xrtPtrFixedStackPop(xptrfixedstack* pStack, ptr* pValue)
{
	if ( !__xrtPtrFixedStackTypeValid(pStack) ) {
		return false;
	}

	return xrtFixedStackPop(pStack, pValue);
}



/* 返回距栈顶指定深度的指针值，深度 0 表示栈顶。 */
XRT_API ptr xrtPtrFixedStackPeek(const xptrfixedstack* pStack, size_t iDepth)
{
	ptr const* pValue;

	if ( !__xrtPtrFixedStackTypeValid(pStack) ) {
		return NULL;
	}
	pValue = (ptr const*)xrtFixedStackConstPeek(pStack, iDepth);

	return pValue != NULL ? *pValue : NULL;
}



/* 返回栈顶指针值；合法空值与错误通过错误状态区分。 */
XRT_API ptr xrtPtrFixedStackTop(const xptrfixedstack* pStack)
{
	return xrtPtrFixedStackPeek(pStack, 0);
}

#endif
