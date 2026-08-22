#ifndef XRT_TYPED_DICT_H
#define XRT_TYPED_DICT_H

#include <xrt/map.h>
#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_TYPED_DICT) && !defined(XRT_FEATURE_MAP)
	#error "XRUNTIME_FEATURE_TYPED_DICT requires XRT_FEATURE_MAP"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_DICT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 类型字典复制保存文本键，并拥有每一个运行时类型值。 */
typedef struct xtypeddict {
	xmap Storage;
	const xrttype* ItemType;
} xtypeddict;



/* 类型字典外置迭代器按稳定插入顺序借用键和值槽。 */
typedef struct xtypeddictiter {
	xmapiter Base;
	xtypeddict* Dict;
} xtypeddictiter;



/* 类型字典模块稳定错误代码。 */
typedef enum xtypeddicterror {
	XTYPED_DICT_ERROR_ARGUMENT = 1,
	XTYPED_DICT_ERROR_TYPE,
	XTYPED_DICT_ERROR_RANGE,
	XTYPED_DICT_ERROR_OPERATION,
	XTYPED_DICT_ERROR_STATE
} xtypeddicterror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有类型值的空字典。 */
XRT_API bool xrtTypedDictInit(
	xtypeddict* pDict,
	const xrttype* pItemType
);
XRT_API xtypeddict* xrtTypedDictCreate(const xrttype* pItemType);
XRT_API void xrtTypedDictUnit(xtypeddict* pDict);
XRT_API void xrtTypedDictDestroy(xtypeddict* pDict);



/* 返回借用元素类型、当前键数和再次扩容前的容量。 */
XRT_API const xrttype* xrtTypedDictItemType(const xtypeddict* pDict);
XRT_API size_t xrtTypedDictCount(const xtypeddict* pDict);
XRT_API size_t xrtTypedDictCapacity(const xtypeddict* pDict);



/* 清空、预留或裁剪字典存储。 */
XRT_API bool xrtTypedDictClear(xtypeddict* pDict);
XRT_API bool xrtTypedDictReserve(xtypeddict* pDict, size_t iCapacity);
XRT_API bool xrtTypedDictTrim(xtypeddict* pDict);



/* 返回已有值槽，或按元素类型默认初始化一个新值。 */
XRT_API ptr xrtTypedDictGetOrAdd(
	xtypeddict* pDict,
	xstrview Key,
	bool* pNew
);



/* 失败原子地复制插入或替换一个键值。 */
XRT_API bool xrtTypedDictSet(
	xtypeddict* pDict,
	xstrview Key,
	const void* pItem
);



/* 成功时把外部已初始化值移交给字典，并把来源恢复为类型空值。 */
XRT_API bool xrtTypedDictSetTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pItem
);



/* 返回指定键的可写或只读借用值槽；缺失是正常结果。 */
XRT_API ptr xrtTypedDictGet(xtypeddict* pDict, xstrview Key);
XRT_API const void* xrtTypedDictConstGet(
	const xtypeddict* pDict,
	xstrview Key
);
XRT_API bool xrtTypedDictHas(const xtypeddict* pDict, xstrview Key);



/* 返回与查询等价的内部规范键视图，缺失时清空输出。 */
XRT_API bool xrtTypedDictStoredKey(
	const xtypeddict* pDict,
	xstrview Key,
	xstrview* pStoredKey
);



/* 删除指定键，或把值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedDictRemove(xtypeddict* pDict, xstrview Key);
XRT_API bool xrtTypedDictTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pValue
);



/* 按插入顺序返回指定位置的借用键和值，复杂度为 O(n)。 */
XRT_API ptr xrtTypedDictAt(
	xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
);
XRT_API const void* xrtTypedDictConstAt(
	const xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
);



/* 启动按插入顺序或逆序的外置迭代。 */
XRT_API bool xrtTypedDictIterBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
);
XRT_API bool xrtTypedDictIterRBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
);
XRT_API ptr xrtTypedDictIterNext(
	xtypeddictiter* pIterator,
	xstrview* pKey
);
XRT_API void xrtTypedDictIterEnd(xtypeddictiter* pIterator);



/* 事务合并同类型字典，并深复制创建独立字典。 */
XRT_API bool xrtTypedDictMerge(
	xtypeddict* pTarget,
	const xtypeddict* pSource,
	bool bReplace
);
XRT_API xtypeddict* xrtTypedDictClone(const xtypeddict* pDict);



/* 比较两个字典的精确类型身份、键集合和值内容。 */
XRT_API bool xrtTypedDictEquals(
	const xtypeddict* pLeft,
	const xtypeddict* pRight
);



/* 验证对象字典类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedDictTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedDictInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif
