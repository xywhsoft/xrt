#include "../internal/xrt_stack.h"



#if defined(XRT_FEATURE_STACK)

/* 在调用方已经选择读写语义后统一验证并定位栈顶深度。 */
static const void* __xrtStackPeek(const xstack* pStack, size_t iDepth)
{
	if ( !__xrtArrayValid(pStack) ) {
		return NULL;
	}
	if ( iDepth >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return pStack->Data + ((pStack->Count - iDepth - 1u) * pStack->ItemSize);
}



/* 初始化使用默认对齐的空动态栈。 */
XRT_API bool xrtStackInit(xstack* pStack, size_t iItemSize)
{
	return xrtArrayInit(pStack, iItemSize);
}



/* 初始化显式过对齐动态栈。 */
XRT_API bool xrtStackInitAligned(xstack* pStack, size_t iItemSize, size_t iAlignment)
{
	return xrtArrayInitAligned(pStack, iItemSize, iAlignment);
}



/* 创建使用默认对齐的空动态栈。 */
XRT_API xstack* xrtStackCreate(size_t iItemSize)
{
	return xrtArrayCreate(iItemSize);
}



/* 创建显式过对齐动态栈。 */
XRT_API xstack* xrtStackCreateAligned(size_t iItemSize, size_t iAlignment)
{
	return xrtArrayCreateAligned(iItemSize, iAlignment);
}



/* 释放动态栈元素内存，但不释放栈结构。 */
XRT_API void xrtStackUnit(xstack* pStack)
{
	xrtArrayUnit(pStack);
}



/* 释放动态栈全部资源和栈结构。 */
XRT_API void xrtStackDestroy(xstack* pStack)
{
	xrtArrayDestroy(pStack);
}



/* 清空动态栈并保留容量。 */
XRT_API void xrtStackClear(xstack* pStack)
{
	xrtArrayClear(pStack);
}



/* 保证动态栈至少具有指定元素容量。 */
XRT_API bool xrtStackReserve(xstack* pStack, size_t iCapacity)
{
	return xrtArrayReserve(pStack, iCapacity);
}



/* 将动态栈容量裁剪到当前深度。 */
XRT_API bool xrtStackTrim(xstack* pStack)
{
	return xrtArrayTrim(pStack);
}



/* 返回指定 0 基位置的可写元素借用地址。 */
XRT_API ptr xrtStackGet(xstack* pStack, size_t iIndex)
{
	return xrtArrayGet(pStack, iIndex);
}



/* 返回指定 0 基位置的只读元素借用地址。 */
XRT_API const void* xrtStackConstGet(const xstack* pStack, size_t iIndex)
{
	return xrtArrayConstGet(pStack, iIndex);
}



/* 取得一个未初始化栈顶槽。 */
XRT_API ptr xrtStackAdd(xstack* pStack)
{
	return xrtArrayAdd(pStack, 1);
}



/* 复制一个元素压入动态栈。 */
XRT_API bool xrtStackPush(xstack* pStack, const void* pItem)
{
	return xrtArrayPush(pStack, pItem);
}



/* 弹出栈顶元素，并可把内容复制到外部输出缓冲。 */
XRT_API bool xrtStackPop(xstack* pStack, ptr pItem)
{
	return xrtArrayPop(pStack, pItem);
}



/* 返回距栈顶指定深度的可写元素，深度 0 表示栈顶。 */
XRT_API ptr xrtStackPeek(xstack* pStack, size_t iDepth)
{
	return (ptr)__xrtStackPeek(pStack, iDepth);
}



/* 返回距栈顶指定深度的只读元素，深度 0 表示栈顶。 */
XRT_API const void* xrtStackConstPeek(const xstack* pStack, size_t iDepth)
{
	return __xrtStackPeek(pStack, iDepth);
}



/* 返回可写栈顶元素借用地址。 */
XRT_API ptr xrtStackTop(xstack* pStack)
{
	return xrtStackPeek(pStack, 0);
}



/* 返回只读栈顶元素借用地址。 */
XRT_API const void* xrtStackConstTop(const xstack* pStack)
{
	return xrtStackConstPeek(pStack, 0);
}

#endif
