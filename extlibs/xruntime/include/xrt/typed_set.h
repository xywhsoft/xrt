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

/* 显式键策略只要求相等和散列，不为元素声明排序能力。
 * 记录、回调代码和 UserData 都由调用方保持不可变，并必须比集合及其
 * 全部克隆/集合运算结果存活更久；相等键必须具有相同散列。
 * 回调遵循 xset 的错误边界与不可重入规则。 */
typedef struct xtypedsetkeypolicy {
	xsethash Hash;
	xsetequal Equal;
	ptr UserData;
} xtypedsetkeypolicy;



/* 类型集合拥有规范值；键策略在初始化时确定，不能通过底层存储更换。 */
typedef struct xtypedset {
	xset Storage;
	const xrttype* ItemType;
	const xtypedsetkeypolicy* KeyPolicy;
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
/* 生命周期仍由 ItemType 提供；元素只须有效、非零大小且可复制。
 * Policy 必须非空，Hash/Equal 必须成对非空。普通 Init/Create 仍使用
 * 类型 Compare/Hash；此入口不会给类型补入虚假的 Compare/Hash 操作。 */
XRT_API bool xrtTypedSetInitWithKeyPolicy(
	xtypedset* pSet,
	const xrttype* pItemType,
	const xtypedsetkeypolicy* pPolicy
);
XRT_API xtypedset* xrtTypedSetCreateWithKeyPolicy(
	const xrttype* pItemType,
	const xtypedsetkeypolicy* pPolicy
);
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
