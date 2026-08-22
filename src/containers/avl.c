#include "../internal/xrt_avl.h"



#if defined(XRT_FEATURE_AVL)

/* 检查树的公开摘要状态是否自洽。 */
static bool __xrtAVLValid(const xavl* pTree)
{
	if ( pTree == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pTree->Count == 0) && (pTree->Root != NULL)) ||
		((pTree->Count != 0) && (pTree->Root == NULL))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 返回空节点为零的 AVL 高度。 */
static uint8 __xrtAVLHeight(const xavlnode* pNode)
{
	return pNode != NULL ? pNode->Height : 0;
}



/* 根据两个已经平衡的子树更新节点高度。 */
static void __xrtAVLUpdateHeight(xavlnode* pNode)
{
	uint8 iLeft = __xrtAVLHeight(pNode->Left);
	uint8 iRight = __xrtAVLHeight(pNode->Right);

	pNode->Height = (uint8)((iLeft > iRight ? iLeft : iRight) + 1u);
}



/* 返回右子树高度减左子树高度。 */
static int __xrtAVLBalance(const xavlnode* pNode)
{
	return (int)__xrtAVLHeight(pNode->Right) - (int)__xrtAVLHeight(pNode->Left);
}



/* 将左子节点旋转到当前子树根。 */
static xavlnode* __xrtAVLRotateRight(xavlnode* pRoot)
{
	xavlnode* pLeft = pRoot->Left;

	pRoot->Left = pLeft->Right;
	pLeft->Right = pRoot;
	__xrtAVLUpdateHeight(pRoot);
	__xrtAVLUpdateHeight(pLeft);
	return pLeft;
}



/* 将右子节点旋转到当前子树根。 */
static xavlnode* __xrtAVLRotateLeft(xavlnode* pRoot)
{
	xavlnode* pRight = pRoot->Right;

	pRoot->Right = pRight->Left;
	pRight->Left = pRoot;
	__xrtAVLUpdateHeight(pRoot);
	__xrtAVLUpdateHeight(pRight);
	return pRight;
}



/* 从变动位置向根逐层恢复 AVL 高度和平衡约束。 */
static void __xrtAVLRebalance(xavlnode*** pPath, size_t iDepth)
{
	while ( iDepth != 0 ) {
		xavlnode** ppRoot = pPath[--iDepth];
		xavlnode* pRoot = *ppRoot;
		int iBalance;

		if ( pRoot == NULL ) {
			continue;
		}
		__xrtAVLUpdateHeight(pRoot);
		iBalance = __xrtAVLBalance(pRoot);

		/* 双旋转先修正高子树方向，再完成根旋转。 */
		if ( iBalance < -1 ) {
			if ( __xrtAVLBalance(pRoot->Left) > 0 ) {
				pRoot->Left = __xrtAVLRotateLeft(pRoot->Left);
			}
			*ppRoot = __xrtAVLRotateRight(pRoot);
		} else if ( iBalance > 1 ) {
			if ( __xrtAVLBalance(pRoot->Right) < 0 ) {
				pRoot->Right = __xrtAVLRotateRight(pRoot->Right);
			}
			*ppRoot = __xrtAVLRotateLeft(pRoot);
		}
	}
}



/* 推进结构版本，并避免初始化状态与修改后状态相同。 */
static void __xrtAVLChanged(xavl* pTree)
{
	pTree->Version++;
	if ( pTree->Version == 0 ) {
		pTree->Version = 1;
	}
}



/* 初始化外置迭代器的共同借用状态。 */
static bool __xrtAVLIterPrepare(const xavl* pTree, xavliter* pIterator, bool bReverse)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(xavliter));
	}
	if ( (pIterator == NULL) || !__xrtAVLValid(pTree) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	pIterator->Tree = pTree;
	pIterator->Version = pTree->Version;
	pIterator->Reverse = bReverse;
	pIterator->Active = true;
	return true;
}



/* 以指定方向初始化外置迭代路径。 */
static bool __xrtAVLIterStart(const xavl* pTree, xavliter* pIterator, bool bReverse)
{
	xavlnode* pNode;

	if ( !__xrtAVLIterPrepare(pTree, pIterator, bReverse) ) {
		return false;
	}

	/* 保存通向顺序首项或末项的整条路径。 */
	pNode = pTree->Root;
	while ( pNode != NULL ) {
		if ( pIterator->Depth >= XRT_AVL_HEIGHT_MAX ) {
			xrtAVLIterEnd(pIterator);
			__xrtErrorSetInvalidState();
			return false;
		}
		pIterator->Path[pIterator->Depth++] = pNode;
		pNode = bReverse ? pNode->Right : pNode->Left;
	}

	return true;
}



/* 从指定包含边界构造升序或降序迭代路径。 */
static bool __xrtAVLIterStartFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	bool bReverse,
	xavliter* pIterator
)
{
	xavlnode* pNode;

	if ( pCompare == NULL ) {
		if ( pIterator != NULL ) {
			memset(pIterator, 0, sizeof(xavliter));
		}
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtAVLIterPrepare(pTree, pIterator, bReverse) ) {
		return false;
	}

	/* 路径只保留可能成为下一结果的候选祖先。 */
	pNode = pTree->Root;
	while ( pNode != NULL ) {
		int iOrder;

		if ( pIterator->Depth >= XRT_AVL_HEIGHT_MAX ) {
			xrtAVLIterEnd(pIterator);
			__xrtErrorSetInvalidState();
			return false;
		}
		iOrder = pCompare(pKey, pNode, pUserData);
		if ( (!bReverse && (iOrder <= 0)) || (bReverse && (iOrder >= 0)) ) {
			pIterator->Path[pIterator->Depth++] = pNode;
			pNode = bReverse ? pNode->Right : pNode->Left;
		} else {
			pNode = bReverse ? pNode->Left : pNode->Right;
		}
	}

	return true;
}



/* 将节点恢复为未挂入任何树的状态。 */
XRT_API void xrtAVLNodeInit(xavlnode* pNode)
{
	if ( pNode == NULL ) {
		return;
	}

	pNode->Left = NULL;
	pNode->Right = NULL;
	pNode->Height = 0;
}



/* 初始化一棵不拥有节点内存的空树。 */
XRT_API bool xrtAVLInit(xavl* pTree)
{
	if ( pTree == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	memset(pTree, 0, sizeof(xavl));
	return true;
}



/* 忘记全部节点但不释放或逐个重置节点。 */
XRT_API void xrtAVLClear(xavl* pTree)
{
	if ( !__xrtAVLValid(pTree) ) {
		return;
	}
	if ( pTree->Count == 0 ) {
		return;
	}

	pTree->Root = NULL;
	pTree->Count = 0;
	__xrtAVLChanged(pTree);
}



/* 插入外部节点；重复键返回已有节点且不修改候选节点。 */
XRT_API xavlnode* xrtAVLInsert(
	xavl* pTree,
	xavlnode* pNode,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	bool* pNew
)
{
	xavlnode** ppNode;
	xavlnode** pPath[XRT_AVL_HEIGHT_MAX];
	size_t iDepth = 0;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtAVLValid(pTree) || (pNode == NULL) || (pCompare == NULL) ) {
		if ( (pNode == NULL) || (pCompare == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	if ( pTree->Count == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}

	/* 一次查找同时形成自底向上再平衡所需的父链接路径。 */
	ppNode = &pTree->Root;
	while ( *ppNode != NULL ) {
		int iOrder;

		if ( iDepth >= XRT_AVL_HEIGHT_MAX ) {
			__xrtErrorSetInvalidState();
			return NULL;
		}
		pPath[iDepth++] = ppNode;
		iOrder = pCompare(pKey, *ppNode, pUserData);
		if ( iOrder < 0 ) {
			ppNode = &(*ppNode)->Left;
		} else if ( iOrder > 0 ) {
			ppNode = &(*ppNode)->Right;
		} else {
			return *ppNode;
		}
	}

	/* 新键只接受独立节点，避免把已挂树节点再次链接成环。 */
	if ( (pNode->Left != NULL) || (pNode->Right != NULL) || (pNode->Height != 0) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}

	pNode->Left = NULL;
	pNode->Right = NULL;
	pNode->Height = 1;
	*ppNode = pNode;
	__xrtAVLRebalance(pPath, iDepth);
	pTree->Count++;
	__xrtAVLChanged(pTree);
	if ( pNew != NULL ) {
		*pNew = true;
	}
	return pNode;
}



/* 删除指定键并返回已经恢复为独立状态的原节点。 */
XRT_API xavlnode* xrtAVLRemove(
	xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
)
{
	xavlnode** ppNode;
	xavlnode** pPath[XRT_AVL_HEIGHT_MAX];
	size_t iDepth = 0;
	xavlnode* pDelete;

	if ( !__xrtAVLValid(pTree) || (pCompare == NULL) ) {
		if ( pCompare == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}

	/* 查找删除位置，并保存沿途所有父链接。 */
	ppNode = &pTree->Root;
	while ( *ppNode != NULL ) {
		int iOrder;

		if ( iDepth >= XRT_AVL_HEIGHT_MAX ) {
			__xrtErrorSetInvalidState();
			return NULL;
		}
		pPath[iDepth++] = ppNode;
		iOrder = pCompare(pKey, *ppNode, pUserData);
		if ( iOrder < 0 ) {
			ppNode = &(*ppNode)->Left;
		} else if ( iOrder > 0 ) {
			ppNode = &(*ppNode)->Right;
		} else {
			break;
		}
	}
	if ( *ppNode == NULL ) {
		return NULL;
	}
	pDelete = *ppNode;

	if ( pDelete->Left == NULL ) {
		/* 零或一个右子节点时可直接用右子树替换。 */
		*ppNode = pDelete->Right;
	} else {
		xavlnode** ppDelete = ppNode;
		size_t iDeleteDepth = iDepth;
		xavlnode* pReplace;

		/* 取左子树最右节点作为顺序前驱，并从原位置摘除。 */
		ppNode = &pDelete->Left;
		while ( (*ppNode)->Right != NULL ) {
			if ( iDepth >= XRT_AVL_HEIGHT_MAX ) {
				__xrtErrorSetInvalidState();
				return NULL;
			}
			pPath[iDepth++] = ppNode;
			ppNode = &(*ppNode)->Right;
		}
		pReplace = *ppNode;
		*ppNode = pReplace->Left;

		pReplace->Left = pDelete->Left;
		pReplace->Right = pDelete->Right;
		pReplace->Height = pDelete->Height;
		*ppDelete = pReplace;

		/* 深层前驱路径原先指向旧根左子树，替换后必须改指向新根。 */
		if ( iDepth > iDeleteDepth ) {
			pPath[iDeleteDepth] = &pReplace->Left;
		}
	}

	__xrtAVLRebalance(pPath, iDepth);
	pTree->Count--;
	__xrtAVLChanged(pTree);
	xrtAVLNodeInit(pDelete);
	return pDelete;
}



/* 查找与 key 相等的节点，未找到不设置错误。 */
XRT_API xavlnode* xrtAVLFind(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
)
{
	xavlnode* pNode;

	if ( !__xrtAVLValid(pTree) || (pCompare == NULL) ) {
		if ( pCompare == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}

	pNode = pTree->Root;
	while ( pNode != NULL ) {
		int iOrder = pCompare(pKey, pNode, pUserData);

		if ( iOrder < 0 ) {
			pNode = pNode->Left;
		} else if ( iOrder > 0 ) {
			pNode = pNode->Right;
		} else {
			return pNode;
		}
	}
	return NULL;
}



/* 返回第一项不小于 key 的节点。 */
XRT_API xavlnode* xrtAVLLowerBound(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
)
{
	xavlnode* pNode;
	xavlnode* pResult = NULL;

	if ( !__xrtAVLValid(pTree) || (pCompare == NULL) ) {
		if ( pCompare == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}

	pNode = pTree->Root;
	while ( pNode != NULL ) {
		int iOrder = pCompare(pKey, pNode, pUserData);

		if ( iOrder <= 0 ) {
			pResult = pNode;
			pNode = pNode->Left;
		} else {
			pNode = pNode->Right;
		}
	}
	return pResult;
}



/* 返回第一项严格大于 key 的节点。 */
XRT_API xavlnode* xrtAVLUpperBound(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
)
{
	xavlnode* pNode;
	xavlnode* pResult = NULL;

	if ( !__xrtAVLValid(pTree) || (pCompare == NULL) ) {
		if ( pCompare == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}

	pNode = pTree->Root;
	while ( pNode != NULL ) {
		int iOrder = pCompare(pKey, pNode, pUserData);

		if ( iOrder < 0 ) {
			pResult = pNode;
			pNode = pNode->Left;
		} else {
			pNode = pNode->Right;
		}
	}
	return pResult;
}



/* 返回按比较器顺序排列的第一项。 */
XRT_API xavlnode* xrtAVLFirst(const xavl* pTree)
{
	xavlnode* pNode;

	if ( !__xrtAVLValid(pTree) ) {
		return NULL;
	}
	pNode = pTree->Root;
	while ( (pNode != NULL) && (pNode->Left != NULL) ) {
		pNode = pNode->Left;
	}
	return pNode;
}



/* 返回按比较器顺序排列的最后一项。 */
XRT_API xavlnode* xrtAVLLast(const xavl* pTree)
{
	xavlnode* pNode;

	if ( !__xrtAVLValid(pTree) ) {
		return NULL;
	}
	pNode = pTree->Root;
	while ( (pNode != NULL) && (pNode->Right != NULL) ) {
		pNode = pNode->Right;
	}
	return pNode;
}



/* 按升序访问节点并返回实际访问数量。 */
XRT_API size_t xrtAVLVisit(const xavl* pTree, xavlvisitor pVisitor, ptr pUserData)
{
	xavliter tIterator;
	xavlnode* pNode;
	size_t iVisited = 0;

	if ( pVisitor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !xrtAVLIterBegin(pTree, &tIterator) ) {
		return 0;
	}

	while ( (pNode = xrtAVLIterNext(&tIterator)) != NULL ) {
		iVisited++;
		if ( !pVisitor(pNode, pUserData) ) {
			break;
		}
	}
	xrtAVLIterEnd(&tIterator);
	return iVisited;
}



/* 启动升序外置迭代器。 */
XRT_API bool xrtAVLIterBegin(const xavl* pTree, xavliter* pIterator)
{
	return __xrtAVLIterStart(pTree, pIterator, false);
}



/* 启动降序外置迭代器。 */
XRT_API bool xrtAVLIterRBegin(const xavl* pTree, xavliter* pIterator)
{
	return __xrtAVLIterStart(pTree, pIterator, true);
}



/* 从第一项不小于 key 的节点开始升序迭代。 */
XRT_API bool xrtAVLIterFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
)
{
	return __xrtAVLIterStartFrom(
		pTree,
		pKey,
		pCompare,
		pUserData,
		false,
		pIterator
	);
}



/* 从第一项不大于 key 的节点开始降序迭代。 */
XRT_API bool xrtAVLIterRFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
)
{
	return __xrtAVLIterStartFrom(
		pTree,
		pKey,
		pCompare,
		pUserData,
		true,
		pIterator
	);
}



/* 返回下一节点，并在结构变化时终止失效迭代器。 */
XRT_API xavlnode* xrtAVLIterNext(xavliter* pIterator)
{
	xavlnode* pNode;
	xavlnode* pBranch;

	if ( (pIterator == NULL) || !pIterator->Active || (pIterator->Tree == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	if ( pIterator->Version != pIterator->Tree->Version ) {
		xrtAVLIterEnd(pIterator);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( pIterator->Depth == 0 ) {
		xrtAVLIterEnd(pIterator);
		return NULL;
	}

	pNode = pIterator->Path[--pIterator->Depth];
	pBranch = pIterator->Reverse ? pNode->Left : pNode->Right;
	while ( pBranch != NULL ) {
		if ( pIterator->Depth >= XRT_AVL_HEIGHT_MAX ) {
			xrtAVLIterEnd(pIterator);
			__xrtErrorSetInvalidState();
			return NULL;
		}
		pIterator->Path[pIterator->Depth++] = pBranch;
		pBranch = pIterator->Reverse ? pBranch->Right : pBranch->Left;
	}

	return pNode;
}



/* 提前结束迭代并清除它持有的借用状态。 */
XRT_API void xrtAVLIterEnd(xavliter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}

	memset(pIterator, 0, sizeof(xavliter));
}

#endif
