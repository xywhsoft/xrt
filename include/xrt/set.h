#ifndef XRT_SET_H
#define XRT_SET_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_SET) && !defined(XRT_FEATURE_HASH64)
	#error "XRT_FEATURE_SET requires XRT_FEATURE_HASH64"
#endif



#define XRT_SET_ALIGNMENT_DEFAULT 16u
#define XRT_SET_BUCKETS_MIN 16u



#if defined(XRT_FEATURE_SET)

typedef struct xsetentry xsetentry;



/* 哈希器必须保证相等元素产生相同哈希值，回调中不得调用同一集合的 API。 */
typedef uint64 (*xsethash)(const void* pItem, ptr pUserData);



/* 相等器只借用元素且不得调用同一集合的 API。 */
typedef bool (*xsetequal)(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
);



/* 复制器成功时保持键等价，失败时不得在已清零目标槽遗留资源。 */
typedef bool (*xsetcopy)(ptr pTarget, const void* pSource, ptr pUserData);



/* 释放器处理元素内部资源且不得调用同一集合的 API，不释放元素槽本身。 */
typedef void (*xsetdrop)(ptr pItem, ptr pUserData);



/* 访问器可查询同一集合但不得修改、结束或再次访问，返回 false 时停止。 */
typedef bool (*xsetvisitor)(const void* pItem, ptr pUserData);



/* 集合使用独立哈希条目保存固定大小元素，并保持元素地址和插入顺序稳定。 */
typedef struct xset {
	xsetentry** Buckets;
	xsetentry* First;
	xsetentry* Last;
	size_t ItemSize;
	size_t ItemOffset;
	size_t Alignment;
	size_t Count;
	size_t BucketCount;
	size_t Threshold;
	uint64 Version;
	xsethash Hash;
	xsetequal Equal;
	xsetcopy Copy;
	xsetdrop Drop;
	ptr KeyUserData;
	ptr LifecycleUserData;
	uint32 Flags;
} xset;



/* 外置迭代器允许同一集合存在多个独立遍历状态。 */
typedef struct xsetiter {
	xset* Set;
	xsetentry* Next;
	uint64 Version;
	int Direction;
} xsetiter;



XRT_EXTERN_C_BEGIN



/* 使用默认 16 字节对齐初始化空集合。 */
XRT_API bool xrtSetInit(xset* pSet, size_t iItemSize);



/* 使用显式元素对齐初始化空集合。 */
XRT_API bool xrtSetInitAligned(
	xset* pSet,
	size_t iItemSize,
	size_t iAlignment
);



/* 创建使用默认 16 字节对齐的空集合。 */
XRT_API xset* xrtSetCreate(size_t iItemSize);



/* 创建使用显式元素对齐的空集合。 */
XRT_API xset* xrtSetCreateAligned(size_t iItemSize, size_t iAlignment);



/* 为仍为空的集合设置成对的自定义哈希器和相等器。 */
XRT_API bool xrtSetSetKeyPolicy(
	xset* pSet,
	xsethash pHash,
	xsetequal pEqual,
	ptr pUserData
);



/* 为仍为空的集合设置成对的资源复制器和释放器。 */
XRT_API bool xrtSetSetLifecycle(
	xset* pSet,
	xsetcopy pCopy,
	xsetdrop pDrop,
	ptr pUserData
);



/* 释放全部元素和桶数组，但不释放集合结构。 */
XRT_API void xrtSetUnit(xset* pSet);



/* 释放全部元素、桶数组和集合结构。 */
XRT_API void xrtSetDestroy(xset* pSet);



/* 清空全部元素并保留桶数组供后续复用。 */
XRT_API void xrtSetClear(xset* pSet);



/* 确保集合无需扩容即可容纳指定数量的元素。 */
XRT_API bool xrtSetReserve(xset* pSet, size_t iCapacity);



/* 把桶数组收缩到当前元素数需要的最小容量。 */
XRT_API bool xrtSetTrim(xset* pSet);



/* 返回当前元素数，非法集合返回零。 */
XRT_API size_t xrtSetCount(const xset* pSet);



/* 返回再次扩容前可容纳的元素数。 */
XRT_API size_t xrtSetCapacity(const xset* pSet);



/* 返回规范存储元素，缺失时失败原子地复制插入。 */
XRT_API const void* xrtSetGetOrAdd(
	xset* pSet,
	const void* pItem,
	bool* pNew
);



/* 复制加入元素，已有等价元素时成功且不替换规范元素。 */
XRT_API bool xrtSetAdd(xset* pSet, const void* pItem);



/* 返回集合内部的规范元素，缺失是正常结果。 */
XRT_API const void* xrtSetGet(const xset* pSet, const void* pItem);



/* 判断等价元素是否存在。 */
XRT_API bool xrtSetHas(const xset* pSet, const void* pItem);



/* 删除等价元素并调用资源释放器。 */
XRT_API bool xrtSetRemove(xset* pSet, const void* pItem);



/* 把规范元素移交后删除；输出区间不得接触集合拥有的任何内存。 */
XRT_API bool xrtSetTake(xset* pSet, const void* pItem, ptr pValue);



/* 按插入顺序访问元素，并返回实际访问数量。 */
XRT_API size_t xrtSetVisit(xset* pSet, xsetvisitor pVisitor, ptr pUserData);



/* 启动按插入顺序的外置迭代器。 */
XRT_API bool xrtSetIterBegin(xset* pSet, xsetiter* pIterator);



/* 启动按插入顺序逆序遍历的外置迭代器。 */
XRT_API bool xrtSetIterRBegin(xset* pSet, xsetiter* pIterator);



/* 返回下一规范元素，结构修改后报告状态错误。 */
XRT_API const void* xrtSetIterNext(xsetiter* pIterator);



/* 提前结束迭代并清除借用状态。 */
XRT_API void xrtSetIterEnd(xsetiter* pIterator);



/* 深度复制集合结构，并按生命周期复制器复制元素。 */
XRT_API xset* xrtSetClone(const xset* pSet);



/* 事务合并缺失元素，失败不变且保留已有元素地址和相对顺序。 */
XRT_API bool xrtSetMerge(xset* pTarget, const xset* pSource);



/* 创建两个兼容集合的并集。 */
XRT_API xset* xrtSetUnion(const xset* pLeft, const xset* pRight);



/* 创建两个兼容集合的交集。 */
XRT_API xset* xrtSetIntersection(const xset* pLeft, const xset* pRight);



/* 创建左集合相对右集合的差集。 */
XRT_API xset* xrtSetDifference(const xset* pLeft, const xset* pRight);



/* 创建两个兼容集合的对称差集。 */
XRT_API xset* xrtSetSymmetricDifference(
	const xset* pLeft,
	const xset* pRight
);



/* 判断左集合是否为右集合的子集，可选择严格子集。 */
XRT_API bool xrtSetIsSubset(
	const xset* pLeft,
	const xset* pRight,
	bool bProper
);



/* 判断左集合是否为右集合的超集，可选择严格超集。 */
XRT_API bool xrtSetIsSuperset(
	const xset* pLeft,
	const xset* pRight,
	bool bProper
);



/* 判断两个兼容集合是否没有任何共同元素。 */
XRT_API bool xrtSetIsDisjoint(
	const xset* pLeft,
	const xset* pRight
);



/* 判断两个兼容集合是否拥有相同元素。 */
XRT_API bool xrtSetEqual(const xset* pLeft, const xset* pRight);



XRT_EXTERN_C_END

#endif

#endif
