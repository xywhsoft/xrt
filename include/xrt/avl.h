#ifndef XRT_AVL_H
#define XRT_AVL_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_AVL_TREE) && !defined(XRT_FEATURE_AVL)
	#error "XRT_FEATURE_AVL_TREE requires XRT_FEATURE_AVL"
#endif

#if defined(XRT_FEATURE_AVL_TREE) && !defined(XRT_FEATURE_POOL)
	#error "XRT_FEATURE_AVL_TREE requires XRT_FEATURE_POOL"
#endif



/* 64 位 size_t 可表达的 AVL 树高度不会超过该界限。 */
#define XRT_AVL_HEIGHT_MAX 96u



#if defined(XRT_FEATURE_AVL)

/* 侵入式节点只保存平衡树链接，业务结构可将它嵌入任意位置。 */
typedef struct xavlnode {
	struct xavlnode* Left;
	struct xavlnode* Right;
	uint8 Height;
} xavlnode;



/* 侵入式树不拥有节点内存，版本号用于检测遍历期结构修改。 */
typedef struct xavl {
	xavlnode* Root;
	size_t Count;
	uint64 Version;
} xavl;



/* 比较器返回 key 与节点的顺序关系，并可通过用户数据恢复业务结构。 */
typedef int (*xavlcompare)(const void* pKey, const xavlnode* pNode, ptr pUserData);



/* 访问器返回 false 时停止遍历。 */
typedef bool (*xavlvisitor)(xavlnode* pNode, ptr pUserData);



/* 外置迭代器允许同一棵树存在多个并行读迭代，不发生堆分配。 */
typedef struct xavliter {
	const xavl* Tree;
	xavlnode* Path[XRT_AVL_HEIGHT_MAX];
	size_t Depth;
	uint64 Version;
	bool Reverse;
	bool Active;
} xavliter;



XRT_EXTERN_C_BEGIN



/* 将节点恢复为未挂入任何树的状态。 */
XRT_API void xrtAVLNodeInit(xavlnode* pNode);



/* 初始化一棵不拥有节点内存的空树。 */
XRT_API bool xrtAVLInit(xavl* pTree);



/* 忘记全部节点但不释放或逐个重置节点。 */
XRT_API void xrtAVLClear(xavl* pTree);



/* 插入已初始化的独立节点；重复键返回已有节点并通过 pNew 返回 false。 */
XRT_API xavlnode* xrtAVLInsert(
	xavl* pTree,
	xavlnode* pNode,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	bool* pNew
);



/* 删除指定键并返回已经恢复为独立状态的原节点。 */
XRT_API xavlnode* xrtAVLRemove(
	xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);



/* 查找与 key 相等的节点，未找到是正常结果。 */
XRT_API xavlnode* xrtAVLFind(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);



/* 返回第一项不小于 key 的节点。 */
XRT_API xavlnode* xrtAVLLowerBound(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);



/* 返回第一项严格大于 key 的节点。 */
XRT_API xavlnode* xrtAVLUpperBound(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);



/* 返回按比较器顺序排列的第一项。 */
XRT_API xavlnode* xrtAVLFirst(const xavl* pTree);



/* 返回按比较器顺序排列的最后一项。 */
XRT_API xavlnode* xrtAVLLast(const xavl* pTree);



/* 按升序访问节点并返回实际访问数量。 */
XRT_API size_t xrtAVLVisit(const xavl* pTree, xavlvisitor pVisitor, ptr pUserData);



/* 启动升序外置迭代器。 */
XRT_API bool xrtAVLIterBegin(const xavl* pTree, xavliter* pIterator);



/* 启动降序外置迭代器。 */
XRT_API bool xrtAVLIterRBegin(const xavl* pTree, xavliter* pIterator);



/* 从第一项不小于 key 的节点开始升序迭代。 */
XRT_API bool xrtAVLIterFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
);



/* 从第一项不大于 key 的节点开始降序迭代。 */
XRT_API bool xrtAVLIterRFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
);



/* 返回下一节点；正常结束或结构已修改时返回空指针。 */
XRT_API xavlnode* xrtAVLIterNext(xavliter* pIterator);



/* 提前结束迭代并清除它持有的借用状态。 */
XRT_API void xrtAVLIterEnd(xavliter* pIterator);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_AVL_TREE)

#include <xrt/pool.h>



/* 拥有式树比较器返回 key 与对象的顺序关系，不得重入同一棵树。 */
typedef int (*xavltreecompare)(const void* pKey, const void* pItem, ptr pUserData);



/* 对象释放器只处理内部资源，回调期间不得调用同一棵树的 API。 */
typedef void (*xavltreedrop)(ptr pItem, ptr pUserData);



/* 访问器可查询树和修改非键字段，不得修改结构或生命周期。 */
typedef bool (*xavltreevisitor)(ptr pItem, ptr pUserData);



/* 拥有式树使用固定对象池，节点地址在删除前保持稳定。 */
typedef struct xavltree {
	xavl Base;
	xpool Pool;
	size_t ItemSize;
	size_t ItemOffset;
	size_t Alignment;
	xavltreecompare Compare;
	xavltreedrop Drop;
	ptr UserData;
	uint32 Flags;
} xavltree;



/* 拥有式迭代器复用零分配侵入式路径栈。 */
typedef struct xavltreeiter {
	xavltree* Tree;
	xavliter Base;
} xavltreeiter;



XRT_EXTERN_C_BEGIN



/* 使用默认 16 字节对象对齐初始化拥有式树。 */
XRT_API bool xrtAVLTreeInit(
	xavltree* pTree,
	size_t iItemSize,
	xavltreecompare pCompare,
	ptr pUserData
);



/* 使用显式对象对齐初始化拥有式树。 */
XRT_API bool xrtAVLTreeInitAligned(
	xavltree* pTree,
	size_t iItemSize,
	size_t iAlignment,
	xavltreecompare pCompare,
	ptr pUserData
);



/* 创建使用默认 16 字节对象对齐的拥有式树。 */
XRT_API xavltree* xrtAVLTreeCreate(
	size_t iItemSize,
	xavltreecompare pCompare,
	ptr pUserData
);



/* 创建使用显式对象对齐的拥有式树。 */
XRT_API xavltree* xrtAVLTreeCreateAligned(
	size_t iItemSize,
	size_t iAlignment,
	xavltreecompare pCompare,
	ptr pUserData
);



/* 为仍为空的树设置对象资源释放器。 */
XRT_API bool xrtAVLTreeSetDrop(xavltree* pTree, xavltreedrop pDrop);



/* 释放全部对象和池页，但不释放树结构。 */
XRT_API void xrtAVLTreeUnit(xavltree* pTree);



/* 释放全部对象、池页和树结构。 */
XRT_API void xrtAVLTreeDestroy(xavltree* pTree);



/* 清空全部对象并保留固定池的复用能力。 */
XRT_API void xrtAVLTreeClear(xavltree* pTree);



/* 返回当前对象数量，非法树返回零。 */
XRT_API size_t xrtAVLTreeCount(const xavltree* pTree);



/* 复制添加对象；pKey 必须等价于对象内排序键，重复时不覆盖已有对象。 */
XRT_API ptr xrtAVLTreeAdd(
	xavltree* pTree,
	const void* pKey,
	const void* pItem,
	bool* pNew
);



/* 查找可修改对象，未找到是正常结果。 */
XRT_API ptr xrtAVLTreeFind(xavltree* pTree, const void* pKey);



/* 查找只读对象，未找到是正常结果。 */
XRT_API const void* xrtAVLTreeConstFind(const xavltree* pTree, const void* pKey);



/* 判断指定键是否存在。 */
XRT_API bool xrtAVLTreeHas(const xavltree* pTree, const void* pKey);



/* 删除对象并调用资源释放器。 */
XRT_API bool xrtAVLTreeRemove(xavltree* pTree, const void* pKey);



/* 将对象字节移交给调用方后删除，不调用资源释放器。 */
XRT_API bool xrtAVLTreeTake(xavltree* pTree, const void* pKey, ptr pItem);



/* 返回顺序第一项。 */
XRT_API ptr xrtAVLTreeFirst(xavltree* pTree);



/* 返回顺序最后一项。 */
XRT_API ptr xrtAVLTreeLast(xavltree* pTree);



/* 返回第一项不小于 key 的对象。 */
XRT_API ptr xrtAVLTreeLowerBound(xavltree* pTree, const void* pKey);



/* 返回第一项严格大于 key 的对象。 */
XRT_API ptr xrtAVLTreeUpperBound(xavltree* pTree, const void* pKey);



/* 按升序访问对象；回调期间查询可用，结构和生命周期修改被拒绝。 */
XRT_API size_t xrtAVLTreeVisit(
	xavltree* pTree,
	xavltreevisitor pVisitor,
	ptr pUserData
);



/* 启动拥有式树升序迭代。 */
XRT_API bool xrtAVLTreeIterBegin(xavltree* pTree, xavltreeiter* pIterator);



/* 启动拥有式树降序迭代。 */
XRT_API bool xrtAVLTreeIterRBegin(xavltree* pTree, xavltreeiter* pIterator);



/* 从第一项不小于 key 的对象开始升序迭代。 */
XRT_API bool xrtAVLTreeIterFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
);



/* 从第一项不大于 key 的对象开始降序迭代。 */
XRT_API bool xrtAVLTreeIterRFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
);



/* 返回下一对象；正常结束或结构已修改时返回空指针。 */
XRT_API ptr xrtAVLTreeIterNext(xavltreeiter* pIterator);



/* 提前结束拥有式树迭代。 */
XRT_API void xrtAVLTreeIterEnd(xavltreeiter* pIterator);



XRT_EXTERN_C_END

#endif

#endif
