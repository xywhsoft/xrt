#include <xrt/typed_stack.h>



#if defined(XRUNTIME_FEATURE_TYPED_STACK)

/* 初始化一个拥有类型值的空栈。 */
XRT_API bool xrtTypedStackInit(
	xtypedstack* pStack,
	const xrttype* pItemType
)
{
	return xrtTypedArrayInit(pStack, pItemType);
}



/* 在堆上创建一个拥有类型值的空栈。 */
XRT_API xtypedstack* xrtTypedStackCreate(const xrttype* pItemType)
{
	return (xtypedstack*)xrtTypedArrayCreate(pItemType);
}



/* 释放全部元素和连续存储，但不释放栈结构。 */
XRT_API void xrtTypedStackUnit(xtypedstack* pStack)
{
	xrtTypedArrayUnit(pStack);
}



/* 释放全部元素、连续存储和堆栈结构。 */
XRT_API void xrtTypedStackDestroy(xtypedstack* pStack)
{
	xrtTypedArrayDestroy(pStack);
}



/* 返回栈借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedStackItemType(const xtypedstack* pStack)
{
	return xrtTypedArrayItemType(pStack);
}



/* 返回当前栈深度。 */
XRT_API size_t xrtTypedStackCount(const xtypedstack* pStack)
{
	return xrtTypedArrayCount(pStack);
}



/* 返回当前连续存储容量。 */
XRT_API size_t xrtTypedStackCapacity(const xtypedstack* pStack)
{
	return xrtTypedArrayCapacity(pStack);
}



/* 销毁全部元素并保留容量。 */
XRT_API void xrtTypedStackClear(xtypedstack* pStack)
{
	xrtTypedArrayClear(pStack);
}



/* 保证栈至少具有指定容量。 */
XRT_API bool xrtTypedStackReserve(xtypedstack* pStack, size_t iCapacity)
{
	return xrtTypedArrayReserve(pStack, iCapacity);
}



/* 把容量裁剪到当前深度。 */
XRT_API bool xrtTypedStackTrim(xtypedstack* pStack)
{
	return xrtTypedArrayTrim(pStack);
}



/* 失败原子地复制压入一个类型值。 */
XRT_API bool xrtTypedStackPush(
	xtypedstack* pStack,
	const void* pItem
)
{
	return xrtTypedArrayPush(pStack, pItem);
}



/* 移动或销毁栈顶值，并从栈中删除它。 */
XRT_API bool xrtTypedStackPop(xtypedstack* pStack, ptr pValue)
{
	size_t iCount = xrtTypedArrayCount(pStack);

	if ( iCount == 0u ) {
		return false;
	}
	if ( pValue != NULL ) {
		return xrtTypedArrayPop(pStack, pValue);
	}
	return xrtTypedArrayRemove(pStack, iCount - 1u, 1u);
}



/* 按距栈顶深度返回可写借用值。 */
XRT_API ptr xrtTypedStackPeek(xtypedstack* pStack, size_t iDepth)
{
	size_t iCount = xrtTypedArrayCount(pStack);

	return iDepth < iCount ?
		xrtTypedArrayGet(pStack, iCount - iDepth - 1u) : NULL;
}



/* 按距栈顶深度返回只读借用值。 */
XRT_API const void* xrtTypedStackConstPeek(
	const xtypedstack* pStack,
	size_t iDepth
)
{
	size_t iCount = xrtTypedArrayCount(pStack);

	return iDepth < iCount ?
		xrtTypedArrayConstGet(pStack, iCount - iDepth - 1u) : NULL;
}



/* 返回可写栈顶借用值。 */
XRT_API ptr xrtTypedStackTop(xtypedstack* pStack)
{
	return xrtTypedStackPeek(pStack, 0u);
}



/* 返回只读栈顶借用值。 */
XRT_API const void* xrtTypedStackConstTop(const xtypedstack* pStack)
{
	return xrtTypedStackConstPeek(pStack, 0u);
}



/* 深复制一个独立类型栈。 */
XRT_API xtypedstack* xrtTypedStackClone(const xtypedstack* pStack)
{
	return (xtypedstack*)xrtTypedArrayClone(pStack);
}



/* 比较两个栈的精确元素类型、深度和顺序。 */
XRT_API bool xrtTypedStackEquals(
	const xtypedstack* pLeft,
	const xtypedstack* pRight
)
{
	return xrtTypedArrayEquals(pLeft, pRight);
}

#endif
