#include "../internal/xrt_pool.h"



#if defined(XRT_FEATURE_MEMORY_POOL)

#define XRT_MEMPOOL_LARGE_EMPTY	0u
#define XRT_MEMPOOL_LARGE_USED		1u
#define XRT_MEMPOOL_LARGE_DELETED	2u



/* 每个尺寸类直接复用完整的固定对象池实现。 */
struct xmempoolbucket {
	xpool Pool;
};



/* 独立大块使用外部哈希表登记，不读取用户指针前方内存。 */
struct xmempoollarge {
	ptr User;
	ptr Raw;
	size_t Size;
	size_t Alignment;
	uint8 State;
	uint8 Marked;
};



/* 检查变长池是否允许改变分配集合。 */
static bool __xrtMemPoolCanMutate(xmempool* pPool, cstr sOperation)
{
	if ( (pPool == NULL) || ((pPool->Flags & XRT_MEMPOOL_FLAG_READY) == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pPool->Flags & XRT_MEMPOOL_FLAG_VISITING) != 0 ) {
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



/* 同步变长池、尺寸类和全部小块页的访问保护状态。 */
static void __xrtMemPoolSetVisiting(xmempool* pPool, bool bVisiting)
{
	if ( bVisiting ) {
		pPool->Flags |= XRT_MEMPOOL_FLAG_VISITING;
	} else {
		pPool->Flags &= ~XRT_MEMPOOL_FLAG_VISITING;
	}
	for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
		__xrtPoolSetVisiting(&pPool->Buckets[i].Pool, bVisiting);
	}
}



/* 判断对齐参数是否为有效的二次幂。 */
static bool __xrtMemPoolAlignmentValid(size_t iAlignment)
{
	return (iAlignment != 0) && ((iAlignment & (iAlignment - 1)) == 0);
}



/* 对指针值生成适合二次幂表容量的哈希。 */
static size_t __xrtMemPoolPointerHash(const void* pMemory)
{
	uintptr_t iValue = (uintptr_t)pMemory;

	#if UINTPTR_MAX > UINT32_MAX
		iValue ^= iValue >> 33;
		iValue *= (uintptr_t)0xff51afd7ed558ccdu;
		iValue ^= iValue >> 33;
	#else
		iValue ^= iValue >> 16;
		iValue *= (uintptr_t)0x7feb352du;
		iValue ^= iValue >> 15;
	#endif
	return (size_t)iValue;
}



/* 在指定大块表中找到插入位置。 */
static size_t __xrtMemPoolLargeSlot(
	xmempoollarge* pTable,
	size_t iCapacity,
	const void* pMemory,
	bool* pFound
)
{
	size_t iMask = iCapacity - 1;
	size_t iSlot = __xrtMemPoolPointerHash(pMemory) & iMask;
	size_t iDeleted = XRT_NPOS;

	for ( ;; ) {
		xmempoollarge* pEntry = &pTable[iSlot];

		if ( pEntry->State == XRT_MEMPOOL_LARGE_EMPTY ) {
			*pFound = false;
			return iDeleted != XRT_NPOS ? iDeleted : iSlot;
		}
		if ( (pEntry->State == XRT_MEMPOOL_LARGE_USED) && (pEntry->User == pMemory) ) {
			*pFound = true;
			return iSlot;
		}
		if ( (pEntry->State == XRT_MEMPOOL_LARGE_DELETED) && (iDeleted == XRT_NPOS) ) {
			iDeleted = iSlot;
		}
		iSlot = (iSlot + 1) & iMask;
	}
}



/* 将大块登记表重建到指定二次幂容量。 */
static bool __xrtMemPoolLargeRehash(xmempool* pPool, size_t iCapacity)
{
	xmempoollarge* pTable;

	if ( iCapacity > (SIZE_MAX / sizeof(xmempoollarge)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pTable = (xmempoollarge*)xrtCalloc(iCapacity, sizeof(xmempoollarge));
	if ( pTable == NULL ) {
		return false;
	}
	for ( size_t i = 0; i < pPool->LargeCapacity; i++ ) {
		xmempoollarge* pOld = &pPool->Large[i];
		bool bFound;
		size_t iSlot;

		if ( pOld->State != XRT_MEMPOOL_LARGE_USED ) {
			continue;
		}
		iSlot = __xrtMemPoolLargeSlot(pTable, iCapacity, pOld->User, &bFound);
		(void)bFound;
		pTable[iSlot] = *pOld;
	}
	xrtFree(pPool->Large);
	pPool->Large = pTable;
	pPool->LargeCapacity = iCapacity;
	pPool->LargeDeleted = 0;
	return true;
}



/* 确保大块登记表能够再插入一个对象。 */
static bool __xrtMemPoolLargeReserve(xmempool* pPool)
{
	size_t iCapacity;
	size_t iLimit;

	if ( pPool->LargeCapacity != 0 ) {
		iLimit = pPool->LargeCapacity - (pPool->LargeCapacity / 4);
		if ( (pPool->LargeCount + pPool->LargeDeleted + 1) < iLimit ) {
			return true;
		}
		if ( (pPool->LargeCount + 1) < iLimit ) {
			return __xrtMemPoolLargeRehash(pPool, pPool->LargeCapacity);
		}
	}
	iCapacity = pPool->LargeCapacity != 0 ? pPool->LargeCapacity * 2 : 16;
	if ( (iCapacity < pPool->LargeCapacity) || (iCapacity < 16) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return __xrtMemPoolLargeRehash(pPool, iCapacity);
}



/* 查找活动的大块登记项。 */
static xmempoollarge* __xrtMemPoolLargeFind(const xmempool* pPool, const void* pMemory)
{
	bool bFound;
	size_t iSlot;

	if ( (pPool->LargeCapacity == 0) || (pMemory == NULL) ) {
		return NULL;
	}
	iSlot = __xrtMemPoolLargeSlot(pPool->Large, pPool->LargeCapacity, pMemory, &bFound);
	return bFound ? &pPool->Large[iSlot] : NULL;
}



/* 登记一个已经成功分配的独立大块。 */
static void __xrtMemPoolLargeInsert(
	xmempool* pPool,
	ptr pUser,
	ptr pRaw,
	size_t iSize,
	size_t iAlignment
)
{
	bool bFound;
	size_t iSlot = __xrtMemPoolLargeSlot(
		pPool->Large,
		pPool->LargeCapacity,
		pUser,
		&bFound
	);
	xmempoollarge* pEntry = &pPool->Large[iSlot];

	(void)bFound;
	if ( pEntry->State == XRT_MEMPOOL_LARGE_DELETED ) {
		pPool->LargeDeleted--;
	}
	pEntry->User = pUser;
	pEntry->Raw = pRaw;
	pEntry->Size = iSize;
	pEntry->Alignment = iAlignment;
	pEntry->State = XRT_MEMPOOL_LARGE_USED;
	pEntry->Marked = 0;
	pPool->LargeCount++;
}



/* 注销并释放一个独立大块。 */
static void __xrtMemPoolLargeRelease(xmempool* pPool, xmempoollarge* pEntry)
{
	size_t iSize = pEntry->Size;
	ptr pRaw = pEntry->Raw;

	pEntry->User = NULL;
	pEntry->Raw = NULL;
	pEntry->Size = 0;
	pEntry->Alignment = 0;
	pEntry->Marked = 0;
	pEntry->State = XRT_MEMPOOL_LARGE_DELETED;
	pPool->LargeCount--;
	pPool->LargeDeleted++;
	pPool->LiveCount--;
	pPool->LiveBytes -= iSize;
	pPool->FreeCount++;
	xrtFree(pRaw);
}



/* 确保全局页地址索引能够再容纳一个页。 */
static bool __xrtMemPoolPageReserve(xmempool* pPool, size_t iNeed)
{
	size_t iCapacity;
	xpoolpage** pPages;

	if ( iNeed <= pPool->PageCapacity ) {
		return true;
	}
	iCapacity = pPool->PageCapacity != 0 ? pPool->PageCapacity : 16;
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
	pPages = (xpoolpage**)xrtRealloc(pPool->Pages, iCapacity * sizeof(xpoolpage*));
	if ( pPages == NULL ) {
		return false;
	}
	pPool->Pages = pPages;
	pPool->PageCapacity = iCapacity;
	return true;
}



/* 按用户区地址将页插入变长池全局索引。 */
static void __xrtMemPoolPageInsert(xmempool* pPool, xpoolpage* pPage)
{
	size_t iPosition = 0;
	uintptr_t iBase = (uintptr_t)pPage->Memory;

	while (
		(iPosition < pPool->PageCount) &&
		((uintptr_t)pPool->Pages[iPosition]->Memory < iBase)
	) {
		iPosition++;
	}
	if ( iPosition < pPool->PageCount ) {
		memmove(
			&pPool->Pages[iPosition + 1],
			&pPool->Pages[iPosition],
			(pPool->PageCount - iPosition) * sizeof(xpoolpage*)
		);
	}
	pPool->Pages[iPosition] = pPage;
	pPool->PageCount++;
}



/* 在页回收后从所有尺寸类重建全局地址索引。 */
static void __xrtMemPoolPageRebuild(xmempool* pPool)
{
	size_t iCount = 0;

	for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
		xpool* pClass = &pPool->Buckets[i].Pool;

		for ( xpoolpage* pPage = pClass->Pages; pPage != NULL; pPage = pPage->Next ) {
			pPool->Pages[iCount] = pPage;
			iCount++;
		}
	}
	for ( size_t i = 1; i < iCount; i++ ) {
		xpoolpage* pPage = pPool->Pages[i];
		size_t j = i;

		while (
			(j != 0) &&
			((uintptr_t)pPool->Pages[j - 1]->Memory > (uintptr_t)pPage->Memory)
		) {
			pPool->Pages[j] = pPool->Pages[j - 1];
			j--;
		}
		pPool->Pages[j] = pPage;
	}
	pPool->PageCount = iCount;
}



/* 通过有序页区间查找池化小块所属页。 */
static xpoolpage* __xrtMemPoolFindPage(const xmempool* pPool, const void* pMemory)
{
	size_t iLeft = 0;
	size_t iRight = pPool->PageCount;
	uintptr_t iValue = (uintptr_t)pMemory;
	xpoolpage* pPage;
	uintptr_t iBase;

	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2);

		if ( (uintptr_t)pPool->Pages[iMiddle]->Memory <= iValue ) {
			iLeft = iMiddle + 1;
		} else {
			iRight = iMiddle;
		}
	}
	if ( iLeft == 0 ) {
		return NULL;
	}
	pPage = pPool->Pages[iLeft - 1];
	iBase = (uintptr_t)pPage->Memory;
	if ( (iValue - iBase) >= pPage->MemorySize ) {
		return NULL;
	}
	return pPage;
}



/* 从一个尺寸类分配小块并同步全局页索引。 */
static ptr __xrtMemPoolAllocSmall(xmempool* pPool, size_t iClass, bool bZero)
{
	xpool* pClass = &pPool->Buckets[iClass].Pool;
	size_t iOldPageCount = pClass->PageCount;
	xpoolpage* pPage = NULL;
	ptr pMemory;
	size_t iUsable = pClass->ItemSize;

	if (
		(pPool->LiveCount == SIZE_MAX) ||
		(pPool->LiveBytes > (SIZE_MAX - iUsable))
	) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if (
		(pClass->Available == NULL) &&
		!__xrtMemPoolPageReserve(pPool, pPool->PageCount + 1)
	) {
		return NULL;
	}
	pMemory = __xrtPoolAllocObject(pClass, bZero, &pPage);
	if ( pMemory == NULL ) {
		return NULL;
	}
	if ( pClass->PageCount > iOldPageCount ) {
		__xrtMemPoolPageInsert(pPool, pPage);
	}
	pPool->LiveCount++;
	pPool->LiveBytes += pClass->ItemSize;
	pPool->AllocCount++;
	if ( pPool->LiveCount > pPool->PeakCount ) {
		pPool->PeakCount = pPool->LiveCount;
	}
	if ( pPool->LiveBytes > pPool->PeakBytes ) {
		pPool->PeakBytes = pPool->LiveBytes;
	}
	return pMemory;
}



/* 分配并登记一个独立大块。 */
static ptr __xrtMemPoolAllocLarge(
	xmempool* pPool,
	size_t iSize,
	size_t iAlignment,
	bool bZero
)
{
	size_t iAllocationSize;
	ptr pRaw;
	ptr pUser;

	if (
		(pPool->LiveCount == SIZE_MAX) ||
		(pPool->LiveBytes > (SIZE_MAX - iSize))
	) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if (
		!__xrtPoolAlignedAllocationSize(
			iSize,
			iAlignment,
			&iAllocationSize
		)
	) {
		return NULL;
	}
	if ( !__xrtMemPoolLargeReserve(pPool) ) {
		return NULL;
	}
	pRaw = xrtMalloc(iAllocationSize);
	if ( pRaw == NULL ) {
		return NULL;
	}
	if ( !__xrtPoolAlignAfter(pRaw, iAlignment, &pUser) ) {
		xrtFree(pRaw);
		return NULL;
	}
	if ( bZero ) {
		memset(pUser, 0, iSize);
	}
	__xrtMemPoolLargeInsert(pPool, pUser, pRaw, iSize, iAlignment);
	pPool->LiveCount++;
	pPool->LiveBytes += iSize;
	pPool->AllocCount++;
	if ( pPool->LiveCount > pPool->PeakCount ) {
		pPool->PeakCount = pPool->LiveCount;
	}
	if ( pPool->LiveBytes > pPool->PeakBytes ) {
		pPool->PeakBytes = pPool->LiveBytes;
	}
	return pUser;
}



/* 初始化变长池。 */
XRT_API bool xrtMemPoolInit(xmempool* pPool, size_t iCutoff)
{
	size_t iClassCount;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pPool, 0, sizeof(*pPool));
	if ( iCutoff == 0 ) {
		iCutoff = XRT_MEMPOOL_CUTOFF_DEFAULT;
	}
	if ( iCutoff > (SIZE_MAX - (XRT_MEMPOOL_CLASS_STEP - 1)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iClassCount = (iCutoff + (XRT_MEMPOOL_CLASS_STEP - 1)) / XRT_MEMPOOL_CLASS_STEP;
	if ( iClassCount > (SIZE_MAX / sizeof(xmempoolbucket)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pPool->Buckets = (xmempoolbucket*)xrtCalloc(iClassCount, sizeof(xmempoolbucket));
	if ( pPool->Buckets == NULL ) {
		return false;
	}
	for ( size_t i = 0; i < iClassCount; i++ ) {
		size_t iItemSize = (i + 1) * XRT_MEMPOOL_CLASS_STEP;

		if ( !xrtPoolInit(&pPool->Buckets[i].Pool, iItemSize) ) {
			for ( size_t j = 0; j < i; j++ ) {
				xrtPoolUnit(&pPool->Buckets[j].Pool);
			}
			xrtFree(pPool->Buckets);
			memset(pPool, 0, sizeof(*pPool));
			return false;
		}
	}
	pPool->Cutoff = iCutoff;
	pPool->ClassCount = iClassCount;
	pPool->Flags = XRT_MEMPOOL_FLAG_READY;
	return true;
}



/* 创建变长池。 */
XRT_API xmempool* xrtMemPoolCreate(size_t iCutoff)
{
	xmempool* pPool = (xmempool*)xrtMalloc(sizeof(xmempool));

	if ( pPool == NULL ) {
		return NULL;
	}
	if ( !xrtMemPoolInit(pPool, iCutoff) ) {
		xrtFree(pPool);
		return NULL;
	}
	return pPool;
}



/* 释放池持有的全部资源。 */
XRT_API void xrtMemPoolUnit(xmempool* pPool)
{
	if ( pPool == NULL ) {
		return;
	}
	if ( (pPool->Flags & XRT_MEMPOOL_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"memory.unit",
			"memory pool cannot be released during a visit."
		);
		return;
	}
	if ( pPool->Buckets != NULL ) {
		for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
			xrtPoolUnit(&pPool->Buckets[i].Pool);
		}
	}
	for ( size_t i = 0; i < pPool->LargeCapacity; i++ ) {
		if ( pPool->Large[i].State == XRT_MEMPOOL_LARGE_USED ) {
			xrtFree(pPool->Large[i].Raw);
		}
	}
	xrtFree(pPool->Large);
	xrtFree(pPool->Pages);
	xrtFree(pPool->Buckets);
	memset(pPool, 0, sizeof(*pPool));
}



/* 释放池持有的全部资源和池结构。 */
XRT_API void xrtMemPoolDestroy(xmempool* pPool)
{
	if ( pPool == NULL ) {
		return;
	}
	if ( (pPool->Flags & XRT_MEMPOOL_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"memory.destroy",
			"memory pool cannot be destroyed during a visit."
		);
		return;
	}
	xrtMemPoolUnit(pPool);
	xrtFree(pPool);
}



/* 按默认 16 字节对齐分配内存。 */
XRT_API ptr xrtMemPoolAlloc(xmempool* pPool, size_t iSize)
{
	return xrtMemPoolAllocAligned(pPool, iSize, XRT_POOL_ALIGNMENT_DEFAULT);
}



/* 分配并清零数组内存。 */
XRT_API ptr xrtMemPoolCalloc(xmempool* pPool, size_t iCount, size_t iSize)
{
	size_t iTotal;
	ptr pMemory;

	if ( (iCount != 0) && (iSize > (SIZE_MAX / iCount)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = iCount * iSize;
	if ( iTotal == 0 ) {
		iTotal = 1;
	}
	pMemory = xrtMemPoolAlloc(pPool, iTotal);
	if ( pMemory != NULL ) {
		memset(pMemory, 0, iTotal);
	}
	return pMemory;
}



/* 按指定对齐分配内存。 */
XRT_API ptr xrtMemPoolAllocAligned(xmempool* pPool, size_t iSize, size_t iAlignment)
{
	size_t iClass;

	if ( !__xrtMemPoolCanMutate(pPool, "memory.alloc") ) {
		return NULL;
	}
	if ( iSize == 0 ) {
		iSize = 1;
	}
	if ( !__xrtMemPoolAlignmentValid(iAlignment) ) {
		__xrtPoolSetError(
			XERR_ARGUMENT,
			XPOOL_ERROR_INVALID_ALIGNMENT,
			"memory.alloc_aligned",
			"alignment must be a power of two."
		);
		return NULL;
	}
	if ( (iAlignment <= XRT_POOL_ALIGNMENT_DEFAULT) && (iSize <= pPool->Cutoff) ) {
		iClass = (iSize - 1) / XRT_MEMPOOL_CLASS_STEP;
		return __xrtMemPoolAllocSmall(pPool, iClass, false);
	}
	return __xrtMemPoolAllocLarge(pPool, iSize, iAlignment, false);
}



/* 调整池内块大小并保留已有内容。 */
XRT_API ptr xrtMemPoolRealloc(xmempool* pPool, ptr pMemory, size_t iSize)
{
	xpoolpage* pPage;
	xmempoollarge* pLarge;
	size_t iOldSize;
	size_t iAlignment;
	ptr pNew;

	if ( pMemory == NULL ) {
		return xrtMemPoolAlloc(pPool, iSize);
	}
	if ( iSize == 0 ) {
		(void)xrtMemPoolFree(pPool, pMemory);
		return NULL;
	}
	if ( !__xrtMemPoolCanMutate(pPool, "memory.realloc") ) {
		return NULL;
	}
	pPage = __xrtMemPoolFindPage(pPool, pMemory);
	if ( (pPage != NULL) && xrtPoolPageOwns(pPage, pMemory) ) {
		iOldSize = ((xpool*)pPage->Parent)->ItemSize;
		iAlignment = XRT_POOL_ALIGNMENT_DEFAULT;
		if ( iSize <= iOldSize ) {
			return pMemory;
		}
	} else {
		pLarge = __xrtMemPoolLargeFind(pPool, pMemory);
		if ( pLarge == NULL ) {
			__xrtPoolSetError(
				XERR_ARGUMENT,
				XPOOL_ERROR_INVALID_POINTER,
				"memory.realloc",
				"pointer does not belong to this variable-size memory pool."
			);
			return NULL;
		}
		iOldSize = pLarge->Size;
		iAlignment = pLarge->Alignment;
		if ( iSize <= iOldSize ) {
			pLarge->Size = iSize;
			pPool->LiveBytes -= iOldSize - iSize;
			return pMemory;
		}
	}
	pNew = xrtMemPoolAllocAligned(pPool, iSize, iAlignment);
	if ( pNew == NULL ) {
		return NULL;
	}
	memcpy(pNew, pMemory, iOldSize < iSize ? iOldSize : iSize);
	if ( !xrtMemPoolFree(pPool, pMemory) ) {
		(void)xrtMemPoolFree(pPool, pNew);
		return NULL;
	}
	return pNew;
}



/* 安全释放池内活动块。 */
XRT_API bool xrtMemPoolFree(xmempool* pPool, ptr pMemory)
{
	xpoolpage* pPage;
	xmempoollarge* pLarge;
	xpool* pClass;
	size_t iOldPageCount;
	size_t iUsable;

	if ( pMemory == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtMemPoolCanMutate(pPool, "memory.free") ) {
		return false;
	}
	pPage = __xrtMemPoolFindPage(pPool, pMemory);
	if ( pPage != NULL ) {
		pClass = (xpool*)pPage->Parent;
		iOldPageCount = pClass->PageCount;
		iUsable = pClass->ItemSize;
		if ( !xrtPoolFree(pClass, pMemory) ) {
			return false;
		}
		if ( pClass->PageCount != iOldPageCount ) {
			__xrtMemPoolPageRebuild(pPool);
		}
		pPool->LiveCount--;
		pPool->LiveBytes -= iUsable;
		pPool->FreeCount++;
		return true;
	}
	pLarge = __xrtMemPoolLargeFind(pPool, pMemory);
	if ( pLarge != NULL ) {
		__xrtMemPoolLargeRelease(pPool, pLarge);
		return true;
	}
	__xrtPoolSetError(
		XERR_ARGUMENT,
		XPOOL_ERROR_INVALID_POINTER,
		"memory.free",
		"pointer does not belong to this variable-size memory pool."
	);
	return false;
}



/* 返回活动块可安全使用的字节数。 */
XRT_API size_t xrtMemPoolSize(const xmempool* pPool, const void* pMemory)
{
	xpoolpage* pPage;
	xmempoollarge* pLarge;

	if ( (pPool == NULL) || (pMemory == NULL) ) {
		return 0;
	}
	pPage = __xrtMemPoolFindPage(pPool, pMemory);
	if ( (pPage != NULL) && xrtPoolPageOwns(pPage, pMemory) ) {
		return ((xpool*)pPage->Parent)->ItemSize;
	}
	pLarge = __xrtMemPoolLargeFind(pPool, pMemory);
	return pLarge != NULL ? pLarge->Size : 0;
}



/* 判断指针当前是否属于该池的活动块。 */
XRT_API bool xrtMemPoolOwns(const xmempool* pPool, const void* pMemory)
{
	return xrtMemPoolSize(pPool, pMemory) != 0;
}



/* 标记一个活动块为本轮可达。 */
XRT_API bool xrtMemPoolMark(xmempool* pPool, ptr pMemory)
{
	xpoolpage* pPage;
	xmempoollarge* pLarge;

	if (
		(pPool == NULL) ||
		((pPool->Flags & XRT_MEMPOOL_FLAG_READY) == 0) ||
		(pMemory == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pPage = __xrtMemPoolFindPage(pPool, pMemory);
	if ( pPage != NULL ) {
		return xrtPoolPageMark(pPage, pMemory);
	}
	pLarge = __xrtMemPoolLargeFind(pPool, pMemory);
	if ( pLarge != NULL ) {
		pLarge->Marked = 1;
		return true;
	}
	__xrtPoolSetError(
		XERR_ARGUMENT,
		XPOOL_ERROR_INVALID_POINTER,
		"memory.mark",
		"pointer does not belong to this variable-size memory pool."
	);
	return false;
}



/* 释放全部未标记块，并清除幸存块标记。 */
XRT_API size_t xrtMemPoolSweep(xmempool* pPool)
{
	size_t iFreed = 0;
	size_t iSmallFreed = 0;

	if ( !__xrtMemPoolCanMutate(pPool, "memory.sweep") ) {
		return 0;
	}
	for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
		xpool* pClass = &pPool->Buckets[i].Pool;
		size_t iClassFreed = xrtPoolSweep(pClass);

		iSmallFreed += iClassFreed;
		iFreed += iClassFreed;
		pPool->LiveBytes -= iClassFreed * pClass->ItemSize;
	}
	__xrtMemPoolPageRebuild(pPool);
	for ( size_t i = 0; i < pPool->LargeCapacity; i++ ) {
		xmempoollarge* pEntry = &pPool->Large[i];

		if ( pEntry->State != XRT_MEMPOOL_LARGE_USED ) {
			continue;
		}
		if ( pEntry->Marked != 0 ) {
			pEntry->Marked = 0;
		} else {
			__xrtMemPoolLargeRelease(pPool, pEntry);
			iFreed++;
		}
	}
	pPool->LiveCount -= iSmallFreed;
	pPool->FreeCount += iSmallFreed;
	if ( (pPool->LargeCount == 0) && (pPool->LargeDeleted != 0) ) {
		memset(pPool->Large, 0, pPool->LargeCapacity * sizeof(xmempoollarge));
		pPool->LargeDeleted = 0;
	}
	return iFreed;
}



/* 释放全部已标记块。 */
XRT_API size_t xrtMemPoolFreeMarked(xmempool* pPool)
{
	size_t iFreed = 0;
	size_t iSmallFreed = 0;

	if ( !__xrtMemPoolCanMutate(pPool, "memory.free_marked") ) {
		return 0;
	}
	for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
		xpool* pClass = &pPool->Buckets[i].Pool;
		size_t iClassFreed = xrtPoolFreeMarked(pClass);

		iSmallFreed += iClassFreed;
		iFreed += iClassFreed;
		pPool->LiveBytes -= iClassFreed * pClass->ItemSize;
	}
	__xrtMemPoolPageRebuild(pPool);
	for ( size_t i = 0; i < pPool->LargeCapacity; i++ ) {
		xmempoollarge* pEntry = &pPool->Large[i];

		if (
			(pEntry->State == XRT_MEMPOOL_LARGE_USED) &&
			(pEntry->Marked != 0)
		) {
			__xrtMemPoolLargeRelease(pPool, pEntry);
			iFreed++;
		}
	}
	pPool->LiveCount -= iSmallFreed;
	pPool->FreeCount += iSmallFreed;
	if ( (pPool->LargeCount == 0) && (pPool->LargeDeleted != 0) ) {
		memset(pPool->Large, 0, pPool->LargeCapacity * sizeof(xmempoollarge));
		pPool->LargeDeleted = 0;
	}
	return iFreed;
}



/* 释放全部活动块，并保留每个已使用尺寸类的一个空页。 */
XRT_API size_t xrtMemPoolReset(xmempool* pPool)
{
	size_t iFreed = 0;
	size_t iSmallFreed = 0;

	if ( !__xrtMemPoolCanMutate(pPool, "memory.reset") ) {
		return 0;
	}
	for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
		xpool* pClass = &pPool->Buckets[i].Pool;
		size_t iClassFreed = xrtPoolReset(pClass);

		iSmallFreed += iClassFreed;
		iFreed += iClassFreed;
		pPool->LiveBytes -= iClassFreed * pClass->ItemSize;
	}
	__xrtMemPoolPageRebuild(pPool);
	for ( size_t i = 0; i < pPool->LargeCapacity; i++ ) {
		if ( pPool->Large[i].State == XRT_MEMPOOL_LARGE_USED ) {
			__xrtMemPoolLargeRelease(pPool, &pPool->Large[i]);
			iFreed++;
		}
	}
	if ( pPool->LargeCapacity != 0 ) {
		memset(pPool->Large, 0, pPool->LargeCapacity * sizeof(xmempoollarge));
		pPool->LargeDeleted = 0;
	}
	pPool->LiveCount -= iSmallFreed;
	pPool->FreeCount += iSmallFreed;
	return iFreed;
}



/* 将每个尺寸类的空页裁剪到指定数量。 */
XRT_API size_t xrtMemPoolTrim(xmempool* pPool, size_t iRetainEmptyPerClass)
{
	size_t iFreed = 0;

	if ( !__xrtMemPoolCanMutate(pPool, "memory.trim") ) {
		return 0;
	}
	for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
		iFreed += xrtPoolTrim(&pPool->Buckets[i].Pool, iRetainEmptyPerClass);
	}
	__xrtMemPoolPageRebuild(pPool);
	return iFreed;
}



/* 获取变长池当前状态。 */
XRT_API void xrtMemPoolGet(const xmempool* pPool, xmempoolinfo* pInfo)
{
	if ( pInfo == NULL ) {
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
	if ( (pPool == NULL) || ((pPool->Flags & XRT_MEMPOOL_FLAG_READY) == 0) ) {
		return;
	}
	pInfo->Cutoff = pPool->Cutoff;
	pInfo->ClassStep = XRT_MEMPOOL_CLASS_STEP;
	pInfo->ClassCount = pPool->ClassCount;
	pInfo->PageCount = pPool->PageCount;
	pInfo->SmallCount = pPool->LiveCount >= pPool->LargeCount ?
		pPool->LiveCount - pPool->LargeCount : 0;
	pInfo->LargeCount = pPool->LargeCount;
	pInfo->LiveCount = pPool->LiveCount;
	pInfo->PeakCount = pPool->PeakCount;
	pInfo->LiveBytes = pPool->LiveBytes;
	pInfo->PeakBytes = pPool->PeakBytes;
	pInfo->AllocCount = pPool->AllocCount;
	pInfo->FreeCount = pPool->FreeCount;
}



/* 访问当前活动块，并阻止回调改变池的分配集合。 */
XRT_API size_t xrtMemPoolVisit(
	xmempool* pPool,
	xmempoolvisitor pVisitor,
	ptr pUserData
)
{
	size_t iVisited = 0;

	if (
		(pPool == NULL) ||
		((pPool->Flags & XRT_MEMPOOL_FLAG_READY) == 0) ||
		(pVisitor == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( (pPool->Flags & XRT_MEMPOOL_FLAG_VISITING) != 0 ) {
		__xrtPoolSetError(
			XERR_STATE,
			XPOOL_ERROR_VISIT_ACTIVE,
			"memory.visit",
			"memory pool is already being visited."
		);
		return 0;
	}
	__xrtMemPoolSetVisiting(pPool, true);

	/* 小块按尺寸类和页顺序访问。 */
	for ( size_t i = 0; i < pPool->ClassCount; i++ ) {
		const xpool* pClass = &pPool->Buckets[i].Pool;

		for ( xpoolpage* pPage = pClass->Pages; pPage != NULL; pPage = pPage->Next ) {
			for ( size_t j = 0; j < pPage->NextIndex; j++ ) {
				ptr pMemory = xrtPoolPageGet(pPage, j);

				if ( pMemory == NULL ) {
					continue;
				}
				iVisited++;
				if ( !pVisitor(
					pMemory,
					pClass->ItemSize,
					pClass->Alignment,
					pUserData
				) ) {
					__xrtMemPoolSetVisiting(pPool, false);
					return iVisited;
				}
			}
		}
	}

	/* 大块直接从登记表访问，不暴露内部表位置。 */
	for ( size_t i = 0; i < pPool->LargeCapacity; i++ ) {
		const xmempoollarge* pEntry = &pPool->Large[i];

		if ( pEntry->State != XRT_MEMPOOL_LARGE_USED ) {
			continue;
		}
		iVisited++;
		if ( !pVisitor(
			pEntry->User,
			pEntry->Size,
			pEntry->Alignment,
			pUserData
		) ) {
			__xrtMemPoolSetVisiting(pPool, false);
			return iVisited;
		}
	}
	__xrtMemPoolSetVisiting(pPool, false);
	return iVisited;
}

#endif
