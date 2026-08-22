#include "../internal/xrt_int_map.h"



#if defined(XRT_FEATURE_INT_MAP)

#define XRT_INT_MAP_FLAG_READY 0x0001u
#define XRT_INT_MAP_FLAGS      0x0001u



/* 整数映射访问适配器保存公开回调及其用户数据。 */
typedef struct xintmapvisitcontext {
	xintmap* Map;
	xintmapvisitor Visitor;
	ptr UserData;
} xintmapvisitcontext;



/* 新值初始化适配器同时保存映射布局和公开回调上下文。 */
typedef struct xintmapinitcontext {
	xintmap* Map;
	xintmapinit Init;
	ptr UserData;
} xintmapinitcontext;



/* 把查找键与条目首部的 int64 键进行全范围比较。 */
static int __xrtIntMapCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int64 iKey = *(const int64*)pKey;
	int64 iItemKey = *(const int64*)pItem;

	(void)pUserData;
	return (iKey > iItemKey) - (iKey < iItemKey);
}



/* 将条目释放过程适配到公开的键值释放器。 */
static void __xrtIntMapDropEntry(ptr pItem, ptr pUserData)
{
	xintmap* pMap = (xintmap*)pUserData;

	if ( pMap->Drop != NULL ) {
		pMap->Drop(
			*(int64*)pItem,
			(bytes)pItem + pMap->ValueOffset,
			pMap->UserData
		);
	}
}



/* 检查整数映射布局、回调和拥有式树状态是否一致。 */
bool __xrtIntMapValid(const xintmap* pMap)
{
	if ( pMap == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pMap->Flags & XRT_INT_MAP_FLAG_READY) == 0) ||
		((pMap->Flags & ~XRT_INT_MAP_FLAGS) != 0) ||
		(pMap->ValueSize == 0) ||
		(pMap->ValueOffset < sizeof(int64)) ||
		(pMap->Alignment == 0) ||
		((pMap->Alignment & (pMap->Alignment - 1u)) != 0) ||
		((pMap->ValueOffset & (pMap->Alignment - 1u)) != 0) ||
		(pMap->ValueSize > (SIZE_MAX - pMap->ValueOffset)) ||
		(pMap->Tree.ItemSize != (pMap->ValueOffset + pMap->ValueSize)) ||
		(pMap->Tree.Compare != __xrtIntMapCompare) ||
		(pMap->Tree.Drop != __xrtIntMapDropEntry) ||
		(pMap->Tree.UserData != pMap) ||
		!__xrtAVLTreeValid(&pMap->Tree)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 检查整数映射当前是否允许修改结构和生命周期。 */
bool __xrtIntMapCanMutate(const xintmap* pMap)
{
	if ( !__xrtIntMapValid(pMap) ) {
		return false;
	}
	return __xrtAVLTreeCanMutate(&pMap->Tree);
}



/* 判断调用方字节区间是否触及映射自身或节点池存储。 */
bool __xrtIntMapOwnsRange(
	const xintmap* pMap,
	const void* pMemory,
	size_t iSize
)
{
	if ( !__xrtIntMapValid(pMap) ) {
		return false;
	}
	return __xrtRangesOverlap(pMemory, iSize, pMap, sizeof(*pMap)) ||
		__xrtAVLTreeOwnsRange(&pMap->Tree, pMemory, iSize);
}



/* 把内联条目转换为对齐后的值槽。 */
static ptr __xrtIntMapValue(const xintmap* pMap, const void* pItem)
{
	return pItem != NULL ? (ptr)((bytes)pItem + pMap->ValueOffset) : NULL;
}



/* 在已清零的新条目首部写入整数键。 */
static bool __xrtIntMapInitEntry(ptr pItem, const void* pKey, ptr pUserData)
{
	(void)pUserData;
	memcpy(pItem, pKey, sizeof(int64));
	return true;
}



/* 写入整数键并调用公开的新值初始化器。 */
static bool __xrtIntMapInitValueEntry(
	ptr pItem,
	const void* pKey,
	ptr pUserData
)
{
	xintmapinitcontext* pContext = (xintmapinitcontext*)pUserData;
	int64 iKey;

	if ( (pContext == NULL) || (pContext->Map == NULL) ||
		 (pContext->Init == NULL) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	memcpy(&iKey, pKey, sizeof(iKey));
	memcpy(pItem, &iKey, sizeof(iKey));
	return pContext->Init(
		iKey,
		__xrtIntMapValue(pContext->Map, pItem),
		pContext->UserData
	);
}



/* 把拥有式树对象访问适配为整数键和值槽访问。 */
static bool __xrtIntMapVisitEntry(ptr pItem, ptr pUserData)
{
	xintmapvisitcontext* pContext = (xintmapvisitcontext*)pUserData;

	/* 内部适配边界仍显式拒绝不完整上下文，避免传播非法地址。 */
	if (
		(pItem == NULL) ||
		(pContext == NULL) ||
		(pContext->Map == NULL) ||
		(pContext->Visitor == NULL)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return pContext->Visitor(
		*(int64*)pItem,
		__xrtIntMapValue(pContext->Map, pItem),
		pContext->UserData
	);
}



/* 按指定对齐建立条目布局和拥有式树。 */
static bool __xrtIntMapInit(
	xintmap* pMap,
	size_t iValueSize,
	size_t iAlignment
)
{
	size_t iValueOffset;
	size_t iItemSize;
	size_t iStorageAlignment;

	if (
		(pMap == NULL) ||
		(iValueSize == 0) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sizeof(int64) > (SIZE_MAX - (iAlignment - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iValueOffset = (sizeof(int64) + (iAlignment - 1u)) & ~(iAlignment - 1u);
	if ( iValueSize > (SIZE_MAX - iValueOffset) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iItemSize = iValueOffset + iValueSize;
	iStorageAlignment = iAlignment > sizeof(int64) ? iAlignment : sizeof(int64);

	memset(pMap, 0, sizeof(xintmap));
	pMap->ValueSize = iValueSize;
	pMap->ValueOffset = iValueOffset;
	pMap->Alignment = iAlignment;
	pMap->Flags = XRT_INT_MAP_FLAG_READY;
	if (
		!xrtAVLTreeInitAligned(
			&pMap->Tree,
			iItemSize,
			iStorageAlignment,
			__xrtIntMapCompare,
			pMap
		)
	) {
		memset(pMap, 0, sizeof(xintmap));
		return false;
	}
	if ( !xrtAVLTreeSetDrop(&pMap->Tree, __xrtIntMapDropEntry) ) {
		xrtAVLTreeUnit(&pMap->Tree);
		memset(pMap, 0, sizeof(xintmap));
		return false;
	}
	return true;
}



/* 返回边界查询条目的值槽和实际键。 */
static ptr __xrtIntMapBoundValue(
	xintmap* pMap,
	ptr pItem,
	int64* pActualKey
)
{
	if ( pActualKey != NULL ) {
		*pActualKey = pItem != NULL ? *(int64*)pItem : 0;
	}
	return __xrtIntMapValue(pMap, pItem);
}



/* 使用默认 16 字节值对齐初始化空整数映射。 */
XRT_API bool xrtIntMapInit(xintmap* pMap, size_t iValueSize)
{
	return __xrtIntMapInit(pMap, iValueSize, XRT_INT_MAP_ALIGNMENT_DEFAULT);
}



/* 使用显式值对齐初始化空整数映射。 */
XRT_API bool xrtIntMapInitAligned(
	xintmap* pMap,
	size_t iValueSize,
	size_t iAlignment
)
{
	return __xrtIntMapInit(pMap, iValueSize, iAlignment);
}



/* 创建使用默认 16 字节值对齐的空整数映射。 */
XRT_API xintmap* xrtIntMapCreate(size_t iValueSize)
{
	xintmap* pMap = (xintmap*)xrtMalloc(sizeof(xintmap));

	if ( pMap == NULL ) {
		return NULL;
	}
	if ( !xrtIntMapInit(pMap, iValueSize) ) {
		xrtFree(pMap);
		return NULL;
	}
	return pMap;
}



/* 创建使用显式值对齐的空整数映射。 */
XRT_API xintmap* xrtIntMapCreateAligned(size_t iValueSize, size_t iAlignment)
{
	xintmap* pMap = (xintmap*)xrtMalloc(sizeof(xintmap));

	if ( pMap == NULL ) {
		return NULL;
	}
	if ( !xrtIntMapInitAligned(pMap, iValueSize, iAlignment) ) {
		xrtFree(pMap);
		return NULL;
	}
	return pMap;
}



/* 为仍为空的映射设置值资源释放器和用户数据。 */
XRT_API bool xrtIntMapSetDrop(
	xintmap* pMap,
	xintmapdrop pDrop,
	ptr pUserData
)
{
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return false;
	}
	if ( pMap->Tree.Base.Count != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	pMap->Drop = pDrop;
	pMap->UserData = pUserData;
	return true;
}



/* 释放全部值和池页，但不释放映射结构。 */
XRT_API void xrtIntMapUnit(xintmap* pMap)
{
	if ( pMap == NULL ) {
		return;
	}
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return;
	}

	xrtAVLTreeUnit(&pMap->Tree);
	memset(pMap, 0, sizeof(xintmap));
}



/* 释放全部值、池页和映射结构。 */
XRT_API void xrtIntMapDestroy(xintmap* pMap)
{
	if ( pMap == NULL ) {
		return;
	}
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return;
	}

	xrtIntMapUnit(pMap);
	xrtFree(pMap);
}



/* 清空全部值并保留固定池的复用能力。 */
XRT_API void xrtIntMapClear(xintmap* pMap)
{
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return;
	}

	xrtAVLTreeClear(&pMap->Tree);
}



/* 释放空闲池页，并返回实际释放的页数。 */
XRT_API size_t xrtIntMapTrim(xintmap* pMap, size_t iRetainEmpty)
{
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return 0;
	}

	return xrtPoolTrim(&pMap->Tree.Pool, iRetainEmpty);
}



/* 返回当前键值数量，非法映射返回零。 */
XRT_API size_t xrtIntMapCount(const xintmap* pMap)
{
	return __xrtIntMapValid(pMap) ? pMap->Tree.Base.Count : 0;
}



/* 返回已有值槽，或原地创建并清零一个新值槽。 */
XRT_API ptr xrtIntMapGetOrAdd(xintmap* pMap, int64 iKey, bool* pNew)
{
	ptr pItem;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return NULL;
	}
	pItem = __xrtAVLTreeGetOrAdd(
		&pMap->Tree,
		&iKey,
		__xrtIntMapInitEntry,
		NULL,
		NULL,
		NULL,
		pNew
	);
	return __xrtIntMapValue(pMap, pItem);
}



/* 返回已有值槽，或失败原子地原位初始化一个新值。 */
XRT_API ptr xrtIntMapGetOrInit(
	xintmap* pMap,
	int64 iKey,
	xintmapinit pInit,
	ptr pUserData,
	bool* pNew
)
{
	xintmapinitcontext Context;
	ptr pItem;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtIntMapCanMutate(pMap) || (pInit == NULL) ) {
		if ( pInit == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	Context.Map = pMap;
	Context.Init = pInit;
	Context.UserData = pUserData;
	pItem = __xrtAVLTreeGetOrAdd(
		&pMap->Tree,
		&iKey,
		__xrtIntMapInitValueEntry,
		&Context,
		__xrtIntMapDropEntry,
		pMap,
		pNew
	);
	return __xrtIntMapValue(pMap, pItem);
}



/* 复制插入或替换值，不允许从同一映射的其他值槽浅拷贝。 */
XRT_API bool xrtIntMapSet(xintmap* pMap, int64 iKey, const void* pValue)
{
	ptr pItem;
	ptr pStored;

	if ( !__xrtIntMapCanMutate(pMap) || (pValue == NULL) ) {
		if ( pValue == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pItem = xrtAVLTreeFind(&pMap->Tree, &iKey);
	pStored = __xrtIntMapValue(pMap, pItem);
	if ( pValue == pStored ) {
		return true;
	}
	if ( __xrtAVLTreeOwnsRange(
		&pMap->Tree, pValue, pMap->ValueSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	if ( pItem == NULL ) {
		pStored = xrtIntMapGetOrAdd(pMap, iKey, NULL);
		if ( pStored == NULL ) {
			return false;
		}
	} else if ( pMap->Drop != NULL ) {
		__xrtAVLTreeDropItem(&pMap->Tree, pItem);
	}
	memcpy(pStored, pValue, pMap->ValueSize);
	return true;
}



/* 返回指定键的可写值槽，未找到是正常结果。 */
XRT_API ptr xrtIntMapGet(xintmap* pMap, int64 iKey)
{
	if ( !__xrtIntMapValid(pMap) ) {
		return NULL;
	}
	return __xrtIntMapValue(pMap, xrtAVLTreeFind(&pMap->Tree, &iKey));
}



/* 返回指定键的只读值槽，未找到是正常结果。 */
XRT_API const void* xrtIntMapConstGet(const xintmap* pMap, int64 iKey)
{
	if ( !__xrtIntMapValid(pMap) ) {
		return NULL;
	}
	return __xrtIntMapValue(pMap, xrtAVLTreeConstFind(&pMap->Tree, &iKey));
}



/* 判断指定整数键是否存在。 */
XRT_API bool xrtIntMapHas(const xintmap* pMap, int64 iKey)
{
	if ( !__xrtIntMapValid(pMap) ) {
		return false;
	}
	return xrtAVLTreeHas(&pMap->Tree, &iKey);
}



/* 删除指定键并调用值释放器。 */
XRT_API bool xrtIntMapRemove(xintmap* pMap, int64 iKey)
{
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return false;
	}
	return xrtAVLTreeRemove(&pMap->Tree, &iKey);
}



/* 将指定键的值字节移交给调用方后删除，不调用值释放器。 */
XRT_API bool xrtIntMapTake(xintmap* pMap, int64 iKey, ptr pValue)
{
	if ( !__xrtIntMapCanMutate(pMap) ) {
		return false;
	}
	return __xrtAVLTreeTakePart(
		&pMap->Tree,
		&iKey,
		pMap->ValueOffset,
		pMap->ValueSize,
		pValue
	);
}



/* 检查映射是否使用指针大小值槽。 */
static bool __xrtIntMapPtrValid(const xintmap* pMap)
{
	if ( !__xrtIntMapValid(pMap) ) {
		return false;
	}
	if ( pMap->ValueSize != sizeof(ptr) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 对 sizeof(ptr) 值映射执行指针类型友好的插入或替换。 */
XRT_API bool xrtIntMapSetPtr(xintmap* pMap, int64 iKey, ptr pValue)
{
	ptr* pStored;

	if ( !__xrtIntMapCanMutate(pMap) ) {
		return false;
	}
	if ( pMap->ValueSize != sizeof(ptr) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pStored = (ptr*)xrtIntMapGet(pMap, iKey);
	if ( (pStored != NULL) && (*pStored == pValue) ) {
		return true;
	}
	return xrtIntMapSet(pMap, iKey, &pValue);
}



/* 返回 sizeof(ptr) 值映射中保存的指针。 */
XRT_API ptr xrtIntMapGetPtr(xintmap* pMap, int64 iKey)
{
	ptr* pValue;

	if ( !__xrtIntMapPtrValid(pMap) ) {
		return NULL;
	}
	pValue = (ptr*)xrtIntMapGet(pMap, iKey);
	return pValue != NULL ? *pValue : NULL;
}



/* 从 sizeof(ptr) 值映射中移交指针，不调用值释放器。 */
XRT_API bool xrtIntMapTakePtr(xintmap* pMap, int64 iKey, ptr* pValue)
{
	if ( pValue != NULL ) {
		*pValue = NULL;
	}
	if ( (pValue == NULL) || !__xrtIntMapPtrValid(pMap) ) {
		if ( pValue == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	return xrtIntMapTake(pMap, iKey, pValue);
}



/* 返回顺序第一项的值槽，并可选返回键。 */
XRT_API ptr xrtIntMapFirst(xintmap* pMap, int64* pKey)
{
	ptr pItem;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtIntMapValid(pMap) ) {
		return NULL;
	}
	pItem = xrtAVLTreeFirst(&pMap->Tree);
	return __xrtIntMapBoundValue(pMap, pItem, pKey);
}



/* 返回顺序最后一项的值槽，并可选返回键。 */
XRT_API ptr xrtIntMapLast(xintmap* pMap, int64* pKey)
{
	ptr pItem;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtIntMapValid(pMap) ) {
		return NULL;
	}
	pItem = xrtAVLTreeLast(&pMap->Tree);
	return __xrtIntMapBoundValue(pMap, pItem, pKey);
}



/* 返回第一个不小于指定键的值槽和实际键。 */
XRT_API ptr xrtIntMapLowerBound(xintmap* pMap, int64 iKey, int64* pActualKey)
{
	ptr pItem;

	if ( pActualKey != NULL ) {
		*pActualKey = 0;
	}
	if ( !__xrtIntMapValid(pMap) ) {
		return NULL;
	}
	pItem = xrtAVLTreeLowerBound(&pMap->Tree, &iKey);
	return __xrtIntMapBoundValue(pMap, pItem, pActualKey);
}



/* 返回第一个严格大于指定键的值槽和实际键。 */
XRT_API ptr xrtIntMapUpperBound(xintmap* pMap, int64 iKey, int64* pActualKey)
{
	ptr pItem;

	if ( pActualKey != NULL ) {
		*pActualKey = 0;
	}
	if ( !__xrtIntMapValid(pMap) ) {
		return NULL;
	}
	pItem = xrtAVLTreeUpperBound(&pMap->Tree, &iKey);
	return __xrtIntMapBoundValue(pMap, pItem, pActualKey);
}



/* 按键升序访问值，并返回实际访问数量。 */
XRT_API size_t xrtIntMapVisit(
	xintmap* pMap,
	xintmapvisitor pVisitor,
	ptr pUserData
)
{
	xintmapvisitcontext tContext;

	if ( !__xrtIntMapCanMutate(pMap) || (pVisitor == NULL) ) {
		if ( pVisitor == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return 0;
	}

	tContext.Map = pMap;
	tContext.Visitor = pVisitor;
	tContext.UserData = pUserData;
	return xrtAVLTreeVisit(
		&pMap->Tree,
		__xrtIntMapVisitEntry,
		&tContext
	);
}



/* 启动按键升序的外置迭代器。 */
XRT_API bool xrtIntMapIterBegin(xintmap* pMap, xintmapiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xintmapiter));
	}
	if ( (pIterator == NULL) || !__xrtIntMapValid(pMap) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Map = pMap;
	if ( !xrtAVLTreeIterBegin(&pMap->Tree, &pIterator->Base) ) {
		pIterator->Map = NULL;
		return false;
	}
	return true;
}



/* 启动按键降序的外置迭代器。 */
XRT_API bool xrtIntMapIterRBegin(xintmap* pMap, xintmapiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xintmapiter));
	}
	if ( (pIterator == NULL) || !__xrtIntMapValid(pMap) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Map = pMap;
	if ( !xrtAVLTreeIterRBegin(&pMap->Tree, &pIterator->Base) ) {
		pIterator->Map = NULL;
		return false;
	}
	return true;
}



/* 从第一个不小于指定键的条目开始升序迭代。 */
XRT_API bool xrtIntMapIterFrom(
	xintmap* pMap,
	int64 iKey,
	xintmapiter* pIterator
)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xintmapiter));
	}
	if ( (pIterator == NULL) || !__xrtIntMapValid(pMap) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Map = pMap;
	if ( !xrtAVLTreeIterFrom(&pMap->Tree, &iKey, &pIterator->Base) ) {
		pIterator->Map = NULL;
		return false;
	}
	return true;
}



/* 从第一个不大于指定键的条目开始降序迭代。 */
XRT_API bool xrtIntMapIterRFrom(
	xintmap* pMap,
	int64 iKey,
	xintmapiter* pIterator
)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xintmapiter));
	}
	if ( (pIterator == NULL) || !__xrtIntMapValid(pMap) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Map = pMap;
	if ( !xrtAVLTreeIterRFrom(&pMap->Tree, &iKey, &pIterator->Base) ) {
		pIterator->Map = NULL;
		return false;
	}
	return true;
}



/* 返回下一值槽和键，并在结构修改时由 AVL 版本检查终止。 */
XRT_API ptr xrtIntMapIterNext(xintmapiter* pIterator, int64* pKey)
{
	ptr pItem;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( (pIterator == NULL) || (pIterator->Map == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pItem = xrtAVLTreeIterNext(&pIterator->Base);
	if ( pItem == NULL ) {
		pIterator->Map = NULL;
		return NULL;
	}
	if ( pKey != NULL ) {
		*pKey = *(int64*)pItem;
	}
	return __xrtIntMapValue(pIterator->Map, pItem);
}



/* 提前结束迭代并清除它持有的借用状态。 */
XRT_API void xrtIntMapIterEnd(xintmapiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}

	xrtAVLTreeIterEnd(&pIterator->Base);
	pIterator->Map = NULL;
}

#endif
