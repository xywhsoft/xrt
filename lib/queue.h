#ifndef __XRT_QUEUE_MAX_CAPACITY
	#define __XRT_QUEUE_MAX_CAPACITY (1u << 30)
#endif

// 内部函数：加载队列原子 32 位值
static inline uint32 __xrtQueueAtomicLoad32(const volatile uint32* pValue)
{
	return __xrtAtomicLoadU32(pValue);
}


// 内部函数：保存队列原子 32 位值
static inline void __xrtQueueAtomicStore32(volatile uint32* pValue, uint32 iValue)
{
	__xrtAtomicStoreU32(pValue, iValue);
}


// 内部函数：加载队列原子 64 位值
static inline uint64 __xrtQueueAtomicLoad64(const volatile uint64* pValue)
{
	return (uint64)__xrtAtomicLoad64((const volatile int64*)pValue);
}


// 内部函数：保存队列原子 64 位值
static inline void __xrtQueueAtomicStore64(volatile uint64* pValue, uint64 iValue)
{
	__xrtAtomicStore64((volatile int64*)pValue, (int64)iValue);
}


// 内部函数：比较并交换队列原子 64 位值
static inline bool __xrtQueueAtomicCAS64(volatile uint64* pValue, uint64 iExpected, uint64 iDesired)
{
	return (uint64)__xrtAtomicCompareExchange64((volatile int64*)pValue, (int64)iDesired, (int64)iExpected) == iExpected;
}


// 内部函数：将队列容量向上对齐到 2 次幂
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


// 内部函数：解析队列容量
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


// 内部函数：获取 SPSC 队列元素数量
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


// 内部函数：获取 MPSC 队列元素数量
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


// 内部函数：获取 MPMC 队列元素数量
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

#ifndef XRT_NO_QUEUE_WAIT
// 内部函数：获取等待队列当前毫秒时间
static inline uint64 __xrtQueueWaitNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetTickCount64();
	#else
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) {
			return 0;
		}
		return ((uint64)tNow.tv_sec * 1000u) + ((uint64)tNow.tv_nsec / 1000000u);
	#endif
}


// 内部函数：加载等待队列长整型状态
static inline long __xrtQueueWaitLoadLong(const volatile long* pValue)
{
	return __xrtAtomicCompareExchange32((volatile long*)pValue, 0, 0);
}


// 内部函数：累加等待队列长整型状态
static inline void __xrtQueueWaitAddLong(volatile long* pValue, long iDelta)
{
	(void)__xrtAtomicAddFetch32(pValue, iDelta);
}


// 内部函数：判断等待队列是否已关闭且排空
static inline bool __xrtMPSCQWaitIsClosedAndDrained(const xmpscqwait pQueue)
{
	return pQueue != NULL && xrtQueueIsClosed(&pQueue->tQueue.tBase) && xrtQueueIsDrained(&pQueue->tQueue.tBase);
}


// 内部函数：唤醒全部等待中的消费者
static inline void __xrtMPSCQWaitWakeAll(xmpscqwait pQueue)
{
	long iWaiters;
	uint32 iWakeCount;

	if ( pQueue == NULL || pQueue->hItems == NULL ) {
		return;
	}

	iWaiters = __xrtQueueWaitLoadLong(&pQueue->iWaiters);
	if ( iWaiters <= 0 ) {
		return;
	}

	iWakeCount = (iWaiters > (long)0xffffffffu) ? 0xffffffffu : (uint32)iWaiters;
	(void)xrtSemPostMultiple(pQueue->hItems, iWakeCount);
}


// 内部函数：__xrtMPSCQWaitPopWithToken
static xqueue_result __xrtMPSCQWaitPopWithToken(xmpscqwait pQueue, ptr* ppItem)
{
	xqueue_result iRet;
	bool bWake = FALSE;

	if ( pQueue == NULL || pQueue->hPopLock == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}

	xrtMutexLock(pQueue->hPopLock);
	iRet = xrtMPSCQTryPop(&pQueue->tQueue, ppItem);
	bWake = __xrtMPSCQWaitIsClosedAndDrained(pQueue);
	xrtMutexUnlock(pQueue->hPopLock);

	if ( bWake ) {
		__xrtMPSCQWaitWakeAll(pQueue);
	}
	if ( iRet == XQUEUE_OK || iRet == XQUEUE_CLOSED || iRet == XQUEUE_ERROR ) {
		return iRet;
	}
	return __xrtMPSCQWaitIsClosedAndDrained(pQueue) ? XQUEUE_CLOSED : XQUEUE_EMPTY;
}


// 内部函数：__xrtMPSCQWaitPopCore
static xqueue_result __xrtMPSCQWaitPopCore(xmpscqwait pQueue, ptr* ppItem, bool bInfinite, uint32 iTimeoutMs)
{
	uint64 iDeadlineMs = 0;

	if ( pQueue == NULL || pQueue->hItems == NULL || pQueue->hPopLock == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}

	*ppItem = NULL;
	if ( !bInfinite ) {
		iDeadlineMs = __xrtQueueWaitNowMs() + (uint64)iTimeoutMs;
	}

	for ( ;; ) {
		if ( xrtSemTryWait(pQueue->hItems) ) {
			return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
		}
		else {
			if ( __xrtMPSCQWaitIsClosedAndDrained(pQueue) ) {
				return XQUEUE_CLOSED;
			}

			__xrtQueueWaitAddLong(&pQueue->iWaiters, 1);
			if ( xrtSemTryWait(pQueue->hItems) ) {
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
			}
			else if ( __xrtMPSCQWaitIsClosedAndDrained(pQueue) ) {
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				return XQUEUE_CLOSED;
			}
			else if ( bInfinite ) {
				xrtSemWait(pQueue->hItems);
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
			}
			else {
				uint64 iNowMs = __xrtQueueWaitNowMs();
				uint64 iRemainMs;
				int iWaitRet;

				if ( iNowMs >= iDeadlineMs ) {
					__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
					return XQUEUE_TIMEOUT;
				}

				iRemainMs = iDeadlineMs - iNowMs;
				if ( iRemainMs > 0xffffffffu ) {
					iRemainMs = 0xffffffffu;
				}
				iWaitRet = xrtSemWaitTimeout(pQueue->hItems, (uint32)iRemainMs);
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				if ( iWaitRet == XRT_WAIT_OK ) {
					return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
				}
				else if ( iWaitRet == XRT_WAIT_TIMEOUT ) {
					if ( __xrtMPSCQWaitIsClosedAndDrained(pQueue) ) {
						return XQUEUE_CLOSED;
					}
					return XQUEUE_TIMEOUT;
				}
				else {
					return XQUEUE_ERROR;
				}
			}
		}
	}
}
#endif

// xrtSPSCQInit 相关处理
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


// xrtSPSCQUnit 相关处理
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


// xrtSPSCQCreate 相关处理
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


// xrtSPSCQDestroy 相关处理
XXAPI void xrtSPSCQDestroy(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtSPSCQUnit(pQueue);
	xrtFree(pQueue);
}


// xrtSPSCQTryPush 相关处理
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


// xrtSPSCQTryPop 相关处理
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


// xrtSPSCQApproxCount 相关处理
XXAPI uint32 xrtSPSCQApproxCount(xspscq pQueue)
{
	return __xrtQueueSPSCCount(pQueue);
}


// xrtSPSCQClose 相关处理
XXAPI void xrtSPSCQClose(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}


// xrtSPSCQDrain 相关处理
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


// xrtSPSCQReset 相关处理
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


// xrtMPSCQInit 相关处理
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


// xrtMPSCQUnit 相关处理
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


// xrtMPSCQCreate 相关处理
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


// xrtMPSCQDestroy 相关处理
XXAPI void xrtMPSCQDestroy(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPSCQUnit(pQueue);
	xrtFree(pQueue);
}


// xrtMPSCQTryPush 相关处理
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


// xrtMPSCQTryPop 相关处理
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


// xrtMPSCQPushBatch 相关处理
XXAPI uint32 xrtMPSCQPushBatch(xmpscq pQueue, ptr* arrItems, uint32 iCount)
{
	uint64 iTail;
	uint32 iReady;
	uint32 i;
	uint64 iPos;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;

	if ( pQueue == NULL || arrItems == NULL || iCount == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}

	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return 0u;
		}

		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		iReady = 0u;
		iDiff = 0;
		while ( iReady < iCount ) {
			iPos = iTail + iReady;
			pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
			iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
			iDiff = (int64)iSeq - (int64)iPos;
			if ( iDiff == 0 ) {
				iReady++;
				continue;
			}
			break;
		}

		if ( iReady == 0u ) {
			if ( iDiff < 0 ) {
				return 0u;
			}
			xrtThreadYield();
			continue;
		}

		if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + iReady) ) {
			for ( i = 0; i < iReady; ++i ) {
				iPos = iTail + i;
				pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
				pSlot->pItem = arrItems[i];
				__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + 1u);
			}
			return iReady;
		}

		xrtThreadYield();
	}
}


// xrtMPSCQPopBatch 相关处理
XXAPI uint32 xrtMPSCQPopBatch(xmpscq pQueue, ptr* arrItems, uint32 iCap)
{
	uint64 iHead;
	uint32 iPopped;
	uint32 i;
	uint64 iPos;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;

	if ( pQueue == NULL || arrItems == NULL || iCap == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}

	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	iPopped = 0u;
	while ( iPopped < iCap ) {
		iPos = iHead + iPopped;
		pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)(iPos + 1u);
		if ( iDiff != 0 ) {
			break;
		}
		iPopped++;
	}

	for ( i = 0; i < iPopped; ++i ) {
		iPos = iHead + i;
		pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
		arrItems[i] = pSlot->pItem;
		pSlot->pItem = NULL;
		__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + pQueue->iCapacity);
	}
	if ( iPopped != 0u ) {
		__xrtQueueAtomicStore64(&pQueue->iHead, iHead + iPopped);
	}

	return iPopped;
}


// xrtMPSCQApproxCount 相关处理
XXAPI uint32 xrtMPSCQApproxCount(xmpscq pQueue)
{
	return __xrtQueueMPSCCount(pQueue);
}


// xrtMPSCQClose 相关处理
XXAPI void xrtMPSCQClose(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}


// xrtMPSCQDrain 相关处理
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


// xrtMPSCQReset 相关处理
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


// xrtMPMCQInit 相关处理
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


// xrtMPMCQUnit 相关处理
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


// xrtMPMCQCreate 相关处理
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


// xrtMPMCQDestroy 相关处理
XXAPI void xrtMPMCQDestroy(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPMCQUnit(pQueue);
	xrtFree(pQueue);
}


// xrtMPMCQTryPush 相关处理
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


// xrtMPMCQTryPop 相关处理
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


// xrtMPMCQPushBatch 相关处理
XXAPI uint32 xrtMPMCQPushBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCount)
{
	uint64 iTail;
	uint32 iReady;
	uint32 i;
	uint64 iPos;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;

	if ( pQueue == NULL || arrItems == NULL || iCount == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}

	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return 0u;
		}

		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		iReady = 0u;
		iDiff = 0;
		while ( iReady < iCount ) {
			iPos = iTail + iReady;
			pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
			iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
			iDiff = (int64)iSeq - (int64)iPos;
			if ( iDiff == 0 ) {
				iReady++;
				continue;
			}
			break;
		}

		if ( iReady == 0u ) {
			if ( iDiff < 0 ) {
				return 0u;
			}
			xrtThreadYield();
			continue;
		}

		if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + iReady) ) {
			for ( i = 0; i < iReady; ++i ) {
				iPos = iTail + i;
				pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
				pSlot->pItem = arrItems[i];
				__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + 1u);
			}
			return iReady;
		}

		xrtThreadYield();
	}
}


// xrtMPMCQPopBatch 相关处理
XXAPI uint32 xrtMPMCQPopBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCap)
{
	uint64 iHead;
	uint32 iPopped;
	uint32 i;
	uint64 iPos;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;

	if ( pQueue == NULL || arrItems == NULL || iCap == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}

	for ( ;; ) {
		iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
		iPopped = 0u;
		iDiff = 0;
		while ( iPopped < iCap ) {
			iPos = iHead + iPopped;
			pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
			iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
			iDiff = (int64)iSeq - (int64)(iPos + 1u);
			if ( iDiff == 0 ) {
				iPopped++;
				continue;
			}
			break;
		}

		if ( iPopped == 0u ) {
			if ( iDiff < 0 ) {
				return 0u;
			}
			xrtThreadYield();
			continue;
		}

		if ( __xrtQueueAtomicCAS64(&pQueue->iHead, iHead, iHead + iPopped) ) {
			for ( i = 0; i < iPopped; ++i ) {
				iPos = iHead + i;
				pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
				arrItems[i] = pSlot->pItem;
				pSlot->pItem = NULL;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + pQueue->iCapacity);
			}
			return iPopped;
		}

		xrtThreadYield();
	}
}


// xrtMPMCQApproxCount 相关处理
XXAPI uint32 xrtMPMCQApproxCount(xmpmcq pQueue)
{
	return __xrtQueueMPMCCount(pQueue);
}


// xrtMPMCQClose 相关处理
XXAPI void xrtMPMCQClose(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}


// xrtMPMCQDrain 相关处理
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


// xrtMPMCQReset 相关处理
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


// xrtQueueIsClosed 相关处理
XXAPI bool xrtQueueIsClosed(const xqueuebase* pQueue)
{
	if ( pQueue == NULL ) {
		return FALSE;
	}
	return __xrtQueueAtomicLoad32(&pQueue->bClosed) != 0;
}


// xrtQueueIsDrained 相关处理
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

#ifndef XRT_NO_QUEUE_WAIT
// xrtMPSCQWaitInit 相关处理
XXAPI bool xrtMPSCQWaitInit(xmpscqwait pQueue, const xqueue_config* pCfg)
{
	if ( pQueue == NULL ) {
		return FALSE;
	}

	memset(pQueue, 0, sizeof(xmpscqwait_struct));
	if ( !xrtMPSCQInit(&pQueue->tQueue, pCfg) ) {
		memset(pQueue, 0, sizeof(xmpscqwait_struct));
		return FALSE;
	}

	pQueue->hItems = xrtSemCreate(0u, 0x7fffffffu);
	if ( pQueue->hItems == NULL ) {
		xrtMPSCQUnit(&pQueue->tQueue);
		memset(pQueue, 0, sizeof(xmpscqwait_struct));
		xrtSetError("mpsc wait queue semaphore init failed.", FALSE);
		return FALSE;
	}
	pQueue->hPopLock = xrtMutexCreate();
	if ( pQueue->hPopLock == NULL ) {
		xrtSemDestroy(pQueue->hItems);
		pQueue->hItems = NULL;
		xrtMPSCQUnit(&pQueue->tQueue);
		memset(pQueue, 0, sizeof(xmpscqwait_struct));
		xrtSetError("mpsc wait queue mutex init failed.", FALSE);
		return FALSE;
	}

	pQueue->iWaiters = 0;
	return TRUE;
}


// xrtMPSCQWaitUnit 相关处理
XXAPI void xrtMPSCQWaitUnit(xmpscqwait pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}

	if ( pQueue->hPopLock != NULL ) {
		xrtMutexDestroy(pQueue->hPopLock);
		pQueue->hPopLock = NULL;
	}
	if ( pQueue->hItems != NULL ) {
		xrtSemDestroy(pQueue->hItems);
		pQueue->hItems = NULL;
	}
	xrtMPSCQUnit(&pQueue->tQueue);
	memset(pQueue, 0, sizeof(xmpscqwait_struct));
}


// xrtMPSCQWaitCreate 相关处理
XXAPI xmpscqwait xrtMPSCQWaitCreate(const xqueue_config* pCfg)
{
	xmpscqwait pQueue = (xmpscqwait)xrtCalloc(1, sizeof(xmpscqwait_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtMPSCQWaitInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}


// xrtMPSCQWaitDestroy 相关处理
XXAPI void xrtMPSCQWaitDestroy(xmpscqwait pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPSCQWaitUnit(pQueue);
	xrtFree(pQueue);
}


// xrtMPSCQWaitTryPush 相关处理
XXAPI xqueue_result xrtMPSCQWaitTryPush(xmpscqwait pQueue, ptr pItem)
{
	xqueue_result iRet;

	if ( pQueue == NULL || pQueue->hItems == NULL ) {
		return XQUEUE_ERROR;
	}

	iRet = xrtMPSCQTryPush(&pQueue->tQueue, pItem);
	if ( iRet != XQUEUE_OK ) {
		return iRet;
	}
	if ( !xrtSemPost(pQueue->hItems) ) {
		xrtSetError("mpsc wait queue semaphore post failed.", FALSE);
		return XQUEUE_ERROR;
	}
	return XQUEUE_OK;
}


// xrtMPSCQWaitTryPop 相关处理
XXAPI xqueue_result xrtMPSCQWaitTryPop(xmpscqwait pQueue, ptr* ppItem)
{
	if ( pQueue == NULL || pQueue->hItems == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}

	*ppItem = NULL;
	if ( !xrtSemTryWait(pQueue->hItems) ) {
		return __xrtMPSCQWaitIsClosedAndDrained(pQueue) ? XQUEUE_CLOSED : XQUEUE_EMPTY;
	}

	return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
}


// xrtMPSCQWaitPop 相关处理
XXAPI xqueue_result xrtMPSCQWaitPop(xmpscqwait pQueue, ptr* ppItem)
{
	return __xrtMPSCQWaitPopCore(pQueue, ppItem, TRUE, 0u);
}


// xrtMPSCQWaitPopTimeout 相关处理
XXAPI xqueue_result xrtMPSCQWaitPopTimeout(xmpscqwait pQueue, ptr* ppItem, uint32 iTimeoutMs)
{
	return __xrtMPSCQWaitPopCore(pQueue, ppItem, FALSE, iTimeoutMs);
}


// xrtMPSCQWaitApproxCount 相关处理
XXAPI uint32 xrtMPSCQWaitApproxCount(xmpscqwait pQueue)
{
	if ( pQueue == NULL ) {
		return 0u;
	}
	return xrtMPSCQApproxCount(&pQueue->tQueue);
}


// xrtMPSCQWaitClose 相关处理
XXAPI void xrtMPSCQWaitClose(xmpscqwait pQueue)
{
	if ( pQueue == NULL || pQueue->hItems == NULL ) {
		return;
	}
	if ( __xrtAtomicCompareExchangeU32(&pQueue->tQueue.tBase.bClosed, 1u, 0u) != 0u ) {
		return;
	}
	__xrtMPSCQWaitWakeAll(pQueue);
}
#endif
