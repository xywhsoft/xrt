#include "../internal/xrt_pool.h"



#if defined(XRT_FEATURE_POOL_PAGE)

/* 设置带稳定错误域和操作名的内存池错误。 */
void __xrtPoolSetError(xerrkind Kind, int32 iCode, cstr sOperation, cstr sMessage)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = Kind;
	tDesc.Code = iCode;
	tDesc.Domain = "xrt.pool";
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 检查单页是否允许改变槽集合。 */
static bool __xrtPoolPageCanMutate(xpoolpage* pPage, cstr sOperation)
{
	if ( (pPage == NULL) || ((pPage->Flags & XRT_POOL_PAGE_FLAG_READY) == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pPage->Capacity == 0) ||
		(pPage->Capacity > XRT_POOL_PAGE_CAPACITY) ||
		(pPage->LiveCount > pPage->Capacity) ||
		(pPage->NextIndex > pPage->Capacity) ||
		(pPage->FreeCount > pPage->NextIndex) ||
		((size_t)pPage->LiveCount + pPage->FreeCount != pPage->NextIndex) ||
		(pPage->Stride == 0) ||
		(pPage->ItemSize == 0) ||
		(pPage->ItemSize > pPage->Stride) ||
		(pPage->Memory == NULL) ||
		(pPage->Allocation == NULL) ||
		(pPage->Stride > (SIZE_MAX / pPage->Capacity)) ||
		(pPage->MemorySize != (pPage->Stride * pPage->Capacity))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pPage->Flags & XRT_POOL_PAGE_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			sOperation,
			"memory pool page allocation set cannot change during a visit."
		);
		return false;
	}
	return true;
}



/* 判断对齐参数是否为有效的二次幂。 */
static bool __xrtPoolAlignmentValid(size_t iAlignment)
{
	return (iAlignment != 0) && ((iAlignment & (iAlignment - 1)) == 0);
}



/* 计算槽步长和整页用户区大小。 */
static bool __xrtPoolPageLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iCapacity,
	size_t* pStride,
	size_t* pMemorySize
)
{
	size_t iStride;

	if ( iItemSize == 0 ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_SIZE,
			"page.init",
			"item size must be non-zero."
		);
		return false;
	}
	if ( !__xrtPoolAlignmentValid(iAlignment) ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_ALIGNMENT,
			"page.init",
			"alignment must be a power of two."
		);
		return false;
	}
	if ( (iCapacity == 0) || (iCapacity > XRT_POOL_PAGE_CAPACITY) ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_CAPACITY,
			"page.init",
			"page capacity must be between one and 256."
		);
		return false;
	}
	if ( iItemSize > (SIZE_MAX - (iAlignment - 1)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iStride = (iItemSize + (iAlignment - 1)) & ~(iAlignment - 1);
	if ( iStride > (SIZE_MAX / iCapacity) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pStride = iStride;
	*pMemorySize = iStride * iCapacity;
	return true;
}



/* 返回指定位图中的槽状态。 */
static bool __xrtPoolPageBitGet(const uint64* pBits, size_t iIndex)
{
	return (pBits[iIndex >> 6] & ((uint64)1u << (iIndex & 63u))) != 0;
}



/* 设置指定位图中的槽状态。 */
static void __xrtPoolPageBitSet(uint64* pBits, size_t iIndex)
{
	pBits[iIndex >> 6] |= (uint64)1u << (iIndex & 63u);
}



/* 清除指定位图中的槽状态。 */
static void __xrtPoolPageBitClear(uint64* pBits, size_t iIndex)
{
	pBits[iIndex >> 6] &= ~((uint64)1u << (iIndex & 63u));
}



/* 将任意指针转换为页内精确槽索引，不读取指针前后的内存。 */
static bool __xrtPoolPageLocate(
	const xpoolpage* pPage,
	const void* pMemory,
	bool bRequireLive,
	size_t* pIndex
)
{
	uintptr_t iBase;
	uintptr_t iValue;
	size_t iOffset;
	size_t iIndex;

	if (
		(pPage == NULL) ||
		((pPage->Flags & XRT_POOL_PAGE_FLAG_READY) == 0) ||
		(pMemory == NULL) ||
		(pPage->Memory == NULL) ||
		(pPage->Stride == 0) ||
		(pPage->Capacity == 0) ||
		(pPage->Capacity > XRT_POOL_PAGE_CAPACITY) ||
		(pPage->NextIndex > pPage->Capacity)
	) {
		return false;
	}
	iBase = (uintptr_t)pPage->Memory;
	iValue = (uintptr_t)pMemory;
	if ( (iValue < iBase) || ((iValue - iBase) >= pPage->MemorySize) ) {
		return false;
	}
	iOffset = (size_t)(iValue - iBase);
	if ( (iOffset % pPage->Stride) != 0 ) {
		return false;
	}
	iIndex = iOffset / pPage->Stride;
	if ( iIndex >= pPage->NextIndex ) {
		return false;
	}
	if ( bRequireLive && !__xrtPoolPageBitGet(pPage->Used, iIndex) ) {
		return false;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	return true;
}



/* 将一个已确认活动的槽放回空闲栈。 */
static void __xrtPoolPageReleaseIndex(xpoolpage* pPage, size_t iIndex)
{
	__xrtPoolPageBitClear(pPage->Used, iIndex);
	__xrtPoolPageBitClear(pPage->Marked, iIndex);
	pPage->LiveCount--;
	if ( pPage->LiveCount == 0 ) {
		pPage->NextIndex = 0;
		pPage->FreeCount = 0;
		memset(pPage->Used, 0, sizeof(pPage->Used));
		memset(pPage->Marked, 0, sizeof(pPage->Marked));
		return;
	}
	pPage->FreeList[pPage->FreeCount] = (uint8)iIndex;
	pPage->FreeCount++;
}



/* 使用默认 16 字节对齐初始化一个空页。 */
XRT_API bool xrtPoolPageInit(xpoolpage* pPage, size_t iItemSize)
{
	return xrtPoolPageInitAligned(pPage, iItemSize, XRT_POOL_ALIGNMENT_DEFAULT);
}



/* 使用指定对齐初始化一个空页。 */
XRT_API bool xrtPoolPageInitAligned(xpoolpage* pPage, size_t iItemSize, size_t iAlignment)
{
	return xrtPoolPageInitLayout(
		pPage,
		iItemSize,
		iAlignment,
		XRT_POOL_PAGE_CAPACITY
	);
}



/* 使用显式对齐和槽数初始化一个空页。 */
XRT_API bool xrtPoolPageInitLayout(
	xpoolpage* pPage,
	size_t iItemSize,
	size_t iAlignment,
	size_t iCapacity
)
{
	size_t iStride;
	size_t iMemorySize;
	size_t iAllocationSize;
	ptr pAllocation;
	ptr pMemory;

	if ( pPage == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pPage, 0, sizeof(*pPage));
	if (
		!__xrtPoolPageLayout(
			iItemSize,
			iAlignment,
			iCapacity,
			&iStride,
			&iMemorySize
		)
	) {
		return false;
	}
	if (
		!__xrtPoolAlignedAllocationSize(
			iMemorySize,
			iAlignment,
			&iAllocationSize
		)
	) {
		return false;
	}
	pAllocation = xrtMalloc(iAllocationSize);
	if ( pAllocation == NULL ) {
		return false;
	}
	if ( !__xrtPoolAlignAfter(pAllocation, iAlignment, &pMemory) ) {
		xrtFree(pAllocation);
		return false;
	}
	pPage->Allocation = pAllocation;
	pPage->Memory = (bytes)pMemory;
	pPage->ItemSize = iItemSize;
	pPage->Stride = iStride;
	pPage->Alignment = iAlignment;
	pPage->MemorySize = iMemorySize;
	pPage->Capacity = (uint16)iCapacity;
	pPage->Flags = XRT_POOL_PAGE_FLAG_READY;
	return true;
}



/* 创建使用默认对齐的单页对象。 */
XRT_API xpoolpage* xrtPoolPageCreate(size_t iItemSize)
{
	return xrtPoolPageCreateAligned(iItemSize, XRT_POOL_ALIGNMENT_DEFAULT);
}



/* 创建使用指定对齐的单页对象。 */
XRT_API xpoolpage* xrtPoolPageCreateAligned(size_t iItemSize, size_t iAlignment)
{
	return xrtPoolPageCreateLayout(
		iItemSize,
		iAlignment,
		XRT_POOL_PAGE_CAPACITY
	);
}



/* 创建使用显式对齐和槽数的单页对象。 */
XRT_API xpoolpage* xrtPoolPageCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iCapacity
)
{
	xpoolpage* pPage = (xpoolpage*)xrtMalloc(sizeof(xpoolpage));

	if ( pPage == NULL ) {
		return NULL;
	}
	if ( !xrtPoolPageInitLayout(pPage, iItemSize, iAlignment, iCapacity) ) {
		xrtFree(pPage);
		return NULL;
	}
	return pPage;
}



/* 释放页持有的槽内存。 */
XRT_API void xrtPoolPageUnit(xpoolpage* pPage)
{
	if ( pPage == NULL ) {
		return;
	}
	if ( (pPage->Flags & XRT_POOL_PAGE_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"page.unit",
			"memory pool page cannot be released during a visit."
		);
		return;
	}
	if ( (pPage->Flags & XRT_POOL_PAGE_FLAG_READY) != 0 ) {
		xrtFree(pPage->Allocation);
	}
	memset(pPage, 0, sizeof(*pPage));
}



/* 释放页持有的全部资源和页结构。 */
XRT_API void xrtPoolPageDestroy(xpoolpage* pPage)
{
	if ( pPage == NULL ) {
		return;
	}
	if ( (pPage->Flags & XRT_POOL_PAGE_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"page.destroy",
			"memory pool page cannot be destroyed during a visit."
		);
		return;
	}
	xrtPoolPageUnit(pPage);
	xrtFree(pPage);
}



/* 分配一个未初始化槽。 */
XRT_API ptr xrtPoolPageAlloc(xpoolpage* pPage)
{
	size_t iIndex;
	ptr pMemory;

	if ( !__xrtPoolPageCanMutate(pPage, "page.alloc") ) {
		return NULL;
	}
	if ( pPage->LiveCount >= pPage->Capacity ) {
		__xrtPoolSetError(
			XERR_AGAIN,
			XPOOL_ERROR_PAGE_FULL,
			"page.alloc",
			"memory pool page is full."
		);
		return NULL;
	}
	if ( pPage->FreeCount != 0 ) {
		pPage->FreeCount--;
		iIndex = pPage->FreeList[pPage->FreeCount];
	} else {
		iIndex = pPage->NextIndex;
		pPage->NextIndex++;
	}
	__xrtPoolPageBitSet(pPage->Used, iIndex);
	__xrtPoolPageBitClear(pPage->Marked, iIndex);
	pPage->LiveCount++;
	pMemory = pPage->Memory + (iIndex * pPage->Stride);
	return pMemory;
}



/* 分配并清零一个槽。 */
XRT_API ptr xrtPoolPageCalloc(xpoolpage* pPage)
{
	ptr pMemory = xrtPoolPageAlloc(pPage);

	if ( pMemory != NULL ) {
		memset(pMemory, 0, pPage->ItemSize);
	}
	return pMemory;
}



/* 安全释放一个活动槽。 */
XRT_API bool xrtPoolPageFree(xpoolpage* pPage, ptr pMemory)
{
	size_t iIndex;

	if ( pMemory == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPoolPageCanMutate(pPage, "page.free") ) {
		return false;
	}
	if ( !__xrtPoolPageLocate(pPage, pMemory, false, &iIndex) ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_POINTER,
			"page.free",
			"pointer does not identify a slot in this memory pool page."
		);
		return false;
	}
	if ( !__xrtPoolPageBitGet(pPage->Used, iIndex) ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_NOT_ALLOCATED,
			"page.free",
			"memory pool slot is not allocated."
		);
		return false;
	}
	__xrtPoolPageReleaseIndex(pPage, iIndex);
	return true;
}



/* 按槽索引释放活动对象。 */
XRT_API bool xrtPoolPageFreeAt(xpoolpage* pPage, size_t iIndex)
{
	if ( !__xrtPoolPageCanMutate(pPage, "page.free_at") ) {
		return false;
	}
	if ( (iIndex >= pPage->Capacity) || (iIndex >= pPage->NextIndex) ) {
		__xrtPoolSetError(
			XERR_RANGE,
			XPOOL_ERROR_INDEX_OUT_OF_RANGE,
			"page.free_at",
			"memory pool slot index is out of range."
		);
		return false;
	}
	if ( !__xrtPoolPageBitGet(pPage->Used, iIndex) ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_NOT_ALLOCATED,
			"page.free_at",
			"memory pool slot is not allocated."
		);
		return false;
	}
	__xrtPoolPageReleaseIndex(pPage, iIndex);
	return true;
}



/* 返回指定索引处的活动对象。 */
XRT_API ptr xrtPoolPageGet(const xpoolpage* pPage, size_t iIndex)
{
	if (
		(pPage == NULL) ||
		((pPage->Flags & XRT_POOL_PAGE_FLAG_READY) == 0) ||
		(pPage->Capacity == 0) ||
		(pPage->Capacity > XRT_POOL_PAGE_CAPACITY) ||
		(pPage->NextIndex > pPage->Capacity) ||
		(iIndex >= pPage->NextIndex) ||
		!__xrtPoolPageBitGet(pPage->Used, iIndex)
	) {
		return NULL;
	}
	return pPage->Memory + (iIndex * pPage->Stride);
}



/* 获取活动对象的槽索引。 */
XRT_API bool xrtPoolPageIndex(const xpoolpage* pPage, const void* pMemory, size_t* pIndex)
{
	if (
		(pPage == NULL) ||
		((pPage->Flags & XRT_POOL_PAGE_FLAG_READY) == 0) ||
		(pMemory == NULL) ||
		(pIndex == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPoolPageLocate(pPage, pMemory, true, pIndex) ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_POINTER,
			"page.index",
			"pointer does not identify an allocated slot in this memory pool page."
		);
		return false;
	}
	return true;
}



/* 判断指针当前是否属于该页的活动槽。 */
XRT_API bool xrtPoolPageOwns(const xpoolpage* pPage, const void* pMemory)
{
	return __xrtPoolPageLocate(pPage, pMemory, true, NULL);
}



/* 标记一个活动槽为本轮可达对象。 */
XRT_API bool xrtPoolPageMark(xpoolpage* pPage, ptr pMemory)
{
	size_t iIndex;

	if (
		(pPage == NULL) ||
		((pPage->Flags & XRT_POOL_PAGE_FLAG_READY) == 0) ||
		(pMemory == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPoolPageLocate(pPage, pMemory, true, &iIndex) ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_POINTER,
			"page.mark",
			"pointer does not identify an allocated slot in this memory pool page."
		);
		return false;
	}
	__xrtPoolPageBitSet(pPage->Marked, iIndex);
	return true;
}



/* 释放未标记槽，并清除幸存槽的标记。 */
XRT_API size_t xrtPoolPageSweep(xpoolpage* pPage)
{
	size_t iFreed = 0;
	size_t iLimit;

	if ( !__xrtPoolPageCanMutate(pPage, "page.sweep") ) {
		return 0;
	}
	iLimit = pPage->NextIndex;
	for ( size_t i = 0; i < iLimit; i++ ) {
		if ( !__xrtPoolPageBitGet(pPage->Used, i) ) {
			continue;
		}
		if ( __xrtPoolPageBitGet(pPage->Marked, i) ) {
			__xrtPoolPageBitClear(pPage->Marked, i);
		} else {
			__xrtPoolPageReleaseIndex(pPage, i);
			iFreed++;
		}
	}
	return iFreed;
}



/* 释放已标记槽。 */
XRT_API size_t xrtPoolPageFreeMarked(xpoolpage* pPage)
{
	size_t iFreed = 0;
	size_t iLimit;

	if ( !__xrtPoolPageCanMutate(pPage, "page.free_marked") ) {
		return 0;
	}
	iLimit = pPage->NextIndex;
	for ( size_t i = 0; i < iLimit; i++ ) {
		if (
			__xrtPoolPageBitGet(pPage->Used, i) &&
			__xrtPoolPageBitGet(pPage->Marked, i)
		) {
			__xrtPoolPageReleaseIndex(pPage, i);
			iFreed++;
		}
	}
	return iFreed;
}



/* 将页内全部槽恢复为空闲状态。 */
XRT_API size_t xrtPoolPageReset(xpoolpage* pPage)
{
	size_t iFreed;

	if ( !__xrtPoolPageCanMutate(pPage, "page.reset") ) {
		return 0;
	}
	iFreed = pPage->LiveCount;
	pPage->LiveCount = 0;
	pPage->NextIndex = 0;
	pPage->FreeCount = 0;
	memset(pPage->Used, 0, sizeof(pPage->Used));
	memset(pPage->Marked, 0, sizeof(pPage->Marked));
	return iFreed;
}



/* 获取单页当前状态。 */
XRT_API void xrtPoolPageGetInfo(const xpoolpage* pPage, xpoolpageinfo* pInfo)
{
	if ( pInfo == NULL ) {
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
	if (
		(pPage == NULL) ||
		((pPage->Flags & XRT_POOL_PAGE_FLAG_READY) == 0) ||
		(pPage->Capacity == 0) ||
		(pPage->Capacity > XRT_POOL_PAGE_CAPACITY)
	) {
		return;
	}
	pInfo->ItemSize = pPage->ItemSize;
	pInfo->Stride = pPage->Stride;
	pInfo->Alignment = pPage->Alignment;
	pInfo->LiveCount = pPage->LiveCount;
	pInfo->FreeCount = pPage->FreeCount;
	pInfo->Capacity = pPage->Capacity;
}

#endif
