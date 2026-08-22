#ifndef XRT_TYPED_LIST_H
#define XRT_TYPED_LIST_H

#include <xrt/map.h>
#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_TYPED_LIST) && !defined(XRT_FEATURE_INT_MAP)
	#error "XRUNTIME_FEATURE_TYPED_LIST requires XRT_FEATURE_INT_MAP"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_LIST requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST)

/* 类型列表按 int64 键排序保存稳定地址值，并拥有每一个值的生命周期。 */
typedef struct xtypedlist {
	xintmap Storage;
	const xrttype* ItemType;
	uint32 Flags;
} xtypedlist;



/* 类型列表外置迭代器按键升序或降序借用稳定值槽。 */
typedef struct xtypedlistiter {
	xintmapiter Base;
	xtypedlist* List;
} xtypedlistiter;



/* 类型列表模块稳定错误代码。 */
typedef enum xtypedlisterror {
	XTYPED_LIST_ERROR_ARGUMENT = 1,
	XTYPED_LIST_ERROR_TYPE,
	XTYPED_LIST_ERROR_KEY,
	XTYPED_LIST_ERROR_OPERATION,
	XTYPED_LIST_ERROR_STATE
} xtypedlisterror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有类型值的空稀疏列表。 */
XRT_API bool xrtTypedListInit(
	xtypedlist* pList,
	const xrttype* pItemType
);
XRT_API xtypedlist* xrtTypedListCreate(const xrttype* pItemType);
XRT_API void xrtTypedListUnit(xtypedlist* pList);
XRT_API void xrtTypedListDestroy(xtypedlist* pList);



/* 返回借用元素类型和当前键值数量。 */
XRT_API const xrttype* xrtTypedListItemType(const xtypedlist* pList);
XRT_API size_t xrtTypedListCount(const xtypedlist* pList);



/* 清空全部值，或释放空闲节点池页。 */
XRT_API bool xrtTypedListClear(xtypedlist* pList);
XRT_API size_t xrtTypedListTrim(xtypedlist* pList, size_t iRetainEmpty);



/* 复制设置指定键，或在最大键后追加并返回实际键。 */
XRT_API bool xrtTypedListSet(
	xtypedlist* pList,
	int64 iKey,
	const void* pItem
);
XRT_API bool xrtTypedListAppend(
	xtypedlist* pList,
	const void* pItem,
	int64* pKey
);



/* 返回指定键的借用值槽；缺失键是正常结果。 */
XRT_API ptr xrtTypedListGet(xtypedlist* pList, int64 iKey);
XRT_API const void* xrtTypedListConstGet(
	const xtypedlist* pList,
	int64 iKey
);
XRT_API bool xrtTypedListHas(const xtypedlist* pList, int64 iKey);



/* 按键顺序返回指定位置的借用值槽和可选实际键。 */
XRT_API ptr xrtTypedListAt(
	xtypedlist* pList,
	size_t iIndex,
	int64* pKey
);
XRT_API const void* xrtTypedListConstAt(
	const xtypedlist* pList,
	size_t iIndex,
	int64* pKey
);



/* 删除指定键，或把值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedListRemove(xtypedlist* pList, int64 iKey);
XRT_API bool xrtTypedListTake(
	xtypedlist* pList,
	int64 iKey,
	ptr pValue
);



/* 查找第一个相等值或判断是否存在；未找到不设置错误。 */
XRT_API bool xrtTypedListFind(
	const xtypedlist* pList,
	const void* pItem,
	int64* pKey
);
XRT_API bool xrtTypedListContains(
	const xtypedlist* pList,
	const void* pItem
);



/* 失败原子地合并同类型列表、深复制列表或比较键值内容。 */
XRT_API bool xrtTypedListMerge(
	xtypedlist* pTarget,
	const xtypedlist* pSource,
	bool bReplace
);
XRT_API xtypedlist* xrtTypedListClone(const xtypedlist* pList);
XRT_API bool xrtTypedListEquals(
	const xtypedlist* pLeft,
	const xtypedlist* pRight
);



/* 启动完整或有界的正反迭代。 */
XRT_API bool xrtTypedListIterBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
);
XRT_API bool xrtTypedListIterRBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
);
XRT_API bool xrtTypedListIterFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
);
XRT_API bool xrtTypedListIterRFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
);
XRT_API ptr xrtTypedListIterNext(
	xtypedlistiter* pIterator,
	int64* pKey
);
XRT_API void xrtTypedListIterEnd(xtypedlistiter* pIterator);



/* 验证对象列表类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedListTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedListInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif
