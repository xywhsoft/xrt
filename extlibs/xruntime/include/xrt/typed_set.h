#ifndef XRT_TYPED_SET_H
#define XRT_TYPED_SET_H

#include <xrt/runtime_type.h>
#include <xrt/set.h>



#if defined(XRUNTIME_FEATURE_TYPED_SET) && !defined(XRT_FEATURE_SET)
	#error "XRUNTIME_FEATURE_TYPED_SET requires XRT_FEATURE_SET"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_SET requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET)

/* 类型集合按类型散列和比较规则保存唯一值，并拥有每一个规范值。 */
typedef struct xtypedset {
	xset Storage;
	const xrttype* ItemType;
} xtypedset;



/* 类型集合外置迭代器按稳定插入顺序借用只读规范值。 */
typedef struct xtypedsetiter {
	xsetiter Base;
	xtypedset* Set;
} xtypedsetiter;



/* 类型集合模块稳定错误代码。 */
typedef enum xtypedseterror {
	XTYPED_SET_ERROR_ARGUMENT = 1,
	XTYPED_SET_ERROR_TYPE,
	XTYPED_SET_ERROR_RANGE,
	XTYPED_SET_ERROR_OPERATION,
	XTYPED_SET_ERROR_STATE
} xtypedseterror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有类型值的空集合。 */
XRT_API bool xrtTypedSetInit(
	xtypedset* pSet,
	const xrttype* pItemType
);
XRT_API xtypedset* xrtTypedSetCreate(const xrttype* pItemType);
XRT_API void xrtTypedSetUnit(xtypedset* pSet);
XRT_API void xrtTypedSetDestroy(xtypedset* pSet);



/* 返回借用元素类型、当前元素数和再次扩容前的容量。 */
XRT_API const xrttype* xrtTypedSetItemType(const xtypedset* pSet);
XRT_API size_t xrtTypedSetCount(const xtypedset* pSet);
XRT_API size_t xrtTypedSetCapacity(const xtypedset* pSet);



/* 清空、预留或裁剪集合存储。 */
XRT_API bool xrtTypedSetClear(xtypedset* pSet);
XRT_API bool xrtTypedSetReserve(xtypedset* pSet, size_t iCapacity);
XRT_API bool xrtTypedSetTrim(xtypedset* pSet);



/* 返回已有或失败原子地复制加入的只读规范值。 */
XRT_API const void* xrtTypedSetGetOrAdd(
	xtypedset* pSet,
	const void* pItem,
	bool* pNew
);
XRT_API bool xrtTypedSetAdd(xtypedset* pSet, const void* pItem);



/* 返回或判断等价规范值；缺失是正常结果。 */
XRT_API const void* xrtTypedSetGet(
	const xtypedset* pSet,
	const void* pItem
);
XRT_API bool xrtTypedSetHas(
	const xtypedset* pSet,
	const void* pItem
);



/* 删除等价值，或把规范值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedSetRemove(xtypedset* pSet, const void* pItem);
XRT_API bool xrtTypedSetTake(
	xtypedset* pSet,
	const void* pItem,
	ptr pValue
);



/* 按插入顺序返回指定位置的只读规范值，复杂度为 O(n)。 */
XRT_API const void* xrtTypedSetAt(
	const xtypedset* pSet,
	size_t iIndex
);



/* 启动按插入顺序或逆序的外置迭代。 */
XRT_API bool xrtTypedSetIterBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
);
XRT_API bool xrtTypedSetIterRBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
);
XRT_API const void* xrtTypedSetIterNext(xtypedsetiter* pIterator);
XRT_API void xrtTypedSetIterEnd(xtypedsetiter* pIterator);



/* 事务合并同类型集合，并深复制创建独立集合。 */
XRT_API bool xrtTypedSetMerge(
	xtypedset* pTarget,
	const xtypedset* pSource
);
XRT_API xtypedset* xrtTypedSetClone(const xtypedset* pSet);



/* 创建两个同类型集合的并、交、差或对称差。 */
XRT_API xtypedset* xrtTypedSetUnion(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API xtypedset* xrtTypedSetIntersection(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API xtypedset* xrtTypedSetDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API xtypedset* xrtTypedSetSymmetricDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
);



/* 判断同类型集合的包含、相离或相等关系。 */
XRT_API bool xrtTypedSetIsSubset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
);
XRT_API bool xrtTypedSetIsSuperset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
);
XRT_API bool xrtTypedSetIsDisjoint(
	const xtypedset* pLeft,
	const xtypedset* pRight
);
XRT_API bool xrtTypedSetEquals(
	const xtypedset* pLeft,
	const xtypedset* pRight
);



/* 验证对象集合类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedSetTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedSetInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif
