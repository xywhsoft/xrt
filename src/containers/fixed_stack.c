#include "../internal/xrt_stack.h"



#if defined(XRT_FEATURE_FIXED_STACK)

/* 检查固定栈公开状态是否自洽。 */
static bool __xrtFixedStackValid(const xfixedstack* pStack)
{
	size_t iBytes;

	if ( pStack == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pStack->Data == NULL) ||
		(pStack->ItemSize == 0) ||
		(pStack->Capacity == 0) ||
		(pStack->Count > pStack->Capacity) ||
		(pStack->Capacity > (SIZE_MAX / pStack->ItemSize)) ||
		((pStack->Allocation != NULL) && (pStack->Allocation != pStack->Data))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iBytes = pStack->Capacity * pStack->ItemSize;
	if ( (uintptr_t)pStack->Data > (UINTPTR_MAX - iBytes) ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 判断一个元素范围是否触及固定栈缓冲。 */
static bool __xrtFixedStackRange(
	const xfixedstack* pStack,
	const void* pMemory,
	bool* pOverlaps
)
{
	uintptr_t iData = (uintptr_t)pStack->Data;
	uintptr_t iMemory = (uintptr_t)pMemory;
	size_t iBytes;

	*pOverlaps = false;
	if ( pMemory == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBytes = pStack->Capacity * pStack->ItemSize;
	if ( iMemory > (UINTPTR_MAX - pStack->ItemSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pOverlaps =
		(iMemory < (iData + iBytes)) &&
		((iMemory + pStack->ItemSize) > iData);
	return true;
}



/* 验证压栈来源位于外部或当前完整活动元素内。 */
static bool __xrtFixedStackSource(const xfixedstack* pStack, const void* pItem)
{
	bool bOverlaps;
	uintptr_t iData;
	uintptr_t iItem;
	size_t iLiveBytes;

	if ( !__xrtFixedStackRange(pStack, pItem, &bOverlaps) ) {
		return false;
	}
	if ( !bOverlaps ) {
		return true;
	}

	iData = (uintptr_t)pStack->Data;
	iItem = (uintptr_t)pItem;
	iLiveBytes = pStack->Count * pStack->ItemSize;
	if (
		(iItem < iData) ||
		((iItem + pStack->ItemSize) > (iData + iLiveBytes)) ||
		(((size_t)(iItem - iData) % pStack->ItemSize) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 在调用方缓冲上初始化固定容量栈。 */
XRT_API bool xrtFixedStackInit(
	xfixedstack* pStack,
	ptr pMemory,
	size_t iMemorySize,
	size_t iItemSize
)
{
	uintptr_t iMemory;
	uintptr_t iStack;
	size_t iCapacity;
	size_t iBytes;

	if (
		(pStack == NULL) ||
		(pMemory == NULL) ||
		(iItemSize == 0) ||
		(iMemorySize < iItemSize)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iCapacity = iMemorySize / iItemSize;
	iBytes = iCapacity * iItemSize;
	iMemory = (uintptr_t)pMemory;
	iStack = (uintptr_t)pStack;
	if (
		(iMemory > (UINTPTR_MAX - iBytes)) ||
		(iStack > (UINTPTR_MAX - sizeof(xfixedstack)))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	/* 元数据必须位于数据区之外，避免首次写入破坏栈结构。 */
	if (
		(iMemory < (iStack + sizeof(xfixedstack))) &&
		(iStack < (iMemory + iBytes))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	memset(pStack, 0, sizeof(xfixedstack));
	pStack->Data = (bytes)pMemory;
	pStack->ItemSize = iItemSize;
	pStack->Capacity = iCapacity;
	return true;
}



/* 创建拥有固定容量缓冲的栈。 */
XRT_API xfixedstack* xrtFixedStackCreate(size_t iCapacity, size_t iItemSize)
{
	xfixedstack* pStack;
	ptr pMemory;
	size_t iBytes;

	if ( (iCapacity == 0) || (iItemSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iCapacity > (SIZE_MAX / iItemSize) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBytes = iCapacity * iItemSize;
	pStack = (xfixedstack*)xrtMalloc(sizeof(xfixedstack));
	if ( pStack == NULL ) {
		return NULL;
	}
	pMemory = xrtMalloc(iBytes);
	if ( pMemory == NULL ) {
		xrtFree(pStack);
		return NULL;
	}
	if ( !xrtFixedStackInit(pStack, pMemory, iBytes, iItemSize) ) {
		xrtFree(pMemory);
		xrtFree(pStack);
		return NULL;
	}
	pStack->Allocation = pMemory;
	return pStack;
}



/* 释放创建时取得的固定缓冲，但不释放栈结构。 */
XRT_API void xrtFixedStackUnit(xfixedstack* pStack)
{
	if ( pStack == NULL ) {
		return;
	}

	xrtFree(pStack->Allocation);
	memset(pStack, 0, sizeof(xfixedstack));
}



/* 释放固定缓冲和创建的栈结构。 */
XRT_API void xrtFixedStackDestroy(xfixedstack* pStack)
{
	if ( pStack == NULL ) {
		return;
	}

	xrtFixedStackUnit(pStack);
	xrtFree(pStack);
}



/* 清空栈内容并保留固定容量。 */
XRT_API void xrtFixedStackClear(xfixedstack* pStack)
{
	if ( !__xrtFixedStackValid(pStack) ) {
		return;
	}

	pStack->Count = 0;
}



/* 返回剩余可压入元素数量。 */
XRT_API size_t xrtFixedStackSpace(const xfixedstack* pStack)
{
	return __xrtFixedStackValid(pStack) ? pStack->Capacity - pStack->Count : 0;
}



/* 返回指定 0 基位置的可写元素借用地址。 */
XRT_API ptr xrtFixedStackGet(xfixedstack* pStack, size_t iIndex)
{
	if ( !__xrtFixedStackValid(pStack) ) {
		return NULL;
	}
	if ( iIndex >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return pStack->Data + (iIndex * pStack->ItemSize);
}



/* 返回指定 0 基位置的只读元素借用地址。 */
XRT_API const void* xrtFixedStackConstGet(const xfixedstack* pStack, size_t iIndex)
{
	if ( !__xrtFixedStackValid(pStack) ) {
		return NULL;
	}
	if ( iIndex >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return pStack->Data + (iIndex * pStack->ItemSize);
}



/* 取得一个未初始化栈顶槽，栈满时失败。 */
XRT_API ptr xrtFixedStackAdd(xfixedstack* pStack)
{
	ptr pItem;

	if ( !__xrtFixedStackValid(pStack) ) {
		return NULL;
	}
	if ( pStack->Count == pStack->Capacity ) {
		__xrtErrorSetAgain();
		return NULL;
	}
	pItem = pStack->Data + (pStack->Count * pStack->ItemSize);
	pStack->Count++;
	return pItem;
}



/* 复制一个元素压入固定栈。 */
XRT_API bool xrtFixedStackPush(xfixedstack* pStack, const void* pItem)
{
	ptr pTarget;

	if ( !__xrtFixedStackValid(pStack) || !__xrtFixedStackSource(pStack, pItem) ) {
		return false;
	}
	if ( pStack->Count == pStack->Capacity ) {
		__xrtErrorSetAgain();
		return false;
	}
	pTarget = pStack->Data + (pStack->Count * pStack->ItemSize);
	memmove(pTarget, pItem, pStack->ItemSize);
	pStack->Count++;
	return true;
}



/* 弹出栈顶元素，并可把内容复制到外部输出缓冲。 */
XRT_API bool xrtFixedStackPop(xfixedstack* pStack, ptr pItem)
{
	bool bOverlaps;

	if ( !__xrtFixedStackValid(pStack) ) {
		return false;
	}
	if ( pStack->Count == 0 ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( pItem != NULL ) {
		if ( !__xrtFixedStackRange(pStack, pItem, &bOverlaps) ) {
			return false;
		}
		if ( bOverlaps ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memmove(
			pItem,
			pStack->Data + ((pStack->Count - 1u) * pStack->ItemSize),
			pStack->ItemSize
		);
	}
	pStack->Count--;
	return true;
}



/* 返回距栈顶指定深度的可写元素，深度 0 表示栈顶。 */
XRT_API ptr xrtFixedStackPeek(xfixedstack* pStack, size_t iDepth)
{
	if ( !__xrtFixedStackValid(pStack) ) {
		return NULL;
	}
	if ( iDepth >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return pStack->Data + ((pStack->Count - iDepth - 1u) * pStack->ItemSize);
}



/* 返回距栈顶指定深度的只读元素，深度 0 表示栈顶。 */
XRT_API const void* xrtFixedStackConstPeek(const xfixedstack* pStack, size_t iDepth)
{
	if ( !__xrtFixedStackValid(pStack) ) {
		return NULL;
	}
	if ( iDepth >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return pStack->Data + ((pStack->Count - iDepth - 1u) * pStack->ItemSize);
}



/* 返回可写栈顶元素借用地址。 */
XRT_API ptr xrtFixedStackTop(xfixedstack* pStack)
{
	return xrtFixedStackPeek(pStack, 0);
}



/* 返回只读栈顶元素借用地址。 */
XRT_API const void* xrtFixedStackConstTop(const xfixedstack* pStack)
{
	return xrtFixedStackConstPeek(pStack, 0);
}

#endif
