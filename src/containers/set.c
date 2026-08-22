#include <xrt/set.h>
#include <xrt/hash.h>

#include "../internal/xrt_internal.h"
#include "../internal/xrt_set.h"



#if defined(XRT_FEATURE_SET)

#define XRT_SET_FLAG_READY    0x0001u
#define XRT_SET_FLAG_HEAP     0x0002u
#define XRT_SET_FLAG_BUSY     0x0004u
#define XRT_SET_FLAG_VISITING 0x0008u
#define XRT_SET_FLAGS         0x000Fu



/* 独立条目同时参加哈希桶链和稳定插入顺序链。 */
struct xsetentry {
	xsetentry* BucketNext;
	xsetentry* OrderPrev;
	xsetentry* OrderNext;
	ptr Allocation;
	uint64 Hash;
};



/* 默认策略按完整元素字节计算稳定哈希。 */
static uint64 __xrtSetDefaultHash(const void* pItem, ptr pUserData)
{
	const xset* pSet = (const xset*)pUserData;

	return xrtHash64(pItem, pSet->ItemSize);
}



/* 默认策略按完整元素字节判断相等。 */
static bool __xrtSetDefaultEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	const xset* pSet = (const xset*)pUserData;

	return memcmp(pLeft, pRight, pSet->ItemSize) == 0;
}



/* 默认复制器复制完整元素字节。 */
static bool __xrtSetDefaultCopy(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	const xset* pSet = (const xset*)pUserData;

	memcpy(pTarget, pSource, pSet->ItemSize);
	return true;
}



/* 检查集合布局、桶数组和回调合同是否一致。 */
bool __xrtSetValid(const xset* pSet)
{
	if ( pSet == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pSet->Flags & XRT_SET_FLAG_READY) == 0) ||
		((pSet->Flags & ~XRT_SET_FLAGS) != 0) ||
		(pSet->ItemSize == 0) ||
		(pSet->ItemOffset < sizeof(xsetentry)) ||
		(pSet->Alignment == 0) ||
		((pSet->Alignment & (pSet->Alignment - 1u)) != 0) ||
		((pSet->ItemOffset & (pSet->Alignment - 1u)) != 0) ||
		(pSet->ItemSize > (SIZE_MAX - pSet->ItemOffset)) ||
		(pSet->Hash == NULL) ||
		(pSet->Equal == NULL) ||
		(pSet->Copy == NULL) ||
		(pSet->Version == 0) ||
		((pSet->Hash == __xrtSetDefaultHash) !=
			(pSet->Equal == __xrtSetDefaultEqual)) ||
		(
			(pSet->Hash == __xrtSetDefaultHash) &&
			(pSet->KeyUserData != pSet)
		) ||
		(
			(pSet->Copy == __xrtSetDefaultCopy) &&
			(
				(pSet->Drop != NULL) ||
				(pSet->LifecycleUserData != pSet)
			)
		) ||
		(
			(pSet->Copy != __xrtSetDefaultCopy) &&
			(pSet->Drop == NULL)
		)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pSet->BucketCount == 0 ) {
		if (
			(pSet->Buckets != NULL) ||
			(pSet->Threshold != 0) ||
			(pSet->Count != 0)
		) {
			__xrtErrorSetInvalidState();
			return false;
		}
	} else if (
		(pSet->Buckets == NULL) ||
		(pSet->BucketCount < XRT_SET_BUCKETS_MIN) ||
		((pSet->BucketCount & (pSet->BucketCount - 1u)) != 0) ||
		(pSet->BucketCount > (SIZE_MAX / sizeof(xsetentry*))) ||
		(pSet->Threshold !=
			(pSet->BucketCount - (pSet->BucketCount >> 2u))) ||
		(pSet->Count > pSet->Threshold)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if (
		((pSet->Count == 0) &&
			((pSet->First != NULL) || (pSet->Last != NULL))) ||
		((pSet->Count != 0) &&
			((pSet->First == NULL) || (pSet->Last == NULL)))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 检查集合是否允许查询或推进外置迭代器。 */
bool __xrtSetCanRead(const xset* pSet)
{
	if ( !__xrtSetValid(pSet) ) {
		return false;
	}
	if ( (pSet->Flags & XRT_SET_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 检查集合是否允许改变元素、容量、策略或生命周期。 */
bool __xrtSetCanMutate(xset* pSet)
{
	if ( !__xrtSetCanRead(pSet) ) {
		return false;
	}
	if ( (pSet->Flags & XRT_SET_FLAG_VISITING) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 调用自定义哈希器期间拒绝同一集合的 API 重入。 */
static uint64 __xrtSetHashValue(xset* pSet, const void* pItem)
{
	uint64 iHash;

	if ( pSet->Hash == __xrtSetDefaultHash ) {
		return __xrtSetDefaultHash(pItem, pSet);
	}
	pSet->Flags |= XRT_SET_FLAG_BUSY;
	iHash = pSet->Hash(pItem, pSet->KeyUserData);
	pSet->Flags &= ~XRT_SET_FLAG_BUSY;
	return iHash;
}



/* 调用自定义相等器期间拒绝同一集合的 API 重入。 */
static bool __xrtSetItemsEqual(
	xset* pSet,
	const void* pLeft,
	const void* pRight
)
{
	bool bEqual;

	if ( pSet->Equal == __xrtSetDefaultEqual ) {
		return __xrtSetDefaultEqual(pLeft, pRight, pSet);
	}
	pSet->Flags |= XRT_SET_FLAG_BUSY;
	bEqual = pSet->Equal(pLeft, pRight, pSet->KeyUserData);
	pSet->Flags &= ~XRT_SET_FLAG_BUSY;
	return bEqual;
}



/* 调用自定义复制器期间拒绝同一集合的 API 重入。 */
static bool __xrtSetCopyItem(
	xset* pSet,
	ptr pTarget,
	const void* pSource
)
{
	bool bCopied;

	if ( pSet->Copy == __xrtSetDefaultCopy ) {
		return __xrtSetDefaultCopy(pTarget, pSource, pSet);
	}
	pSet->Flags |= XRT_SET_FLAG_BUSY;
	bCopied = pSet->Copy(
		pTarget,
		pSource,
		pSet->LifecycleUserData
	);
	pSet->Flags &= ~XRT_SET_FLAG_BUSY;
	return bCopied;
}



/* 调用元素释放器期间拒绝同一集合的全部 API 重入。 */
static void __xrtSetDropItem(xset* pSet, ptr pItem)
{
	if ( pSet->Drop == NULL ) {
		return;
	}

	pSet->Flags |= XRT_SET_FLAG_BUSY;
	pSet->Drop(pItem, pSet->LifecycleUserData);
	pSet->Flags &= ~XRT_SET_FLAG_BUSY;
}



/* 判断两个非空地址区间是否相交，不计算可能溢出的末地址。 */
static bool __xrtSetRangeOverlaps(
	const void* pLeft,
	size_t iLeftSize,
	const void* pRight,
	size_t iRightSize
)
{
	uintptr_t iLeft = (uintptr_t)pLeft;
	uintptr_t iRight = (uintptr_t)pRight;

	if ( (iLeftSize == 0) || (iRightSize == 0) ) {
		return false;
	}
	if ( iLeft <= iRight ) {
		return (iRight - iLeft) < iLeftSize;
	}
	return (iLeft - iRight) < iRightSize;
}



/* 判断完整区间是否触及集合结构或桶数组。 */
static bool __xrtSetOwnsCoreRange(
	const xset* pSet,
	const void* pMemory,
	size_t iSize
)
{
	size_t iBucketBytes;

	if ( __xrtSetRangeOverlaps(pMemory, iSize, pSet, sizeof(xset)) ) {
		return true;
	}
	iBucketBytes = pSet->BucketCount * sizeof(xsetentry*);
	return
		(pSet->Buckets != NULL) &&
		__xrtSetRangeOverlaps(
			pMemory,
			iSize,
			pSet->Buckets,
			iBucketBytes
		);
}



/* 判断完整区间是否触及指定条目的原始分配。 */
static bool __xrtSetEntryOwnsRange(
	const xset* pSet,
	const xsetentry* pEntry,
	const void* pMemory,
	size_t iSize
)
{
	size_t iExtra = pSet->Alignment > XRT_SET_ALIGNMENT_DEFAULT ?
		pSet->Alignment - 1u : 0;
	size_t iEntryBytes;

	if (
		(pEntry->Allocation == NULL) ||
		((pSet->ItemOffset + pSet->ItemSize) > (SIZE_MAX - iExtra))
	) {
		__xrtErrorSetInvalidState();
		return true;
	}
	iEntryBytes = pSet->ItemOffset + pSet->ItemSize + iExtra;
	return __xrtSetRangeOverlaps(
		pMemory,
		iSize,
		pEntry->Allocation,
		iEntryBytes
	);
}



/* 判断完整区间是否触及集合拥有的任意内存。 */
bool __xrtSetOwnsRange(
	const xset* pSet,
	const void* pMemory,
	size_t iSize
)
{
	xsetentry* pEntry;

	if ( __xrtSetOwnsCoreRange(pSet, pMemory, iSize) ) {
		return true;
	}
	for ( pEntry = pSet->First; pEntry != NULL; pEntry = pEntry->OrderNext ) {
		if ( __xrtSetEntryOwnsRange(pSet, pEntry, pMemory, iSize) ) {
			return true;
		}
	}

	return false;
}



/* 返回条目中的对齐元素地址。 */
static ptr __xrtSetItem(const xset* pSet, const xsetentry* pEntry)
{
	return pEntry != NULL ? (bytes)pEntry + pSet->ItemOffset : NULL;
}



/* 计算桶数量在 75% 负载下对应的可用元素容量。 */
static size_t __xrtSetThreshold(size_t iBucketCount)
{
	return iBucketCount - (iBucketCount >> 2u);
}



/* 计算容纳目标元素数需要的二次幂桶数量。 */
static bool __xrtSetBucketCount(size_t iCapacity, size_t* pBucketCount)
{
	size_t iBuckets = XRT_SET_BUCKETS_MIN;

	if ( iCapacity == 0 ) {
		*pBucketCount = 0;
		return true;
	}
	while ( __xrtSetThreshold(iBuckets) < iCapacity ) {
		if ( iBuckets > (SIZE_MAX >> 1u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iBuckets <<= 1u;
	}
	if ( iBuckets > (SIZE_MAX / sizeof(xsetentry*)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	*pBucketCount = iBuckets;
	return true;
}



/* 分配并清零桶数组。 */
static xsetentry** __xrtSetAllocBuckets(size_t iBucketCount)
{
	if ( iBucketCount == 0 ) {
		return NULL;
	}

	return (xsetentry**)xrtCalloc(iBucketCount, sizeof(xsetentry*));
}



/* 仅重建桶链，不移动条目和元素。 */
static void __xrtSetRehash(
	const xset* pSet,
	xsetentry** pBuckets,
	size_t iBucketCount
)
{
	xsetentry* pEntry = pSet->First;

	while ( pEntry != NULL ) {
		size_t iBucket = (size_t)pEntry->Hash & (iBucketCount - 1u);

		pEntry->BucketNext = pBuckets[iBucket];
		pBuckets[iBucket] = pEntry;
		pEntry = pEntry->OrderNext;
	}
}



/* 为一次插入预先取得可能需要的新桶数组。 */
static bool __xrtSetPrepareInsert(
	const xset* pSet,
	xsetentry*** ppBuckets,
	size_t* pBucketCount
)
{
	size_t iNeed;

	*ppBuckets = NULL;
	*pBucketCount = 0;
	if ( pSet->Count < pSet->Threshold ) {
		return true;
	}
	if ( pSet->Count == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNeed = pSet->Count + 1u;
	if ( !__xrtSetBucketCount(iNeed, pBucketCount) ) {
		return false;
	}
	if ( *pBucketCount <= pSet->BucketCount ) {
		return true;
	}
	*ppBuckets = __xrtSetAllocBuckets(*pBucketCount);
	return *ppBuckets != NULL;
}



/* 分配一个对齐条目并按生命周期复制器建立元素。 */
static xsetentry* __xrtSetAllocEntry(
	const xset* pSet,
	const void* pItem,
	uint64 iHash
)
{
	size_t iExtra = pSet->Alignment > XRT_SET_ALIGNMENT_DEFAULT ?
		pSet->Alignment - 1u : 0;
	size_t iBytes;
	ptr pAllocation;
	uintptr_t iAddress;
	xsetentry* pEntry;
	ptr pStored;

	if ( pSet->ItemSize > (SIZE_MAX - pSet->ItemOffset) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBytes = pSet->ItemOffset + pSet->ItemSize;
	if ( iBytes > (SIZE_MAX - iExtra) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pAllocation = xrtMalloc(iBytes + iExtra);
	if ( pAllocation == NULL ) {
		return NULL;
	}
	if ( (uintptr_t)pAllocation > (UINTPTR_MAX - iExtra) ) {
		xrtFree(pAllocation);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAddress = ((uintptr_t)pAllocation + iExtra) &
		~((uintptr_t)pSet->Alignment - 1u);
	pEntry = (xsetentry*)iAddress;
	pEntry->BucketNext = NULL;
	pEntry->OrderPrev = NULL;
	pEntry->OrderNext = NULL;
	pEntry->Allocation = pAllocation;
	pEntry->Hash = iHash;
	pStored = __xrtSetItem(pSet, pEntry);
	memset(pStored, 0, pSet->ItemSize);
	if ( !__xrtSetCopyItem((xset*)pSet, pStored, pItem) ) {
		xrtFree(pAllocation);
		return NULL;
	}
	if (
		(pSet->Copy != __xrtSetDefaultCopy) &&
		(
			(__xrtSetHashValue((xset*)pSet, pStored) != iHash) ||
			!__xrtSetItemsEqual((xset*)pSet, pItem, pStored)
		)
	) {
		__xrtSetDropItem((xset*)pSet, pStored);
		xrtFree(pAllocation);
		__xrtErrorSetInvalidState();
		return NULL;
	}

	return pEntry;
}



/* 计算哈希并在桶链中查找等价元素。 */
static xsetentry* __xrtSetFind(
	const xset* pSet,
	const void* pItem,
	uint64* pHash,
	xsetentry*** ppLink
)
{
	xset* pMutable = (xset*)pSet;
	uint64 iHash = __xrtSetHashValue(pMutable, pItem);
	xsetentry** pLink;
	xsetentry* pEntry;

	if ( pHash != NULL ) {
		*pHash = iHash;
	}
	if ( ppLink != NULL ) {
		*ppLink = NULL;
	}
	if ( pSet->BucketCount == 0 ) {
		return NULL;
	}
	pLink = &((xset*)pSet)->Buckets[(size_t)iHash & (pSet->BucketCount - 1u)];
	pEntry = *pLink;
	while ( pEntry != NULL ) {
		if (
			(pEntry->Hash == iHash) &&
			__xrtSetItemsEqual(
				pMutable,
				pItem,
				__xrtSetItem(pSet, pEntry)
			)
		) {
			if ( ppLink != NULL ) {
				*ppLink = pLink;
			}
			return pEntry;
		}
		pLink = &pEntry->BucketNext;
		pEntry = *pLink;
	}

	return NULL;
}



/* 递增结构版本，并跳过保留的零值。 */
static void __xrtSetChange(xset* pSet)
{
	pSet->Version++;
	if ( pSet->Version == 0 ) {
		pSet->Version = 1;
	}
}



/* 在多步只读操作期间阻止回调修改来源集合。 */
static bool __xrtSetProtectRead(const xset* pSet, bool* pAcquired)
{
	xset* pMutable = (xset*)pSet;

	*pAcquired = false;
	if ( !__xrtSetCanRead(pSet) ) {
		return false;
	}
	if ( (pSet->Flags & XRT_SET_FLAG_VISITING) == 0 ) {
		pMutable->Flags |= XRT_SET_FLAG_VISITING;
		*pAcquired = true;
	}
	return true;
}



/* 结束由当前多步只读操作取得的来源保护。 */
static void __xrtSetUnprotectRead(const xset* pSet, bool bAcquired)
{
	if ( bAcquired ) {
		((xset*)pSet)->Flags &= ~XRT_SET_FLAG_VISITING;
	}
}



/* 在用户回调期间拒绝当前集合的全部 API 重入。 */
bool __xrtSetCallbackBegin(const xset* pSet)
{
	if ( !__xrtSetCanRead(pSet) ) {
		return false;
	}
	((xset*)pSet)->Flags |= XRT_SET_FLAG_BUSY;
	return true;
}



/* 结束当前集合的用户回调门禁。 */
void __xrtSetCallbackEnd(const xset* pSet)
{
	if ( pSet != NULL ) {
		((xset*)pSet)->Flags &= ~XRT_SET_FLAG_BUSY;
	}
}



/* 把新条目接入桶链和插入顺序尾部。 */
static void __xrtSetLink(xset* pSet, xsetentry* pEntry)
{
	size_t iBucket = (size_t)pEntry->Hash & (pSet->BucketCount - 1u);

	pEntry->BucketNext = pSet->Buckets[iBucket];
	pSet->Buckets[iBucket] = pEntry;
	pEntry->OrderPrev = pSet->Last;
	if ( pSet->Last != NULL ) {
		pSet->Last->OrderNext = pEntry;
	} else {
		pSet->First = pEntry;
	}
	pSet->Last = pEntry;
	pSet->Count++;
	__xrtSetChange(pSet);
}



/* 返回已有条目，或失败原子地创建并提交一个新条目。 */
static xsetentry* __xrtSetGetOrAddEntry(
	xset* pSet,
	const void* pItem,
	bool* pNew
)
{
	xsetentry** pNewBuckets;
	xsetentry* pEntry;
	uint64 iHash;
	size_t iNewBucketCount;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	pEntry = __xrtSetFind(pSet, pItem, &iHash, NULL);
	if ( pEntry != NULL ) {
		return pEntry;
	}
	if ( !__xrtSetPrepareInsert(pSet, &pNewBuckets, &iNewBucketCount) ) {
		return NULL;
	}
	pEntry = __xrtSetAllocEntry(pSet, pItem, iHash);
	if ( pEntry == NULL ) {
		xrtFree(pNewBuckets);
		return NULL;
	}
	if ( pNewBuckets != NULL ) {
		__xrtSetRehash(pSet, pNewBuckets, iNewBucketCount);
		xrtFree(pSet->Buckets);
		pSet->Buckets = pNewBuckets;
		pSet->BucketCount = iNewBucketCount;
		pSet->Threshold = __xrtSetThreshold(iNewBucketCount);
	}
	__xrtSetLink(pSet, pEntry);
	if ( pNew != NULL ) {
		*pNew = true;
	}

	return pEntry;
}



/* 从桶链和插入顺序链中摘除条目。 */
static void __xrtSetUnlink(
	xset* pSet,
	xsetentry* pEntry,
	xsetentry** pLink
)
{
	*pLink = pEntry->BucketNext;
	if ( pEntry->OrderPrev != NULL ) {
		pEntry->OrderPrev->OrderNext = pEntry->OrderNext;
	} else {
		pSet->First = pEntry->OrderNext;
	}
	if ( pEntry->OrderNext != NULL ) {
		pEntry->OrderNext->OrderPrev = pEntry->OrderPrev;
	} else {
		pSet->Last = pEntry->OrderPrev;
	}
	pSet->Count--;
	__xrtSetChange(pSet);
}



/* 释放全部条目并按需调用元素释放器。 */
static void __xrtSetReleaseAll(xset* pSet)
{
	xsetentry* pEntry = pSet->First;

	while ( pEntry != NULL ) {
		xsetentry* pNext = pEntry->OrderNext;

		__xrtSetDropItem(pSet, __xrtSetItem(pSet, pEntry));
		xrtFree(pEntry->Allocation);
		pEntry = pNext;
	}
}



/* 按指定对齐建立固定大小元素布局。 */
static bool __xrtSetInit(xset* pSet, size_t iItemSize, size_t iAlignment)
{
	size_t iItemOffset;

	if (
		(pSet == NULL) ||
		(iItemSize == 0) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sizeof(xsetentry) > (SIZE_MAX - (iAlignment - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iItemOffset = (sizeof(xsetentry) + (iAlignment - 1u)) &
		~(iAlignment - 1u);
	if ( iItemSize > (SIZE_MAX - iItemOffset) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	memset(pSet, 0, sizeof(xset));
	pSet->ItemSize = iItemSize;
	pSet->ItemOffset = iItemOffset;
	pSet->Alignment = iAlignment;
	pSet->Version = 1;
	pSet->Hash = __xrtSetDefaultHash;
	pSet->Equal = __xrtSetDefaultEqual;
	pSet->Copy = __xrtSetDefaultCopy;
	pSet->KeyUserData = pSet;
	pSet->LifecycleUserData = pSet;
	pSet->Flags = XRT_SET_FLAG_READY;
	return true;
}



/* 判断两个集合是否可以安全交换和复制元素。 */
static bool __xrtSetCompatible(const xset* pLeft, const xset* pRight)
{
	return
		(pLeft->ItemSize == pRight->ItemSize) &&
		(pLeft->Alignment == pRight->Alignment) &&
		(pLeft->Hash == pRight->Hash) &&
		(pLeft->Equal == pRight->Equal) &&
		(pLeft->Copy == pRight->Copy) &&
		(pLeft->Drop == pRight->Drop) &&
		(
			(pLeft->KeyUserData == pRight->KeyUserData) ||
			(
				(pLeft->Hash == __xrtSetDefaultHash) &&
				(pRight->Hash == __xrtSetDefaultHash)
			)
		) &&
		(
			(pLeft->LifecycleUserData == pRight->LifecycleUserData) ||
			(
				(pLeft->Copy == __xrtSetDefaultCopy) &&
				(pRight->Copy == __xrtSetDefaultCopy)
			)
		);
}



/* 创建一个继承源集合策略但仍为空的新集合。 */
static xset* __xrtSetCreateLike(const xset* pSource)
{
	xset* pResult = xrtSetCreateAligned(
		pSource->ItemSize,
		pSource->Alignment
	);

	if ( pResult == NULL ) {
		return NULL;
	}
	if ( pSource->Hash != __xrtSetDefaultHash ) {
		if ( !xrtSetSetKeyPolicy(
			pResult,
			pSource->Hash,
			pSource->Equal,
			pSource->KeyUserData
		) ) {
			xrtSetDestroy(pResult);
			return NULL;
		}
	}
	if ( pSource->Copy != __xrtSetDefaultCopy ) {
		if ( !xrtSetSetLifecycle(
			pResult,
			pSource->Copy,
			pSource->Drop,
			pSource->LifecycleUserData
		) ) {
			xrtSetDestroy(pResult);
			return NULL;
		}
	}

	return pResult;
}



/* 把受保护来源的元素加入临时目标，失败时由调用方丢弃目标。 */
static bool __xrtSetMergeInto(xset* pTarget, const xset* pSource)
{
	xsetentry* pEntry = pSource->First;

	while ( pEntry != NULL ) {
		if ( !xrtSetAdd(pTarget, __xrtSetItem(pSource, pEntry)) ) {
			return false;
		}
		pEntry = pEntry->OrderNext;
	}

	return true;
}



/* 释放尚未提交到集合的条目链。 */
static void __xrtSetReleaseStaged(xset* pSet, xsetentry* pFirst)
{
	xsetentry* pEntry = pFirst;

	while ( pEntry != NULL ) {
		xsetentry* pNext = pEntry->OrderNext;

		__xrtSetDropItem(pSet, __xrtSetItem(pSet, pEntry));
		xrtFree(pEntry->Allocation);
		pEntry = pNext;
	}
}



/* 预先建立全部缺失条目和桶数组，成功后一次提交到目标。 */
static bool __xrtSetMergeAtomic(xset* pTarget, const xset* pSource)
{
	xsetentry* pStagedFirst = NULL;
	xsetentry* pStagedLast = NULL;
	xsetentry* pSourceEntry;
	xsetentry** pBuckets = NULL;
	size_t iAdded = 0;
	size_t iFinalCount;
	size_t iBucketCount;

	pSourceEntry = pSource->First;
	while ( pSourceEntry != NULL ) {
		const void* pItem = __xrtSetItem(pSource, pSourceEntry);
		xsetentry* pEntry;
		uint64 iHash;

		if ( __xrtSetFind(pTarget, pItem, &iHash, NULL) == NULL ) {
			pEntry = __xrtSetAllocEntry(pTarget, pItem, iHash);
			if ( pEntry == NULL ) {
				__xrtSetReleaseStaged(pTarget, pStagedFirst);
				return false;
			}
			if ( pStagedLast != NULL ) {
				pStagedLast->OrderNext = pEntry;
			} else {
				pStagedFirst = pEntry;
			}
			pStagedLast = pEntry;
			iAdded++;
		}
		pSourceEntry = pSourceEntry->OrderNext;
	}
	if ( iAdded == 0 ) {
		return true;
	}

	iFinalCount = pTarget->Count + iAdded;
	if ( !__xrtSetBucketCount(iFinalCount, &iBucketCount) ) {
		__xrtSetReleaseStaged(pTarget, pStagedFirst);
		return false;
	}
	if ( iBucketCount > pTarget->BucketCount ) {
		pBuckets = __xrtSetAllocBuckets(iBucketCount);
		if ( pBuckets == NULL ) {
			__xrtSetReleaseStaged(pTarget, pStagedFirst);
			return false;
		}
	}

	/* 从这里开始没有失败路径，旧条目地址和相对顺序保持不变。 */
	if ( pBuckets != NULL ) {
		__xrtSetRehash(pTarget, pBuckets, iBucketCount);
		xrtFree(pTarget->Buckets);
		pTarget->Buckets = pBuckets;
		pTarget->BucketCount = iBucketCount;
		pTarget->Threshold = __xrtSetThreshold(iBucketCount);
	}
	while ( pStagedFirst != NULL ) {
		xsetentry* pEntry = pStagedFirst;

		pStagedFirst = pEntry->OrderNext;
		pEntry->OrderNext = NULL;
		__xrtSetLink(pTarget, pEntry);
	}
	return true;
}



/* 使用默认 16 字节对齐初始化空集合。 */
XRT_API bool xrtSetInit(xset* pSet, size_t iItemSize)
{
	return __xrtSetInit(pSet, iItemSize, XRT_SET_ALIGNMENT_DEFAULT);
}



/* 使用显式元素对齐初始化空集合。 */
XRT_API bool xrtSetInitAligned(
	xset* pSet,
	size_t iItemSize,
	size_t iAlignment
)
{
	return __xrtSetInit(pSet, iItemSize, iAlignment);
}



/* 创建使用默认 16 字节对齐的空集合。 */
XRT_API xset* xrtSetCreate(size_t iItemSize)
{
	return xrtSetCreateAligned(iItemSize, XRT_SET_ALIGNMENT_DEFAULT);
}



/* 创建使用显式元素对齐的空集合。 */
XRT_API xset* xrtSetCreateAligned(size_t iItemSize, size_t iAlignment)
{
	xset* pSet = (xset*)xrtMalloc(sizeof(xset));

	if ( pSet == NULL ) {
		return NULL;
	}
	if ( !xrtSetInitAligned(pSet, iItemSize, iAlignment) ) {
		xrtFree(pSet);
		return NULL;
	}
	pSet->Flags |= XRT_SET_FLAG_HEAP;
	return pSet;
}



/* 为仍为空的集合设置成对的自定义哈希器和相等器。 */
XRT_API bool xrtSetSetKeyPolicy(
	xset* pSet,
	xsethash pHash,
	xsetequal pEqual,
	ptr pUserData
)
{
	if ( !__xrtSetCanMutate(pSet) ) {
		return false;
	}
	if ( pSet->Count != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pHash == NULL) != (pEqual == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	pSet->Hash = pHash != NULL ? pHash : __xrtSetDefaultHash;
	pSet->Equal = pEqual != NULL ? pEqual : __xrtSetDefaultEqual;
	pSet->KeyUserData = pHash != NULL ? pUserData : pSet;
	return true;
}



/* 为仍为空的集合设置成对的资源复制器和释放器。 */
XRT_API bool xrtSetSetLifecycle(
	xset* pSet,
	xsetcopy pCopy,
	xsetdrop pDrop,
	ptr pUserData
)
{
	if ( !__xrtSetCanMutate(pSet) ) {
		return false;
	}
	if ( pSet->Count != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pCopy == NULL) != (pDrop == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	pSet->Copy = pCopy != NULL ? pCopy : __xrtSetDefaultCopy;
	pSet->Drop = pDrop;
	pSet->LifecycleUserData = pCopy != NULL ? pUserData : pSet;
	return true;
}



/* 释放全部元素和桶数组，但不释放集合结构。 */
XRT_API void xrtSetUnit(xset* pSet)
{
	if ( pSet == NULL ) {
		return;
	}
	if ( !__xrtSetCanMutate(pSet) ) {
		return;
	}

	__xrtSetReleaseAll(pSet);
	xrtFree(pSet->Buckets);
	memset(pSet, 0, sizeof(xset));
}



/* 释放全部元素、桶数组和集合结构。 */
XRT_API void xrtSetDestroy(xset* pSet)
{
	bool bHeap;

	if ( pSet == NULL ) {
		return;
	}
	if ( !__xrtSetCanMutate(pSet) ) {
		return;
	}
	bHeap = (pSet->Flags & XRT_SET_FLAG_HEAP) != 0;
	xrtSetUnit(pSet);
	if ( bHeap ) {
		xrtFree(pSet);
	}
}



/* 清空全部元素并保留桶数组供后续复用。 */
XRT_API void xrtSetClear(xset* pSet)
{
	if ( !__xrtSetCanMutate(pSet) || (pSet->Count == 0) ) {
		return;
	}

	__xrtSetReleaseAll(pSet);
	memset(pSet->Buckets, 0, pSet->BucketCount * sizeof(xsetentry*));
	pSet->First = NULL;
	pSet->Last = NULL;
	pSet->Count = 0;
	__xrtSetChange(pSet);
}



/* 确保集合无需扩容即可容纳指定数量的元素。 */
XRT_API bool xrtSetReserve(xset* pSet, size_t iCapacity)
{
	xsetentry** pBuckets;
	size_t iBucketCount;

	if ( !__xrtSetCanMutate(pSet) ) {
		return false;
	}
	if ( iCapacity <= pSet->Threshold ) {
		return true;
	}
	if ( !__xrtSetBucketCount(iCapacity, &iBucketCount) ) {
		return false;
	}
	pBuckets = __xrtSetAllocBuckets(iBucketCount);
	if ( pBuckets == NULL ) {
		return false;
	}
	__xrtSetRehash(pSet, pBuckets, iBucketCount);
	xrtFree(pSet->Buckets);
	pSet->Buckets = pBuckets;
	pSet->BucketCount = iBucketCount;
	pSet->Threshold = __xrtSetThreshold(iBucketCount);
	return true;
}



/* 把桶数组收缩到当前元素数需要的最小容量。 */
XRT_API bool xrtSetTrim(xset* pSet)
{
	xsetentry** pBuckets;
	size_t iBucketCount;

	if ( !__xrtSetCanMutate(pSet) ) {
		return false;
	}
	if ( !__xrtSetBucketCount(pSet->Count, &iBucketCount) ) {
		return false;
	}
	if ( iBucketCount == pSet->BucketCount ) {
		return true;
	}
	if ( iBucketCount == 0 ) {
		xrtFree(pSet->Buckets);
		pSet->Buckets = NULL;
		pSet->BucketCount = 0;
		pSet->Threshold = 0;
		return true;
	}
	pBuckets = __xrtSetAllocBuckets(iBucketCount);
	if ( pBuckets == NULL ) {
		return false;
	}
	__xrtSetRehash(pSet, pBuckets, iBucketCount);
	xrtFree(pSet->Buckets);
	pSet->Buckets = pBuckets;
	pSet->BucketCount = iBucketCount;
	pSet->Threshold = __xrtSetThreshold(iBucketCount);
	return true;
}



/* 返回当前元素数，非法集合返回零。 */
XRT_API size_t xrtSetCount(const xset* pSet)
{
	return __xrtSetCanRead(pSet) ? pSet->Count : 0;
}



/* 返回再次扩容前可容纳的元素数。 */
XRT_API size_t xrtSetCapacity(const xset* pSet)
{
	return __xrtSetCanRead(pSet) ? pSet->Threshold : 0;
}



/* 返回规范存储元素，缺失时失败原子地复制插入。 */
XRT_API const void* xrtSetGetOrAdd(
	xset* pSet,
	const void* pItem,
	bool* pNew
)
{
	xsetentry* pEntry;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtSetCanMutate(pSet) ) {
		return NULL;
	}
	if (
		(pItem == NULL) ||
		__xrtSetOwnsCoreRange(pSet, pItem, pSet->ItemSize)
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pEntry = __xrtSetGetOrAddEntry(pSet, pItem, pNew);
	return __xrtSetItem(pSet, pEntry);
}



/* 复制加入元素，已有等价元素时成功且不替换规范元素。 */
XRT_API bool xrtSetAdd(xset* pSet, const void* pItem)
{
	return xrtSetGetOrAdd(pSet, pItem, NULL) != NULL;
}



/* 返回集合内部的规范元素，缺失是正常结果。 */
XRT_API const void* xrtSetGet(const xset* pSet, const void* pItem)
{
	if ( !__xrtSetCanRead(pSet) ) {
		return NULL;
	}
	if (
		(pItem == NULL) ||
		__xrtSetOwnsCoreRange(pSet, pItem, pSet->ItemSize)
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}

	return __xrtSetItem(pSet, __xrtSetFind(pSet, pItem, NULL, NULL));
}



/* 判断等价元素是否存在。 */
XRT_API bool xrtSetHas(const xset* pSet, const void* pItem)
{
	return xrtSetGet(pSet, pItem) != NULL;
}



/* 删除等价元素并调用资源释放器。 */
XRT_API bool xrtSetRemove(xset* pSet, const void* pItem)
{
	xsetentry** pLink;
	xsetentry* pEntry;

	if ( !__xrtSetCanMutate(pSet) ) {
		return false;
	}
	if (
		(pItem == NULL) ||
		__xrtSetOwnsCoreRange(pSet, pItem, pSet->ItemSize)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pEntry = __xrtSetFind(pSet, pItem, NULL, &pLink);
	if ( pEntry == NULL ) {
		return false;
	}
	__xrtSetUnlink(pSet, pEntry, pLink);
	__xrtSetDropItem(pSet, __xrtSetItem(pSet, pEntry));
	xrtFree(pEntry->Allocation);
	return true;
}



/* 把规范元素移交给调用方后删除，不调用资源释放器。 */
XRT_API bool xrtSetTake(xset* pSet, const void* pItem, ptr pValue)
{
	xsetentry** pLink;
	xsetentry* pEntry;

	if ( !__xrtSetCanMutate(pSet) ) {
		return false;
	}
	if (
		(pItem == NULL) ||
		(pValue == NULL) ||
		__xrtSetOwnsCoreRange(pSet, pItem, pSet->ItemSize)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pEntry = __xrtSetFind(pSet, pItem, NULL, &pLink);
	if ( pEntry == NULL ) {
		return false;
	}
	if ( __xrtSetOwnsRange(pSet, pValue, pSet->ItemSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memmove(pValue, __xrtSetItem(pSet, pEntry), pSet->ItemSize);
	__xrtSetUnlink(pSet, pEntry, pLink);
	xrtFree(pEntry->Allocation);
	return true;
}



/* 使用类型移动器移出规范元素，成功后删除集合条目。 */
bool __xrtSetMoveOut(
	xset* pSet,
	const void* pItem,
	ptr pValue,
	xrtsetmoveproc pMove,
	ptr pUserData
)
{
	xsetentry** pLink;
	xsetentry* pEntry;
	ptr pStored;
	bool bMoved;

	if ( !__xrtSetCanMutate(pSet) ) {
		return false;
	}
	if (
		(pItem == NULL) ||
		(pValue == NULL) ||
		(pMove == NULL) ||
		__xrtSetOwnsCoreRange(pSet, pItem, pSet->ItemSize)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pEntry = __xrtSetFind(pSet, pItem, NULL, &pLink);
	if ( pEntry == NULL ) {
		return false;
	}
	if ( __xrtSetOwnsRange(pSet, pValue, pSet->ItemSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	pStored = __xrtSetItem(pSet, pEntry);
	pSet->Flags |= XRT_SET_FLAG_BUSY;
	bMoved = pMove(pValue, pStored, pUserData);
	pSet->Flags &= ~XRT_SET_FLAG_BUSY;
	if ( !bMoved ) {
		return false;
	}

	__xrtSetUnlink(pSet, pEntry, pLink);
	__xrtSetDropItem(pSet, pStored);
	xrtFree(pEntry->Allocation);
	return true;
}



/* 接管堆集合的全部存储并释放其外层结构。 */
bool __xrtSetAdoptHeap(xset* pTarget, xset* pSource)
{
	uint32 iTargetHeap;

	if (
		!__xrtSetCanMutate(pTarget) ||
		!__xrtSetCanMutate(pSource)
	) {
		return false;
	}
	if (
		(pTarget == pSource) ||
		(pTarget->Count != 0) ||
		((pSource->Flags & XRT_SET_FLAG_HEAP) == 0) ||
		!__xrtSetCompatible(pTarget, pSource)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	iTargetHeap = pTarget->Flags & XRT_SET_FLAG_HEAP;
	xrtSetUnit(pTarget);
	*pTarget = *pSource;
	pTarget->Flags = (pTarget->Flags & ~XRT_SET_FLAG_HEAP) | iTargetHeap;
	if ( pTarget->Hash == __xrtSetDefaultHash ) {
		pTarget->KeyUserData = pTarget;
	}
	if ( pTarget->Copy == __xrtSetDefaultCopy ) {
		pTarget->LifecycleUserData = pTarget;
	}

	memset(pSource, 0, sizeof(xset));
	xrtFree(pSource);
	return true;
}



/* 按插入顺序访问元素，并返回实际访问数量。 */
XRT_API size_t xrtSetVisit(xset* pSet, xsetvisitor pVisitor, ptr pUserData)
{
	xsetiter tIterator;
	const void* pItem;
	size_t iVisited = 0;

	if ( !__xrtSetCanMutate(pSet) || (pVisitor == NULL) ) {
		if ( pVisitor == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return 0;
	}
	pSet->Flags |= XRT_SET_FLAG_VISITING;
	if ( !xrtSetIterBegin(pSet, &tIterator) ) {
		pSet->Flags &= ~XRT_SET_FLAG_VISITING;
		return 0;
	}
	while ( (pItem = xrtSetIterNext(&tIterator)) != NULL ) {
		iVisited++;
		if ( !pVisitor(pItem, pUserData) ) {
			break;
		}
	}
	xrtSetIterEnd(&tIterator);
	pSet->Flags &= ~XRT_SET_FLAG_VISITING;
	return iVisited;
}



/* 启动按插入顺序的外置迭代器。 */
XRT_API bool xrtSetIterBegin(xset* pSet, xsetiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xsetiter));
	}
	if ( (pIterator == NULL) || !__xrtSetCanRead(pSet) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Set = pSet;
	pIterator->Next = pSet->First;
	pIterator->Version = pSet->Version;
	pIterator->Direction = 1;
	return true;
}



/* 启动按插入顺序逆序遍历的外置迭代器。 */
XRT_API bool xrtSetIterRBegin(xset* pSet, xsetiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xsetiter));
	}
	if ( (pIterator == NULL) || !__xrtSetCanRead(pSet) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Set = pSet;
	pIterator->Next = pSet->Last;
	pIterator->Version = pSet->Version;
	pIterator->Direction = -1;
	return true;
}



/* 返回下一规范元素，结构修改后报告状态错误。 */
XRT_API const void* xrtSetIterNext(xsetiter* pIterator)
{
	xsetentry* pEntry;

	if ( pIterator == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pIterator->Set == NULL ) {
		return NULL;
	}
	if (
		!__xrtSetCanRead(pIterator->Set) ||
		(pIterator->Version != pIterator->Set->Version)
	) {
		if ( pIterator->Version != pIterator->Set->Version ) {
			__xrtErrorSetInvalidState();
		}
		pIterator->Set = NULL;
		pIterator->Next = NULL;
		return NULL;
	}
	pEntry = pIterator->Next;
	if ( pEntry == NULL ) {
		pIterator->Set = NULL;
		return NULL;
	}
	pIterator->Next = pIterator->Direction > 0 ?
		pEntry->OrderNext : pEntry->OrderPrev;
	return __xrtSetItem(pIterator->Set, pEntry);
}



/* 提前结束迭代并清除借用状态。 */
XRT_API void xrtSetIterEnd(xsetiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xsetiter));
	}
}



/* 深度复制集合结构，并按生命周期复制器复制元素。 */
XRT_API xset* xrtSetClone(const xset* pSet)
{
	xset* pResult;
	bool bProtected;

	if ( !__xrtSetProtectRead(pSet, &bProtected) ) {
		return NULL;
	}
	pResult = __xrtSetCreateLike(pSet);
	if ( pResult == NULL ) {
		__xrtSetUnprotectRead(pSet, bProtected);
		return NULL;
	}
	if ( !xrtSetReserve(pResult, pSet->Count) ||
		!__xrtSetMergeInto(pResult, pSet) ) {
		xrtSetDestroy(pResult);
		__xrtSetUnprotectRead(pSet, bProtected);
		return NULL;
	}

	__xrtSetUnprotectRead(pSet, bProtected);
	return pResult;
}



/* 失败原子地把源集合中的缺失元素合并到目标集合。 */
XRT_API bool xrtSetMerge(xset* pTarget, const xset* pSource)
{
	bool bProtected;
	bool bResult;

	if (
		!__xrtSetCanMutate(pTarget) ||
		!__xrtSetCanRead(pSource)
	) {
		return false;
	}
	if ( !__xrtSetCompatible(pTarget, pSource) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( pSource->Count > (SIZE_MAX - pTarget->Count) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( !__xrtSetProtectRead(pSource, &bProtected) ) {
		return false;
	}
	bResult = __xrtSetMergeAtomic(pTarget, pSource);
	__xrtSetUnprotectRead(pSource, bProtected);
	return bResult;
}



/* 创建两个兼容集合的并集。 */
XRT_API xset* xrtSetUnion(const xset* pLeft, const xset* pRight)
{
	xset* pResult;

	if (
		!__xrtSetCanRead(pLeft) ||
		!__xrtSetCanRead(pRight)
	) {
		return NULL;
	}
	if ( !__xrtSetCompatible(pLeft, pRight) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pRight->Count > (SIZE_MAX - pLeft->Count) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pResult = xrtSetClone(pLeft);
	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtSetMerge(pResult, pRight) ) {
		xrtSetDestroy(pResult);
		return NULL;
	}

	return pResult;
}



/* 创建满足指定成员关系的集合结果。 */
static xset* __xrtSetFilter(
	const xset* pLeft,
	const xset* pRight,
	bool bKeepPresent
)
{
	xset* pResult = NULL;
	xsetentry* pEntry;
	bool bLeftProtected;
	bool bRightProtected;

	if (
		!__xrtSetCanRead(pLeft) ||
		!__xrtSetCanRead(pRight)
	) {
		return NULL;
	}
	if ( !__xrtSetCompatible(pLeft, pRight) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtSetProtectRead(pLeft, &bLeftProtected) ) {
		return NULL;
	}
	if ( !__xrtSetProtectRead(pRight, &bRightProtected) ) {
		__xrtSetUnprotectRead(pLeft, bLeftProtected);
		return NULL;
	}
	pResult = __xrtSetCreateLike(pLeft);
	if (
		(pResult != NULL) &&
		!xrtSetReserve(pResult, pLeft->Count)
	) {
		xrtSetDestroy(pResult);
		pResult = NULL;
	}
	pEntry = pLeft->First;
	while ( (pResult != NULL) && (pEntry != NULL) ) {
		const void* pItem = __xrtSetItem(pLeft, pEntry);

		if ( xrtSetHas(pRight, pItem) == bKeepPresent ) {
			if ( !xrtSetAdd(pResult, pItem) ) {
				xrtSetDestroy(pResult);
				pResult = NULL;
				break;
			}
		}
		pEntry = pEntry->OrderNext;
	}

	__xrtSetUnprotectRead(pRight, bRightProtected);
	__xrtSetUnprotectRead(pLeft, bLeftProtected);
	return pResult;
}



/* 创建两个兼容集合的交集。 */
XRT_API xset* xrtSetIntersection(const xset* pLeft, const xset* pRight)
{
	return __xrtSetFilter(pLeft, pRight, true);
}



/* 创建左集合相对右集合的差集。 */
XRT_API xset* xrtSetDifference(const xset* pLeft, const xset* pRight)
{
	return __xrtSetFilter(pLeft, pRight, false);
}



/* 创建两个兼容集合的对称差集。 */
XRT_API xset* xrtSetSymmetricDifference(
	const xset* pLeft,
	const xset* pRight
)
{
	xset* pResult;
	xsetentry* pEntry;
	bool bLeftProtected;
	bool bRightProtected;

	if (
		!__xrtSetCanRead(pLeft) ||
		!__xrtSetCanRead(pRight)
	) {
		return NULL;
	}
	if ( !__xrtSetCompatible(pLeft, pRight) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pResult = __xrtSetFilter(pLeft, pRight, false);
	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !__xrtSetProtectRead(pLeft, &bLeftProtected) ) {
		xrtSetDestroy(pResult);
		return NULL;
	}
	if ( !__xrtSetProtectRead(pRight, &bRightProtected) ) {
		__xrtSetUnprotectRead(pLeft, bLeftProtected);
		xrtSetDestroy(pResult);
		return NULL;
	}
	pEntry = pRight->First;
	while ( pEntry != NULL ) {
		const void* pItem = __xrtSetItem(pRight, pEntry);

		if ( !xrtSetHas(pLeft, pItem) && !xrtSetAdd(pResult, pItem) ) {
			xrtSetDestroy(pResult);
			pResult = NULL;
			break;
		}
		pEntry = pEntry->OrderNext;
	}

	__xrtSetUnprotectRead(pRight, bRightProtected);
	__xrtSetUnprotectRead(pLeft, bLeftProtected);
	return pResult;
}



/* 判断左集合是否为右集合的子集，可选择严格子集。 */
XRT_API bool xrtSetIsSubset(
	const xset* pLeft,
	const xset* pRight,
	bool bProper
)
{
	xsetentry* pEntry;
	bool bLeftProtected;
	bool bRightProtected;
	bool bResult = true;

	if (
		!__xrtSetCanRead(pLeft) ||
		!__xrtSetCanRead(pRight)
	) {
		return false;
	}
	if ( !__xrtSetCompatible(pLeft, pRight) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pLeft->Count > pRight->Count) ||
		(bProper && (pLeft->Count == pRight->Count))
	) {
		return false;
	}
	if ( pLeft == pRight ) {
		return !bProper;
	}
	if ( !__xrtSetProtectRead(pLeft, &bLeftProtected) ) {
		return false;
	}
	if ( !__xrtSetProtectRead(pRight, &bRightProtected) ) {
		__xrtSetUnprotectRead(pLeft, bLeftProtected);
		return false;
	}
	pEntry = pLeft->First;
	while ( pEntry != NULL ) {
		if ( !xrtSetHas(pRight, __xrtSetItem(pLeft, pEntry)) ) {
			bResult = false;
			break;
		}
		pEntry = pEntry->OrderNext;
	}

	__xrtSetUnprotectRead(pRight, bRightProtected);
	__xrtSetUnprotectRead(pLeft, bLeftProtected);
	return bResult;
}



/* 判断左集合是否为右集合的超集，可选择严格超集。 */
XRT_API bool xrtSetIsSuperset(
	const xset* pLeft,
	const xset* pRight,
	bool bProper
)
{
	return xrtSetIsSubset(pRight, pLeft, bProper);
}



/* 判断两个兼容集合是否没有任何共同元素。 */
XRT_API bool xrtSetIsDisjoint(
	const xset* pLeft,
	const xset* pRight
)
{
	const xset* pScan;
	const xset* pLookup;
	xsetentry* pEntry;
	bool bLeftProtected;
	bool bRightProtected;
	bool bResult = true;

	if (
		!__xrtSetCanRead(pLeft) ||
		!__xrtSetCanRead(pRight)
	) {
		return false;
	}
	if ( !__xrtSetCompatible(pLeft, pRight) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pLeft == pRight ) {
		return pLeft->Count == 0;
	}
	pScan = pLeft->Count <= pRight->Count ? pLeft : pRight;
	pLookup = pScan == pLeft ? pRight : pLeft;
	if ( !__xrtSetProtectRead(pLeft, &bLeftProtected) ) {
		return false;
	}
	if ( !__xrtSetProtectRead(pRight, &bRightProtected) ) {
		__xrtSetUnprotectRead(pLeft, bLeftProtected);
		return false;
	}
	pEntry = pScan->First;
	while ( pEntry != NULL ) {
		if ( xrtSetHas(pLookup, __xrtSetItem(pScan, pEntry)) ) {
			bResult = false;
			break;
		}
		pEntry = pEntry->OrderNext;
	}

	__xrtSetUnprotectRead(pRight, bRightProtected);
	__xrtSetUnprotectRead(pLeft, bLeftProtected);
	return bResult;
}



/* 判断两个兼容集合是否拥有相同元素。 */
XRT_API bool xrtSetEqual(const xset* pLeft, const xset* pRight)
{
	if (
		!__xrtSetCanRead(pLeft) ||
		!__xrtSetCanRead(pRight)
	) {
		return false;
	}
	if ( !__xrtSetCompatible(pLeft, pRight) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pLeft->Count != pRight->Count ) {
		return false;
	}

	return xrtSetIsSubset(pLeft, pRight, false);
}

#endif
