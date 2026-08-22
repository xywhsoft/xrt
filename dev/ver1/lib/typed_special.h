// 带类型描述的专用数据结构。
//
// 本层只为 XRT 现有的栈、AVL 树和无锁队列补充运行时类型与所有权管理。
// 底层算法仍保留在原模块中，C 程序也可以继续直接使用它们。

static uint32 __xrtTypedAlignOffset(uint32 iValue, size_t iAlign)
{
	uint32 iMask;

	if ( iAlign <= 1u ) {
		return iValue;
	}
	if ( iAlign > (size_t)UINT32_MAX ) {
		return 0u;
	}
	iMask = (uint32)iAlign - 1u;
	if ( (iAlign & iMask) != 0u || iValue > UINT32_MAX - iMask ) {
		return 0u;
	}
	return (iValue + iMask) & ~iMask;
}


static void __xrtTypedReplaceItem(const xrt_type_desc* pType, ptr pOut, const ptr pItem)
{
	if ( pOut == NULL ) {
		return;
	}
	__xrtTypedDropItem(pType, pOut);
	__xrtTypedCopyItem(pType, pOut, pItem);
}


// ------------------------------------ 带类型栈 ------------------------------------

XXAPI xtstack xrtTypedStackCreate(const xrt_type_desc* pItemType)
{
	xtstack pStack;

	pStack = (xtstack)xrtCalloc(1u, sizeof(xrt_typed_stack_struct));
	if ( pStack == NULL ) {
		return NULL;
	}
	pStack->ItemType = pItemType;
	pStack->ItemLength = __xrtTypedItemLength(pItemType);
	pStack->RefCount = 1;
	pStack->HeapOwned = TRUE;
	xrtDynStackInit(&pStack->Stack, pStack->ItemLength);
	return pStack;
}


XXAPI xtstack xrtTypedStackRetain(xtstack pStack)
{
	if ( pStack != NULL && pStack->HeapOwned && pStack->RefCount > 0 ) {
		__xrtAtomicAddFetch32(&pStack->RefCount, 1);
	}
	return pStack;
}


XXAPI void xrtTypedStackClear(xtstack pStack)
{
	ptr pItem;

	if ( pStack == NULL ) {
		return;
	}
	while ( (pItem = xrtDynStackTop(&pStack->Stack)) != NULL ) {
		__xrtTypedDropItem(pStack->ItemType, pItem);
		(void)xrtDynStackPop(&pStack->Stack);
	}
}


XXAPI void xrtTypedStackRelease(xtstack pStack)
{
	if ( pStack == NULL || !pStack->HeapOwned || pStack->RefCount <= 0 ) {
		return;
	}
	if ( __xrtAtomicAddFetch32(&pStack->RefCount, -1) == 0 ) {
		xrtTypedStackClear(pStack);
		xrtDynStackUnit(&pStack->Stack);
		pStack->ItemType = NULL;
		pStack->ItemLength = 0u;
		xrtFree(pStack);
	}
}


XXAPI bool xrtTypedStackPush(xtstack pStack, const ptr pItem)
{
	ptr pSlot;

	if ( pStack == NULL ) {
		return FALSE;
	}
	pSlot = xrtDynStackPush(&pStack->Stack);
	if ( pSlot == NULL ) {
		return FALSE;
	}
	__xrtTypedCopyItem(pStack->ItemType, pSlot, pItem);
	return TRUE;
}


XXAPI bool xrtTypedStackPop(xtstack pStack, ptr pOut)
{
	ptr pItem;

	if ( pStack == NULL || pOut == NULL ) {
		return FALSE;
	}
	pItem = xrtDynStackTop(&pStack->Stack);
	if ( pItem == NULL ) {
		return FALSE;
	}
	__xrtTypedReplaceItem(pStack->ItemType, pOut, pItem);
	__xrtTypedDropItem(pStack->ItemType, pItem);
	(void)xrtDynStackPop(&pStack->Stack);
	return TRUE;
}


XXAPI bool xrtTypedStackPeek(xtstack pStack, ptr pOut)
{
	ptr pItem;

	if ( pStack == NULL || pOut == NULL ) {
		return FALSE;
	}
	pItem = xrtDynStackTop(&pStack->Stack);
	if ( pItem == NULL ) {
		return FALSE;
	}
	__xrtTypedReplaceItem(pStack->ItemType, pOut, pItem);
	return TRUE;
}


XXAPI uint32 xrtTypedStackCount(xtstack pStack)
{
	return pStack != NULL ? pStack->Stack.Count : 0u;
}


XXAPI const xrt_type_desc* xrtTypedStackItemType(xtstack pStack)
{
	return pStack != NULL ? pStack->ItemType : NULL;
}


// ------------------------------------ 带类型 AVL 树 ------------------------------------

typedef struct __xrt_typed_avl_probe {
	xtavltree Tree;
	const void* Key;
} __xrt_typed_avl_probe;


static int __xrtTypedAvlCompare(ptr pNode, ptr pKey)
{
	xtavltree pTree;
	__xrt_typed_avl_probe* pProbe;
	ptr pNodeKey;

	if ( pNode == NULL || pKey == NULL ) {
		return pNode == pKey ? 0 : (pNode != NULL ? 1 : -1);
	}
	pTree = *(xtavltree*)pNode;
	pProbe = (__xrt_typed_avl_probe*)pKey;
	if ( pTree == NULL || pProbe->Tree != pTree ) {
		return pTree == pProbe->Tree ? 0 : (pTree != NULL ? 1 : -1);
	}
	pNodeKey = (uint8*)pNode + pTree->KeyOffset;
	{
		int iCompare = __xrtTypedCompareItem(pTree->KeyType, pNodeKey, (ptr)pProbe->Key);
		/* XRT AVL 的左分支接收较大的节点；这里反转比较结果，公开自然升序。 */
		return iCompare < 0 ? 1 : (iCompare > 0 ? -1 : 0);
	}
}


static void __xrtTypedAvlFree(ptr pTreeValue, ptr pNode)
{
	xtavltree pTree;

	(void)pTreeValue;
	if ( pNode == NULL ) {
		return;
	}
	pTree = *(xtavltree*)pNode;
	if ( pTree == NULL ) {
		return;
	}
	__xrtTypedDropItem(pTree->KeyType, (uint8*)pNode + pTree->KeyOffset);
	__xrtTypedDropItem(pTree->ValueType, (uint8*)pNode + pTree->ValueOffset);
}


static bool __xrtTypedAvlInitRaw(xtavltree pTree)
{
	if ( pTree == NULL || pTree->EntrySize == 0u ) {
		return FALSE;
	}
	xrtAVLTreeInit(&pTree->Tree, pTree->EntrySize, __xrtTypedAvlCompare, pTree->Mode);
	pTree->Tree.FreeProc = __xrtTypedAvlFree;
	return TRUE;
}


XXAPI xtavltree xrtTypedAvlTreeCreate(
	const xrt_type_desc* pKeyType,
	const xrt_type_desc* pValueType,
	uint32 iMode)
{
	xtavltree pTree;
	uint32 iKeyOffset;
	uint32 iValueOffset;
	uint32 iEntrySize;
	uint32 iKeySize = __xrtTypedItemLength(pKeyType);
	uint32 iValueSize = __xrtTypedItemLength(pValueType);

	if ( pKeyType == NULL || pKeyType->Ops == NULL || pKeyType->Ops->compare == NULL ) {
		xrtSetError("typed AVL tree key type must provide compare.", FALSE);
		return NULL;
	}
	iKeyOffset = __xrtTypedAlignOffset((uint32)sizeof(xtavltree), pKeyType->Align);
	if ( iKeyOffset == 0u || iKeyOffset > UINT32_MAX - iKeySize ) {
		return NULL;
	}
	iValueOffset = __xrtTypedAlignOffset(iKeyOffset + iKeySize, pValueType != NULL ? pValueType->Align : sizeof(ptr));
	if ( iValueOffset == 0u || iValueOffset > UINT32_MAX - iValueSize ) {
		return NULL;
	}
	iEntrySize = iValueOffset + iValueSize;
	pTree = (xtavltree)xrtCalloc(1u, sizeof(xrt_typed_avl_tree_struct));
	if ( pTree == NULL ) {
		return NULL;
	}
	pTree->RefCount = 1;
	pTree->HeapOwned = TRUE;
	pTree->KeyType = pKeyType;
	pTree->ValueType = pValueType;
	pTree->KeyOffset = iKeyOffset;
	pTree->ValueOffset = iValueOffset;
	pTree->EntrySize = iEntrySize;
	pTree->Mode = iMode;
	pTree->Lock = xrtMutexCreate();
	if ( pTree->Lock == NULL ) {
		xrtFree(pTree);
		return NULL;
	}
	if ( !__xrtTypedAvlInitRaw(pTree) ) {
		xrtMutexDestroy(pTree->Lock);
		xrtFree(pTree);
		return NULL;
	}
	return pTree;
}


XXAPI xtavltree xrtTypedAvlTreeRetain(xtavltree pTree)
{
	if ( pTree != NULL && pTree->HeapOwned && pTree->RefCount > 0 ) {
		__xrtAtomicAddFetch32(&pTree->RefCount, 1);
	}
	return pTree;
}


XXAPI void xrtTypedAvlTreeRelease(xtavltree pTree)
{
	if ( pTree == NULL || !pTree->HeapOwned || pTree->RefCount <= 0 ) {
		return;
	}
	if ( __xrtAtomicAddFetch32(&pTree->RefCount, -1) == 0 ) {
		xrtMutexLock(pTree->Lock);
		xrtAVLTreeUnit(&pTree->Tree);
		pTree->KeyType = NULL;
		pTree->ValueType = NULL;
		xrtMutexUnlock(pTree->Lock);
		xrtMutexDestroy(pTree->Lock);
		pTree->Lock = NULL;
		xrtFree(pTree);
	}
}


XXAPI bool xrtTypedAvlTreeSet(xtavltree pTree, const ptr pKey, const ptr pValue)
{
	__xrt_typed_avl_probe tProbe;
	ptr pEntry;
	ptr pEntryValue;
	bool bNew = FALSE;

	if ( pTree == NULL || pKey == NULL ) {
		return FALSE;
	}
	xrtMutexLock(pTree->Lock);
	tProbe.Tree = pTree;
	tProbe.Key = pKey;
	pEntry = xrtAVLTreeInsert(&pTree->Tree, &tProbe, &bNew);
	if ( pEntry == NULL ) {
		xrtMutexUnlock(pTree->Lock);
		return FALSE;
	}
	pEntryValue = (uint8*)pEntry + pTree->ValueOffset;
	if ( bNew ) {
		*(xtavltree*)pEntry = pTree;
		__xrtTypedCopyItem(pTree->KeyType, (uint8*)pEntry + pTree->KeyOffset, pKey);
		__xrtTypedCopyItem(pTree->ValueType, pEntryValue, pValue);
	} else {
		__xrtTypedDropItem(pTree->ValueType, pEntryValue);
		__xrtTypedCopyItem(pTree->ValueType, pEntryValue, pValue);
	}
	xrtMutexUnlock(pTree->Lock);
	return TRUE;
}


static ptr __xrtTypedAvlFind(xtavltree pTree, const ptr pKey)
{
	__xrt_typed_avl_probe tProbe;

	if ( pTree == NULL || pKey == NULL ) {
		return NULL;
	}
	tProbe.Tree = pTree;
	tProbe.Key = pKey;
	return xrtAVLTreeSearch(&pTree->Tree, &tProbe);
}


XXAPI bool xrtTypedAvlTreeGet(xtavltree pTree, const ptr pKey, ptr pOut)
{
	ptr pEntry;

	if ( pTree == NULL || pOut == NULL ) {
		return FALSE;
	}
	xrtMutexLock(pTree->Lock);
	pEntry = __xrtTypedAvlFind(pTree, pKey);
	if ( pEntry == NULL ) {
		xrtMutexUnlock(pTree->Lock);
		return FALSE;
	}
	__xrtTypedReplaceItem(pTree->ValueType, pOut, (uint8*)pEntry + pTree->ValueOffset);
	xrtMutexUnlock(pTree->Lock);
	return TRUE;
}


XXAPI bool xrtTypedAvlTreeContains(xtavltree pTree, const ptr pKey)
{
	bool bExists;

	if ( pTree == NULL ) {
		return FALSE;
	}
	xrtMutexLock(pTree->Lock);
	bExists = __xrtTypedAvlFind(pTree, pKey) != NULL;
	xrtMutexUnlock(pTree->Lock);
	return bExists;
}


XXAPI bool xrtTypedAvlTreeRemove(xtavltree pTree, const ptr pKey)
{
	__xrt_typed_avl_probe tProbe;

	if ( pTree == NULL || pKey == NULL ) {
		return FALSE;
	}
	xrtMutexLock(pTree->Lock);
	tProbe.Tree = pTree;
	tProbe.Key = pKey;
	{
		bool bRemoved = xrtAVLTreeRemove(&pTree->Tree, &tProbe);
		xrtMutexUnlock(pTree->Lock);
		return bRemoved;
	}
}


XXAPI void xrtTypedAvlTreeClear(xtavltree pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	xrtMutexLock(pTree->Lock);
	xrtAVLTreeUnit(&pTree->Tree);
	(void)__xrtTypedAvlInitRaw(pTree);
	xrtMutexUnlock(pTree->Lock);
}


XXAPI uint32 xrtTypedAvlTreeCount(xtavltree pTree)
{
	uint32 iCount;

	if ( pTree == NULL ) {
		return 0u;
	}
	xrtMutexLock(pTree->Lock);
	iCount = pTree->Tree.Count;
	xrtMutexUnlock(pTree->Lock);
	return iCount;
}


static xtarray __xrtTypedAvlCollect(xtavltree pTree, bool bKeys, uint32 iMode)
{
	xtarray pResult;
	ptr pEntry;
	const xrt_type_desc* pType;

	if ( pTree == NULL ) {
		return NULL;
	}
	xrtMutexLock(pTree->Lock);
	pType = bKeys ? pTree->KeyType : pTree->ValueType;
	pResult = xrtTypedArrayCreate(pType, iMode);
	if ( pResult == NULL ) {
		xrtMutexUnlock(pTree->Lock);
		return NULL;
	}
	xrtAVLTreeIterBegin(&pTree->Tree);
	while ( (pEntry = xrtAVLTreeIterNext(&pTree->Tree)) != NULL ) {
		ptr pItem = (uint8*)pEntry + (bKeys ? pTree->KeyOffset : pTree->ValueOffset);
		if ( xrtTypedArrayAppend(pResult, pItem) == NULL ) {
			xrtAVLTreeIterEnd(&pTree->Tree);
			xrtTypedArrayRelease(pResult);
			xrtMutexUnlock(pTree->Lock);
			return NULL;
		}
	}
	xrtAVLTreeIterEnd(&pTree->Tree);
	xrtMutexUnlock(pTree->Lock);
	return pResult;
}


XXAPI xtarray xrtTypedAvlTreeKeys(xtavltree pTree, uint32 iMode)
{
	return __xrtTypedAvlCollect(pTree, TRUE, iMode);
}


XXAPI xtarray xrtTypedAvlTreeValues(xtavltree pTree, uint32 iMode)
{
	return __xrtTypedAvlCollect(pTree, FALSE, iMode);
}


XXAPI const xrt_type_desc* xrtTypedAvlTreeKeyType(xtavltree pTree)
{
	return pTree != NULL ? pTree->KeyType : NULL;
}


XXAPI const xrt_type_desc* xrtTypedAvlTreeValueType(xtavltree pTree)
{
	return pTree != NULL ? pTree->ValueType : NULL;
}


// ------------------------------------ 带类型队列 ------------------------------------

static ptr __xrtTypedQueueCreateRaw(uint32 iKind, const xqueue_config* pConfig)
{
	switch ( iKind ) {
		case XRT_TYPED_QUEUE_SPSC: return xrtSPSCQCreate(pConfig);
		case XRT_TYPED_QUEUE_MPSC: return xrtMPSCQCreate(pConfig);
		case XRT_TYPED_QUEUE_MPMC: return xrtMPMCQCreate(pConfig);
		case XRT_TYPED_QUEUE_WAIT: return xrtMPSCQWaitCreate(pConfig);
		default: return NULL;
	}
}


static xqueue_result __xrtTypedQueueTryPushRaw(xtqueue pQueue, ptr pItem)
{
	switch ( pQueue->Kind ) {
		case XRT_TYPED_QUEUE_SPSC: return xrtSPSCQTryPush((xspscq)pQueue->Queue, pItem);
		case XRT_TYPED_QUEUE_MPSC: return xrtMPSCQTryPush((xmpscq)pQueue->Queue, pItem);
		case XRT_TYPED_QUEUE_MPMC: return xrtMPMCQTryPush((xmpmcq)pQueue->Queue, pItem);
		case XRT_TYPED_QUEUE_WAIT: return xrtMPSCQWaitTryPush((xmpscqwait)pQueue->Queue, pItem);
		default: return XQUEUE_ERROR;
	}
}


static xqueue_result __xrtTypedQueueTryPopRaw(xtqueue pQueue, ptr* ppItem)
{
	switch ( pQueue->Kind ) {
		case XRT_TYPED_QUEUE_SPSC: return xrtSPSCQTryPop((xspscq)pQueue->Queue, ppItem);
		case XRT_TYPED_QUEUE_MPSC: return xrtMPSCQTryPop((xmpscq)pQueue->Queue, ppItem);
		case XRT_TYPED_QUEUE_MPMC: return xrtMPMCQTryPop((xmpmcq)pQueue->Queue, ppItem);
		case XRT_TYPED_QUEUE_WAIT: return xrtMPSCQWaitTryPop((xmpscqwait)pQueue->Queue, ppItem);
		default: return XQUEUE_ERROR;
	}
}


static void __xrtTypedQueueDestroyRaw(xtqueue pQueue)
{
	switch ( pQueue->Kind ) {
		case XRT_TYPED_QUEUE_SPSC: xrtSPSCQDestroy((xspscq)pQueue->Queue); break;
		case XRT_TYPED_QUEUE_MPSC: xrtMPSCQDestroy((xmpscq)pQueue->Queue); break;
		case XRT_TYPED_QUEUE_MPMC: xrtMPMCQDestroy((xmpmcq)pQueue->Queue); break;
		case XRT_TYPED_QUEUE_WAIT: xrtMPSCQWaitDestroy((xmpscqwait)pQueue->Queue); break;
		default: break;
	}
}


XXAPI xtqueue xrtTypedQueueCreate(uint32 iKind, const xrt_type_desc* pItemType, uint32 iCapacity)
{
	xqueue_config tConfig;
	xtqueue pQueue;

	if ( iKind < XRT_TYPED_QUEUE_SPSC || iKind > XRT_TYPED_QUEUE_WAIT ) {
		return NULL;
	}
	tConfig.iCapacity = iCapacity;
	tConfig.iFlags = 0u;
	pQueue = (xtqueue)xrtCalloc(1u, sizeof(xrt_typed_queue_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	pQueue->Queue = __xrtTypedQueueCreateRaw(iKind, &tConfig);
	if ( pQueue->Queue == NULL ) {
		xrtFree(pQueue);
		return NULL;
	}
	pQueue->RefCount = 1;
	pQueue->HeapOwned = TRUE;
	pQueue->Kind = iKind;
	pQueue->ItemType = pItemType;
	pQueue->ItemLength = __xrtTypedItemLength(pItemType);
	return pQueue;
}


XXAPI xtqueue xrtTypedQueueRetain(xtqueue pQueue)
{
	if ( pQueue != NULL && pQueue->HeapOwned && pQueue->RefCount > 0 ) {
		__xrtAtomicAddFetch32(&pQueue->RefCount, 1);
	}
	return pQueue;
}


XXAPI void xrtTypedQueueClose(xtqueue pQueue)
{
	if ( pQueue == NULL || pQueue->Queue == NULL ) {
		return;
	}
	switch ( pQueue->Kind ) {
		case XRT_TYPED_QUEUE_SPSC: xrtSPSCQClose((xspscq)pQueue->Queue); break;
		case XRT_TYPED_QUEUE_MPSC: xrtMPSCQClose((xmpscq)pQueue->Queue); break;
		case XRT_TYPED_QUEUE_MPMC: xrtMPMCQClose((xmpmcq)pQueue->Queue); break;
		case XRT_TYPED_QUEUE_WAIT: xrtMPSCQWaitClose((xmpscqwait)pQueue->Queue); break;
		default: break;
	}
}


XXAPI void xrtTypedQueueRelease(xtqueue pQueue)
{
	ptr pItem;

	if ( pQueue == NULL || !pQueue->HeapOwned || pQueue->RefCount <= 0 ) {
		return;
	}
	if ( __xrtAtomicAddFetch32(&pQueue->RefCount, -1) != 0 ) {
		return;
	}
	xrtTypedQueueClose(pQueue);
	while ( __xrtTypedQueueTryPopRaw(pQueue, &pItem) == XQUEUE_OK ) {
		__xrtTypedDropItem(pQueue->ItemType, pItem);
		xrtFree(pItem);
	}
	__xrtTypedQueueDestroyRaw(pQueue);
	pQueue->Queue = NULL;
	pQueue->ItemType = NULL;
	xrtFree(pQueue);
}


XXAPI xqueue_result xrtTypedQueueTryPush(xtqueue pQueue, const ptr pItem)
{
	ptr pCopy;
	xqueue_result iResult;

	if ( pQueue == NULL || pQueue->Queue == NULL ) {
		return XQUEUE_ERROR;
	}
	pCopy = xrtMalloc(pQueue->ItemLength);
	if ( pCopy == NULL ) {
		return XQUEUE_ERROR;
	}
	__xrtTypedCopyItem(pQueue->ItemType, pCopy, pItem);
	iResult = __xrtTypedQueueTryPushRaw(pQueue, pCopy);
	if ( iResult != XQUEUE_OK ) {
		__xrtTypedDropItem(pQueue->ItemType, pCopy);
		xrtFree(pCopy);
	}
	return iResult;
}


static xqueue_result __xrtTypedQueueFinishPop(xtqueue pQueue, ptr pItem, ptr pOut, xqueue_result iResult)
{
	if ( iResult != XQUEUE_OK ) {
		return iResult;
	}
	if ( pItem == NULL ) {
		return XQUEUE_ERROR;
	}
	if ( pOut != NULL ) {
		__xrtTypedReplaceItem(pQueue->ItemType, pOut, pItem);
	}
	__xrtTypedDropItem(pQueue->ItemType, pItem);
	xrtFree(pItem);
	return pOut != NULL ? XQUEUE_OK : XQUEUE_ERROR;
}


XXAPI xqueue_result xrtTypedQueueTryPop(xtqueue pQueue, ptr pOut)
{
	ptr pItem = NULL;
	xqueue_result iResult;

	if ( pQueue == NULL || pQueue->Queue == NULL ) {
		return XQUEUE_ERROR;
	}
	iResult = __xrtTypedQueueTryPopRaw(pQueue, &pItem);
	return __xrtTypedQueueFinishPop(pQueue, pItem, pOut, iResult);
}


XXAPI xqueue_result xrtTypedQueuePop(xtqueue pQueue, ptr pOut)
{
	ptr pItem = NULL;
	xqueue_result iResult;

	if ( pQueue == NULL || pQueue->Queue == NULL ) {
		return XQUEUE_ERROR;
	}
	if ( pQueue->Kind != XRT_TYPED_QUEUE_WAIT ) {
		return xrtTypedQueueTryPop(pQueue, pOut);
	}
	iResult = xrtMPSCQWaitPop((xmpscqwait)pQueue->Queue, &pItem);
	return __xrtTypedQueueFinishPop(pQueue, pItem, pOut, iResult);
}


XXAPI xqueue_result xrtTypedQueuePopTimeout(xtqueue pQueue, ptr pOut, uint32 iTimeoutMs)
{
	ptr pItem = NULL;
	xqueue_result iResult;

	if ( pQueue == NULL || pQueue->Queue == NULL ) {
		return XQUEUE_ERROR;
	}
	if ( pQueue->Kind != XRT_TYPED_QUEUE_WAIT ) {
		return xrtTypedQueueTryPop(pQueue, pOut);
	}
	iResult = xrtMPSCQWaitPopTimeout((xmpscqwait)pQueue->Queue, &pItem, iTimeoutMs);
	return __xrtTypedQueueFinishPop(pQueue, pItem, pOut, iResult);
}


XXAPI uint32 xrtTypedQueueApproxCount(xtqueue pQueue)
{
	if ( pQueue == NULL || pQueue->Queue == NULL ) {
		return 0u;
	}
	switch ( pQueue->Kind ) {
		case XRT_TYPED_QUEUE_SPSC: return xrtSPSCQApproxCount((xspscq)pQueue->Queue);
		case XRT_TYPED_QUEUE_MPSC: return xrtMPSCQApproxCount((xmpscq)pQueue->Queue);
		case XRT_TYPED_QUEUE_MPMC: return xrtMPMCQApproxCount((xmpmcq)pQueue->Queue);
		case XRT_TYPED_QUEUE_WAIT: return xrtMPSCQWaitApproxCount((xmpscqwait)pQueue->Queue);
		default: return 0u;
	}
}


XXAPI bool xrtTypedQueueIsClosed(xtqueue pQueue)
{
	return pQueue != NULL && pQueue->Queue != NULL && xrtQueueIsClosed((xqueuebase*)pQueue->Queue);
}


XXAPI bool xrtTypedQueueIsDrained(xtqueue pQueue)
{
	return pQueue != NULL && pQueue->Queue != NULL && xrtQueueIsDrained((xqueuebase*)pQueue->Queue);
}


XXAPI const xrt_type_desc* xrtTypedQueueItemType(xtqueue pQueue)
{
	return pQueue != NULL ? pQueue->ItemType : NULL;
}
