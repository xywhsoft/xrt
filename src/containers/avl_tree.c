#include "../internal/xrt_avl.h"



#if defined(XRT_FEATURE_AVL_TREE)

#define XRT_AVL_TREE_FLAG_READY 0x0001u
#define XRT_AVL_TREE_FLAG_BUSY  0x0002u
#define XRT_AVL_TREE_FLAG_VISITING 0x0004u
#define XRT_AVL_TREE_FLAGS      0x0007u



/* 检查拥有式树公开状态是否自洽。 */
bool __xrtAVLTreeValid(const xavltree* pTree)
{
	if ( pTree == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pTree->Flags & XRT_AVL_TREE_FLAG_READY) == 0) ||
		((pTree->Flags & XRT_AVL_TREE_FLAG_BUSY) != 0) ||
		((pTree->Flags & ~XRT_AVL_TREE_FLAGS) != 0) ||
		(pTree->ItemSize == 0) ||
		(pTree->ItemOffset < sizeof(xavlnode)) ||
		(pTree->Alignment == 0) ||
		((pTree->Alignment & (pTree->Alignment - 1u)) != 0) ||
		((pTree->ItemOffset % pTree->Alignment) != 0) ||
		(pTree->ItemOffset > (SIZE_MAX - pTree->ItemSize)) ||
		((pTree->Pool.Flags & XRT_POOL_FLAG_READY) == 0) ||
		((pTree->Pool.Flags & XRT_POOL_FLAG_VISITING) != 0) ||
		(pTree->Pool.ItemSize != (pTree->ItemOffset + pTree->ItemSize)) ||
		(pTree->Pool.Alignment < pTree->Alignment) ||
		(pTree->Compare == NULL) ||
		((pTree->Base.Count == 0) && (pTree->Base.Root != NULL)) ||
		((pTree->Base.Count != 0) && (pTree->Base.Root == NULL)) ||
		(pTree->Base.Count != pTree->Pool.LiveCount)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 检查拥有式树当前是否允许修改结构和生命周期。 */
bool __xrtAVLTreeCanMutate(const xavltree* pTree)
{
	if ( !__xrtAVLTreeValid(pTree) ) {
		return false;
	}
	if ( (pTree->Flags & XRT_AVL_TREE_FLAG_VISITING) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 判断调用方字节区间是否触及树结构或固定池内部存储。 */
bool __xrtAVLTreeOwnsRange(
	const xavltree* pTree,
	const void* pMemory,
	size_t iSize
)
{
	xpoolpage* pPage;

	if ( __xrtRangesOverlap(pMemory, iSize, pTree, sizeof(xavltree)) ) {
		return true;
	}
	if (
		(pTree->Pool.Index != NULL) &&
		(pTree->Pool.IndexCapacity <= (SIZE_MAX / sizeof(xpoolpage*))) &&
		__xrtRangesOverlap(
			pMemory,
			iSize,
			pTree->Pool.Index,
			pTree->Pool.IndexCapacity * sizeof(xpoolpage*)
		)
	) {
		return true;
	}
	for ( pPage = pTree->Pool.Pages; pPage != NULL; pPage = pPage->Next ) {
		if (
			__xrtRangesOverlap(pMemory, iSize, pPage, sizeof(xpoolpage)) ||
			__xrtRangesOverlap(
				pMemory,
				iSize,
				pPage->Memory,
				pPage->MemorySize
			)
		) {
			return true;
		}
	}

	return false;
}



/* 将拥有式节点转换为对齐后的对象地址。 */
static ptr __xrtAVLTreeItem(const xavltree* pTree, const xavlnode* pNode)
{
	return pNode != NULL ? (ptr)((bytes)pNode + pTree->ItemOffset) : NULL;
}



/* 将公开对象比较器适配到侵入式节点比较器。 */
static int __xrtAVLTreeCompare(const void* pKey, const xavlnode* pNode, ptr pUserData)
{
	const xavltree* pTree = (const xavltree*)pUserData;

	return pTree->Compare(pKey, __xrtAVLTreeItem(pTree, pNode), pTree->UserData);
}



/* 在受保护状态下调用对象释放器。 */
void __xrtAVLTreeDropItem(xavltree* pTree, ptr pItem)
{
	uint32 iFlags;

	if ( pTree->Drop == NULL ) {
		return;
	}

	iFlags = pTree->Flags;
	pTree->Flags |= XRT_AVL_TREE_FLAG_BUSY;
	pTree->Drop(pItem, pTree->UserData);
	pTree->Flags = iFlags;
}



/* 清理时逐个释放对象内部拥有的资源。 */
static bool __xrtAVLTreeDropObject(ptr pObject, size_t iIndex, ptr pUserData)
{
	xavltree* pTree = (xavltree*)pUserData;

	(void)iIndex;
	__xrtAVLTreeDropItem(
		pTree,
		(bytes)pObject + pTree->ItemOffset
	);
	return true;
}



/* 以指定对齐建立拥有式树和节点池。 */
static bool __xrtAVLTreeInit(
	xavltree* pTree,
	size_t iItemSize,
	size_t iAlignment,
	xavltreecompare pCompare,
	ptr pUserData
)
{
	size_t iItemOffset;
	size_t iNodeSize;
	size_t iPoolAlignment;

	if (
		(pTree == NULL) ||
		(iItemSize == 0) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0) ||
		(pCompare == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sizeof(xavlnode) > (SIZE_MAX - (iAlignment - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iItemOffset = (sizeof(xavlnode) + (iAlignment - 1u)) & ~(iAlignment - 1u);
	if ( iItemSize > (SIZE_MAX - iItemOffset) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNodeSize = iItemOffset + iItemSize;
	iPoolAlignment = iAlignment > XRT_POOL_ALIGNMENT_DEFAULT ?
		iAlignment : XRT_POOL_ALIGNMENT_DEFAULT;

	memset(pTree, 0, sizeof(xavltree));
	if ( !xrtAVLInit(&pTree->Base) ) {
		return false;
	}
	if ( !xrtPoolInitAligned(&pTree->Pool, iNodeSize, iPoolAlignment) ) {
		memset(pTree, 0, sizeof(xavltree));
		return false;
	}

	pTree->ItemSize = iItemSize;
	pTree->ItemOffset = iItemOffset;
	pTree->Alignment = iAlignment;
	pTree->Compare = pCompare;
	pTree->UserData = pUserData;
	pTree->Flags = XRT_AVL_TREE_FLAG_READY;
	return true;
}



/* 命中时不分配，缺失时在池槽内直接建立对象。 */
ptr __xrtAVLTreeGetOrAdd(
	xavltree* pTree,
	const void* pKey,
	xavltreeinit pInit,
	ptr pInitUserData,
	xavltreerollback pRollback,
	ptr pRollbackUserData,
	bool* pNew
)
{
	xavlnode* pNode;
	xavlnode* pActual;
	ptr pItem;
	bool bNew = false;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtAVLTreeCanMutate(pTree) || (pInit == NULL) ) {
		if ( pInit == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}

	/* 重复键必须在任何分配之前返回，保持 OOM 下的命中路径。 */
	pActual = xrtAVLFind(&pTree->Base, pKey, __xrtAVLTreeCompare, pTree);
	if ( pActual != NULL ) {
		return __xrtAVLTreeItem(pTree, pActual);
	}

	pNode = (xavlnode*)xrtPoolAlloc(&pTree->Pool);
	if ( pNode == NULL ) {
		return NULL;
	}
	xrtAVLNodeInit(pNode);
	pItem = __xrtAVLTreeItem(pTree, pNode);
	memset(pItem, 0, pTree->ItemSize);
	if ( !pInit(pItem, pKey, pInitUserData) ) {
		xrtPoolFree(&pTree->Pool, pNode);
		return NULL;
	}
	if ( pTree->Compare(pKey, pItem, pTree->UserData) != 0 ) {
		if ( pRollback != NULL ) {
			pRollback(pItem, pRollbackUserData);
		}
		xrtPoolFree(&pTree->Pool, pNode);
		__xrtErrorSetInvalidState();
		return NULL;
	}

	pActual = xrtAVLInsert(
		&pTree->Base,
		pNode,
		pKey,
		__xrtAVLTreeCompare,
		pTree,
		&bNew
	);
	if ( pActual == NULL ) {
		if ( pRollback != NULL ) {
			pRollback(pItem, pRollbackUserData);
		}
		xrtPoolFree(&pTree->Pool, pNode);
		return NULL;
	}
	if ( !bNew ) {
		if ( pRollback != NULL ) {
			pRollback(pItem, pRollbackUserData);
		}
		xrtPoolFree(&pTree->Pool, pNode);
	}
	if ( pNew != NULL ) {
		*pNew = bNew;
	}
	return __xrtAVLTreeItem(pTree, pActual);
}



/* 删除对象并移交指定字节区间，不触发对象释放器。 */
bool __xrtAVLTreeTakePart(
	xavltree* pTree,
	const void* pKey,
	size_t iOffset,
	size_t iSize,
	ptr pOutput
)
{
	xavlnode* pNode;
	ptr pItem;

	if ( !__xrtAVLTreeCanMutate(pTree) ) {
		return false;
	}
	if (
		(pOutput == NULL) ||
		(iSize == 0) ||
		(iOffset > pTree->ItemSize) ||
		(iSize > (pTree->ItemSize - iOffset))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtAVLTreeOwnsRange(pTree, pOutput, iSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	pNode = xrtAVLRemove(&pTree->Base, pKey, __xrtAVLTreeCompare, pTree);
	if ( pNode == NULL ) {
		return false;
	}
	pItem = __xrtAVLTreeItem(pTree, pNode);
	memcpy(pOutput, (bytes)pItem + iOffset, iSize);
	return xrtPoolFree(&pTree->Pool, pNode);
}



/* 使用默认 16 字节对象对齐初始化拥有式树。 */
XRT_API bool xrtAVLTreeInit(
	xavltree* pTree,
	size_t iItemSize,
	xavltreecompare pCompare,
	ptr pUserData
)
{
	return __xrtAVLTreeInit(
		pTree,
		iItemSize,
		XRT_POOL_ALIGNMENT_DEFAULT,
		pCompare,
		pUserData
	);
}



/* 使用显式对象对齐初始化拥有式树。 */
XRT_API bool xrtAVLTreeInitAligned(
	xavltree* pTree,
	size_t iItemSize,
	size_t iAlignment,
	xavltreecompare pCompare,
	ptr pUserData
)
{
	return __xrtAVLTreeInit(pTree, iItemSize, iAlignment, pCompare, pUserData);
}



/* 创建使用默认 16 字节对象对齐的拥有式树。 */
XRT_API xavltree* xrtAVLTreeCreate(
	size_t iItemSize,
	xavltreecompare pCompare,
	ptr pUserData
)
{
	xavltree* pTree = (xavltree*)xrtMalloc(sizeof(xavltree));

	if ( pTree == NULL ) {
		return NULL;
	}
	if ( !xrtAVLTreeInit(pTree, iItemSize, pCompare, pUserData) ) {
		xrtFree(pTree);
		return NULL;
	}
	return pTree;
}



/* 创建使用显式对象对齐的拥有式树。 */
XRT_API xavltree* xrtAVLTreeCreateAligned(
	size_t iItemSize,
	size_t iAlignment,
	xavltreecompare pCompare,
	ptr pUserData
)
{
	xavltree* pTree = (xavltree*)xrtMalloc(sizeof(xavltree));

	if ( pTree == NULL ) {
		return NULL;
	}
	if ( !xrtAVLTreeInitAligned(pTree, iItemSize, iAlignment, pCompare, pUserData) ) {
		xrtFree(pTree);
		return NULL;
	}
	return pTree;
}



/* 为仍为空的树设置对象资源释放器。 */
XRT_API bool xrtAVLTreeSetDrop(xavltree* pTree, xavltreedrop pDrop)
{
	if ( !__xrtAVLTreeCanMutate(pTree) ) {
		return false;
	}
	if ( pTree->Base.Count != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	pTree->Drop = pDrop;
	return true;
}



/* 释放全部对象和池页，但不释放树结构。 */
XRT_API void xrtAVLTreeUnit(xavltree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	if ( !__xrtAVLTreeCanMutate(pTree) ) {
		return;
	}

	xrtAVLTreeClear(pTree);
	xrtPoolUnit(&pTree->Pool);
	memset(pTree, 0, sizeof(xavltree));
}



/* 释放全部对象、池页和树结构。 */
XRT_API void xrtAVLTreeDestroy(xavltree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	if ( !__xrtAVLTreeCanMutate(pTree) ) {
		return;
	}

	xrtAVLTreeUnit(pTree);
	xrtFree(pTree);
}



/* 清空全部对象并保留固定池的复用能力。 */
XRT_API void xrtAVLTreeClear(xavltree* pTree)
{
	if ( !__xrtAVLTreeCanMutate(pTree) ) {
		return;
	}
	if ( pTree->Base.Count == 0 ) {
		return;
	}

	/* 回调期间拒绝树 API 重入，再同时重置树索引和池活动槽。 */
	pTree->Flags |= XRT_AVL_TREE_FLAG_BUSY;
	if ( pTree->Drop != NULL ) {
		xrtPoolVisit(&pTree->Pool, __xrtAVLTreeDropObject, pTree);
	}
	xrtAVLClear(&pTree->Base);
	xrtPoolReset(&pTree->Pool);
	pTree->Flags &= ~XRT_AVL_TREE_FLAG_BUSY;
}



/* 返回当前对象数量，非法树返回零。 */
XRT_API size_t xrtAVLTreeCount(const xavltree* pTree)
{
	return __xrtAVLTreeValid(pTree) ? pTree->Base.Count : 0;
}



/* 复制添加对象；重复键返回已有对象且不覆盖它。 */
XRT_API ptr xrtAVLTreeAdd(
	xavltree* pTree,
	const void* pKey,
	const void* pItem,
	bool* pNew
)
{
	xavlnode* pNode;
	xavlnode* pActual;
	bool bNew = false;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtAVLTreeCanMutate(pTree) || (pItem == NULL) ) {
		if ( pItem == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	if (
		__xrtAVLTreeOwnsRange(pTree, pItem, pTree->ItemSize) ||
		(pTree->Compare(pKey, pItem, pTree->UserData) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}

	/* 重复键命中不分配，也不受后续 OOM 影响。 */
	pActual = xrtAVLFind(&pTree->Base, pKey, __xrtAVLTreeCompare, pTree);
	if ( pActual != NULL ) {
		return __xrtAVLTreeItem(pTree, pActual);
	}

	pNode = (xavlnode*)xrtPoolAlloc(&pTree->Pool);
	if ( pNode == NULL ) {
		return NULL;
	}
	xrtAVLNodeInit(pNode);
	memcpy(__xrtAVLTreeItem(pTree, pNode), pItem, pTree->ItemSize);
	pActual = xrtAVLInsert(
		&pTree->Base,
		pNode,
		pKey,
		__xrtAVLTreeCompare,
		pTree,
		&bNew
	);
	if ( pActual == NULL ) {
		xrtPoolFree(&pTree->Pool, pNode);
		return NULL;
	}
	if ( !bNew ) {
		xrtPoolFree(&pTree->Pool, pNode);
	}
	if ( pNew != NULL ) {
		*pNew = bNew;
	}
	return __xrtAVLTreeItem(pTree, pActual);
}



/* 查找可修改对象，未找到是正常结果。 */
XRT_API ptr xrtAVLTreeFind(xavltree* pTree, const void* pKey)
{
	xavlnode* pNode;

	if ( !__xrtAVLTreeValid(pTree) ) {
		return NULL;
	}
	pNode = xrtAVLFind(&pTree->Base, pKey, __xrtAVLTreeCompare, pTree);
	return __xrtAVLTreeItem(pTree, pNode);
}



/* 查找只读对象，未找到是正常结果。 */
XRT_API const void* xrtAVLTreeConstFind(const xavltree* pTree, const void* pKey)
{
	xavlnode* pNode;

	if ( !__xrtAVLTreeValid(pTree) ) {
		return NULL;
	}
	pNode = xrtAVLFind(&pTree->Base, pKey, __xrtAVLTreeCompare, (ptr)pTree);
	return __xrtAVLTreeItem(pTree, pNode);
}



/* 判断指定键是否存在。 */
XRT_API bool xrtAVLTreeHas(const xavltree* pTree, const void* pKey)
{
	if ( !__xrtAVLTreeValid(pTree) ) {
		return false;
	}
	return xrtAVLFind(&pTree->Base, pKey, __xrtAVLTreeCompare, (ptr)pTree) != NULL;
}



/* 删除对象并调用资源释放器。 */
XRT_API bool xrtAVLTreeRemove(xavltree* pTree, const void* pKey)
{
	xavlnode* pNode;
	ptr pItem;

	if ( !__xrtAVLTreeCanMutate(pTree) ) {
		return false;
	}
	pNode = xrtAVLRemove(&pTree->Base, pKey, __xrtAVLTreeCompare, pTree);
	if ( pNode == NULL ) {
		return false;
	}
	pItem = __xrtAVLTreeItem(pTree, pNode);
	__xrtAVLTreeDropItem(pTree, pItem);
	return xrtPoolFree(&pTree->Pool, pNode);
}



/* 将对象字节移交给调用方后删除，不调用资源释放器。 */
XRT_API bool xrtAVLTreeTake(xavltree* pTree, const void* pKey, ptr pItem)
{
	if ( !__xrtAVLTreeCanMutate(pTree) ) {
		return false;
	}
	return __xrtAVLTreeTakePart(pTree, pKey, 0, pTree->ItemSize, pItem);
}



/* 返回顺序第一项。 */
XRT_API ptr xrtAVLTreeFirst(xavltree* pTree)
{
	if ( !__xrtAVLTreeValid(pTree) ) {
		return NULL;
	}
	return __xrtAVLTreeItem(pTree, xrtAVLFirst(&pTree->Base));
}



/* 返回顺序最后一项。 */
XRT_API ptr xrtAVLTreeLast(xavltree* pTree)
{
	if ( !__xrtAVLTreeValid(pTree) ) {
		return NULL;
	}
	return __xrtAVLTreeItem(pTree, xrtAVLLast(&pTree->Base));
}



/* 返回第一项不小于 key 的对象。 */
XRT_API ptr xrtAVLTreeLowerBound(xavltree* pTree, const void* pKey)
{
	xavlnode* pNode;

	if ( !__xrtAVLTreeValid(pTree) ) {
		return NULL;
	}
	pNode = xrtAVLLowerBound(&pTree->Base, pKey, __xrtAVLTreeCompare, pTree);
	return __xrtAVLTreeItem(pTree, pNode);
}



/* 返回第一项严格大于 key 的对象。 */
XRT_API ptr xrtAVLTreeUpperBound(xavltree* pTree, const void* pKey)
{
	xavlnode* pNode;

	if ( !__xrtAVLTreeValid(pTree) ) {
		return NULL;
	}
	pNode = xrtAVLUpperBound(&pTree->Base, pKey, __xrtAVLTreeCompare, pTree);
	return __xrtAVLTreeItem(pTree, pNode);
}



/* 按升序访问对象并返回实际访问数量。 */
XRT_API size_t xrtAVLTreeVisit(
	xavltree* pTree,
	xavltreevisitor pVisitor,
	ptr pUserData
)
{
	xavltreeiter tIterator;
	ptr pItem;
	size_t iVisited = 0;

	if ( !__xrtAVLTreeCanMutate(pTree) || (pVisitor == NULL) ) {
		if ( pVisitor == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return 0;
	}
	if ( !xrtAVLTreeIterBegin(pTree, &tIterator) ) {
		return 0;
	}

	pTree->Flags |= XRT_AVL_TREE_FLAG_VISITING;
	while ( (pItem = xrtAVLTreeIterNext(&tIterator)) != NULL ) {
		iVisited++;
		if ( !pVisitor(pItem, pUserData) ) {
			break;
		}
	}
	xrtAVLTreeIterEnd(&tIterator);
	pTree->Flags &= ~XRT_AVL_TREE_FLAG_VISITING;
	return iVisited;
}



/* 启动拥有式树升序迭代。 */
XRT_API bool xrtAVLTreeIterBegin(xavltree* pTree, xavltreeiter* pIterator)
{
	if ( (pIterator == NULL) || !__xrtAVLTreeValid(pTree) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	memset(pIterator, 0, sizeof(xavltreeiter));
	pIterator->Tree = pTree;
	if ( !xrtAVLIterBegin(&pTree->Base, &pIterator->Base) ) {
		pIterator->Tree = NULL;
		return false;
	}
	return true;
}



/* 启动拥有式树降序迭代。 */
XRT_API bool xrtAVLTreeIterRBegin(xavltree* pTree, xavltreeiter* pIterator)
{
	if ( (pIterator == NULL) || !__xrtAVLTreeValid(pTree) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	memset(pIterator, 0, sizeof(xavltreeiter));
	pIterator->Tree = pTree;
	if ( !xrtAVLIterRBegin(&pTree->Base, &pIterator->Base) ) {
		pIterator->Tree = NULL;
		return false;
	}
	return true;
}



/* 从第一项不小于 key 的对象开始升序迭代。 */
XRT_API bool xrtAVLTreeIterFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xavltreeiter));
	}
	if ( (pIterator == NULL) || !__xrtAVLTreeValid(pTree) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Tree = pTree;
	if (
		!xrtAVLIterFrom(
			&pTree->Base,
			pKey,
			__xrtAVLTreeCompare,
			pTree,
			&pIterator->Base
		)
	) {
		pIterator->Tree = NULL;
		return false;
	}
	return true;
}



/* 从第一项不大于 key 的对象开始降序迭代。 */
XRT_API bool xrtAVLTreeIterRFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xavltreeiter));
	}
	if ( (pIterator == NULL) || !__xrtAVLTreeValid(pTree) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pIterator->Tree = pTree;
	if (
		!xrtAVLIterRFrom(
			&pTree->Base,
			pKey,
			__xrtAVLTreeCompare,
			pTree,
			&pIterator->Base
		)
	) {
		pIterator->Tree = NULL;
		return false;
	}
	return true;
}



/* 返回下一对象；正常结束或结构已修改时返回空指针。 */
XRT_API ptr xrtAVLTreeIterNext(xavltreeiter* pIterator)
{
	xavlnode* pNode;

	if ( (pIterator == NULL) || (pIterator->Tree == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pNode = xrtAVLIterNext(&pIterator->Base);
	return __xrtAVLTreeItem(pIterator->Tree, pNode);
}



/* 提前结束拥有式树迭代。 */
XRT_API void xrtAVLTreeIterEnd(xavltreeiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}

	xrtAVLIterEnd(&pIterator->Base);
	pIterator->Tree = NULL;
}

#endif
