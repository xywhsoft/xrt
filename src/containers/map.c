#include "../internal/xrt_internal.h"
#include "../internal/xrt_map.h"



#if defined(XRT_FEATURE_MAP)

#define XRT_MAP_FLAG_READY    0x0001u
#define XRT_MAP_FLAG_BUSY     0x0002u
#define XRT_MAP_FLAG_VISITING 0x0004u
#define XRT_MAP_FLAG_DROP_REVERSE 0x0008u
#define XRT_MAP_FLAGS         0x000Fu



/* 条目把桶链、插入顺序、值和键保存在同一次分配中。 */
struct xmapentry {
	struct xmapentry* BucketNext;
	struct xmapentry* OrderPrev;
	struct xmapentry* OrderNext;
	ptr Allocation;
	uint64 Hash;
	size_t KeySize;
};



/* 默认键策略使用跨架构一致的 64 位哈希。 */
static uint64 __xrtMapDefaultHash(xbytesview Key, ptr pUserData)
{
	(void)pUserData;
	return xrtHash64(Key.Data, Key.Size);
}



/* 默认键策略按完整二进制内容判断相等。 */
static bool __xrtMapDefaultEqual(xbytesview Left, xbytesview Right, ptr pUserData)
{
	(void)pUserData;
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 返回桶数对应的 75% 键容量。 */
static size_t __xrtMapThreshold(size_t iBucketCount)
{
	return iBucketCount - (iBucketCount / 4u);
}



/* 检查映射布局、桶数组和回调合同是否一致。 */
bool __xrtMapValid(const xmap* pMap)
{
	if ( pMap == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pMap->Flags & XRT_MAP_FLAG_READY) == 0) ||
		((pMap->Flags & ~XRT_MAP_FLAGS) != 0) ||
		(pMap->ValueSize == 0) ||
		(pMap->ValueOffset < sizeof(xmapentry)) ||
		(pMap->Alignment == 0) ||
		((pMap->Alignment & (pMap->Alignment - 1u)) != 0) ||
		((pMap->ValueOffset & (pMap->Alignment - 1u)) != 0) ||
		(pMap->ValueSize > (SIZE_MAX - pMap->ValueOffset)) ||
		(pMap->KeyOffset != (pMap->ValueOffset + pMap->ValueSize)) ||
		(pMap->Hash == NULL) ||
		(pMap->Equal == NULL) ||
		(pMap->Version == 0)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pMap->BucketCount == 0 ) {
		if (
			(pMap->Buckets != NULL) ||
			(pMap->Threshold != 0) ||
			(pMap->Count != 0)
		) {
			__xrtErrorSetInvalidState();
			return false;
		}
	} else if (
		(pMap->Buckets == NULL) ||
		(pMap->BucketCount < XRT_MAP_BUCKETS_MIN) ||
		((pMap->BucketCount & (pMap->BucketCount - 1u)) != 0) ||
		(pMap->Threshold != __xrtMapThreshold(pMap->BucketCount)) ||
		(pMap->Count > pMap->Threshold)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if (
		((pMap->Count == 0) && ((pMap->First != NULL) || (pMap->Last != NULL))) ||
		((pMap->Count != 0) && ((pMap->First == NULL) || (pMap->Last == NULL)))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 检查映射是否允许查询或推进外置迭代器。 */
bool __xrtMapCanRead(const xmap* pMap)
{
	if ( !__xrtMapValid(pMap) ) {
		return false;
	}
	if ( (pMap->Flags & XRT_MAP_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 检查映射是否允许改变值、键集合、容量或生命周期。 */
bool __xrtMapCanMutate(xmap* pMap)
{
	if ( !__xrtMapCanRead(pMap) ) {
		return false;
	}
	if ( (pMap->Flags & XRT_MAP_FLAG_VISITING) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 配置值释放器的调用顺序，公开键顺序和桶结构不受影响。 */
bool __xrtMapSetDropReverse(xmap* pMap, bool bReverse)
{
	if ( !__xrtMapCanMutate(pMap) ) {
		return false;
	}
	if ( bReverse ) {
		pMap->Flags |= XRT_MAP_FLAG_DROP_REVERSE;
	} else {
		pMap->Flags &= ~XRT_MAP_FLAG_DROP_REVERSE;
	}
	return true;
}



/* 查询拥有型封装已经固定的释放顺序。 */
bool __xrtMapDropsReverse(const xmap* pMap)
{
	return __xrtMapValid(pMap) &&
		((pMap->Flags & XRT_MAP_FLAG_DROP_REVERSE) != 0);
}



/* 检查借用键视图是否可以安全读取。 */
static bool __xrtMapKeyValid(xbytesview Key)
{
	if ( (Key.Data == NULL) && (Key.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



/* 调用自定义哈希器期间拒绝同一映射的 API 重入。 */
static uint64 __xrtMapHashValue(xmap* pMap, xbytesview Key)
{
	uint64 iHash;

	if ( pMap->Hash == __xrtMapDefaultHash ) {
		return __xrtMapDefaultHash(Key, pMap->KeyUserData);
	}
	pMap->Flags |= XRT_MAP_FLAG_BUSY;
	iHash = pMap->Hash(Key, pMap->KeyUserData);
	pMap->Flags &= ~XRT_MAP_FLAG_BUSY;
	return iHash;
}



/* 调用自定义相等器期间拒绝同一映射的 API 重入。 */
static bool __xrtMapKeysEqual(
	xmap* pMap,
	xbytesview Left,
	xbytesview Right
)
{
	bool bEqual;

	if ( pMap->Equal == __xrtMapDefaultEqual ) {
		return __xrtMapDefaultEqual(Left, Right, pMap->KeyUserData);
	}
	pMap->Flags |= XRT_MAP_FLAG_BUSY;
	bEqual = pMap->Equal(Left, Right, pMap->KeyUserData);
	pMap->Flags &= ~XRT_MAP_FLAG_BUSY;
	return bEqual;
}



/* 判断两个非空地址区间是否相交，不执行可能溢出的末地址加法。 */
static bool __xrtMapRangeOverlaps(
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



/* 判断一个完整区间是否触及映射结构或可能在扩容时释放的桶数组。 */
static bool __xrtMapOwnsCoreRange(
	const xmap* pMap,
	const void* pMemory,
	size_t iSize
)
{
	size_t iBucketBytes;

	if ( __xrtMapRangeOverlaps(pMemory, iSize, pMap, sizeof(xmap)) ) {
		return true;
	}
	iBucketBytes = pMap->BucketCount * sizeof(xmapentry*);
	if (
		(pMap->Buckets != NULL) &&
		__xrtMapRangeOverlaps(
			pMemory,
			iSize,
			pMap->Buckets,
			iBucketBytes
		)
	) {
		return true;
	}

	return false;
}



/* 判断一个完整区间是否触及指定条目的原始分配。 */
static bool __xrtMapEntryOwnsRange(
	const xmap* pMap,
	const xmapentry* pEntry,
	const void* pMemory,
	size_t iSize
)
{
	size_t iEntryBytes;
	size_t iExtra = pMap->Alignment > XRT_MAP_ALIGNMENT_DEFAULT ?
		pMap->Alignment - 1u : 0;

	if (
		(pEntry->Allocation == NULL) ||
		(pEntry->KeySize > (SIZE_MAX - pMap->KeyOffset - 1u)) ||
		((pMap->KeyOffset + pEntry->KeySize + 1u) > (SIZE_MAX - iExtra))
	) {
		__xrtErrorSetInvalidState();
		return true;
	}
	iEntryBytes = pMap->KeyOffset + pEntry->KeySize + 1u + iExtra;
	return __xrtMapRangeOverlaps(
		pMemory,
		iSize,
		pEntry->Allocation,
		iEntryBytes
	);
}



/* 判断一个完整区间是否触及映射拥有的任意内存。 */
bool __xrtMapOwnsRange(
	const xmap* pMap,
	const void* pMemory,
	size_t iSize
)
{
	xmapentry* pEntry;

	if ( __xrtMapOwnsCoreRange(pMap, pMemory, iSize) ) {
		return true;
	}
	for ( pEntry = pMap->First; pEntry != NULL; pEntry = pEntry->OrderNext ) {
		if ( __xrtMapEntryOwnsRange(pMap, pEntry, pMemory, iSize) ) {
			return true;
		}
	}

	return false;
}



/* 判断映射是否仍使用精确二进制默认键策略。 */
bool __xrtMapUsesDefaultKeyPolicy(const xmap* pMap)
{
	return __xrtMapValid(pMap) &&
		(pMap->Hash == __xrtMapDefaultHash) &&
		(pMap->Equal == __xrtMapDefaultEqual) &&
		(pMap->KeyUserData == NULL);
}



/* 把条目转换为内联值槽。 */
static ptr __xrtMapValue(const xmap* pMap, const xmapentry* pEntry)
{
	return pEntry != NULL ? (ptr)((bytes)pEntry + pMap->ValueOffset) : NULL;
}



/* 把条目转换为由映射拥有的键视图。 */
static xbytesview __xrtMapKey(const xmap* pMap, const xmapentry* pEntry)
{
	xbytesview Key = { NULL, 0 };

	if ( pEntry != NULL ) {
		Key.Data = (cbytes)pEntry + pMap->KeyOffset;
		Key.Size = pEntry->KeySize;
	}
	return Key;
}



/* 调用值释放器期间拒绝同一映射的全部 API 重入。 */
static void __xrtMapDropValue(xmap* pMap, xmapentry* pEntry)
{
	if ( pMap->Drop == NULL ) {
		return;
	}

	pMap->Flags |= XRT_MAP_FLAG_BUSY;
	pMap->Drop(
		__xrtMapKey(pMap, pEntry),
		__xrtMapValue(pMap, pEntry),
		pMap->DropUserData
	);
	pMap->Flags &= ~XRT_MAP_FLAG_BUSY;
}



/* 在指定桶数下重新建立桶链，不移动任何条目。 */
static void __xrtMapRehash(
	const xmap* pMap,
	xmapentry** pBuckets,
	size_t iBucketCount
)
{
	xmapentry* pEntry = pMap->First;
	size_t iMask = iBucketCount - 1u;

	while ( pEntry != NULL ) {
		size_t iBucket = (size_t)pEntry->Hash & iMask;

		pEntry->BucketNext = pBuckets[iBucket];
		pBuckets[iBucket] = pEntry;
		pEntry = pEntry->OrderNext;
	}
}



/* 计算容纳指定键数所需的最小二次幂桶数。 */
static bool __xrtMapBucketCount(size_t iCapacity, size_t* pBucketCount)
{
	size_t iMaxBuckets = SIZE_MAX / sizeof(xmapentry*);
	size_t iBuckets = XRT_MAP_BUCKETS_MIN;

	*pBucketCount = 0;
	if ( iCapacity == 0 ) {
		return true;
	}
	while ( __xrtMapThreshold(iBuckets) < iCapacity ) {
		if ( iBuckets > (iMaxBuckets / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iBuckets *= 2u;
	}
	*pBucketCount = iBuckets;
	return true;
}



/* 分配并清零一个桶数组。 */
static xmapentry** __xrtMapAllocBuckets(size_t iBucketCount)
{
	if ( iBucketCount > (SIZE_MAX / sizeof(xmapentry*)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}

	return (xmapentry**)xrtCalloc(iBucketCount, sizeof(xmapentry*));
}



/* 为一次新键插入预备扩容桶，失败时不改变映射。 */
static bool __xrtMapPrepareInsert(
	const xmap* pMap,
	xmapentry*** ppBuckets,
	size_t* pBucketCount
)
{
	size_t iNeeded;
	size_t iBuckets;

	*ppBuckets = NULL;
	*pBucketCount = 0;
	if ( pMap->Count == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNeeded = pMap->Count + 1u;
	if ( iNeeded <= pMap->Threshold ) {
		return true;
	}
	if ( !__xrtMapBucketCount(iNeeded, &iBuckets) ) {
		return false;
	}
	*ppBuckets = __xrtMapAllocBuckets(iBuckets);
	if ( *ppBuckets == NULL ) {
		return false;
	}
	*pBucketCount = iBuckets;
	return true;
}



/* 分配一条紧凑条目，并复制键、清零值槽。 */
static xmapentry* __xrtMapAllocEntry(
	const xmap* pMap,
	xbytesview Key,
	uint64 iHash
)
{
	size_t iBytes;
	size_t iExtra = pMap->Alignment > XRT_MAP_ALIGNMENT_DEFAULT ?
		pMap->Alignment - 1u : 0;
	ptr pAllocation;
	uintptr_t iAddress;
	xmapentry* pEntry;
	bytes pKey;

	if ( Key.Size > (SIZE_MAX - pMap->KeyOffset - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBytes = pMap->KeyOffset + Key.Size + 1u;
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
	iAddress = ((uintptr_t)pAllocation + iExtra) & ~((uintptr_t)pMap->Alignment - 1u);
	pEntry = (xmapentry*)iAddress;
	pEntry->BucketNext = NULL;
	pEntry->OrderPrev = NULL;
	pEntry->OrderNext = NULL;
	pEntry->Allocation = pAllocation;
	pEntry->Hash = iHash;
	pEntry->KeySize = Key.Size;
	memset(__xrtMapValue(pMap, pEntry), 0, pMap->ValueSize);
	pKey = (bytes)pEntry + pMap->KeyOffset;
	if ( Key.Size != 0 ) {
		memcpy(pKey, Key.Data, Key.Size);
	}
	pKey[Key.Size] = 0;
	return pEntry;
}



/* 释放一条目的原始分配。 */
static void __xrtMapFreeEntry(xmapentry* pEntry)
{
	xrtFree(pEntry->Allocation);
}



/* 计算哈希并在桶链中查找等价键，可选返回链指针。 */
static xmapentry* __xrtMapFind(
	const xmap* pMap,
	xbytesview Key,
	uint64* pHash,
	xmapentry*** ppLink
)
{
	uint64 iHash;
	xmapentry** pLink;
	xmapentry* pEntry;

	if ( (pMap->BucketCount == 0) && (pHash == NULL) ) {
		if ( ppLink != NULL ) {
			*ppLink = NULL;
		}
		return NULL;
	}
	iHash = __xrtMapHashValue((xmap*)pMap, Key);
	if ( pHash != NULL ) {
		*pHash = iHash;
	}
	if ( ppLink != NULL ) {
		*ppLink = NULL;
	}
	if ( pMap->BucketCount == 0 ) {
		return NULL;
	}
	pLink = &((xmap*)pMap)->Buckets[(size_t)iHash & (pMap->BucketCount - 1u)];
	pEntry = *pLink;
	while ( pEntry != NULL ) {
		if (
			(pEntry->Hash == iHash) &&
			__xrtMapKeysEqual(
				(xmap*)pMap,
				Key,
				__xrtMapKey(pMap, pEntry)
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
static void __xrtMapChange(xmap* pMap)
{
	pMap->Version++;
	if ( pMap->Version == 0 ) {
		pMap->Version = 1;
	}
}



/* 在多步只读操作期间阻止回调修改映射结构。 */
bool __xrtMapProtectRead(const xmap* pMap, bool* pAcquired)
{
	xmap* pMutable = (xmap*)pMap;

	if ( pAcquired == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pAcquired = false;
	if ( !__xrtMapCanRead(pMap) ) {
		return false;
	}
	if ( (pMap->Flags & XRT_MAP_FLAG_VISITING) == 0u ) {
		pMutable->Flags |= XRT_MAP_FLAG_VISITING;
		*pAcquired = true;
	}
	return true;
}



/* 结束由当前多步只读操作取得的结构保护。 */
void __xrtMapUnprotectRead(const xmap* pMap, bool bAcquired)
{
	if ( bAcquired && (pMap != NULL) ) {
		((xmap*)pMap)->Flags &= ~XRT_MAP_FLAG_VISITING;
	}
}



/* 在用户回调期间拒绝当前映射的全部 API 重入。 */
bool __xrtMapCallbackBegin(const xmap* pMap)
{
	if ( !__xrtMapCanRead(pMap) ) {
		return false;
	}
	((xmap*)pMap)->Flags |= XRT_MAP_FLAG_BUSY;
	return true;
}



/* 结束当前映射的用户回调门禁。 */
void __xrtMapCallbackEnd(const xmap* pMap)
{
	if ( pMap != NULL ) {
		((xmap*)pMap)->Flags &= ~XRT_MAP_FLAG_BUSY;
	}
}



/* 新条目实现位于组合设置函数之后，先声明以保持相关操作相邻。 */
static xmapentry* __xrtMapInsertEntry(
	xmap* pMap,
	xbytesview Key,
	uint64 iHash,
	xmapinit pInit,
	ptr pUserData
);



/* 一次哈希查询完成失败原子的替换或初始化。 */
ptr __xrtMapSetOrInit(
	xmap* pMap,
	xbytesview Key,
	const void* pValue,
	xrtmapreplaceproc pReplace,
	ptr pReplaceData,
	xmapinit pInit,
	ptr pInitData,
	bool* pNew
)
{
	xmapentry* pEntry;
	ptr pStored;
	uint64 iHash;
	bool bReplaced;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if (
		!__xrtMapCanMutate(pMap) ||
		!__xrtMapKeyValid(Key) ||
		(pValue == NULL) ||
		(pReplace == NULL) ||
		(pInit == NULL) ||
		(pNew == NULL)
	) {
		if (
			(pValue == NULL) ||
			(pReplace == NULL) ||
			(pInit == NULL) ||
			(pNew == NULL)
		) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pEntry = __xrtMapFind(pMap, Key, &iHash, NULL);
	if ( pEntry == NULL ) {
		if ( __xrtMapOwnsCoreRange(pMap, pValue, pMap->ValueSize) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		pEntry = __xrtMapInsertEntry(
			pMap,
			Key,
			iHash,
			pInit,
			pInitData
		);
		if ( pEntry == NULL ) {
			return NULL;
		}
		*pNew = true;
		return __xrtMapValue(pMap, pEntry);
	}
	pStored = __xrtMapValue(pMap, pEntry);
	if ( pStored == pValue ) {
		return pStored;
	}
	if (
		__xrtMapOwnsCoreRange(pMap, pValue, pMap->ValueSize) ||
		__xrtMapEntryOwnsRange(
			pMap, pEntry, pValue, pMap->ValueSize
		)
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}

	pMap->Flags |= XRT_MAP_FLAG_BUSY;
	bReplaced = pReplace(pStored, pValue, pReplaceData);
	pMap->Flags &= ~XRT_MAP_FLAG_BUSY;
	return bReplaced ? pStored : NULL;
}



/* 把新条目接入桶链和插入顺序尾部。 */
static void __xrtMapLink(xmap* pMap, xmapentry* pEntry)
{
	size_t iBucket = (size_t)pEntry->Hash & (pMap->BucketCount - 1u);

	pEntry->BucketNext = pMap->Buckets[iBucket];
	pMap->Buckets[iBucket] = pEntry;
	pEntry->OrderPrev = pMap->Last;
	if ( pMap->Last != NULL ) {
		pMap->Last->OrderNext = pEntry;
	} else {
		pMap->First = pEntry;
	}
	pMap->Last = pEntry;
	pMap->Count++;
	__xrtMapChange(pMap);
}



/* 使用已计算哈希失败原子地创建一条新条目。 */
static xmapentry* __xrtMapInsertEntry(
	xmap* pMap,
	xbytesview Key,
	uint64 iHash,
	xmapinit pInit,
	ptr pUserData
)
{
	xmapentry** pNewBuckets;
	xmapentry* pEntry;
	size_t iNewBucketCount;
	bool bInitialized;

	if ( !__xrtMapPrepareInsert(pMap, &pNewBuckets, &iNewBucketCount) ) {
		return NULL;
	}
	pEntry = __xrtMapAllocEntry(pMap, Key, iHash);
	if ( pEntry == NULL ) {
		xrtFree(pNewBuckets);
		return NULL;
	}
	if ( pInit != NULL ) {
		pMap->Flags |= XRT_MAP_FLAG_BUSY;
		bInitialized = pInit(
			__xrtMapKey(pMap, pEntry),
			__xrtMapValue(pMap, pEntry),
			pUserData
		);
		pMap->Flags &= ~XRT_MAP_FLAG_BUSY;
		if ( !bInitialized ) {
			__xrtMapFreeEntry(pEntry);
			xrtFree(pNewBuckets);
			return NULL;
		}
	}
	if ( pNewBuckets != NULL ) {
		__xrtMapRehash(pMap, pNewBuckets, iNewBucketCount);
		xrtFree(pMap->Buckets);
		pMap->Buckets = pNewBuckets;
		pMap->BucketCount = iNewBucketCount;
		pMap->Threshold = __xrtMapThreshold(iNewBucketCount);
	}
	__xrtMapLink(pMap, pEntry);
	return pEntry;
}



/* 返回已有条目，或失败原子地创建一条新条目。 */
static xmapentry* __xrtMapGetOrAddEntry(
	xmap* pMap,
	xbytesview Key,
	bool* pNew
)
{
	xmapentry* pEntry;
	uint64 iHash;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	pEntry = __xrtMapFind(pMap, Key, &iHash, NULL);
	if ( pEntry != NULL ) {
		return pEntry;
	}
	pEntry = __xrtMapInsertEntry(pMap, Key, iHash, NULL, NULL);
	if ( pEntry == NULL ) {
		return NULL;
	}
	if ( pNew != NULL ) {
		*pNew = true;
	}
	return pEntry;
}



/* 以一次哈希查询完成普通值或指针值的复制设置。 */
static bool __xrtMapSetValue(
	xmap* pMap,
	xbytesview Key,
	const void* pValue,
	bool bPointerValue
)
{
	xmapentry* pEntry;
	ptr pStored;
	uint64 iHash;

	pEntry = __xrtMapFind(pMap, Key, &iHash, NULL);
	if ( pEntry != NULL ) {
		pStored = __xrtMapValue(pMap, pEntry);
		if ( pValue == pStored ) {
			return true;
		}
		if ( bPointerValue && (*(ptr*)pStored == *(ptr const*)pValue) ) {
			return true;
		}
		if (
			__xrtMapOwnsCoreRange(pMap, pValue, pMap->ValueSize) ||
			__xrtMapEntryOwnsRange(
				pMap,
				pEntry,
				pValue,
				pMap->ValueSize
			)
		) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		__xrtMapDropValue(pMap, pEntry);
		memcpy(pStored, pValue, pMap->ValueSize);
		return true;
	}
	if ( __xrtMapOwnsCoreRange(pMap, pValue, pMap->ValueSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pEntry = __xrtMapInsertEntry(pMap, Key, iHash, NULL, NULL);
	if ( pEntry == NULL ) {
		return false;
	}
	pStored = __xrtMapValue(pMap, pEntry);
	memcpy(pStored, pValue, pMap->ValueSize);
	return true;
}



/* 从桶链和插入顺序中摘除条目。 */
static void __xrtMapUnlink(
	xmap* pMap,
	xmapentry* pEntry,
	xmapentry** pLink
)
{
	*pLink = pEntry->BucketNext;
	if ( pEntry->OrderPrev != NULL ) {
		pEntry->OrderPrev->OrderNext = pEntry->OrderNext;
	} else {
		pMap->First = pEntry->OrderNext;
	}
	if ( pEntry->OrderNext != NULL ) {
		pEntry->OrderNext->OrderPrev = pEntry->OrderPrev;
	} else {
		pMap->Last = pEntry->OrderPrev;
	}
	pMap->Count--;
	__xrtMapChange(pMap);
}



/* 释放全部条目并按需调用值释放器。 */
static void __xrtMapReleaseAll(xmap* pMap)
{
	bool bReverse = (pMap->Flags & XRT_MAP_FLAG_DROP_REVERSE) != 0;
	xmapentry* pEntry = bReverse ? pMap->Last : pMap->First;

	while ( pEntry != NULL ) {
		xmapentry* pNext = bReverse ? pEntry->OrderPrev : pEntry->OrderNext;

		__xrtMapDropValue(pMap, pEntry);
		__xrtMapFreeEntry(pEntry);
		pEntry = pNext;
	}
}



/* 按指定对齐建立固定值布局。 */
static bool __xrtMapInit(xmap* pMap, size_t iValueSize, size_t iAlignment)
{
	size_t iValueOffset;

	if (
		(pMap == NULL) ||
		(iValueSize == 0) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sizeof(xmapentry) > (SIZE_MAX - (iAlignment - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iValueOffset = (sizeof(xmapentry) + (iAlignment - 1u)) & ~(iAlignment - 1u);
	if ( iValueSize > (SIZE_MAX - iValueOffset) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	memset(pMap, 0, sizeof(xmap));
	pMap->ValueSize = iValueSize;
	pMap->ValueOffset = iValueOffset;
	pMap->KeyOffset = iValueOffset + iValueSize;
	pMap->Alignment = iAlignment;
	pMap->Version = 1;
	pMap->Hash = __xrtMapDefaultHash;
	pMap->Equal = __xrtMapDefaultEqual;
	pMap->Flags = XRT_MAP_FLAG_READY;
	return true;
}



/* 检查映射是否使用指针大小值槽。 */
static bool __xrtMapPtrSizeValid(const xmap* pMap)
{
	if ( pMap->ValueSize != sizeof(ptr) ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 使用默认 16 字节值对齐初始化空字节键映射。 */
XRT_API bool xrtMapInit(xmap* pMap, size_t iValueSize)
{
	return __xrtMapInit(pMap, iValueSize, XRT_MAP_ALIGNMENT_DEFAULT);
}



/* 使用显式值对齐初始化空字节键映射。 */
XRT_API bool xrtMapInitAligned(
	xmap* pMap,
	size_t iValueSize,
	size_t iAlignment
)
{
	return __xrtMapInit(pMap, iValueSize, iAlignment);
}



/* 创建使用默认 16 字节值对齐的空字节键映射。 */
XRT_API xmap* xrtMapCreate(size_t iValueSize)
{
	return xrtMapCreateAligned(iValueSize, XRT_MAP_ALIGNMENT_DEFAULT);
}



/* 创建使用显式值对齐的空字节键映射。 */
XRT_API xmap* xrtMapCreateAligned(size_t iValueSize, size_t iAlignment)
{
	xmap* pMap = (xmap*)xrtMalloc(sizeof(xmap));

	if ( pMap == NULL ) {
		return NULL;
	}
	if ( !xrtMapInitAligned(pMap, iValueSize, iAlignment) ) {
		xrtFree(pMap);
		return NULL;
	}
	return pMap;
}



/* 为仍为空的映射设置自定义键策略，空回调对恢复默认策略。 */
XRT_API bool xrtMapSetKeyPolicy(
	xmap* pMap,
	xmaphash pHash,
	xmapequal pEqual,
	ptr pUserData
)
{
	if ( !__xrtMapCanMutate(pMap) ) {
		return false;
	}
	if ( pMap->Count != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pHash == NULL) != (pEqual == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	pMap->Hash = pHash != NULL ? pHash : __xrtMapDefaultHash;
	pMap->Equal = pEqual != NULL ? pEqual : __xrtMapDefaultEqual;
	pMap->KeyUserData = pHash != NULL ? pUserData : NULL;
	return true;
}



/* 为仍为空的映射设置值资源释放器。 */
XRT_API bool xrtMapSetDrop(xmap* pMap, xmapdrop pDrop, ptr pUserData)
{
	if ( !__xrtMapCanMutate(pMap) ) {
		return false;
	}
	if ( pMap->Count != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	pMap->Drop = pDrop;
	pMap->DropUserData = pUserData;
	return true;
}



/* 释放全部键值和桶数组，但不释放映射结构。 */
XRT_API void xrtMapUnit(xmap* pMap)
{
	if ( pMap == NULL ) {
		return;
	}
	if ( !__xrtMapCanMutate(pMap) ) {
		return;
	}

	__xrtMapReleaseAll(pMap);
	xrtFree(pMap->Buckets);
	memset(pMap, 0, sizeof(xmap));
}



/* 释放全部键值、桶数组和映射结构。 */
XRT_API void xrtMapDestroy(xmap* pMap)
{
	if ( pMap == NULL ) {
		return;
	}
	if ( !__xrtMapCanMutate(pMap) ) {
		return;
	}

	__xrtMapReleaseAll(pMap);
	xrtFree(pMap->Buckets);
	memset(pMap, 0, sizeof(xmap));
	xrtFree(pMap);
}



/* 清空全部键值并保留桶数组供后续复用。 */
XRT_API void xrtMapClear(xmap* pMap)
{
	if ( !__xrtMapCanMutate(pMap) ) {
		return;
	}
	if ( pMap->Count == 0 ) {
		return;
	}

	__xrtMapReleaseAll(pMap);
	memset(pMap->Buckets, 0, pMap->BucketCount * sizeof(xmapentry*));
	pMap->First = NULL;
	pMap->Last = NULL;
	pMap->Count = 0;
	__xrtMapChange(pMap);
}



/* 确保映射无需扩容即可容纳指定数量的键。 */
XRT_API bool xrtMapReserve(xmap* pMap, size_t iCapacity)
{
	xmapentry** pBuckets;
	size_t iBucketCount;

	if ( !__xrtMapCanMutate(pMap) ) {
		return false;
	}
	if ( iCapacity <= pMap->Threshold ) {
		return true;
	}
	if ( !__xrtMapBucketCount(iCapacity, &iBucketCount) ) {
		return false;
	}
	pBuckets = __xrtMapAllocBuckets(iBucketCount);
	if ( pBuckets == NULL ) {
		return false;
	}
	__xrtMapRehash(pMap, pBuckets, iBucketCount);
	xrtFree(pMap->Buckets);
	pMap->Buckets = pBuckets;
	pMap->BucketCount = iBucketCount;
	pMap->Threshold = __xrtMapThreshold(iBucketCount);
	return true;
}



/* 把桶数组收缩到当前键数所需的最小容量。 */
XRT_API bool xrtMapTrim(xmap* pMap)
{
	xmapentry** pBuckets;
	size_t iBucketCount;

	if ( !__xrtMapCanMutate(pMap) ) {
		return false;
	}
	if ( !__xrtMapBucketCount(pMap->Count, &iBucketCount) ) {
		return false;
	}
	if ( iBucketCount == pMap->BucketCount ) {
		return true;
	}
	if ( iBucketCount == 0 ) {
		xrtFree(pMap->Buckets);
		pMap->Buckets = NULL;
		pMap->BucketCount = 0;
		pMap->Threshold = 0;
		return true;
	}
	pBuckets = __xrtMapAllocBuckets(iBucketCount);
	if ( pBuckets == NULL ) {
		return false;
	}
	__xrtMapRehash(pMap, pBuckets, iBucketCount);
	xrtFree(pMap->Buckets);
	pMap->Buckets = pBuckets;
	pMap->BucketCount = iBucketCount;
	pMap->Threshold = __xrtMapThreshold(iBucketCount);
	return true;
}



/* 返回当前键值数量，非法映射返回零。 */
XRT_API size_t xrtMapCount(const xmap* pMap)
{
	return __xrtMapCanRead(pMap) ? pMap->Count : 0;
}



/* 返回再次扩容前可容纳的键数。 */
XRT_API size_t xrtMapCapacity(const xmap* pMap)
{
	return __xrtMapCanRead(pMap) ? pMap->Threshold : 0;
}



/* 返回已有值槽，或复制键并原地创建、清零一个新值槽。 */
XRT_API ptr xrtMapGetOrAdd(xmap* pMap, xbytesview Key, bool* pNew)
{
	xmapentry* pEntry;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtMapCanMutate(pMap) || !__xrtMapKeyValid(Key) ) {
		return NULL;
	}
	pEntry = __xrtMapGetOrAddEntry(pMap, Key, pNew);
	return __xrtMapValue(pMap, pEntry);
}



/* 返回已有值槽，或失败原子地复制键并原位初始化新值。 */
XRT_API ptr xrtMapGetOrInit(
	xmap* pMap,
	xbytesview Key,
	xmapinit pInit,
	ptr pUserData,
	bool* pNew
)
{
	xmapentry* pEntry;
	uint64 iHash;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if (
		!__xrtMapCanMutate(pMap) ||
		!__xrtMapKeyValid(Key) ||
		(pInit == NULL)
	) {
		if ( pInit == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pEntry = __xrtMapFind(pMap, Key, &iHash, NULL);
	if ( pEntry != NULL ) {
		return __xrtMapValue(pMap, pEntry);
	}
	pEntry = __xrtMapInsertEntry(
		pMap, Key, iHash, pInit, pUserData
	);
	if ( pEntry != NULL ) {
		if ( pNew != NULL ) {
			*pNew = true;
		}
		return __xrtMapValue(pMap, pEntry);
	}
	return NULL;
}



/* 复制插入或替换值，等价键保留原始键副本。 */
XRT_API bool xrtMapSet(xmap* pMap, xbytesview Key, const void* pValue)
{
	if (
		!__xrtMapCanMutate(pMap) ||
		!__xrtMapKeyValid(Key) ||
		(pValue == NULL)
	) {
		if ( pValue == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	return __xrtMapSetValue(pMap, Key, pValue, false);
}



/* 返回指定键的可写值槽，未找到是正常结果。 */
XRT_API ptr xrtMapGet(xmap* pMap, xbytesview Key)
{
	if ( !__xrtMapCanRead(pMap) || !__xrtMapKeyValid(Key) ) {
		return NULL;
	}

	return __xrtMapValue(pMap, __xrtMapFind(pMap, Key, NULL, NULL));
}



/* 返回指定键的只读值槽，未找到是正常结果。 */
XRT_API const void* xrtMapConstGet(const xmap* pMap, xbytesview Key)
{
	if ( !__xrtMapCanRead(pMap) || !__xrtMapKeyValid(Key) ) {
		return NULL;
	}

	return __xrtMapValue(pMap, __xrtMapFind(pMap, Key, NULL, NULL));
}



/* 判断指定字节键是否存在。 */
XRT_API bool xrtMapHas(const xmap* pMap, xbytesview Key)
{
	if ( !__xrtMapCanRead(pMap) || !__xrtMapKeyValid(Key) ) {
		return false;
	}

	return __xrtMapFind(pMap, Key, NULL, NULL) != NULL;
}



/* 返回与查询键等价的内部键副本。 */
XRT_API bool xrtMapStoredKey(
	const xmap* pMap,
	xbytesview Key,
	xbytesview* pStoredKey
)
{
	xmapentry* pEntry;

	if ( pStoredKey != NULL ) {
		pStoredKey->Data = NULL;
		pStoredKey->Size = 0;
	}
	if (
		(pStoredKey == NULL) ||
		!__xrtMapCanRead(pMap) ||
		!__xrtMapKeyValid(Key)
	) {
		if ( pStoredKey == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pEntry = __xrtMapFind(pMap, Key, NULL, NULL);
	if ( pEntry == NULL ) {
		return false;
	}
	*pStoredKey = __xrtMapKey(pMap, pEntry);
	return true;
}



/* 删除指定键并调用值释放器。 */
XRT_API bool xrtMapRemove(xmap* pMap, xbytesview Key)
{
	xmapentry** pLink;
	xmapentry* pEntry;

	if ( !__xrtMapCanMutate(pMap) || !__xrtMapKeyValid(Key) ) {
		return false;
	}
	pEntry = __xrtMapFind(pMap, Key, NULL, &pLink);
	if ( pEntry == NULL ) {
		return false;
	}
	__xrtMapUnlink(pMap, pEntry, pLink);
	__xrtMapDropValue(pMap, pEntry);
	__xrtMapFreeEntry(pEntry);
	return true;
}



/* 将指定键的值字节移交给调用方后删除。 */
XRT_API bool xrtMapTake(xmap* pMap, xbytesview Key, ptr pValue)
{
	xmapentry** pLink;
	xmapentry* pEntry;

	if (
		(pValue == NULL) ||
		!__xrtMapCanMutate(pMap) ||
		!__xrtMapKeyValid(Key)
	) {
		if ( pValue == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pEntry = __xrtMapFind(pMap, Key, NULL, &pLink);
	if ( pEntry == NULL ) {
		return false;
	}
	if ( __xrtMapOwnsRange(pMap, pValue, pMap->ValueSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memmove(pValue, __xrtMapValue(pMap, pEntry), pMap->ValueSize);
	__xrtMapUnlink(pMap, pEntry, pLink);
	__xrtMapFreeEntry(pEntry);
	return true;
}



/* 使用类型移动器移出指定键的值，成功后删除映射条目。 */
bool __xrtMapMoveOut(
	xmap* pMap,
	xbytesview Key,
	ptr pValue,
	xrtmapmoveproc pMove,
	ptr pUserData
)
{
	xmapentry** pLink;
	xmapentry* pEntry;
	ptr pStored;
	bool bMoved;

	if (
		(pValue == NULL) ||
		(pMove == NULL) ||
		!__xrtMapCanMutate(pMap) ||
		!__xrtMapKeyValid(Key)
	) {
		if ( (pValue == NULL) || (pMove == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pEntry = __xrtMapFind(pMap, Key, NULL, &pLink);
	if ( pEntry == NULL ) {
		return false;
	}
	if ( __xrtMapOwnsRange(pMap, pValue, pMap->ValueSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	pStored = __xrtMapValue(pMap, pEntry);
	pMap->Flags |= XRT_MAP_FLAG_BUSY;
	bMoved = pMove(pValue, pStored, pUserData);
	pMap->Flags &= ~XRT_MAP_FLAG_BUSY;
	if ( !bMoved ) {
		return false;
	}

	__xrtMapUnlink(pMap, pEntry, pLink);
	__xrtMapDropValue(pMap, pEntry);
	__xrtMapFreeEntry(pEntry);
	return true;
}



/* 对 sizeof(ptr) 值映射执行指针类型友好的插入或替换。 */
XRT_API bool xrtMapSetPtr(xmap* pMap, xbytesview Key, ptr pValue)
{
	if (
		!__xrtMapCanMutate(pMap) ||
		!__xrtMapPtrSizeValid(pMap) ||
		!__xrtMapKeyValid(Key)
	) {
		return false;
	}
	return __xrtMapSetValue(pMap, Key, &pValue, true);
}



/* 返回 sizeof(ptr) 值映射中保存的指针。 */
XRT_API ptr xrtMapGetPtr(xmap* pMap, xbytesview Key)
{
	ptr* pValue;

	if (
		!__xrtMapCanRead(pMap) ||
		!__xrtMapPtrSizeValid(pMap) ||
		!__xrtMapKeyValid(Key)
	) {
		return NULL;
	}
	pValue = (ptr*)__xrtMapValue(
		pMap,
		__xrtMapFind(pMap, Key, NULL, NULL)
	);
	return pValue != NULL ? *pValue : NULL;
}



/* 从 sizeof(ptr) 值映射中移交指针。 */
XRT_API bool xrtMapTakePtr(xmap* pMap, xbytesview Key, ptr* pValue)
{
	if ( pValue != NULL ) {
		*pValue = NULL;
	}
	if (
		(pValue == NULL) ||
		!__xrtMapCanMutate(pMap) ||
		!__xrtMapPtrSizeValid(pMap) ||
		!__xrtMapKeyValid(Key)
	) {
		if ( pValue == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	return xrtMapTake(pMap, Key, pValue);
}



/* 按插入顺序访问键值，并返回实际访问数量。 */
XRT_API size_t xrtMapVisit(xmap* pMap, xmapvisitor pVisitor, ptr pUserData)
{
	xmapiter tIterator;
	xbytesview Key;
	ptr pValue;
	size_t iVisited = 0;

	if ( (pVisitor == NULL) || !__xrtMapCanRead(pMap) ) {
		if ( pVisitor == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return 0;
	}
	if ( (pMap->Flags & XRT_MAP_FLAG_VISITING) != 0 ) {
		__xrtErrorSetInvalidState();
		return 0;
	}
	pMap->Flags |= XRT_MAP_FLAG_VISITING;
	if ( !xrtMapIterBegin(pMap, &tIterator) ) {
		pMap->Flags &= ~XRT_MAP_FLAG_VISITING;
		return 0;
	}
	while ( (pValue = xrtMapIterNext(&tIterator, &Key)) != NULL ) {
		iVisited++;
		if ( !pVisitor(Key, pValue, pUserData) ) {
			break;
		}
	}
	xrtMapIterEnd(&tIterator);
	pMap->Flags &= ~XRT_MAP_FLAG_VISITING;
	return iVisited;
}



/* 启动按插入顺序的外置迭代器。 */
XRT_API bool xrtMapIterBegin(xmap* pMap, xmapiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xmapiter));
	}
	if ( (pIterator == NULL) || !__xrtMapCanRead(pMap) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Map = pMap;
	pIterator->Next = pMap->First;
	pIterator->Version = pMap->Version;
	pIterator->Direction = 1;
	return true;
}



/* 启动按插入顺序逆序遍历的外置迭代器。 */
XRT_API bool xrtMapIterRBegin(xmap* pMap, xmapiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xmapiter));
	}
	if ( (pIterator == NULL) || !__xrtMapCanRead(pMap) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Map = pMap;
	pIterator->Next = pMap->Last;
	pIterator->Version = pMap->Version;
	pIterator->Direction = -1;
	return true;
}



/* 返回下一值槽和内部键视图，并检查结构版本。 */
XRT_API ptr xrtMapIterNext(xmapiter* pIterator, xbytesview* pKey)
{
	xmapentry* pEntry;

	if ( pKey != NULL ) {
		pKey->Data = NULL;
		pKey->Size = 0;
	}
	if ( pIterator == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pIterator->Map == NULL ) {
		return NULL;
	}
	if (
		!__xrtMapCanRead(pIterator->Map) ||
		(pIterator->Version != pIterator->Map->Version)
	) {
		if ( pIterator->Version != pIterator->Map->Version ) {
			__xrtErrorSetInvalidState();
		}
		pIterator->Map = NULL;
		pIterator->Next = NULL;
		return NULL;
	}
	pEntry = pIterator->Next;
	if ( pEntry == NULL ) {
		pIterator->Map = NULL;
		return NULL;
	}
	pIterator->Next = pIterator->Direction > 0 ?
		pEntry->OrderNext : pEntry->OrderPrev;
	if ( pKey != NULL ) {
		*pKey = __xrtMapKey(pIterator->Map, pEntry);
	}
	return __xrtMapValue(pIterator->Map, pEntry);
}



/* 提前结束迭代并清除它持有的借用状态。 */
XRT_API void xrtMapIterEnd(xmapiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}

	memset(pIterator, 0, sizeof(xmapiter));
}

#endif
