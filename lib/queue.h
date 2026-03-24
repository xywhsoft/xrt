#ifndef __XRT_QUEUE_MAX_CAPACITY
	#define __XRT_QUEUE_MAX_CAPACITY (1u << 30)
#endif

static inline uint32 __xrtQueueAtomicLoad32(const volatile uint32* pValue)
{
	return (uint32)__xrtAtomicCompareExchange32((volatile long*)pValue, 0, 0);
}

static inline void __xrtQueueAtomicStore32(volatile uint32* pValue, uint32 iValue)
{
	(void)__xrtAtomicExchange32((volatile long*)pValue, (long)iValue);
}

static inline uint64 __xrtQueueAtomicLoad64(const volatile uint64* pValue)
{
	return (uint64)__xrtAtomicLoad64((const volatile int64*)pValue);
}

static inline void __xrtQueueAtomicStore64(volatile uint64* pValue, uint64 iValue)
{
	__xrtAtomicStore64((volatile int64*)pValue, (int64)iValue);
}

static inline bool __xrtQueueAtomicCAS64(volatile uint64* pValue, uint64 iExpected, uint64 iDesired)
{
	return (uint64)__xrtAtomicCompareExchange64((volatile int64*)pValue, (int64)iDesired, (int64)iExpected) == iExpected;
}

static inline uint32 __xrtQueueRoundUpPow2(uint32 iCapacity)
{
	if ( iCapacity == 0 || iCapacity > __XRT_QUEUE_MAX_CAPACITY ) {
		return 0;
	}
	iCapacity--;
	iCapacity |= (iCapacity >> 1);
	iCapacity |= (iCapacity >> 2);
	iCapacity |= (iCapacity >> 4);
	iCapacity |= (iCapacity >> 8);
	iCapacity |= (iCapacity >> 16);
	iCapacity++;
	if ( iCapacity == 0 || iCapacity > __XRT_QUEUE_MAX_CAPACITY ) {
		return 0;
	}
	return iCapacity;
}

static inline bool __xrtQueueResolveCapacity(const xqueue_config* pCfg, uint32* pCapacity)
{
	uint32 iCapacity;

	if ( pCapacity == NULL || pCfg == NULL ) {
		return FALSE;
	}

	iCapacity = __xrtQueueRoundUpPow2(pCfg->iCapacity);
	if ( iCapacity == 0 ) {
		xrtSetError("invalid queue capacity.", FALSE);
		return FALSE;
	}

	*pCapacity = iCapacity;
	return TRUE;
}

static inline uint32 __xrtQueueSPSCCount(const xspscq pQueue)
{
	uint32 iHead;
	uint32 iTail;

	if ( pQueue == NULL || pQueue->arrItems == NULL ) {
		return 0;
	}

	iHead = __xrtQueueAtomicLoad32(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad32(&pQueue->iTail);
	return (uint32)(iTail - iHead);
}

static inline uint32 __xrtQueueMPSCCount(const xmpscq pQueue)
{
	uint64 iHead;
	uint64 iTail;

	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return 0;
	}

	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
	if ( iTail <= iHead ) {
		return 0;
	}
	if ( (iTail - iHead) > 0xffffffffULL ) {
		return 0xffffffffu;
	}
	return (uint32)(iTail - iHead);
}

static inline uint32 __xrtQueueMPMCCount(const xmpmcq pQueue)
{
	uint64 iHead;
	uint64 iTail;

	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return 0;
	}

	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
	if ( iTail <= iHead ) {
		return 0;
	}
	if ( (iTail - iHead) > 0xffffffffULL ) {
		return 0xffffffffu;
	}
	return (uint32)(iTail - iHead);
}

XXAPI bool xrtSPSCQInit(xspscq pQueue, const xqueue_config* pCfg)
{
	uint32 iCapacity;
	ptr* arrItems;

	if ( pQueue == NULL ) {
		return FALSE;
	}
	if ( !__xrtQueueResolveCapacity(pCfg, &iCapacity) ) {
		memset(pQueue, 0, sizeof(xspscq_struct));
		return FALSE;
	}

	arrItems = (ptr*)xrtCalloc(iCapacity, sizeof(ptr));
	if ( arrItems == NULL ) {
		memset(pQueue, 0, sizeof(xspscq_struct));
		return FALSE;
	}

	memset(pQueue, 0, sizeof(xspscq_struct));
	pQueue->tBase.iKind = XQUEUE_KIND_SPSC;
	pQueue->tBase.bClosed = 0;
	pQueue->iCapacity = iCapacity;
	pQueue->iMask = iCapacity - 1u;
	pQueue->arrItems = arrItems;
	return TRUE;
}

XXAPI void xrtSPSCQUnit(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	if ( pQueue->arrItems != NULL ) {
		xrtFree(pQueue->arrItems);
	}
	memset(pQueue, 0, sizeof(xspscq_struct));
}

XXAPI xspscq xrtSPSCQCreate(const xqueue_config* pCfg)
{
	xspscq pQueue = (xspscq)xrtCalloc(1, sizeof(xspscq_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtSPSCQInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}

XXAPI void xrtSPSCQDestroy(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtSPSCQUnit(pQueue);
	xrtFree(pQueue);
}

XXAPI xqueue_result xrtSPSCQTryPush(xspscq pQueue, ptr pItem)
{
	uint32 iHead;
	uint32 iTail;

	if ( pQueue == NULL || pQueue->arrItems == NULL ) {
		return XQUEUE_ERROR;
	}
	if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
		return XQUEUE_CLOSED;
	}

	iTail = __xrtQueueAtomicLoad32(&pQueue->iTail);
	iHead = __xrtQueueAtomicLoad32(&pQueue->iHead);
	if ( (uint32)(iTail - iHead) >= pQueue->iCapacity ) {
		return XQUEUE_FULL;
	}

	pQueue->arrItems[iTail & pQueue->iMask] = pItem;
	__xrtQueueAtomicStore32(&pQueue->iTail, iTail + 1u);
	return XQUEUE_OK;
}

XXAPI xqueue_result xrtSPSCQTryPop(xspscq pQueue, ptr* ppItem)
{
	uint32 iHead;
	uint32 iTail;
	ptr pItem;

	if ( pQueue == NULL || pQueue->arrItems == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}

	iHead = __xrtQueueAtomicLoad32(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad32(&pQueue->iTail);
	if ( iHead == iTail ) {
		*ppItem = NULL;
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return XQUEUE_CLOSED;
		}
		return XQUEUE_EMPTY;
	}

	pItem = pQueue->arrItems[iHead & pQueue->iMask];
	pQueue->arrItems[iHead & pQueue->iMask] = NULL;
	__xrtQueueAtomicStore32(&pQueue->iHead, iHead + 1u);
	*ppItem = pItem;
	return XQUEUE_OK;
}

XXAPI uint32 xrtSPSCQApproxCount(xspscq pQueue)
{
	return __xrtQueueSPSCCount(pQueue);
}

XXAPI void xrtSPSCQClose(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}

XXAPI uint32 xrtSPSCQDrain(xspscq pQueue, xqueue_drain_fn procDrain, ptr pUserData)
{
	uint32 iCount = 0;
	ptr pItem = NULL;
	xqueue_result iRet;

	if ( pQueue == NULL || procDrain == NULL ) {
		return 0;
	}

	for ( ;; ) {
		iRet = xrtSPSCQTryPop(pQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			break;
		}
		procDrain(pItem, pUserData);
		iCount++;
	}

	return iCount;
}

XXAPI bool xrtSPSCQReset(xspscq pQueue)
{
	if ( pQueue == NULL || pQueue->arrItems == NULL ) {
		return FALSE;
	}
	if ( xrtSPSCQApproxCount(pQueue) != 0 ) {
		return FALSE;
	}

	memset(pQueue->arrItems, 0, sizeof(ptr) * pQueue->iCapacity);
	__xrtQueueAtomicStore32(&pQueue->iHead, 0u);
	__xrtQueueAtomicStore32(&pQueue->iTail, 0u);
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 0u);
	return TRUE;
}

XXAPI bool xrtMPSCQInit(xmpscq pQueue, const xqueue_config* pCfg)
{
	uint32 iCapacity;
	xmpscq_slot* arrSlots;
	uint32 i;

	if ( pQueue == NULL ) {
		return FALSE;
	}
	if ( !__xrtQueueResolveCapacity(pCfg, &iCapacity) ) {
		memset(pQueue, 0, sizeof(xmpscq_struct));
		return FALSE;
	}

	arrSlots = (xmpscq_slot*)xrtCalloc(iCapacity, sizeof(xmpscq_slot));
	if ( arrSlots == NULL ) {
		memset(pQueue, 0, sizeof(xmpscq_struct));
		return FALSE;
	}

	memset(pQueue, 0, sizeof(xmpscq_struct));
	pQueue->tBase.iKind = XQUEUE_KIND_MPSC;
	pQueue->tBase.bClosed = 0;
	pQueue->iCapacity = iCapacity;
	pQueue->iMask = iCapacity - 1u;
	pQueue->arrSlots = arrSlots;
	for ( i = 0; i < iCapacity; ++i ) {
		pQueue->arrSlots[i].iSeq = (uint64)i;
		pQueue->arrSlots[i].pItem = NULL;
	}
	return TRUE;
}

XXAPI void xrtMPSCQUnit(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	if ( pQueue->arrSlots != NULL ) {
		xrtFree(pQueue->arrSlots);
	}
	memset(pQueue, 0, sizeof(xmpscq_struct));
}

XXAPI xmpscq xrtMPSCQCreate(const xqueue_config* pCfg)
{
	xmpscq pQueue = (xmpscq)xrtCalloc(1, sizeof(xmpscq_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtMPSCQInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}

XXAPI void xrtMPSCQDestroy(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPSCQUnit(pQueue);
	xrtFree(pQueue);
}

XXAPI xqueue_result xrtMPSCQTryPush(xmpscq pQueue, ptr pItem)
{
	uint64 iTail;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;

	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return XQUEUE_ERROR;
	}

	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return XQUEUE_CLOSED;
		}

		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		pSlot = &pQueue->arrSlots[(uint32)(iTail & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)iTail;

		if ( iDiff == 0 ) {
			if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + 1u) ) {
				pSlot->pItem = pItem;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iTail + 1u);
				return XQUEUE_OK;
			}
			xrtThreadYield();
			continue;
		}
		if ( iDiff < 0 ) {
			return XQUEUE_FULL;
		}
		xrtThreadYield();
	}
}

XXAPI xqueue_result xrtMPSCQTryPop(xmpscq pQueue, ptr* ppItem)
{
	uint64 iHead;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	uint64 iTail;

	if ( pQueue == NULL || pQueue->arrSlots == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}

	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	pSlot = &pQueue->arrSlots[(uint32)(iHead & pQueue->iMask)];
	iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
	iDiff = (int64)iSeq - (int64)(iHead + 1u);

	if ( iDiff == 0 ) {
		*ppItem = pSlot->pItem;
		pSlot->pItem = NULL;
		__xrtQueueAtomicStore64(&pQueue->iHead, iHead + 1u);
		__xrtQueueAtomicStore64(&pSlot->iSeq, iHead + pQueue->iCapacity);
		return XQUEUE_OK;
	}

	*ppItem = NULL;
	if ( iDiff < 0 ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
			if ( iTail == iHead ) {
				return XQUEUE_CLOSED;
			}
		}
		return XQUEUE_EMPTY;
	}

	return XQUEUE_EMPTY;
}

XXAPI uint32 xrtMPSCQApproxCount(xmpscq pQueue)
{
	return __xrtQueueMPSCCount(pQueue);
}

XXAPI void xrtMPSCQClose(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}

XXAPI uint32 xrtMPSCQDrain(xmpscq pQueue, xqueue_drain_fn procDrain, ptr pUserData)
{
	uint32 iCount = 0;
	ptr pItem = NULL;
	xqueue_result iRet;

	if ( pQueue == NULL || procDrain == NULL ) {
		return 0;
	}

	for ( ;; ) {
		iRet = xrtMPSCQTryPop(pQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			break;
		}
		procDrain(pItem, pUserData);
		iCount++;
	}

	return iCount;
}

XXAPI bool xrtMPSCQReset(xmpscq pQueue)
{
	uint32 i;

	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return FALSE;
	}
	if ( xrtMPSCQApproxCount(pQueue) != 0 ) {
		return FALSE;
	}

	for ( i = 0; i < pQueue->iCapacity; ++i ) {
		pQueue->arrSlots[i].pItem = NULL;
		__xrtQueueAtomicStore64(&pQueue->arrSlots[i].iSeq, (uint64)i);
	}
	__xrtQueueAtomicStore64(&pQueue->iHead, 0u);
	__xrtQueueAtomicStore64(&pQueue->iTail, 0u);
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 0u);
	return TRUE;
}

XXAPI bool xrtMPMCQInit(xmpmcq pQueue, const xqueue_config* pCfg)
{
	uint32 iCapacity;
	xmpmcq_slot* arrSlots;
	uint32 i;

	if ( pQueue == NULL ) {
		return FALSE;
	}
	if ( !__xrtQueueResolveCapacity(pCfg, &iCapacity) ) {
		memset(pQueue, 0, sizeof(xmpmcq_struct));
		return FALSE;
	}

	arrSlots = (xmpmcq_slot*)xrtCalloc(iCapacity, sizeof(xmpmcq_slot));
	if ( arrSlots == NULL ) {
		memset(pQueue, 0, sizeof(xmpmcq_struct));
		return FALSE;
	}

	memset(pQueue, 0, sizeof(xmpmcq_struct));
	pQueue->tBase.iKind = XQUEUE_KIND_MPMC;
	pQueue->tBase.bClosed = 0;
	pQueue->iCapacity = iCapacity;
	pQueue->iMask = iCapacity - 1u;
	pQueue->arrSlots = arrSlots;
	for ( i = 0; i < iCapacity; ++i ) {
		pQueue->arrSlots[i].iSeq = (uint64)i;
		pQueue->arrSlots[i].pItem = NULL;
	}
	return TRUE;
}

XXAPI void xrtMPMCQUnit(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	if ( pQueue->arrSlots != NULL ) {
		xrtFree(pQueue->arrSlots);
	}
	memset(pQueue, 0, sizeof(xmpmcq_struct));
}

XXAPI xmpmcq xrtMPMCQCreate(const xqueue_config* pCfg)
{
	xmpmcq pQueue = (xmpmcq)xrtCalloc(1, sizeof(xmpmcq_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtMPMCQInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}

XXAPI void xrtMPMCQDestroy(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPMCQUnit(pQueue);
	xrtFree(pQueue);
}

XXAPI xqueue_result xrtMPMCQTryPush(xmpmcq pQueue, ptr pItem)
{
	uint64 iTail;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;

	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return XQUEUE_ERROR;
	}

	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return XQUEUE_CLOSED;
		}

		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		pSlot = &pQueue->arrSlots[(uint32)(iTail & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)iTail;

		if ( iDiff == 0 ) {
			if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + 1u) ) {
				pSlot->pItem = pItem;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iTail + 1u);
				return XQUEUE_OK;
			}
			xrtThreadYield();
			continue;
		}
		if ( iDiff < 0 ) {
			return XQUEUE_FULL;
		}
		xrtThreadYield();
	}
}

XXAPI xqueue_result xrtMPMCQTryPop(xmpmcq pQueue, ptr* ppItem)
{
	uint64 iHead;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	uint64 iTail;
	ptr pItem;

	if ( pQueue == NULL || pQueue->arrSlots == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}

	for ( ;; ) {
		iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
		pSlot = &pQueue->arrSlots[(uint32)(iHead & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)(iHead + 1u);

		if ( iDiff == 0 ) {
			if ( __xrtQueueAtomicCAS64(&pQueue->iHead, iHead, iHead + 1u) ) {
				pItem = pSlot->pItem;
				pSlot->pItem = NULL;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iHead + pQueue->iCapacity);
				*ppItem = pItem;
				return XQUEUE_OK;
			}
			xrtThreadYield();
			continue;
		}

		*ppItem = NULL;
		if ( iDiff < 0 ) {
			if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
				iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
				if ( iTail == iHead ) {
					return XQUEUE_CLOSED;
				}
			}
			return XQUEUE_EMPTY;
		}
		xrtThreadYield();
	}
}

XXAPI uint32 xrtMPMCQApproxCount(xmpmcq pQueue)
{
	return __xrtQueueMPMCCount(pQueue);
}

XXAPI void xrtMPMCQClose(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}

XXAPI uint32 xrtMPMCQDrain(xmpmcq pQueue, xqueue_drain_fn procDrain, ptr pUserData)
{
	uint32 iCount = 0;
	ptr pItem = NULL;
	xqueue_result iRet;

	if ( pQueue == NULL || procDrain == NULL ) {
		return 0;
	}

	for ( ;; ) {
		iRet = xrtMPMCQTryPop(pQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			break;
		}
		procDrain(pItem, pUserData);
		iCount++;
	}

	return iCount;
}

XXAPI bool xrtMPMCQReset(xmpmcq pQueue)
{
	uint32 i;

	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return FALSE;
	}
	if ( xrtMPMCQApproxCount(pQueue) != 0 ) {
		return FALSE;
	}

	for ( i = 0; i < pQueue->iCapacity; ++i ) {
		pQueue->arrSlots[i].pItem = NULL;
		__xrtQueueAtomicStore64(&pQueue->arrSlots[i].iSeq, (uint64)i);
	}
	__xrtQueueAtomicStore64(&pQueue->iHead, 0u);
	__xrtQueueAtomicStore64(&pQueue->iTail, 0u);
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 0u);
	return TRUE;
}

XXAPI bool xrtQueueIsClosed(const xqueuebase* pQueue)
{
	if ( pQueue == NULL ) {
		return FALSE;
	}
	return __xrtQueueAtomicLoad32(&pQueue->bClosed) != 0;
}

XXAPI bool xrtQueueIsDrained(const xqueuebase* pQueue)
{
	if ( pQueue == NULL || !xrtQueueIsClosed(pQueue) ) {
		return FALSE;
	}

	switch ( pQueue->iKind ) {
		case XQUEUE_KIND_SPSC:
			return __xrtQueueSPSCCount((xspscq)pQueue) == 0;
		case XQUEUE_KIND_MPSC:
			return __xrtQueueMPSCCount((xmpscq)pQueue) == 0;
		case XQUEUE_KIND_MPMC:
			return __xrtQueueMPMCCount((xmpmcq)pQueue) == 0;
		default:
			break;
	}

	return FALSE;
}
