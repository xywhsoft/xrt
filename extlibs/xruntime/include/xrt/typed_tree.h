#ifndef XRT_TYPED_TREE_H
#define XRT_TYPED_TREE_H

#include <xrt/avl.h>
#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_TYPED_TREE) && !defined(XRT_FEATURE_AVL_TREE)
	#error "XRUNTIME_FEATURE_TYPED_TREE requires XRT_FEATURE_AVL_TREE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_TREE) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_TREE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_TREE)

/* 类型树按任意可比较运行时类型键排序，并拥有每一个键和值。 */
typedef struct xtypedtree {
	xavltree Storage;
	const xrttype* KeyType;
	const xrttype* ValueType;
	size_t KeyOffset;
	size_t ValueOffset;
	size_t EntrySize;
	size_t Alignment;
	uint32 Flags;
} xtypedtree;



/* 类型树外置迭代器按键升序或降序借用稳定键值槽。 */
typedef struct xtypedtreeiter {
	xtypedtree* Tree;
	xavltreeiter Base;
} xtypedtreeiter;



/* 类型树模块稳定错误代码。 */
typedef enum xtypedtreeerror {
	XTYPED_TREE_ERROR_ARGUMENT = 1,
	XTYPED_TREE_ERROR_TYPE,
	XTYPED_TREE_ERROR_LAYOUT,
	XTYPED_TREE_ERROR_OPERATION,
	XTYPED_TREE_ERROR_STATE
} xtypedtreeerror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有键值的空类型树。 */
XRT_API bool xrtTypedTreeInit(
	xtypedtree* pTree,
	const xrttype* pKeyType,
	const xrttype* pValueType
);
XRT_API xtypedtree* xrtTypedTreeCreate(
	const xrttype* pKeyType,
	const xrttype* pValueType
);
XRT_API void xrtTypedTreeUnit(xtypedtree* pTree);
XRT_API void xrtTypedTreeDestroy(xtypedtree* pTree);



/* 返回借用键类型、值类型和当前键值数量。 */
XRT_API const xrttype* xrtTypedTreeKeyType(const xtypedtree* pTree);
XRT_API const xrttype* xrtTypedTreeValueType(const xtypedtree* pTree);
XRT_API size_t xrtTypedTreeCount(const xtypedtree* pTree);



/* 清空全部键值，或释放多余空节点池页。 */
XRT_API bool xrtTypedTreeClear(xtypedtree* pTree);
XRT_API size_t xrtTypedTreeTrim(xtypedtree* pTree, size_t iRetainEmpty);



/* 返回已有值槽，或复制键并按值类型初始化一个新值。 */
XRT_API ptr xrtTypedTreeGetOrAdd(
	xtypedtree* pTree,
	const void* pKey,
	bool* pNew
);



/* 失败原子地复制设置，或移动外部已初始化值并清空来源。 */
XRT_API bool xrtTypedTreeSet(
	xtypedtree* pTree,
	const void* pKey,
	const void* pValue
);
XRT_API bool xrtTypedTreeSetTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
);



/* 返回指定键的可写或只读借用值；缺失是正常结果。 */
XRT_API ptr xrtTypedTreeGet(xtypedtree* pTree, const void* pKey);
XRT_API const void* xrtTypedTreeConstGet(
	const xtypedtree* pTree,
	const void* pKey
);
XRT_API bool xrtTypedTreeHas(
	const xtypedtree* pTree,
	const void* pKey
);



/* 返回与查询等价的内部规范键，缺失时返回空。 */
XRT_API const void* xrtTypedTreeStoredKey(
	const xtypedtree* pTree,
	const void* pKey
);



/* 删除指定键值，或把值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedTreeRemove(xtypedtree* pTree, const void* pKey);
XRT_API bool xrtTypedTreeTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
);



/* 返回首尾或上下界值，并可返回对应的内部规范键。 */
XRT_API ptr xrtTypedTreeFirst(xtypedtree* pTree, const void** pKey);
XRT_API ptr xrtTypedTreeLast(xtypedtree* pTree, const void** pKey);
XRT_API ptr xrtTypedTreeLowerBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
);
XRT_API ptr xrtTypedTreeUpperBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
);



/* 启动完整或从指定边界开始的正反零分配迭代。 */
XRT_API bool xrtTypedTreeIterBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
);
XRT_API bool xrtTypedTreeIterRBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
);
XRT_API bool xrtTypedTreeIterFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
);
XRT_API bool xrtTypedTreeIterRFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
);
XRT_API ptr xrtTypedTreeIterNext(
	xtypedtreeiter* pIterator,
	const void** pKey
);
XRT_API void xrtTypedTreeIterEnd(xtypedtreeiter* pIterator);



/* 事务合并同类型树、深复制树或比较完整有序键值内容。 */
XRT_API bool xrtTypedTreeMerge(
	xtypedtree* pTarget,
	const xtypedtree* pSource,
	bool bReplace
);
XRT_API xtypedtree* xrtTypedTreeClone(const xtypedtree* pTree);
XRT_API bool xrtTypedTreeEquals(
	const xtypedtree* pLeft,
	const xtypedtree* pRight
);



/* 验证对象树类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedTreeTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedTreeInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif
