#include "../internal/xrt_stack.h"



#if defined(XRT_FEATURE_PTR_STACK)

/* 检查指针栈完整状态和固定指针元素宽度。 */
static bool __xrtPtrStackValid(const xptrstack* pStack)
{
	if ( !__xrtArrayValid(pStack) ) {
		return false;
	}
	if ( pStack->ItemSize != sizeof(ptr) ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 初始化空指针栈。 */
XRT_API bool xrtPtrStackInit(xptrstack* pStack)
{
	return xrtStackInit(pStack, sizeof(ptr));
}



/* 创建空指针栈。 */
XRT_API xptrstack* xrtPtrStackCreate(void)
{
	return xrtStackCreate(sizeof(ptr));
}



/* 释放指针存储区但不释放指针目标。 */
XRT_API void xrtPtrStackUnit(xptrstack* pStack)
{
	xrtStackUnit(pStack);
}



/* 释放指针栈结构但不释放指针目标。 */
XRT_API void xrtPtrStackDestroy(xptrstack* pStack)
{
	xrtStackDestroy(pStack);
}



/* 清空指针栈但不释放指针目标。 */
XRT_API void xrtPtrStackClear(xptrstack* pStack)
{
	if ( !__xrtPtrStackValid(pStack) ) {
		return;
	}

	pStack->Count = 0;
}



/* 保证指针栈至少具有指定容量。 */
XRT_API bool xrtPtrStackReserve(xptrstack* pStack, size_t iCapacity)
{
	if ( !__xrtPtrStackValid(pStack) ) {
		return false;
	}

	return __xrtArrayReserveValid(pStack, iCapacity);
}



/* 将指针栈容量裁剪到当前深度。 */
XRT_API bool xrtPtrStackTrim(xptrstack* pStack)
{
	if ( !__xrtPtrStackValid(pStack) ) {
		return false;
	}

	return xrtStackTrim(pStack);
}



/* 返回指定 0 基位置的指针值。 */
XRT_API ptr xrtPtrStackGet(const xptrstack* pStack, size_t iIndex)
{
	if ( !__xrtPtrStackValid(pStack) ) {
		return NULL;
	}
	if ( iIndex >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return ((ptr const*)pStack->Data)[iIndex];
}



/* 压入一个可为空的指针值。 */
XRT_API bool xrtPtrStackPush(xptrstack* pStack, ptr pValue)
{
	ptr* pTarget;

	if ( !__xrtPtrStackValid(pStack) ) {
		return false;
	}
	pTarget = (ptr*)__xrtArrayAddValid(pStack, 1);
	if ( pTarget == NULL ) {
		return false;
	}

	*pTarget = pValue;
	return true;
}



/* 弹出指针值；输出为空表示只删除栈顶。 */
XRT_API bool xrtPtrStackPop(xptrstack* pStack, ptr* pValue)
{
	if ( !__xrtPtrStackValid(pStack) ) {
		return false;
	}

	return xrtStackPop(pStack, pValue);
}



/* 返回距栈顶指定深度的指针值，深度 0 表示栈顶。 */
XRT_API ptr xrtPtrStackPeek(const xptrstack* pStack, size_t iDepth)
{
	if ( !__xrtPtrStackValid(pStack) ) {
		return NULL;
	}
	if ( iDepth >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return ((ptr const*)pStack->Data)[pStack->Count - iDepth - 1u];
}



/* 返回栈顶指针值；合法空值与错误通过错误状态区分。 */
XRT_API ptr xrtPtrStackTop(const xptrstack* pStack)
{
	return xrtPtrStackPeek(pStack, 0);
}

#endif
