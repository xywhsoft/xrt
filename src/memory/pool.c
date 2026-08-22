#include "../internal/xrt_pool.h"



#if defined(XRT_FEATURE_POOL)

/* 检查固定池是否允许改变分配集合。 */
static bool __xrtPoolCanMutate(xpool* pPool, cstr sOperation)
{
	if ( (pPool == NULL) || ((pPool->Flags & XRT_POOL_FLAG_READY) == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pPool->ItemSize == 0) ||
		(pPool->Alignment == 0) ||
		((pPool->Alignment & (pPool->Alignment - 1u)) != 0) ||
		(pPool->PageCapacity == 0) ||
		(pPool->PageCapacity > XRT_POOL_PAGE_CAPACITY)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pPool->Flags & XRT_POOL_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			sOperation,
			"memory pool allocation set cannot change during a visit."
		);
		return false;
	}
	return true;
}



/* 验证固定池布局并推导自动页槽数。 */
static bool __xrtPoolLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity,
	size_t* pPageCapacity
)
{
	size_t iStride;

	if ( iItemSize == 0 ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_SIZE,
			"pool.init",
			"item size must be non-zero."
		);
		return false;
	}
	if ( (iAlignment == 0) || ((iAlignment & (iAlignment - 1)) != 0) ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_ALIGNMENT,
			"pool.init",
			"alignment must be a power of two."
		);
		return false;
	}
	if ( iItemSize > (SIZE_MAX - (iAlignment - 1)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iStride = (iItemSize + (iAlignment - 1)) & ~(iAlignment - 1);
	if ( iPageCapacity == 0 ) {
		iPageCapacity = XRT_POOL_PAGE_BYTES_DEFAULT / iStride;
		if ( iPageCapacity == 0 ) {
			iPageCapacity = 1;
		} else if ( iPageCapacity > XRT_POOL_PAGE_CAPACITY ) {
			iPageCapacity = XRT_POOL_PAGE_CAPACITY;
		}
	}
	if (
		(iPageCapacity == 0) ||
		(iPageCapacity > XRT_POOL_PAGE_CAPACITY)
	) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_CAPACITY,
			"pool.init",
			"page capacity must be between one and 256."
		);
		return false;
	}
	if ( iStride > (SIZE_MAX / iPageCapacity) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pPageCapacity = iPageCapacity;
	return true;
}



/* 将页插入可分配页链表头。 */
static void __xrtPoolAvailableAdd(xpool* pPool, xpoolpage* pPage)
{
	pPage->AvailablePrev = NULL;
	pPage->AvailableNext = pPool->Available;
	if ( pPool->Available != NULL ) {
		pPool->Available->AvailablePrev = pPage;
	}
	pPool->Available = pPage;
}



/* 将页从可分配页链表移除。 */
static void __xrtPoolAvailableRemove(xpool* pPool, xpoolpage* pPage)
{
	if ( pPage->AvailablePrev != NULL ) {
		pPage->AvailablePrev->AvailableNext = pPage->AvailableNext;
	} else if ( pPool->Available == pPage ) {
		pPool->Available = pPage->AvailableNext;
	} else {
		return;
	}
	if ( pPage->AvailableNext != NULL ) {
		pPage->AvailableNext->AvailablePrev = pPage->AvailablePrev;
	}
	pPage->AvailablePrev = NULL;
	pPage->AvailableNext = NULL;
}



/* 确保页地址索引能够再容纳一个元素。 */
static bool __xrtPoolIndexReserve(xpool* pPool, size_t iNeed)
{
	size_t iCapacity;
	xpoolpage** pIndex;

	if ( iNeed <= pPool->IndexCapacity ) {
		return true;
	}
	iCapacity = pPool->IndexCapacity != 0 ? pPool->IndexCapacity : 8;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > (SIZE_MAX / 2) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCapacity *= 2;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(xpoolpage*)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pIndex = (xpoolpage**)xrtRealloc(pPool->Index, iCapacity * sizeof(xpoolpage*));
	if ( pIndex == NULL ) {
		return false;
	}
	pPool->Index = pIndex;
	pPool->IndexCapacity = iCapacity;
	return true;
}



/* 按用户区起始地址将页插入有序索引。 */
static void __xrtPoolIndexInsert(xpool* pPool, xpoolpage* pPage)
{
	size_t iPosition = 0;
	uintptr_t iBase = (uintptr_t)pPage->Memory;

	while (
		(iPosition < pPool->PageCount) &&
		((uintptr_t)pPool->Index[iPosition]->Memory < iBase)
	) {
		iPosition++;
	}
	if ( iPosition < pPool->PageCount ) {
		memmove(
			&pPool->Index[iPosition + 1],
			&pPool->Index[iPosition],
			(pPool->PageCount - iPosition) * sizeof(xpoolpage*)
		);
	}
	pPool->Index[iPosition] = pPage;
}



/* 将页从有序索引移除。 */
static void __xrtPoolIndexRemove(xpool* pPool, xpoolpage* pPage)
{
	size_t iPosition = 0;

	while ( (iPosition < pPool->PageCount) && (pPool->Index[iPosition] != pPage) ) {
		iPosition++;
	}
	if ( iPosition >= pPool->PageCount ) {
		return;
	}
	if ( (iPosition + 1) < pPool->PageCount ) {
		memmove(
			&pPool->Index[iPosition],
			&pPool->Index[iPosition + 1],
			(pPool->PageCount - iPosition - 1) * sizeof(xpoolpage*)
		);
	}
}



/* 创建并登记一个新的空页。 */
static xpoolpage* __xrtPoolAddPage(xpool* pPool)
{
	xpoolpage* pPage;

	if ( !__xrtPoolIndexReserve(pPool, pPool->PageCount + 1) ) {
		return NULL;
	}
	pPage = xrtPoolPageCreateLayout(
		pPool->ItemSize,
		pPool->Alignment,
		pPool->PageCapacity
	);
	if ( pPage == NULL ) {
		return NULL;
	}
	pPage->Parent = pPool;
	pPage->Next = pPool->Pages;
	if ( pPool->Pages != NULL ) {
		pPool->Pages->Prev = pPage;
	}
	__xrtPoolIndexInsert(pPool, pPage);
	pPool->Pages = pPage;
	__xrtPoolAvailableAdd(pPool, pPage);
	pPool->PageCount++;
	pPool->EmptyPages++;
	return pPage;
}



/* 注销并销毁一个已经为空的页。 */
static void __xrtPoolRemoveEmptyPage(xpool* pPool, xpoolpage* pPage)
{
	__xrtPoolAvailableRemove(pPool, pPage);
	__xrtPoolIndexRemove(pPool, pPage);
	if ( pPage->Prev != NULL ) {
		pPage->Prev->Next = pPage->Next;
	} else {
		pPool->Pages = pPage->Next;
	}
	if ( pPage->Next != NULL ) {
		pPage->Next->Prev = pPage->Prev;
	}
	pPool->PageCount--;
	pPool->EmptyPages--;
	xrtPoolPageDestroy(pPage);
}



/* 根据所有页的槽状态重建可分配链和汇总计数。 */
static void __xrtPoolRebuildState(xpool* pPool)
{
	xpoolpage* pPage = pPool->Pages;

	pPool->Available = NULL;
	pPool->EmptyPages = 0;
	pPool->LiveCount = 0;
	while ( pPage != NULL ) {
		pPage->AvailablePrev = NULL;
		pPage->AvailableNext = NULL;
		pPool->LiveCount += pPage->LiveCount;
		if ( pPage->LiveCount == 0 ) {
			pPool->EmptyPages++;
		}
		if ( pPage->LiveCount < pPage->Capacity ) {
			__xrtPoolAvailableAdd(pPool, pPage);
		}
		pPage = pPage->Next;
	}
}



/* 同步固定池和全部现有页的访问保护状态。 */
void __xrtPoolSetVisiting(xpool* pPool, bool bVisiting)
{
	xpoolpage* pPage;

	if ( bVisiting ) {
		pPool->Flags |= XRT_POOL_FLAG_VISITING;
	} else {
		pPool->Flags &= ~XRT_POOL_FLAG_VISITING;
	}
	for ( pPage = pPool->Pages; pPage != NULL; pPage = pPage->Next ) {
		if ( bVisiting ) {
			pPage->Flags |= XRT_POOL_PAGE_FLAG_VISITING;
		} else {
			pPage->Flags &= ~XRT_POOL_PAGE_FLAG_VISITING;
		}
	}
}



/* 通过有序地址区间索引查找指针所属页。 */
xpoolpage* __xrtPoolFindPage(const xpool* pPool, const void* pMemory)
{
	size_t iLeft = 0;
	size_t iRight;
	uintptr_t iValue;
	xpoolpage* pPage;
	uintptr_t iBase;

	if (
		(pPool == NULL) ||
		((pPool->Flags & XRT_POOL_FLAG_READY) == 0) ||
		(pMemory == NULL) ||
		(pPool->PageCount == 0)
	) {
		return NULL;
	}
	iRight = pPool->PageCount;
	iValue = (uintptr_t)pMemory;
	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2);

		if ( (uintptr_t)pPool->Index[iMiddle]->Memory <= iValue ) {
			iLeft = iMiddle + 1;
		} else {
			iRight = iMiddle;
		}
	}
	if ( iLeft == 0 ) {
		return NULL;
	}
	pPage = pPool->Index[iLeft - 1];
	iBase = (uintptr_t)pPage->Memory;
	if ( (iValue - iBase) >= pPage->MemorySize ) {
		return NULL;
	}
	return pPage;
}



/* 分配对象并把所属页返回给上层索引。 */
ptr __xrtPoolAllocObject(xpool* pPool, bool bZero, xpoolpage** ppPage)
{
	xpoolpage* pPage;
	ptr pObject;
	bool bWasEmpty;

	if ( !__xrtPoolCanMutate(pPool, "pool.alloc") ) {
		return NULL;
	}
	if ( pPool->Available == NULL ) {
		if ( __xrtPoolAddPage(pPool) == NULL ) {
			return NULL;
		}
	}
	pPage = pPool->Available;
	bWasEmpty = pPage->LiveCount == 0;
	pObject = bZero ? xrtPoolPageCalloc(pPage) : xrtPoolPageAlloc(pPage);
	if ( pObject == NULL ) {
		return NULL;
	}
	if ( bWasEmpty ) {
		pPool->EmptyPages--;
	}
	if ( pPage->LiveCount == pPage->Capacity ) {
		__xrtPoolAvailableRemove(pPool, pPage);
	}
	pPool->LiveCount++;
	pPool->AllocCount++;
	if ( pPool->LiveCount > pPool->PeakCount ) {
		pPool->PeakCount = pPool->LiveCount;
	}
	if ( ppPage != NULL ) {
		*ppPage = pPage;
	}
	return pObject;
}



/* 使用给定布局初始化固定对象池，零槽数表示按目标字节数推导。 */
static bool __xrtPoolInit(
	xpool* pPool,
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity
)
{
	size_t iActualCapacity;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pPool, 0, sizeof(*pPool));
	if (
		!__xrtPoolLayout(
			iItemSize,
			iAlignment,
			iPageCapacity,
			&iActualCapacity
		)
	) {
		return false;
	}
	pPool->ItemSize = iItemSize;
	pPool->Alignment = iAlignment;
	pPool->PageCapacity = iActualCapacity;
	pPool->RetainEmpty = 1;
	pPool->Flags = XRT_POOL_FLAG_READY;
	return true;
}



/* 使用默认对齐和自动页槽数初始化固定对象池。 */
XRT_API bool xrtPoolInit(xpool* pPool, size_t iItemSize)
{
	return __xrtPoolInit(
		pPool,
		iItemSize,
		XRT_POOL_ALIGNMENT_DEFAULT,
		0
	);
}



/* 使用指定对齐和自动页槽数初始化固定对象池。 */
XRT_API bool xrtPoolInitAligned(xpool* pPool, size_t iItemSize, size_t iAlignment)
{
	return __xrtPoolInit(pPool, iItemSize, iAlignment, 0);
}



/* 使用显式对齐和每页槽数初始化固定对象池。 */
XRT_API bool xrtPoolInitLayout(
	xpool* pPool,
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity
)
{
	if ( iPageCapacity == 0 ) {
		if ( pPool == NULL ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memset(pPool, 0, sizeof(*pPool));
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_CAPACITY,
			"pool.init",
			"page capacity must be between one and 256."
		);
		return false;
	}
	return __xrtPoolInit(
		pPool,
		iItemSize,
		iAlignment,
		iPageCapacity
	);
}



/* 创建使用默认对齐的固定对象池。 */
XRT_API xpool* xrtPoolCreate(size_t iItemSize)
{
	return xrtPoolCreateAligned(iItemSize, XRT_POOL_ALIGNMENT_DEFAULT);
}



/* 创建使用指定对齐的固定对象池。 */
XRT_API xpool* xrtPoolCreateAligned(size_t iItemSize, size_t iAlignment)
{
	xpool* pPool = (xpool*)xrtMalloc(sizeof(xpool));

	if ( pPool == NULL ) {
		return NULL;
	}
	if ( !xrtPoolInitAligned(pPool, iItemSize, iAlignment) ) {
		xrtFree(pPool);
		return NULL;
	}
	return pPool;
}



/* 创建使用显式对齐和每页槽数的固定对象池。 */
XRT_API xpool* xrtPoolCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity
)
{
	xpool* pPool = (xpool*)xrtMalloc(sizeof(xpool));

	if ( pPool == NULL ) {
		return NULL;
	}
	if (
		!xrtPoolInitLayout(
			pPool,
			iItemSize,
			iAlignment,
			iPageCapacity
		)
	) {
		xrtFree(pPool);
		return NULL;
	}
	return pPool;
}



/* 释放池持有的全部页。 */
XRT_API void xrtPoolUnit(xpool* pPool)
{
	xpoolpage* pPage;

	if ( pPool == NULL ) {
		return;
	}
	if ( (pPool->Flags & XRT_POOL_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"pool.unit",
			"memory pool cannot be released during a visit."
		);
		return;
	}
	pPage = pPool->Pages;
	while ( pPage != NULL ) {
		xpoolpage* pNext = pPage->Next;

		xrtPoolPageDestroy(pPage);
		pPage = pNext;
	}
	xrtFree(pPool->Index);
	memset(pPool, 0, sizeof(*pPool));
}



/* 释放池持有的全部资源和池结构。 */
XRT_API void xrtPoolDestroy(xpool* pPool)
{
	if ( pPool == NULL ) {
		return;
	}
	if ( (pPool->Flags & XRT_POOL_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"pool.destroy",
			"memory pool cannot be destroyed during a visit."
		);
		return;
	}
	xrtPoolUnit(pPool);
	xrtFree(pPool);
}



/* 分配一个未初始化对象。 */
XRT_API ptr xrtPoolAlloc(xpool* pPool)
{
	return __xrtPoolAllocObject(pPool, false, NULL);
}



/* 分配并清零一个对象。 */
XRT_API ptr xrtPoolCalloc(xpool* pPool)
{
	return __xrtPoolAllocObject(pPool, true, NULL);
}



/* 安全释放一个活动对象。 */
XRT_API bool xrtPoolFree(xpool* pPool, ptr pObject)
{
	xpoolpage* pPage;
	bool bWasFull;

	if ( pObject == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPoolCanMutate(pPool, "pool.free") ) {
		return false;
	}
	pPage = __xrtPoolFindPage(pPool, pObject);
	if ( pPage == NULL ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_POINTER,
			"pool.free",
			"pointer does not belong to this fixed-size memory pool."
		);
		return false;
	}
	bWasFull = pPage->LiveCount == pPage->Capacity;
	if ( !xrtPoolPageFree(pPage, pObject) ) {
		return false;
	}
	if ( bWasFull ) {
		__xrtPoolAvailableAdd(pPool, pPage);
	}
	pPool->LiveCount--;
	pPool->FreeCount++;
	if ( pPage->LiveCount == 0 ) {
		pPool->EmptyPages++;
		if ( pPool->EmptyPages > pPool->RetainEmpty ) {
			__xrtPoolRemoveEmptyPage(pPool, pPage);
		}
	}
	return true;
}



/* 判断指针当前是否属于该池的活动对象。 */
XRT_API bool xrtPoolOwns(const xpool* pPool, const void* pObject)
{
	xpoolpage* pPage = __xrtPoolFindPage(pPool, pObject);

	return (pPage != NULL) && xrtPoolPageOwns(pPage, pObject);
}



/* 标记一个活动对象为本轮可达。 */
XRT_API bool xrtPoolMark(xpool* pPool, ptr pObject)
{
	xpoolpage* pPage;

	if (
		(pPool == NULL) ||
		((pPool->Flags & XRT_POOL_FLAG_READY) == 0) ||
		(pObject == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pPage = __xrtPoolFindPage(pPool, pObject);
	if ( pPage == NULL ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_POINTER,
			"pool.mark",
			"pointer does not belong to this fixed-size memory pool."
		);
		return false;
	}
	return xrtPoolPageMark(pPage, pObject);
}



/* 释放未标记对象，并清除幸存对象标记。 */
XRT_API size_t xrtPoolSweep(xpool* pPool)
{
	size_t iFreed = 0;
	xpoolpage* pPage;

	if ( !__xrtPoolCanMutate(pPool, "pool.sweep") ) {
		return 0;
	}
	for ( pPage = pPool->Pages; pPage != NULL; pPage = pPage->Next ) {
		iFreed += xrtPoolPageSweep(pPage);
	}
	pPool->FreeCount += iFreed;
	__xrtPoolRebuildState(pPool);
	(void)xrtPoolTrim(pPool, pPool->RetainEmpty);
	return iFreed;
}



/* 释放已标记对象。 */
XRT_API size_t xrtPoolFreeMarked(xpool* pPool)
{
	size_t iFreed = 0;
	xpoolpage* pPage;

	if ( !__xrtPoolCanMutate(pPool, "pool.free_marked") ) {
		return 0;
	}
	for ( pPage = pPool->Pages; pPage != NULL; pPage = pPage->Next ) {
		iFreed += xrtPoolPageFreeMarked(pPage);
	}
	pPool->FreeCount += iFreed;
	__xrtPoolRebuildState(pPool);
	(void)xrtPoolTrim(pPool, pPool->RetainEmpty);
	return iFreed;
}



/* 释放全部活动对象，并按保留策略回收空页。 */
XRT_API size_t xrtPoolReset(xpool* pPool)
{
	size_t iFreed = 0;
	xpoolpage* pPage;

	if ( !__xrtPoolCanMutate(pPool, "pool.reset") ) {
		return 0;
	}
	for ( pPage = pPool->Pages; pPage != NULL; pPage = pPage->Next ) {
		iFreed += xrtPoolPageReset(pPage);
	}
	pPool->FreeCount += iFreed;
	__xrtPoolRebuildState(pPool);
	(void)xrtPoolTrim(pPool, pPool->RetainEmpty);
	return iFreed;
}



/* 回收多余空页。 */
XRT_API size_t xrtPoolTrim(xpool* pPool, size_t iRetainEmpty)
{
	size_t iFreed = 0;
	xpoolpage* pPage;

	if ( !__xrtPoolCanMutate(pPool, "pool.trim") ) {
		return 0;
	}
	pPage = pPool->Pages;
	while ( (pPage != NULL) && (pPool->EmptyPages > iRetainEmpty) ) {
		xpoolpage* pNext = pPage->Next;

		if ( pPage->LiveCount == 0 ) {
			__xrtPoolRemoveEmptyPage(pPool, pPage);
			iFreed++;
		}
		pPage = pNext;
	}
	return iFreed;
}



/* 设置自动保留的空页数。 */
XRT_API void xrtPoolSetRetain(xpool* pPool, size_t iRetainEmpty)
{
	if ( !__xrtPoolCanMutate(pPool, "pool.set_retain") ) {
		return;
	}
	pPool->RetainEmpty = iRetainEmpty;
	(void)xrtPoolTrim(pPool, iRetainEmpty);
}



/* 获取固定对象池当前状态。 */
XRT_API void xrtPoolGet(const xpool* pPool, xpoolinfo* pInfo)
{
	if ( pInfo == NULL ) {
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
	if ( (pPool == NULL) || ((pPool->Flags & XRT_POOL_FLAG_READY) == 0) ) {
		return;
	}
	pInfo->ItemSize = pPool->ItemSize;
	pInfo->Stride = (pPool->ItemSize + (pPool->Alignment - 1)) & ~(pPool->Alignment - 1);
	pInfo->Alignment = pPool->Alignment;
	pInfo->PageCapacity = pPool->PageCapacity;
	pInfo->PageCount = pPool->PageCount;
	pInfo->EmptyPages = pPool->EmptyPages;
	pInfo->LiveCount = pPool->LiveCount;
	pInfo->PeakCount = pPool->PeakCount;
	pInfo->Capacity = pPool->PageCount <= (SIZE_MAX / pPool->PageCapacity) ?
		pPool->PageCount * pPool->PageCapacity : SIZE_MAX;
	pInfo->AllocCount = pPool->AllocCount;
	pInfo->FreeCount = pPool->FreeCount;
}



/* 访问当前活动对象，并阻止回调改变池的分配集合。 */
XRT_API size_t xrtPoolVisit(xpool* pPool, xpoolvisitor pVisitor, ptr pUserData)
{
	size_t iVisited = 0;
	xpoolpage* pPage;

	if (
		(pPool == NULL) ||
		((pPool->Flags & XRT_POOL_FLAG_READY) == 0) ||
		(pVisitor == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( (pPool->Flags & XRT_POOL_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"pool.visit",
			"memory pool is already being visited."
		);
		return 0;
	}
	__xrtPoolSetVisiting(pPool, true);
	for ( pPage = pPool->Pages; pPage != NULL; pPage = pPage->Next ) {
		for ( size_t i = 0; i < pPage->NextIndex; i++ ) {
			ptr pObject = xrtPoolPageGet(pPage, i);

			if ( pObject == NULL ) {
				continue;
			}
			if ( !pVisitor(
				pObject,
				iVisited,
				pUserData
			) ) {
				iVisited++;
				__xrtPoolSetVisiting(pPool, false);
				return iVisited;
			}
			iVisited++;
		}
	}
	__xrtPoolSetVisiting(pPool, false);
	return iVisited;
}

#endif
