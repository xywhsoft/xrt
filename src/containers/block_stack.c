#include "../internal/xrt_block_stack.h"



#if defined(XRT_FEATURE_BLOCK_STACK)

/* 从元素大小推导不超过常规堆对齐的默认元素对齐。 */
static size_t __xrtBlockStackDefaultAlignment(size_t iItemSize)
{
	size_t iAlignment = 1;

	while (
		(iAlignment < XRT_ARRAY_ALIGNMENT_DEFAULT) &&
		((iItemSize % (iAlignment * 2u)) == 0)
	) {
		iAlignment *= 2u;
	}

	return iAlignment;
}



/* 按目标块字节数推导默认块元素数，并限制小元素的块深度。 */
static size_t __xrtBlockStackDefaultItems(size_t iItemSize)
{
	size_t iBlockItems = XRT_BLOCK_STACK_BYTES_DEFAULT / iItemSize;

	if ( iBlockItems == 0 ) {
		return 1;
	}
	if ( iBlockItems > XRT_BLOCK_STACK_ITEMS_MAX ) {
		return XRT_BLOCK_STACK_ITEMS_MAX;
	}

	return iBlockItems;
}



/* 计算单个数据块的完整分配尺寸，失败时不修改错误状态。 */
static bool __xrtBlockStackAllocationSize(
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems,
	size_t* pAllocationSize
)
{
	size_t iBlockBytes;
	size_t iOverhead;

	if (
		(iItemSize == 0) ||
		(iAlignment == 0) ||
		(iBlockItems == 0)
	) {
		return false;
	}
	if ( iBlockItems > (SIZE_MAX / iItemSize) ) {
		return false;
	}
	iBlockBytes = iBlockItems * iItemSize;
	if ( (iAlignment - 1u) > (SIZE_MAX - sizeof(xblockstackblock)) ) {
		return false;
	}
	iOverhead = sizeof(xblockstackblock) + (iAlignment - 1u);
	if ( iBlockBytes > (SIZE_MAX - iOverhead) ) {
		return false;
	}
	if ( pAllocationSize != NULL ) {
		*pAllocationSize = iOverhead + iBlockBytes;
	}

	return true;
}



/* 验证每块布局可以安全分配并保持所有元素对齐。 */
static bool __xrtBlockStackLayoutValid(
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
)
{
	if (
		(iItemSize == 0) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0) ||
		((iItemSize % iAlignment) != 0) ||
		(iBlockItems == 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		!__xrtBlockStackAllocationSize(
			iItemSize,
			iAlignment,
			iBlockItems,
			NULL
		)
	) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	return true;
}



/* 检查分块栈公开摘要和块索引数组是否自洽。 */
static bool __xrtBlockStackValid(const xblockstack* pStack)
{
	size_t iCapacity;

	if ( pStack == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pStack->ItemSize == 0) ||
		(pStack->Alignment == 0) ||
		((pStack->Alignment & (pStack->Alignment - 1u)) != 0) ||
		((pStack->ItemSize % pStack->Alignment) != 0) ||
		(pStack->BlockItems == 0) ||
		(pStack->Blocks.ItemSize != sizeof(xblockstackblock*))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if (
		!__xrtBlockStackAllocationSize(
			pStack->ItemSize,
			pStack->Alignment,
			pStack->BlockItems,
			NULL
		)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !__xrtArrayValid(&pStack->Blocks) ) {
		return false;
	}
	if ( pStack->Blocks.Count > (SIZE_MAX / pStack->BlockItems) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iCapacity = pStack->Blocks.Count * pStack->BlockItems;
	if ( (pStack->Capacity != iCapacity) || (pStack->Count > iCapacity) ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 验证一个块头和其过对齐数据地址仍然匹配分配布局。 */
static bool __xrtBlockStackBlockValid(
	const xblockstack* pStack,
	const xblockstackblock* pBlock
)
{
	uintptr_t iData;

	if ( pBlock == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (uintptr_t)pBlock > (UINTPTR_MAX - sizeof(xblockstackblock)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iData = (uintptr_t)pBlock + sizeof(xblockstackblock);
	if ( iData > (UINTPTR_MAX - (pStack->Alignment - 1u)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iData = (iData + (pStack->Alignment - 1u)) &
		~((uintptr_t)pStack->Alignment - 1u);
	if ( (uintptr_t)pBlock->Data != iData ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 分配一个包含管理头和过对齐元素区的数据块。 */
static xblockstackblock* __xrtBlockStackBlockCreate(const xblockstack* pStack)
{
	xblockstackblock* pBlock;
	size_t iAllocationSize;
	uintptr_t iData;

	if (
		!__xrtBlockStackAllocationSize(
			pStack->ItemSize,
			pStack->Alignment,
			pStack->BlockItems,
			&iAllocationSize
		)
	) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pBlock = (xblockstackblock*)xrtMalloc(iAllocationSize);
	if ( pBlock == NULL ) {
		return NULL;
	}
	if ( (uintptr_t)pBlock > (UINTPTR_MAX - sizeof(xblockstackblock)) ) {
		xrtFree(pBlock);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	iData = (uintptr_t)pBlock + sizeof(xblockstackblock);
	if ( iData > (UINTPTR_MAX - (pStack->Alignment - 1u)) ) {
		xrtFree(pBlock);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	iData = (iData + (pStack->Alignment - 1u)) &
		~((uintptr_t)pStack->Alignment - 1u);
	pBlock->Next = NULL;
	pBlock->Data = (bytes)iData;
	return pBlock;
}



/* 释放尚未提交到块索引的临时块链。 */
static void __xrtBlockStackBlockListFree(xblockstackblock* pBlock)
{
	while ( pBlock != NULL ) {
		xblockstackblock* pNext = pBlock->Next;

		xrtFree(pBlock);
		pBlock = pNext;
	}
}



/* 计算覆盖目标元素容量所需的完整块数量。 */
static bool __xrtBlockStackBlockCount(
	const xblockstack* pStack,
	size_t iCapacity,
	size_t* pBlockCount
)
{
	size_t iBlockCount = iCapacity / pStack->BlockItems;

	if ( (iCapacity % pStack->BlockItems) != 0 ) {
		iBlockCount++;
	}
	if ( iBlockCount > (SIZE_MAX / pStack->BlockItems) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	*pBlockCount = iBlockCount;
	return true;
}



/* 在栈状态已经验证后，原子增加足以覆盖目标容量的数据块。 */
static bool __xrtBlockStackReserveValid(xblockstack* pStack, size_t iCapacity)
{
	xblockstackblock* pHead = NULL;
	xblockstackblock* pTail = NULL;
	xblockstackblock** pSlots;
	size_t iTargetBlocks;
	size_t iAddBlocks;

	if ( iCapacity <= pStack->Capacity ) {
		return true;
	}
	if ( !__xrtBlockStackBlockCount(pStack, iCapacity, &iTargetBlocks) ) {
		return false;
	}
	iAddBlocks = iTargetBlocks - pStack->Blocks.Count;

	/* 先取得所有数据块，任何失败都不会触及原栈。 */
	for ( size_t i = 0; i < iAddBlocks; i++ ) {
		xblockstackblock* pBlock = __xrtBlockStackBlockCreate(pStack);

		if ( pBlock == NULL ) {
			__xrtBlockStackBlockListFree(pHead);
			return false;
		}
		if ( pTail == NULL ) {
			pHead = pBlock;
		} else {
			pTail->Next = pBlock;
		}
		pTail = pBlock;
	}

	/* 块索引预留失败时，临时块仍可完整回滚。 */
	if ( !__xrtArrayReserveValid(&pStack->Blocks, iTargetBlocks) ) {
		__xrtBlockStackBlockListFree(pHead);
		return false;
	}
	pSlots = (xblockstackblock**)__xrtArrayAddValid(
		&pStack->Blocks,
		iAddBlocks
	);
	if ( pSlots == NULL ) {
		__xrtBlockStackBlockListFree(pHead);
		return false;
	}

	/* 预留完成后只做不会失败的块链提交。 */
	for ( size_t i = 0; i < iAddBlocks; i++ ) {
		xblockstackblock* pBlock = pHead;

		pHead = pBlock->Next;
		pBlock->Next = NULL;
		pSlots[i] = pBlock;
	}
	pStack->Capacity = iTargetBlocks * pStack->BlockItems;
	return true;
}



/* 在栈状态和索引已经验证后返回元素地址。 */
static ptr __xrtBlockStackAtValid(const xblockstack* pStack, size_t iIndex)
{
	xblockstackblock* const* pBlocks =
		(xblockstackblock* const*)pStack->Blocks.Data;
	size_t iBlock = iIndex / pStack->BlockItems;
	size_t iOffset = iIndex % pStack->BlockItems;
	xblockstackblock* pBlock = pBlocks[iBlock];

	if ( !__xrtBlockStackBlockValid(pStack, pBlock) ) {
		return NULL;
	}

	return pBlock->Data + (iOffset * pStack->ItemSize);
}



/* 验证栈和索引后返回指定活动元素的只读借用地址。 */
static const void* __xrtBlockStackGet(
	const xblockstack* pStack,
	size_t iIndex
)
{
	if ( !__xrtBlockStackValid(pStack) ) {
		return NULL;
	}
	if ( iIndex >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return __xrtBlockStackAtValid(pStack, iIndex);
}



/* 验证栈和深度后返回指定活动元素的只读借用地址。 */
static const void* __xrtBlockStackPeek(
	const xblockstack* pStack,
	size_t iDepth
)
{
	if ( !__xrtBlockStackValid(pStack) ) {
		return NULL;
	}
	if ( iDepth >= pStack->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return __xrtBlockStackAtValid(
		pStack,
		pStack->Count - iDepth - 1u
	);
}



/* 使用自动块尺寸初始化默认对齐分块栈。 */
XRT_API bool xrtBlockStackInit(xblockstack* pStack, size_t iItemSize)
{
	if ( iItemSize == 0 ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return xrtBlockStackInitLayout(
		pStack,
		iItemSize,
		__xrtBlockStackDefaultAlignment(iItemSize),
		__xrtBlockStackDefaultItems(iItemSize)
	);
}



/* 使用指定元素对齐和每块元素数初始化分块栈。 */
XRT_API bool xrtBlockStackInitLayout(
	xblockstack* pStack,
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
)
{
	if (
		(pStack == NULL) ||
		!__xrtBlockStackLayoutValid(iItemSize, iAlignment, iBlockItems)
	) {
		if ( pStack == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	memset(pStack, 0, sizeof(xblockstack));
	if ( !xrtArrayInit(&pStack->Blocks, sizeof(xblockstackblock*)) ) {
		return false;
	}
	pStack->ItemSize = iItemSize;
	pStack->BlockItems = iBlockItems;
	pStack->Alignment = iAlignment;
	return true;
}



/* 创建使用自动块尺寸的默认对齐分块栈。 */
XRT_API xblockstack* xrtBlockStackCreate(size_t iItemSize)
{
	if ( iItemSize == 0 ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}

	return xrtBlockStackCreateLayout(
		iItemSize,
		__xrtBlockStackDefaultAlignment(iItemSize),
		__xrtBlockStackDefaultItems(iItemSize)
	);
}



/* 创建使用指定元素对齐和每块元素数的分块栈。 */
XRT_API xblockstack* xrtBlockStackCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
)
{
	xblockstack* pStack;

	if ( !__xrtBlockStackLayoutValid(iItemSize, iAlignment, iBlockItems) ) {
		return NULL;
	}
	pStack = (xblockstack*)xrtMalloc(sizeof(xblockstack));
	if ( pStack == NULL ) {
		return NULL;
	}
	if (
		!xrtBlockStackInitLayout(
			pStack,
			iItemSize,
			iAlignment,
			iBlockItems
		)
	) {
		xrtFree(pStack);
		return NULL;
	}

	return pStack;
}



/* 释放全部数据块和块索引，但不释放栈结构。 */
XRT_API void xrtBlockStackUnit(xblockstack* pStack)
{
	xblockstackblock** pBlocks;

	if ( pStack == NULL ) {
		return;
	}
	pBlocks = (xblockstackblock**)pStack->Blocks.Data;
	for ( size_t i = 0; i < pStack->Blocks.Count; i++ ) {
		xrtFree(pBlocks[i]);
	}
	xrtArrayUnit(&pStack->Blocks);
	memset(pStack, 0, sizeof(xblockstack));
}



/* 释放分块栈全部资源和创建的栈结构。 */
XRT_API void xrtBlockStackDestroy(xblockstack* pStack)
{
	if ( pStack == NULL ) {
		return;
	}

	xrtBlockStackUnit(pStack);
	xrtFree(pStack);
}



/* 清空分块栈并保留已经分配的数据块。 */
XRT_API void xrtBlockStackClear(xblockstack* pStack)
{
	if ( !__xrtBlockStackValid(pStack) ) {
		return;
	}

	pStack->Count = 0;
}



/* 保证分块栈至少具有指定元素容量，失败时保持原状态。 */
XRT_API bool xrtBlockStackReserve(xblockstack* pStack, size_t iCapacity)
{
	if ( !__xrtBlockStackValid(pStack) ) {
		return false;
	}

	return __xrtBlockStackReserveValid(pStack, iCapacity);
}



/* 释放当前深度不再需要的数据块，保留轻量块索引缓存。 */
XRT_API bool xrtBlockStackTrim(xblockstack* pStack)
{
	xblockstackblock** pBlocks;
	size_t iNeededBlocks;

	if ( !__xrtBlockStackValid(pStack) ) {
		return false;
	}
	if ( !__xrtBlockStackBlockCount(pStack, pStack->Count, &iNeededBlocks) ) {
		return false;
	}
	pBlocks = (xblockstackblock**)pStack->Blocks.Data;

	/* 先验证所有待释放块，避免损坏状态下释放部分资源。 */
	for ( size_t i = iNeededBlocks; i < pStack->Blocks.Count; i++ ) {
		if ( !__xrtBlockStackBlockValid(pStack, pBlocks[i]) ) {
			return false;
		}
	}
	for ( size_t i = iNeededBlocks; i < pStack->Blocks.Count; i++ ) {
		xrtFree(pBlocks[i]);
		pBlocks[i] = NULL;
	}
	pStack->Blocks.Count = iNeededBlocks;
	pStack->Capacity = iNeededBlocks * pStack->BlockItems;
	return true;
}



/* 返回指定 0 基位置的可写元素借用地址。 */
XRT_API ptr xrtBlockStackGet(xblockstack* pStack, size_t iIndex)
{
	return (ptr)__xrtBlockStackGet(pStack, iIndex);
}



/* 返回指定 0 基位置的只读元素借用地址。 */
XRT_API const void* xrtBlockStackConstGet(const xblockstack* pStack, size_t iIndex)
{
	return __xrtBlockStackGet(pStack, iIndex);
}



/* 取得一个未初始化栈顶槽，既有活动元素地址保持稳定。 */
XRT_API ptr xrtBlockStackAdd(xblockstack* pStack)
{
	ptr pItem;

	if ( !__xrtBlockStackValid(pStack) ) {
		return NULL;
	}
	if ( pStack->Count == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if (
		(pStack->Count == pStack->Capacity) &&
		!__xrtBlockStackReserveValid(pStack, pStack->Count + 1u)
	) {
		return NULL;
	}
	pItem = __xrtBlockStackAtValid(pStack, pStack->Count);
	if ( pItem == NULL ) {
		return NULL;
	}
	pStack->Count++;
	return pItem;
}



/* 浅复制一个完整元素压入分块栈。 */
XRT_API bool xrtBlockStackPush(xblockstack* pStack, const void* pItem)
{
	ptr pTarget;

	if ( !__xrtBlockStackValid(pStack) ) {
		return false;
	}
	if ( pItem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pStack->Count == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if (
		(pStack->Count == pStack->Capacity) &&
		!__xrtBlockStackReserveValid(pStack, pStack->Count + 1u)
	) {
		return false;
	}
	pTarget = __xrtBlockStackAtValid(pStack, pStack->Count);
	if ( pTarget == NULL ) {
		return false;
	}
	memmove(pTarget, pItem, pStack->ItemSize);
	pStack->Count++;
	return true;
}



/* 弹出栈顶元素，并可把内容复制到外部输出缓冲。 */
XRT_API bool xrtBlockStackPop(xblockstack* pStack, ptr pItem)
{
	ptr pTop;

	if ( !__xrtBlockStackValid(pStack) ) {
		return false;
	}
	if ( pStack->Count == 0 ) {
		__xrtErrorSetRange();
		return false;
	}
	pTop = __xrtBlockStackAtValid(pStack, pStack->Count - 1u);
	if ( pTop == NULL ) {
		return false;
	}
	if ( pItem != NULL ) {
		memmove(pItem, pTop, pStack->ItemSize);
	}
	pStack->Count--;
	return true;
}



/* 返回距栈顶指定深度的可写元素，深度 0 表示栈顶。 */
XRT_API ptr xrtBlockStackPeek(xblockstack* pStack, size_t iDepth)
{
	return (ptr)__xrtBlockStackPeek(pStack, iDepth);
}



/* 返回距栈顶指定深度的只读元素，深度 0 表示栈顶。 */
XRT_API const void* xrtBlockStackConstPeek(const xblockstack* pStack, size_t iDepth)
{
	return __xrtBlockStackPeek(pStack, iDepth);
}



/* 返回可写栈顶元素借用地址。 */
XRT_API ptr xrtBlockStackTop(xblockstack* pStack)
{
	return xrtBlockStackPeek(pStack, 0);
}



/* 返回只读栈顶元素借用地址。 */
XRT_API const void* xrtBlockStackConstTop(const xblockstack* pStack)
{
	return xrtBlockStackConstPeek(pStack, 0);
}

#endif
