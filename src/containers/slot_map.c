#include "../internal/xrt_slot_map.h"



#if defined(XRT_FEATURE_SLOT_MAP)

/* 推进结构版本并保留零值作为未初始化状态。 */
static void __xrtSlotMapVersion(xslotmap* pMap)
{
	pMap->Version++;
	if ( pMap->Version == 0 ) {
		pMap->Version = 1;
	}
}



/* 将槽索引和非零代际编码成公开句柄。 */
static xslot __xrtSlotPack(uint32 iIndex, uint32 iGeneration)
{
	return (
		((xslot)iGeneration << 32u) |
		((xslot)iIndex + 1u)
	);
}



/* 解码公开句柄，零值和保留的零代际永远无效。 */
static bool __xrtSlotDecode(xslot Slot, uint32* pIndex, uint32* pGeneration)
{
	uint32 iEncodedIndex = (uint32)Slot;
	uint32 iGeneration = (uint32)(Slot >> 32u);

	if ( (iEncodedIndex == 0) || (iGeneration == 0) ) {
		return false;
	}
	if ( pIndex != NULL ) {
		*pIndex = iEncodedIndex - 1u;
	}
	if ( pGeneration != NULL ) {
		*pGeneration = iGeneration;
	}
	return true;
}



/* 返回指定内部索引的槽记录。 */
static xslotentry* __xrtSlotMapEntry(xslotmap* pMap, uint32 iIndex)
{
	return (xslotentry*)pMap->Storage.Data + iIndex;
}



/* 返回指定内部索引的只读槽记录。 */
static const xslotentry* __xrtSlotMapConstEntry(
	const xslotmap* pMap,
	uint32 iIndex
)
{
	return (const xslotentry*)pMap->Storage.Data + iIndex;
}



/* 检查公开槽表状态和空闲链表头是否自洽。 */
static bool __xrtSlotMapValid(const xslotmap* pMap)
{
	const xslotentry* pFree;

	if ( pMap == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtArrayValid(&pMap->Storage) ) {
		return false;
	}
	if (
		(pMap->Storage.ItemSize != sizeof(xslotentry)) ||
		(pMap->Storage.Count > UINT32_MAX) ||
		(pMap->Count > pMap->Storage.Count) ||
		(pMap->Version == 0) ||
		(pMap->Reserved != 0) ||
		(
			(pMap->FreeSlot != XRT_SLOT_INDEX_INVALID) &&
			((size_t)pMap->FreeSlot >= pMap->Storage.Count)
		)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pMap->FreeSlot == XRT_SLOT_INDEX_INVALID ) {
		return true;
	}

	pFree = __xrtSlotMapConstEntry(pMap, pMap->FreeSlot);
	if ( (pFree->Value != NULL) || (pFree->Generation == 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 查找句柄对应的活动槽，可选报告陈旧句柄错误。 */
static xslotentry* __xrtSlotMapFind(
	const xslotmap* pMap,
	xslot Slot,
	bool bReport
)
{
	const xslotentry* pEntry;
	uint32 iIndex;
	uint32 iGeneration;

	if (
		!__xrtSlotDecode(Slot, &iIndex, &iGeneration) ||
		((size_t)iIndex >= pMap->Storage.Count)
	) {
		if ( bReport ) {
			__xrtErrorSetRange();
		}
		return NULL;
	}
	pEntry = __xrtSlotMapConstEntry(pMap, iIndex);
	if (
		(pEntry->Value == NULL) ||
		(pEntry->Generation != iGeneration)
	) {
		if ( bReport ) {
			__xrtErrorSetRange();
		}
		return NULL;
	}
	return (xslotentry*)pEntry;
}



/* 返回句柄中的零基槽索引。 */
XRT_API uint32 xrtSlotIndex(xslot Slot)
{
	uint32 iIndex;

	return __xrtSlotDecode(Slot, &iIndex, NULL) ?
		iIndex : XRT_SLOT_INDEX_INVALID;
}



/* 返回句柄中的非零代际。 */
XRT_API uint32 xrtSlotGeneration(xslot Slot)
{
	uint32 iGeneration;

	return __xrtSlotDecode(Slot, NULL, &iGeneration) ?
		iGeneration : 0;
}



/* 初始化调用方持有的空槽表。 */
XRT_API bool xrtSlotMapInit(xslotmap* pMap)
{
	if ( pMap == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	memset(pMap, 0, sizeof(xslotmap));
	if ( !xrtArrayInit(&pMap->Storage, sizeof(xslotentry)) ) {
		return false;
	}
	pMap->FreeSlot = XRT_SLOT_INDEX_INVALID;
	pMap->Version = 1;
	return true;
}



/* 创建堆上的空槽表。 */
XRT_API xslotmap* xrtSlotMapCreate(void)
{
	xslotmap* pMap = (xslotmap*)xrtMalloc(sizeof(xslotmap));

	if ( pMap == NULL ) {
		return NULL;
	}
	if ( !xrtSlotMapInit(pMap) ) {
		xrtFree(pMap);
		return NULL;
	}
	return pMap;
}



/* 释放槽表存储，但不释放槽内指针指向的对象。 */
XRT_API void xrtSlotMapUnit(xslotmap* pMap)
{
	if ( pMap == NULL ) {
		return;
	}

	xrtArrayUnit(&pMap->Storage);
	memset(pMap, 0, sizeof(xslotmap));
}



/* 释放槽表存储和槽表结构，但不释放槽内对象。 */
XRT_API void xrtSlotMapDestroy(xslotmap* pMap)
{
	if ( pMap == NULL ) {
		return;
	}

	xrtSlotMapUnit(pMap);
	xrtFree(pMap);
}



/* 清空活动槽、推进代际并重建空闲链表。 */
XRT_API void xrtSlotMapClear(xslotmap* pMap)
{
	bool bChanged = false;

	if ( !__xrtSlotMapValid(pMap) ) {
		return;
	}

	pMap->FreeSlot = XRT_SLOT_INDEX_INVALID;
	for ( size_t i = pMap->Storage.Count; i != 0; i-- ) {
		uint32 iIndex = (uint32)(i - 1u);
		xslotentry* pEntry = __xrtSlotMapEntry(pMap, iIndex);

		if ( pEntry->Value != NULL ) {
			bChanged = true;
			pEntry->Value = NULL;
			if ( pEntry->Generation == XRT_SLOT_GENERATION_MAX ) {
				pEntry->Generation = 0;
			} else {
				pEntry->Generation++;
			}
		}
		if ( pEntry->Generation == 0 ) {
			pEntry->NextFree = XRT_SLOT_INDEX_INVALID;
			continue;
		}
		pEntry->NextFree = pMap->FreeSlot;
		pMap->FreeSlot = iIndex;
	}
	pMap->Count = 0;
	if ( bChanged ) {
		__xrtSlotMapVersion(pMap);
	}
}



/* 保证槽表至少具有指定存储容量。 */
XRT_API bool xrtSlotMapReserve(xslotmap* pMap, size_t iCapacity)
{
	if ( !__xrtSlotMapValid(pMap) ) {
		return false;
	}
	if ( iCapacity > UINT32_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	return __xrtArrayReserveValid(&pMap->Storage, iCapacity);
}



/* 插入非空指针，优先以常数时间复用最近释放的槽。 */
XRT_API xslot xrtSlotMapInsert(xslotmap* pMap, ptr pValue)
{
	const xslotentry* pNext;
	xslotentry* pEntry;
	uint32 iIndex;

	if ( !__xrtSlotMapValid(pMap) ) {
		return XRT_SLOT_INVALID;
	}
	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XRT_SLOT_INVALID;
	}

	if ( pMap->FreeSlot != XRT_SLOT_INDEX_INVALID ) {
		iIndex = pMap->FreeSlot;
		pEntry = __xrtSlotMapEntry(pMap, iIndex);
		if ( pEntry->NextFree != XRT_SLOT_INDEX_INVALID ) {
			if ( (size_t)pEntry->NextFree >= pMap->Storage.Count ) {
				__xrtErrorSetInvalidState();
				return XRT_SLOT_INVALID;
			}
			pNext = __xrtSlotMapConstEntry(pMap, pEntry->NextFree);
			if ( (pNext->Value != NULL) || (pNext->Generation == 0) ) {
				__xrtErrorSetInvalidState();
				return XRT_SLOT_INVALID;
			}
		}
		pMap->FreeSlot = pEntry->NextFree;
	} else {
		if ( pMap->Storage.Count >= UINT32_MAX ) {
			__xrtErrorSetSizeOverflow();
			return XRT_SLOT_INVALID;
		}
		iIndex = (uint32)pMap->Storage.Count;
		pEntry = (xslotentry*)__xrtArrayAddValid(&pMap->Storage, 1);
		if ( pEntry == NULL ) {
			return XRT_SLOT_INVALID;
		}
		pEntry->Generation = 1;
	}

	pEntry->Value = pValue;
	pEntry->NextFree = XRT_SLOT_INDEX_INVALID;
	pMap->Count++;
	__xrtSlotMapVersion(pMap);
	return __xrtSlotPack(iIndex, pEntry->Generation);
}



/* 返回有效句柄对应的指针。 */
XRT_API ptr xrtSlotMapGet(const xslotmap* pMap, xslot Slot)
{
	xslotentry* pEntry;

	if ( !__xrtSlotMapValid(pMap) ) {
		return NULL;
	}
	pEntry = __xrtSlotMapFind(pMap, Slot, true);
	return pEntry != NULL ? pEntry->Value : NULL;
}



/* 判断句柄当前是否仍指向活动槽。 */
XRT_API bool xrtSlotMapContains(const xslotmap* pMap, xslot Slot)
{
	if ( !__xrtSlotMapValid(pMap) ) {
		return false;
	}

	return __xrtSlotMapFind(pMap, Slot, false) != NULL;
}



/* 替换有效槽中的非空指针。 */
XRT_API bool xrtSlotMapSet(xslotmap* pMap, xslot Slot, ptr pValue)
{
	xslotentry* pEntry;

	if ( !__xrtSlotMapValid(pMap) ) {
		return false;
	}
	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pEntry = __xrtSlotMapFind(pMap, Slot, true);
	if ( pEntry == NULL ) {
		return false;
	}

	pEntry->Value = pValue;
	return true;
}



/* 删除有效槽并让旧句柄永久失效。 */
XRT_API bool xrtSlotMapRemove(xslotmap* pMap, xslot Slot, ptr* pValue)
{
	xslotentry* pEntry;
	ptr pRemoved;
	uint32 iIndex;
	size_t iStorageBytes;

	if ( !__xrtSlotMapValid(pMap) ) {
		return false;
	}
	if ( pValue != NULL ) {
		iStorageBytes = pMap->Storage.Capacity * pMap->Storage.ItemSize;
		if (
			__xrtRangesOverlap(
				pValue,
				sizeof(ptr),
				pMap->Storage.Data,
				iStorageBytes
			)
		) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	pEntry = __xrtSlotMapFind(pMap, Slot, true);
	if ( pEntry == NULL ) {
		return false;
	}
	if ( pMap->Count == 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	iIndex = xrtSlotIndex(Slot);
	pRemoved = pEntry->Value;
	pEntry->Value = NULL;
	if ( pEntry->Generation == XRT_SLOT_GENERATION_MAX ) {
		pEntry->Generation = 0;
		pEntry->NextFree = XRT_SLOT_INDEX_INVALID;
	} else {
		pEntry->Generation++;
		pEntry->NextFree = pMap->FreeSlot;
		pMap->FreeSlot = iIndex;
	}
	pMap->Count--;
	__xrtSlotMapVersion(pMap);
	if ( pValue != NULL ) {
		*pValue = pRemoved;
	}
	return true;
}



/* 启动按槽索引递增的外置迭代器。 */
XRT_API bool xrtSlotMapIterBegin(const xslotmap* pMap, xslotmapiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xslotmapiter));
	}
	if ( (pIterator == NULL) || !__xrtSlotMapValid(pMap) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	pIterator->Map = pMap;
	pIterator->Version = pMap->Version;
	return true;
}



/* 返回下一个活动指针和与其匹配的稳定句柄。 */
XRT_API ptr xrtSlotMapIterNext(xslotmapiter* pIterator, xslot* pSlot)
{
	const xslotentry* pEntry;

	if ( pSlot != NULL ) {
		*pSlot = XRT_SLOT_INVALID;
	}
	if ( pIterator == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pIterator->Map == NULL ) {
		return NULL;
	}
	if (
		!__xrtSlotMapValid(pIterator->Map) ||
		(pIterator->Version != pIterator->Map->Version)
	) {
		if ( pIterator->Version != pIterator->Map->Version ) {
			__xrtErrorSetInvalidState();
		}
		pIterator->Map = NULL;
		pIterator->Next = 0;
		return NULL;
	}

	while ( pIterator->Next < pIterator->Map->Storage.Count ) {
		uint32 iIndex = (uint32)pIterator->Next++;

		pEntry = __xrtSlotMapConstEntry(pIterator->Map, iIndex);
		if ( pEntry->Value == NULL ) {
			continue;
		}
		if ( pSlot != NULL ) {
			*pSlot = __xrtSlotPack(iIndex, pEntry->Generation);
		}
		return pEntry->Value;
	}

	pIterator->Map = NULL;
	pIterator->Next = 0;
	return NULL;
}



/* 提前结束迭代并清除借用状态。 */
XRT_API void xrtSlotMapIterEnd(xslotmapiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}

	memset(pIterator, 0, sizeof(xslotmapiter));
}

#endif
