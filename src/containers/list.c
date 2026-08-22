#include "../internal/xrt_internal.h"



#if defined(XRT_FEATURE_LIST)

#define XRT_LIST_STATE_READY 1u



/* 设置链表模块结构化错误。 */
static void __xrtListError(
	xerrkind Kind,
	xlisterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtErrorSetDetail(
		Kind,
		"xrt.list",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* O(1) 验证链表初始化状态和端点不变量。 */
static bool __xrtListStateValid(const xlist* pList, cstr sOperation)
{
	if ( pList == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			sOperation, "the list is null");
		return false;
	}
	if ( pList->State != XRT_LIST_STATE_READY ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			sOperation, "the list is not initialized");
		return false;
	}
	if ( pList->Count == 0 ) {
		if ( (pList->First != NULL) || (pList->Last != NULL) ) {
			__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
				sOperation, "the empty list has non-empty endpoints");
			return false;
		}
		return true;
	}
	if (
		(pList->First == NULL) ||
		(pList->Last == NULL) ||
		(pList->First->Owner != pList) ||
		(pList->Last->Owner != pList) ||
		(pList->First->Prev != NULL) ||
		(pList->Last->Next != NULL)
	) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			sOperation, "the list endpoints are inconsistent");
		return false;
	}
	return true;
}



/* 完整验证双向链接和节点计数。 */
static bool __xrtListChainValid(const xlist* pList, cstr sOperation)
{
	xlistnode* pNode;
	xlistnode* pPrevious = NULL;
	size_t iCount = 0;

	if ( !__xrtListStateValid(pList, sOperation) ) {
		return false;
	}
	pNode = pList->First;
	while ( pNode != NULL ) {
		if (
			(iCount >= pList->Count) ||
			(pNode->Owner != pList) ||
			(pNode->Prev != pPrevious)
		) {
			__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
				sOperation, "the list links are inconsistent");
			return false;
		}
		pPrevious = pNode;
		pNode = pNode->Next;
		iCount++;
	}
	if ( (iCount != pList->Count) || (pPrevious != pList->Last) ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			sOperation, "the list count does not match its links");
		return false;
	}
	return true;
}



/* 验证待插入节点处于完整的未连接状态。 */
static bool __xrtListNodeDetached(const xlistnode* pNode, cstr sOperation)
{
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			sOperation, "the node is null");
		return false;
	}
	if (
		(pNode->Owner != NULL) ||
		(pNode->Prev != NULL) ||
		(pNode->Next != NULL)
	) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			sOperation, "the node is already linked or not initialized");
		return false;
	}
	return true;
}



/* 在已经验证的相邻节点之间连接新节点。 */
static bool __xrtListInsert(
	xlist* pList,
	xlistnode* pPrevious,
	xlistnode* pNext,
	xlistnode* pNode,
	cstr sOperation
)
{
	if (
		!__xrtListStateValid(pList, sOperation) ||
		!__xrtListNodeDetached(pNode, sOperation)
	) {
		return false;
	}
	if ( pList->Count == SIZE_MAX ) {
		__xrtListError(XERR_RANGE, XLIST_ERROR_RANGE,
			sOperation, "the list count would overflow");
		return false;
	}
	if (
		((pPrevious == NULL) && (pList->First != pNext)) ||
		((pPrevious != NULL) &&
			((pPrevious->Owner != pList) || (pPrevious->Next != pNext))) ||
		((pNext == NULL) && (pList->Last != pPrevious)) ||
		((pNext != NULL) &&
			((pNext->Owner != pList) || (pNext->Prev != pPrevious)))
	) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			sOperation, "the insertion position is inconsistent");
		return false;
	}

	pNode->Prev = pPrevious;
	pNode->Next = pNext;
	pNode->Owner = pList;
	if ( pPrevious != NULL ) {
		pPrevious->Next = pNode;
	} else {
		pList->First = pNode;
	}
	if ( pNext != NULL ) {
		pNext->Prev = pNode;
	} else {
		pList->Last = pNode;
	}
	pList->Count++;
	pList->Version++;
	return true;
}



/* 验证节点确实位于指定链表并且局部链接一致。 */
static bool __xrtListNodeOwned(
	const xlist* pList,
	const xlistnode* pNode,
	cstr sOperation
)
{
	if ( !__xrtListStateValid(pList, sOperation) ) {
		return false;
	}
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			sOperation, "the node is null");
		return false;
	}
	if ( pNode->Owner != pList ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			sOperation, "the node does not belong to the list");
		return false;
	}
	if (
		((pNode->Prev == NULL) && (pList->First != pNode)) ||
		((pNode->Prev != NULL) &&
			((pNode->Prev->Owner != pList) || (pNode->Prev->Next != pNode))) ||
		((pNode->Next == NULL) && (pList->Last != pNode)) ||
		((pNode->Next != NULL) &&
			((pNode->Next->Owner != pList) || (pNode->Next->Prev != pNode)))
	) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			sOperation, "the node links are inconsistent");
		return false;
	}
	return true;
}



/* 初始化新的链表存储。 */
XRT_API void xrtListInit(xlist* pList)
{
	if ( pList == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"init", "the list is null");
		return;
	}
	pList->First = NULL;
	pList->Last = NULL;
	pList->Count = 0;
	pList->Version = 0;
	pList->State = XRT_LIST_STATE_READY;
}



/* 初始化新的节点存储。 */
XRT_API void xrtListNodeInit(xlistnode* pNode)
{
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"node-init", "the node is null");
		return;
	}
	pNode->Prev = NULL;
	pNode->Next = NULL;
	pNode->Owner = NULL;
}



/* 判断链表是否已经初始化。 */
XRT_API bool xrtListReady(const xlist* pList)
{
	return (pList != NULL) && (pList->State == XRT_LIST_STATE_READY);
}



/* 完整验证链表结构。 */
XRT_API bool xrtListValidate(const xlist* pList)
{
	return __xrtListChainValid(pList, "validate");
}



/* 返回链表是否为空。 */
XRT_API bool xrtListEmpty(const xlist* pList)
{
	if ( !__xrtListStateValid(pList, "empty") ) {
		return true;
	}
	return pList->Count == 0;
}



/* 返回链表节点数量。 */
XRT_API size_t xrtListCount(const xlist* pList)
{
	if ( !__xrtListStateValid(pList, "count") ) {
		return 0;
	}
	return pList->Count;
}



/* 返回链表首节点。 */
XRT_API xlistnode* xrtListFirst(const xlist* pList)
{
	if ( !__xrtListStateValid(pList, "first") ) {
		return NULL;
	}
	return pList->First;
}



/* 返回链表尾节点。 */
XRT_API xlistnode* xrtListLast(const xlist* pList)
{
	if ( !__xrtListStateValid(pList, "last") ) {
		return NULL;
	}
	return pList->Last;
}



/* 返回节点前驱。 */
XRT_API xlistnode* xrtListPrev(const xlistnode* pNode)
{
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"prev", "the node is null");
		return NULL;
	}
	return pNode->Prev;
}



/* 返回节点后继。 */
XRT_API xlistnode* xrtListNext(const xlistnode* pNode)
{
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"next", "the node is null");
		return NULL;
	}
	return pNode->Next;
}



/* 返回节点所属链表。 */
XRT_API xlist* xrtListOwner(const xlistnode* pNode)
{
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"owner", "the node is null");
		return NULL;
	}
	return pNode->Owner;
}



/* 判断节点是否属于指定链表。 */
XRT_API bool xrtListContains(const xlist* pList, const xlistnode* pNode)
{
	if ( !__xrtListStateValid(pList, "contains") ) {
		return false;
	}
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"contains", "the node is null");
		return false;
	}
	return pNode->Owner == pList;
}



/* 判断节点是否连接到任意链表。 */
XRT_API bool xrtListLinked(const xlistnode* pNode)
{
	if ( pNode == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"linked", "the node is null");
		return false;
	}
	return pNode->Owner != NULL;
}



/* 在链表首部插入节点。 */
XRT_API bool xrtListPushFront(xlist* pList, xlistnode* pNode)
{
	if ( !__xrtListStateValid(pList, "push-front") ) {
		return false;
	}
	return __xrtListInsert(pList, NULL, pList->First, pNode, "push-front");
}



/* 在链表尾部插入节点。 */
XRT_API bool xrtListPushBack(xlist* pList, xlistnode* pNode)
{
	if ( !__xrtListStateValid(pList, "push-back") ) {
		return false;
	}
	return __xrtListInsert(pList, pList->Last, NULL, pNode, "push-back");
}



/* 在参考节点之前插入节点。 */
XRT_API bool xrtListInsertBefore(
	xlist* pList,
	xlistnode* pPosition,
	xlistnode* pNode
)
{
	if ( !__xrtListNodeOwned(pList, pPosition, "insert-before") ) {
		return false;
	}
	return __xrtListInsert(
		pList,
		pPosition->Prev,
		pPosition,
		pNode,
		"insert-before"
	);
}



/* 在参考节点之后插入节点。 */
XRT_API bool xrtListInsertAfter(
	xlist* pList,
	xlistnode* pPosition,
	xlistnode* pNode
)
{
	if ( !__xrtListNodeOwned(pList, pPosition, "insert-after") ) {
		return false;
	}
	return __xrtListInsert(
		pList,
		pPosition,
		pPosition->Next,
		pNode,
		"insert-after"
	);
}



/* 从链表分离节点。 */
XRT_API bool xrtListRemove(xlist* pList, xlistnode* pNode)
{
	if ( !__xrtListNodeOwned(pList, pNode, "remove") ) {
		return false;
	}
	if ( pList->Count == 0 ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			"remove", "the non-empty node has an empty owner list");
		return false;
	}
	if ( pNode->Prev != NULL ) {
		pNode->Prev->Next = pNode->Next;
	} else {
		pList->First = pNode->Next;
	}
	if ( pNode->Next != NULL ) {
		pNode->Next->Prev = pNode->Prev;
	} else {
		pList->Last = pNode->Prev;
	}
	pNode->Prev = NULL;
	pNode->Next = NULL;
	pNode->Owner = NULL;
	pList->Count--;
	pList->Version++;
	return true;
}



/* 移除并返回首节点。 */
XRT_API xlistnode* xrtListPopFront(xlist* pList)
{
	xlistnode* pNode;

	if ( !__xrtListStateValid(pList, "pop-front") ) {
		return NULL;
	}
	pNode = pList->First;
	if ( (pNode != NULL) && !xrtListRemove(pList, pNode) ) {
		return NULL;
	}
	return pNode;
}



/* 移除并返回尾节点。 */
XRT_API xlistnode* xrtListPopBack(xlist* pList)
{
	xlistnode* pNode;

	if ( !__xrtListStateValid(pList, "pop-back") ) {
		return NULL;
	}
	pNode = pList->Last;
	if ( (pNode != NULL) && !xrtListRemove(pList, pNode) ) {
		return NULL;
	}
	return pNode;
}



/* 把节点移动到链表首部。 */
XRT_API bool xrtListMoveFront(xlist* pList, xlistnode* pNode)
{
	xlistnode* pPrevious;
	xlistnode* pNext;

	if ( !__xrtListNodeOwned(pList, pNode, "move-front") ) {
		return false;
	}
	if ( pList->First == pNode ) {
		return true;
	}
	pPrevious = pNode->Prev;
	pNext = pNode->Next;
	pPrevious->Next = pNext;
	if ( pNext != NULL ) {
		pNext->Prev = pPrevious;
	} else {
		pList->Last = pPrevious;
	}
	pNode->Prev = NULL;
	pNode->Next = pList->First;
	pList->First->Prev = pNode;
	pList->First = pNode;
	pList->Version++;
	return true;
}



/* 把节点移动到链表尾部。 */
XRT_API bool xrtListMoveBack(xlist* pList, xlistnode* pNode)
{
	xlistnode* pPrevious;
	xlistnode* pNext;

	if ( !__xrtListNodeOwned(pList, pNode, "move-back") ) {
		return false;
	}
	if ( pList->Last == pNode ) {
		return true;
	}
	pPrevious = pNode->Prev;
	pNext = pNode->Next;
	pNext->Prev = pPrevious;
	if ( pPrevious != NULL ) {
		pPrevious->Next = pNext;
	} else {
		pList->First = pNext;
	}
	pNode->Prev = pList->Last;
	pNode->Next = NULL;
	pList->Last->Next = pNode;
	pList->Last = pNode;
	pList->Version++;
	return true;
}



/* 分离链表中的全部节点。 */
XRT_API bool xrtListClear(xlist* pList)
{
	xlistnode* pNode;
	xlistnode* pNext;

	if ( !__xrtListChainValid(pList, "clear") ) {
		return false;
	}
	if ( pList->Count == 0 ) {
		return true;
	}
	pNode = pList->First;
	while ( pNode != NULL ) {
		pNext = pNode->Next;
		pNode->Prev = NULL;
		pNode->Next = NULL;
		pNode->Owner = NULL;
		pNode = pNext;
	}
	pList->First = NULL;
	pList->Last = NULL;
	pList->Count = 0;
	pList->Version++;
	return true;
}



/* 共同启动指定方向的外置迭代器。 */
static bool __xrtListIterBegin(
	xlist* pList,
	xlistiter* pIterator,
	bool bReverse,
	cstr sOperation
)
{
	if ( pIterator == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			sOperation, "the iterator is null");
		return false;
	}
	if ( !__xrtListStateValid(pList, sOperation) ) {
		memset(pIterator, 0, sizeof(*pIterator));
		return false;
	}
	pIterator->List = pList;
	pIterator->Next = bReverse ? pList->Last : pList->First;
	pIterator->Current = NULL;
	pIterator->Version = pList->Version;
	pIterator->Reverse = bReverse;
	return true;
}



/* 启动正向迭代器。 */
XRT_API bool xrtListIterBegin(xlist* pList, xlistiter* pIterator)
{
	return __xrtListIterBegin(pList, pIterator, false, "iter-begin");
}



/* 启动反向迭代器。 */
XRT_API bool xrtListIterRBegin(xlist* pList, xlistiter* pIterator)
{
	return __xrtListIterBegin(pList, pIterator, true, "iter-rbegin");
}



/* 返回迭代器下一节点。 */
XRT_API xlistnode* xrtListIterNext(xlistiter* pIterator)
{
	xlistnode* pNode;

	if ( pIterator == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"iter-next", "the iterator is null");
		return NULL;
	}
	if ( pIterator->List == NULL ) {
		return NULL;
	}
	if ( pIterator->Version != pIterator->List->Version ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_MODIFIED,
			"iter-next", "the list was modified during iteration");
		xrtListIterEnd(pIterator);
		return NULL;
	}
	pNode = pIterator->Next;
	if ( pNode == NULL ) {
		xrtListIterEnd(pIterator);
		return NULL;
	}
	if ( pNode->Owner != pIterator->List ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			"iter-next", "the next node does not belong to the list");
		xrtListIterEnd(pIterator);
		return NULL;
	}
	pIterator->Current = pNode;
	pIterator->Next = pIterator->Reverse ? pNode->Prev : pNode->Next;
	return pNode;
}



/* 通过迭代器移除最近返回的节点。 */
XRT_API bool xrtListIterRemove(xlistiter* pIterator)
{
	xlist* pList;
	xlistnode* pNode;

	if ( pIterator == NULL ) {
		__xrtListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT,
			"iter-remove", "the iterator is null");
		return false;
	}
	pList = pIterator->List;
	pNode = pIterator->Current;
	if ( (pList == NULL) || (pNode == NULL) ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_STATE,
			"iter-remove", "the iterator has no current node");
		return false;
	}
	if ( pIterator->Version != pList->Version ) {
		__xrtListError(XERR_STATE, XLIST_ERROR_MODIFIED,
			"iter-remove", "the list was modified during iteration");
		xrtListIterEnd(pIterator);
		return false;
	}
	if ( !xrtListRemove(pList, pNode) ) {
		xrtListIterEnd(pIterator);
		return false;
	}
	pIterator->Current = NULL;
	pIterator->Version = pList->Version;
	return true;
}



/* 提前结束外置迭代器。 */
XRT_API void xrtListIterEnd(xlistiter* pIterator)
{
	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
}

#endif
