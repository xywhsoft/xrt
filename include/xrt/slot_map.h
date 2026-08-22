#ifndef XRT_SLOT_MAP_H
#define XRT_SLOT_MAP_H

#include <xrt/array.h>



#if defined(XRT_FEATURE_SLOT_MAP) && !defined(XRT_FEATURE_ARRAY)
	#error "XRT_FEATURE_SLOT_MAP requires XRT_FEATURE_ARRAY"
#endif



#if defined(XRT_FEATURE_SLOT_MAP)

/* 槽句柄同时携带零基槽索引和代际，零值永远无效。 */
typedef uint64 xslot;



#define XRT_SLOT_INVALID			((xslot)0)
#define XRT_SLOT_INDEX_INVALID	UINT32_MAX



/* 槽表保存非空借用指针，并通过代际阻止陈旧句柄命中新对象。 */
typedef struct xslotmap {
	xarray Storage;
	size_t Count;
	uint64 Version;
	uint32 FreeSlot;
	uint32 Reserved;
} xslotmap;



/* 外置迭代器按槽索引递增遍历，结构性修改会使它失效。 */
typedef struct xslotmapiter {
	const xslotmap* Map;
	size_t Next;
	uint64 Version;
} xslotmapiter;



XRT_EXTERN_C_BEGIN



/* 返回句柄中的零基槽索引，无效句柄返回 XRT_SLOT_INDEX_INVALID。 */
XRT_API uint32 xrtSlotIndex(xslot Slot);



/* 返回句柄中的代际，无效句柄返回零。 */
XRT_API uint32 xrtSlotGeneration(xslot Slot);



/* 初始化调用方持有的空槽表。 */
XRT_API bool xrtSlotMapInit(xslotmap* pMap);



/* 创建堆上的空槽表。 */
XRT_API xslotmap* xrtSlotMapCreate(void);



/* 释放槽表存储，但不释放槽内指针指向的对象。 */
XRT_API void xrtSlotMapUnit(xslotmap* pMap);



/* 释放槽表存储和槽表结构，但不释放槽内对象。 */
XRT_API void xrtSlotMapDestroy(xslotmap* pMap);



/* 清空全部活动槽并使已有句柄失效，同时保留已分配容量。 */
XRT_API void xrtSlotMapClear(xslotmap* pMap);



/* 保证槽表至少具有指定存储容量。 */
XRT_API bool xrtSlotMapReserve(xslotmap* pMap, size_t iCapacity);



/* 插入非空指针并返回稳定代际句柄，失败返回 XRT_SLOT_INVALID。 */
XRT_API xslot xrtSlotMapInsert(xslotmap* pMap, ptr pValue);



/* 返回有效句柄对应的指针，陈旧或不存在的句柄返回空指针。 */
XRT_API ptr xrtSlotMapGet(const xslotmap* pMap, xslot Slot);



/* 判断句柄当前是否仍指向活动槽，句柄失效不是错误。 */
XRT_API bool xrtSlotMapContains(const xslotmap* pMap, xslot Slot);



/* 替换有效槽中的非空指针，句柄和迭代顺序保持不变。 */
XRT_API bool xrtSlotMapSet(xslotmap* pMap, xslot Slot, ptr pValue);



/* 删除有效槽并可返回原指针，删除后旧句柄永久失效。 */
XRT_API bool xrtSlotMapRemove(xslotmap* pMap, xslot Slot, ptr* pValue);



/* 启动按槽索引递增的外置迭代器。 */
XRT_API bool xrtSlotMapIterBegin(const xslotmap* pMap, xslotmapiter* pIterator);



/* 返回下一个活动指针，并可返回与其匹配的稳定句柄。 */
XRT_API ptr xrtSlotMapIterNext(xslotmapiter* pIterator, xslot* pSlot);



/* 提前结束迭代并清除借用状态。 */
XRT_API void xrtSlotMapIterEnd(xslotmapiter* pIterator);



XRT_EXTERN_C_END

#endif

#endif
