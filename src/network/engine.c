#include "../internal/xrt_net_engine.h"



#if defined(XRT_FEATURE_NET_ENGINE)

#define XRT_NET_ENGINE_COMMAND_TASK 1u
#define XRT_NET_ENGINE_COMMAND_TIMER_ADD 2u
#define XRT_NET_ENGINE_COMMAND_TIMER_CANCEL 3u

#define XRT_NET_ENGINE_WORKERS_MAX 256u
#define XRT_NET_ENGINE_AUTO_WORKERS_MAX 64u
#define XRT_NET_ENGINE_COMMAND_DEFAULT 4096u
#define XRT_NET_ENGINE_NODE_CACHE_DEFAULT (64u * 1024u)
#define XRT_NET_ENGINE_TIMER_DEFAULT 65536u
#define XRT_NET_ENGINE_EVENT_BATCH_DEFAULT 128u
#define XRT_NET_ENGINE_EVENT_BATCH_MAX 4096u
#define XRT_NET_ENGINE_COMMAND_BUDGET 256u
#define XRT_NET_ENGINE_SHUTDOWN_ROUNDS 1024u
#define XRT_NET_ENGINE_TIMER_INITIAL 16u
#define XRT_NET_ENGINE_IDLE_WAIT 1000000u
#define XRT_NET_ENGINE_SUBMIT_CLOSED UINT32_C(0x80000000)
#define XRT_NET_ENGINE_SUBMIT_COUNT UINT32_C(0x7fffffff)
#define XRT_NET_ENGINE_WAKE_RETRIES 3u

#define XRT_NET_ENGINE_SHUTDOWN_ACTIVE 0u
#define XRT_NET_ENGINE_SHUTDOWN_DRAINING 1u
#define XRT_NET_ENGINE_SHUTDOWN_SEALED 2u

static const size_t __xrtNetEngineNodeSizes[
	XRT_NET_ENGINE_NODE_CLASS_COUNT
] = { 64u, 128u, 256u, 512u, 1024u };



static void __xrtNetEngineWake(xnetworker* pWorker);



/* 设置 Engine 子系统的结构化错误。 */
static void __xrtNetEngineError(xerrkind Kind, xneterror Code,
	cstr sOperation, cstr sMessage)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
}



/* 保存清理过程的第一条错误，后续失败仍执行但不覆盖根因。 */
static void __xrtNetEngineErrorKeep(xerror** ppError)
{
	xerror* pError = xrtTakeError();

	if ( *ppError == NULL ) {
		*ppError = pError;
	} else if ( pError != NULL ) {
		xrtErrorFree(pError);
	}
}



/* 恢复保存的清理错误；无具体错误时建立统一内部错误。 */
static void __xrtNetEngineErrorRestore(
	xerror* pError,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else {
		__xrtNetEngineError(
			XERR_INTERNAL,
			Code,
			sOperation,
			sMessage
		);
	}
}



/* Engine 普通累计量只在 FULL 构建中产生原子写。 */
#define __xrtNetEngineStatAdd(pValue, iAmount) \
	__xrtNetStatFullAdd((pValue), (iAmount))



/* Engine 失败和拒绝计数从 BASIC 开始保留。 */
#define __xrtNetEngineStatError(pValue, iAmount) \
	__xrtNetStatBasicAdd((pValue), (iAmount))



/* 读取一个 Worker 统计计数。 */
static uint64 __xrtNetEngineStatLoad(const xatomic64* pValue)
{
	return xrtAtomic64Load(pValue, XMEMORY_RELAXED);
}



/* 在 Worker 发布前初始化全部并发统计量。 */
static void __xrtNetEngineStatsInit(__xrt_net_engine_atomic_stats* pStats)
{
	xrtAtomic64Init(&pStats->PostsAccepted, 0);
	xrtAtomic64Init(&pStats->PostsRejected, 0);
	xrtAtomic64Init(&pStats->PostsExecuted, 0);
	xrtAtomic64Init(&pStats->TimersAccepted, 0);
	xrtAtomic64Init(&pStats->TimersRejected, 0);
	xrtAtomic64Init(&pStats->TimersFired, 0);
	xrtAtomic64Init(&pStats->TimersCancelled, 0);
	xrtAtomic64Init(&pStats->TimersClosed, 0);
	xrtAtomic64Init(&pStats->TimerErrors, 0);
	xrtAtomic64Init(&pStats->Events, 0);
	xrtAtomic64Init(&pStats->WaitErrors, 0);
	xrtAtomic64Init(&pStats->WakeErrors, 0);
	xrtAtomic64Init(&pStats->ShutdownStalls, 0);
	xrtAtomic32Init(&pStats->LastWaitError, XNET_ERROR_NONE);
	xrtAtomic32Init(&pStats->LastWaitSystemCode, 0);
	xrtAtomic64Init(&pStats->NodeCacheHits, 0);
	xrtAtomic64Init(&pStats->NodeCacheMisses, 0);
}



/* 选择能够容纳内部节点的最小缓存尺寸类。 */
static uint32 __xrtNetWorkerNodeClass(size_t iSize)
{
	for ( uint32 i = 0; i < XRT_NET_ENGINE_NODE_CLASS_COUNT; i++ ) {
		if ( iSize <= __xrtNetEngineNodeSizes[i] ) {
			return i;
		}
	}
	return XRT_NET_ENGINE_NODE_CLASS_COUNT;
}



/* 从 Worker 的线程安全分级缓存分配并清零一个内部小节点。 */
ptr __xrtNetWorkerNodeAlloc(xnetworker* pWorker, size_t iSize)
{
	__xrt_net_engine_node* pNode = NULL;
	uint32 iClass;
	size_t iAllocation;

	if ( (pWorker == NULL) || (iSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iClass = __xrtNetWorkerNodeClass(iSize);
	iAllocation = iClass < XRT_NET_ENGINE_NODE_CLASS_COUNT ?
		__xrtNetEngineNodeSizes[iClass] : iSize;
	if ( (iClass < XRT_NET_ENGINE_NODE_CLASS_COUNT) &&
		 (pWorker->NodeCacheLimit != 0) && pWorker->CacheLockReady &&
		 xrtMutexLock(&pWorker->CacheLock) ) {
		pNode = pWorker->NodeCache[iClass];
		if ( pNode != NULL ) {
			pWorker->NodeCache[iClass] = pNode->Next;
			(void)xrtAtomic64FetchSub(
				&pWorker->NodeCachedBytes,
				iAllocation,
				XMEMORY_RELAXED
			);
		}
		(void)xrtMutexUnlock(&pWorker->CacheLock);
	}
	if ( pNode == NULL ) {
		__xrtNetEngineStatAdd(&pWorker->Stats.NodeCacheMisses, 1);
		pNode = (__xrt_net_engine_node*)xrtMalloc(iAllocation);
	} else {
		__xrtNetEngineStatAdd(&pWorker->Stats.NodeCacheHits, 1);
	}
	if ( pNode != NULL ) {
		memset(pNode, 0, iAllocation);
	}
	return pNode;
}



/* 清零并回收一个内部小节点，大节点和超预算节点直接释放。 */
void __xrtNetWorkerNodeRecycle(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
)
{
	__xrt_net_engine_node* pNode = (__xrt_net_engine_node*)pMemory;
	uint32 iClass;
	size_t iAllocation;
	bool bCached = false;

	if ( pMemory == NULL ) {
		return;
	}
	if ( (pWorker == NULL) || (iSize == 0) ) {
		xrtFree(pMemory);
		return;
	}
	iClass = __xrtNetWorkerNodeClass(iSize);
	iAllocation = iClass < XRT_NET_ENGINE_NODE_CLASS_COUNT ?
		__xrtNetEngineNodeSizes[iClass] : iSize;
	memset(pMemory, 0, iAllocation);
	if ( (iClass < XRT_NET_ENGINE_NODE_CLASS_COUNT) &&
		 pWorker->CacheLockReady && (iAllocation <= pWorker->NodeCacheLimit) &&
		 xrtMutexLock(&pWorker->CacheLock) ) {
		size_t iCachedBytes = (size_t)xrtAtomic64Load(
			&pWorker->NodeCachedBytes,
			XMEMORY_RELAXED
		);

		if ( iCachedBytes <=
			 (pWorker->NodeCacheLimit - iAllocation) ) {
			pNode->Next = pWorker->NodeCache[iClass];
			pWorker->NodeCache[iClass] = pNode;
			(void)xrtAtomic64FetchAdd(
				&pWorker->NodeCachedBytes,
				iAllocation,
				XMEMORY_RELAXED
			);
			bCached = true;
		}
		(void)xrtMutexUnlock(&pWorker->CacheLock);
	}
	if ( !bCached ) {
		xrtFree(pMemory);
	}
}



/* 向上层协议公开 Worker 小对象缓存，而不暴露缓存尺寸类和节点结构。 */
XRT_API ptr xrtNetWorkerAlloc(xnetworker* pWorker, size_t iSize)
{
	return __xrtNetWorkerNodeAlloc(pWorker, iSize);
}



/* 归还由同一 Worker 分配的内存，并沿用缓存预算和跨线程安全语义。 */
XRT_API void xrtNetWorkerFree(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
)
{
	__xrtNetWorkerNodeRecycle(pWorker, pMemory, iSize);
}



/* 在 Worker 仍由租约保护时归还节点，再允许 Engine 停止。 */
void __xrtNetWorkerNodeRecycleHeld(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
)
{
	xnetengine* pEngine = pWorker != NULL ? pWorker->Engine : NULL;

	__xrtNetWorkerNodeRecycle(pWorker, pMemory, iSize);
	__xrtNetEngineObjectRelease(pEngine);
}



/* 释放 Worker 的全部小节点缓存。 */
static void __xrtNetWorkerNodeCacheClear(xnetworker* pWorker)
{
	for ( uint32 i = 0; i < XRT_NET_ENGINE_NODE_CLASS_COUNT; i++ ) {
		__xrt_net_engine_node* pNode = pWorker->NodeCache[i];

		pWorker->NodeCache[i] = NULL;
		while ( pNode != NULL ) {
			__xrt_net_engine_node* pNext = pNode->Next;

			xrtFree(pNode);
			pNode = pNext;
		}
	}
	xrtAtomic64Store(&pWorker->NodeCachedBytes, 0, XMEMORY_RELAXED);
}



/* 从统一小节点缓存取得一个命令节点。 */
static __xrt_net_engine_command* __xrtNetEngineCommandAlloc(
	xnetworker* pWorker
)
{
	return (__xrt_net_engine_command*)__xrtNetWorkerNodeAlloc(
		pWorker,
		sizeof(__xrt_net_engine_command)
	);
}



/* 把命令节点放回统一小节点缓存。 */
static void __xrtNetEngineCommandRecycle(
	xnetworker* pWorker,
	__xrt_net_engine_command* pCommand
)
{
	__xrtNetWorkerNodeRecycle(pWorker, pCommand, sizeof(*pCommand));
}



/* 从统一小节点缓存取得一个 Timer 节点。 */
static __xrt_net_engine_timer* __xrtNetEngineTimerAlloc(
	xnetworker* pWorker
)
{
	return (__xrt_net_engine_timer*)__xrtNetWorkerNodeAlloc(
		pWorker,
		sizeof(__xrt_net_engine_timer)
	);
}



/* 把 Timer 节点放回统一小节点缓存。 */
static void __xrtNetEngineTimerRecycle(
	xnetworker* pWorker,
	__xrt_net_engine_timer* pTimer
)
{
	__xrtNetWorkerNodeRecycle(pWorker, pTimer, sizeof(*pTimer));
}



/* 为 Timer ID 生成稳定且分布均匀的桶索引。 */
static size_t __xrtNetEngineTimerHash(uint64 Id, size_t iBucketCount)
{
	Id ^= Id >> 30;
	Id *= UINT64_C(0xbf58476d1ce4e5b9);
	Id ^= Id >> 27;
	Id *= UINT64_C(0x94d049bb133111eb);
	Id ^= Id >> 31;
	return (size_t)Id & (iBucketCount - 1u);
}



/* 比较两个 Timer 的截止时间和稳定 ID。 */
static bool __xrtNetEngineTimerBefore(
	const __xrt_net_engine_timer* pLeft,
	const __xrt_net_engine_timer* pRight
)
{
	return (pLeft->Deadline < pRight->Deadline) ||
		((pLeft->Deadline == pRight->Deadline) && (pLeft->Id < pRight->Id));
}



/* 交换最小堆中的两个 Timer 并同步索引。 */
static void __xrtNetEngineTimerSwap(
	__xrt_net_engine_timers* pTimers,
	size_t iLeft,
	size_t iRight
)
{
	__xrt_net_engine_timer* pTimer = pTimers->Heap[iLeft];

	pTimers->Heap[iLeft] = pTimers->Heap[iRight];
	pTimers->Heap[iRight] = pTimer;
	pTimers->Heap[iLeft]->HeapIndex = iLeft;
	pTimers->Heap[iRight]->HeapIndex = iRight;
}



/* 从指定索引向上恢复 Timer 最小堆。 */
static void __xrtNetEngineTimerUp(
	__xrt_net_engine_timers* pTimers,
	size_t iIndex
)
{
	while ( iIndex != 0 ) {
		size_t iParent = (iIndex - 1u) / 2u;

		if ( !__xrtNetEngineTimerBefore(
			pTimers->Heap[iIndex],
			pTimers->Heap[iParent]
		) ) {
			break;
		}
		__xrtNetEngineTimerSwap(pTimers, iIndex, iParent);
		iIndex = iParent;
	}
}



/* 从指定索引向下恢复 Timer 最小堆。 */
static void __xrtNetEngineTimerDown(
	__xrt_net_engine_timers* pTimers,
	size_t iIndex
)
{
	for ( ;; ) {
		size_t iLeft = (iIndex * 2u) + 1u;
		size_t iRight = iLeft + 1u;
		size_t iSmallest = iIndex;

		if ( (iLeft < pTimers->Count) &&
			 __xrtNetEngineTimerBefore(
				pTimers->Heap[iLeft],
				pTimers->Heap[iSmallest]
			) ) {
			iSmallest = iLeft;
		}
		if ( (iRight < pTimers->Count) &&
			 __xrtNetEngineTimerBefore(
				pTimers->Heap[iRight],
				pTimers->Heap[iSmallest]
			) ) {
			iSmallest = iRight;
		}
		if ( iSmallest == iIndex ) {
			break;
		}
		__xrtNetEngineTimerSwap(pTimers, iIndex, iSmallest);
		iIndex = iSmallest;
	}
}



/* 按需扩展 Timer 最小堆，且永不超过配置硬上限。 */
static bool __xrtNetEngineTimerEnsure(xnetworker* pWorker, size_t iNeed)
{
	__xrt_net_engine_timers* pTimers = &pWorker->Timers;
	size_t iCapacity = pTimers->Capacity;
	__xrt_net_engine_timer** pHeap;

	if ( iNeed <= iCapacity ) {
		return true;
	}
	while ( iCapacity < iNeed ) {
		size_t iNext = iCapacity * 2u;

		if ( (iNext < iCapacity) ||
			 (iNext > pWorker->Engine->Config.TimerLimit) ) {
			iNext = pWorker->Engine->Config.TimerLimit;
		}
		if ( iNext <= iCapacity ) {
			return false;
		}
		iCapacity = iNext;
	}
	pHeap = (__xrt_net_engine_timer**)xrtRealloc(
		pTimers->Heap,
		iCapacity * sizeof(*pHeap)
	);
	if ( pHeap == NULL ) {
		return false;
	}
	pTimers->Heap = pHeap;
	pTimers->Capacity = iCapacity;
	return true;
}



/* 在 Timer 数量增长时低频扩展哈希桶，失败时保留原表继续工作。 */
static void __xrtNetEngineTimerRehash(xnetworker* pWorker)
{
	__xrt_net_engine_timers* pTimers = &pWorker->Timers;
	size_t iNewCount;
	__xrt_net_engine_timer** pBuckets;

	if ( (pTimers->Count <= (pTimers->BucketCount * 2u)) ||
		 (pTimers->BucketCount >= 65536u) ) {
		return;
	}
	iNewCount = pTimers->BucketCount * 2u;
	pBuckets = (__xrt_net_engine_timer**)xrtCalloc(
		iNewCount,
		sizeof(*pBuckets)
	);
	if ( pBuckets == NULL ) {
		xrtClearError();
		return;
	}
	for ( size_t i = 0; i < pTimers->Count; i++ ) {
		__xrt_net_engine_timer* pTimer = pTimers->Heap[i];
		size_t iBucket = __xrtNetEngineTimerHash(pTimer->Id, iNewCount);

		pTimer->HashNext = pBuckets[iBucket];
		pBuckets[iBucket] = pTimer;
	}
	xrtFree(pTimers->Buckets);
	pTimers->Buckets = pBuckets;
	pTimers->BucketCount = iNewCount;
}



/* 初始化一个 Worker 的自适应 Timer 表。 */
static bool __xrtNetEngineTimersInit(xnetworker* pWorker)
{
	__xrt_net_engine_timers* pTimers = &pWorker->Timers;
	size_t iCapacity = pWorker->Engine->Config.TimerLimit;

	memset(pTimers, 0, sizeof(*pTimers));
	if ( iCapacity > XRT_NET_ENGINE_TIMER_INITIAL ) {
		iCapacity = XRT_NET_ENGINE_TIMER_INITIAL;
	}
	pTimers->Heap = (__xrt_net_engine_timer**)xrtMalloc(
		iCapacity * sizeof(*pTimers->Heap)
	);
	pTimers->BucketCount = XRT_NET_ENGINE_TIMER_INITIAL;
	pTimers->Buckets = (__xrt_net_engine_timer**)xrtCalloc(
		pTimers->BucketCount,
		sizeof(*pTimers->Buckets)
	);
	if ( (pTimers->Heap == NULL) || (pTimers->Buckets == NULL) ) {
		xrtFree(pTimers->Heap);
		xrtFree(pTimers->Buckets);
		memset(pTimers, 0, sizeof(*pTimers));
		return false;
	}
	pTimers->Capacity = iCapacity;
	return true;
}



/* 从 ID 哈希表移除一个 Timer。 */
static void __xrtNetEngineTimerHashRemove(
	__xrt_net_engine_timers* pTimers,
	__xrt_net_engine_timer* pTimer
)
{
	size_t iBucket = __xrtNetEngineTimerHash(
		pTimer->Id,
		pTimers->BucketCount
	);
	__xrt_net_engine_timer** ppTimer = &pTimers->Buckets[iBucket];

	while ( (*ppTimer != NULL) && (*ppTimer != pTimer) ) {
		ppTimer = &(*ppTimer)->HashNext;
	}
	if ( *ppTimer == pTimer ) {
		*ppTimer = pTimer->HashNext;
	}
	pTimer->HashNext = NULL;
}



/* 从最小堆和哈希表同时摘除一个 Timer。 */
static __xrt_net_engine_timer* __xrtNetEngineTimerRemove(
	xnetworker* pWorker,
	size_t iIndex
)
{
	__xrt_net_engine_timers* pTimers = &pWorker->Timers;
	__xrt_net_engine_timer* pTimer = pTimers->Heap[iIndex];
	size_t iLast = --pTimers->Count;

	__xrtNetEngineTimerHashRemove(pTimers, pTimer);
	if ( iIndex != iLast ) {
		pTimers->Heap[iIndex] = pTimers->Heap[iLast];
		pTimers->Heap[iIndex]->HeapIndex = iIndex;
		if ( (iIndex != 0) && __xrtNetEngineTimerBefore(
			pTimers->Heap[iIndex],
			pTimers->Heap[(iIndex - 1u) / 2u]
		) ) {
			__xrtNetEngineTimerUp(pTimers, iIndex);
		} else {
			__xrtNetEngineTimerDown(pTimers, iIndex);
		}
	}
	pTimer->HeapIndex = SIZE_MAX;
	return pTimer;
}



/* 按 ID 在 Worker 的哈希表中查找 Timer。 */
static __xrt_net_engine_timer* __xrtNetEngineTimerFind(
	xnetworker* pWorker,
	uint64 Id
)
{
	__xrt_net_engine_timers* pTimers = &pWorker->Timers;
	size_t iBucket = __xrtNetEngineTimerHash(Id, pTimers->BucketCount);
	__xrt_net_engine_timer* pTimer = pTimers->Buckets[iBucket];

	while ( (pTimer != NULL) && (pTimer->Id != Id) ) {
		pTimer = pTimer->HashNext;
	}
	return pTimer;
}



/* 终结一个已摘除的 Timer；回调前回收节点，允许周期回调立即复用。 */
static void __xrtNetEngineTimerFinish(
	xnetworker* pWorker,
	__xrt_net_engine_timer* pTimer,
	xnetresult Result
)
{
	xnettimerproc pProc = pTimer->Proc;
	ptr pData = pTimer->Data;
	uint64 Id = pTimer->Id;

	if ( Result == XNET_RESULT_OK ) {
		__xrtNetEngineStatAdd(&pWorker->Stats.TimersFired, 1);
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		__xrtNetEngineStatAdd(&pWorker->Stats.TimersCancelled, 1);
	} else if ( Result == XNET_RESULT_CLOSED ) {
		__xrtNetEngineStatAdd(&pWorker->Stats.TimersClosed, 1);
	} else {
		__xrtNetEngineStatError(&pWorker->Stats.TimerErrors, 1);
	}
	(void)xrtAtomic32FetchSub(
		&pWorker->TimerReserved,
		1,
		XMEMORY_ACQ_REL
	);
	__xrtNetEngineTimerRecycle(pWorker, pTimer);
	pProc(pWorker, Id, Result, pData);
}



/* 把已经确保容量的 Timer 插入 Worker 的最小堆和 ID 哈希表。 */
static void __xrtNetEngineTimerInsert(
	xnetworker* pWorker,
	__xrt_net_engine_timer* pTimer
)
{
	__xrt_net_engine_timers* pTimers = &pWorker->Timers;
	size_t iBucket;

	pTimer->HeapIndex = pTimers->Count;
	pTimers->Heap[pTimers->Count++] = pTimer;
	iBucket = __xrtNetEngineTimerHash(pTimer->Id, pTimers->BucketCount);
	pTimer->HashNext = pTimers->Buckets[iBucket];
	pTimers->Buckets[iBucket] = pTimer;
	__xrtNetEngineTimerUp(pTimers, pTimer->HeapIndex);
	__xrtNetEngineTimerRehash(pWorker);
}



/* 把跨线程提交且已经完整构造的 Timer 加入 Worker。 */
static void __xrtNetEngineTimerAdd(
	xnetworker* pWorker,
	__xrt_net_engine_timer* pTimer
)
{
	if ( !__xrtNetEngineTimerEnsure(
		pWorker,
		pWorker->Timers.Count + 1u
	) ) {
		__xrtNetEngineTimerFinish(
			pWorker,
			pTimer,
			XNET_RESULT_ERROR
		);
		return;
	}
	__xrtNetEngineTimerInsert(pWorker, pTimer);
}



/* 到期并终结当前时刻之前的全部 Timer。 */
static void __xrtNetEngineTimersExpire(xnetworker* pWorker)
{
	__xrt_net_engine_timers* pTimers = &pWorker->Timers;
	xdeadline iNow = xrtClock();

	while ( (pTimers->Count != 0) &&
			(pTimers->Heap[0]->Deadline <= iNow) ) {
		__xrt_net_engine_timer* pTimer =
			__xrtNetEngineTimerRemove(pWorker, 0);

		__xrtNetEngineTimerFinish(pWorker, pTimer, XNET_RESULT_OK);
	}
}



/* 取消指定 Timer；已经终结的 ID 保持无副作用。 */
static void __xrtNetEngineTimerCancelOnWorker(
	xnetworker* pWorker,
	uint64 Id
)
{
	__xrt_net_engine_timer* pTimer = __xrtNetEngineTimerFind(pWorker, Id);

	if ( pTimer != NULL ) {
		pTimer = __xrtNetEngineTimerRemove(pWorker, pTimer->HeapIndex);
		__xrtNetEngineTimerFinish(
			pWorker,
			pTimer,
			XNET_RESULT_CANCELLED
		);
	}
}



/* 以 CLOSED 终结 Worker 停止时仍然等待的全部 Timer。 */
static void __xrtNetEngineTimersClose(xnetworker* pWorker)
{
	while ( pWorker->Timers.Count != 0 ) {
		__xrt_net_engine_timer* pTimer =
			__xrtNetEngineTimerRemove(pWorker, 0);

		__xrtNetEngineTimerFinish(pWorker, pTimer, XNET_RESULT_CLOSED);
	}
}



/* 释放已经排空的 Timer 表。 */
static void __xrtNetEngineTimersUnit(xnetworker* pWorker)
{
	xrtFree(pWorker->Timers.Heap);
	xrtFree(pWorker->Timers.Buckets);
	memset(&pWorker->Timers, 0, sizeof(pWorker->Timers));
	pWorker->TimersReady = false;
}



/* 返回下一次 Worker 等待使用的单调截止时间。 */
static xdeadline __xrtNetEngineNextDeadline(xnetworker* pWorker)
{
	if ( (xrtAtomic32Load(
		&pWorker->CommandPending,
		XMEMORY_ACQUIRE
	 ) != 0) ||
		 (xrtAtomic32Load(
			&pWorker->InternalPending,
			XMEMORY_ACQUIRE
		 ) != 0) ) {
		return xrtClock();
	}
	if ( pWorker->Timers.Count != 0 ) {
		return pWorker->Timers.Heap[0]->Deadline;
	}
	return xrtDeadlineAfter(pWorker->Engine->Config.IdleWait);
}



/* 在固定预算内按生产顺序消费内部命令，并由 owner 保留未消费尾部。 */
static size_t __xrtNetEngineInternalDrain(
	xnetworker* pWorker,
	size_t iBudget
)
{
	__xrt_net_engine_internal* pOrdered = pWorker->InternalReady;
	size_t iCount = 0;

	if ( pOrdered == NULL ) {
		__xrt_net_engine_internal* pCommand =
			(__xrt_net_engine_internal*)xrtAtomicPtrExchange(
				&pWorker->InternalCommands,
				NULL,
				XMEMORY_ACQ_REL
			);

		while ( pCommand != NULL ) {
			__xrt_net_engine_internal* pNext = pCommand->Next;

			pCommand->Next = pOrdered;
			pOrdered = pCommand;
			pCommand = pNext;
		}
	}
	while ( (pOrdered != NULL) && (iCount < iBudget) ) {
		__xrt_net_engine_internal* pNext = pOrdered->Next;
		xnettaskproc pTask = pOrdered->Task;
		ptr pData = pOrdered->Data;

		pOrdered->Next = NULL;
		pOrdered->Task = NULL;
		pOrdered->Data = NULL;
		(void)xrtAtomic32FetchSub(
			&pWorker->InternalPending,
			1,
			XMEMORY_ACQ_REL
		);
		pTask(pWorker, pData);
		pOrdered = pNext;
		iCount++;
	}
	pWorker->InternalReady = pOrdered;
	return iCount;
}



/* 在 Worker 线程内分发端口事件并转移 Accept 结果所有权。 */
static void __xrtNetEngineDispatch(
	xnetworker* pWorker,
	const xnetportevent* pEvents,
	size_t iCount
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		const xnetportevent* pEvent = &pEvents[i];
		xnetcompletion* pCompletion = (xnetcompletion*)pEvent->User;

		__xrtNetEngineStatAdd(&pWorker->Stats.Events, 1);
		if ( (pCompletion != NULL) && (pCompletion->Proc != NULL) ) {
			pCompletion->Proc(pWorker, pEvent, pCompletion->Data);
		} else if ( pEvent->Accepted != NULL ) {
			(void)xrtNetSocketClose(pEvent->Accepted);
		}
	}
}



/* 消费不超过预算的命令，返回实际消费数量。 */
static size_t __xrtNetEngineCommandsDrain(
	xnetworker* pWorker,
	size_t iBudget
)
{
	size_t iCount = 0;

	while ( iCount < iBudget ) {
		__xrt_net_engine_command* pCommand;
		ptr pItem = NULL;
		xqueueresult Result = xrtMPSCQueueTryPop(
			&pWorker->Commands,
			&pItem
		);

		if ( Result != XQUEUE_OK ) {
			break;
		}
		(void)xrtAtomic32FetchSub(
			&pWorker->CommandPending,
			1,
			XMEMORY_ACQ_REL
		);
		pCommand = (__xrt_net_engine_command*)pItem;
		if ( pCommand == NULL ) {
			continue;
		}
		if ( pCommand->Type == XRT_NET_ENGINE_COMMAND_TASK ) {
			pCommand->Task(pWorker, pCommand->Data);
			__xrtNetEngineStatAdd(&pWorker->Stats.PostsExecuted, 1);
		} else if ( pCommand->Type == XRT_NET_ENGINE_COMMAND_TIMER_ADD ) {
			__xrtNetEngineTimerAdd(pWorker, pCommand->Timer);
			pCommand->Timer = NULL;
		} else if ( pCommand->Type ==
			XRT_NET_ENGINE_COMMAND_TIMER_CANCEL ) {
			__xrtNetEngineTimerCancelOnWorker(
				pWorker,
				pCommand->TimerId
			);
		}
		__xrtNetEngineCommandRecycle(pWorker, pCommand);
		iCount++;
	}
	return iCount;
}



/* 按投递代排空停机任务，大批已受理任务只占一代。 */
static bool __xrtNetEngineShutdownDrain(xnetworker* pWorker)
{
	for ( uint32 i = 0; i < XRT_NET_ENGINE_SHUTDOWN_ROUNDS; i++ ) {
		size_t iInternal = __xrtNetEngineInternalDrain(
			pWorker,
			SIZE_MAX
		);
		size_t iCommands = __xrtNetEngineCommandsDrain(
			pWorker,
			SIZE_MAX
		);

		if ( (iInternal == 0) && (iCommands == 0) ) {
			return true;
		}
	}
	return false;
}



/* 记录一次不收敛停机并封闭后续内部投递。 */
static void __xrtNetEngineShutdownStall(xnetworker* pWorker)
{
	uint32 iExpected = 0;

	xrtAtomic32Store(
		&pWorker->ShutdownPhase,
		XRT_NET_ENGINE_SHUTDOWN_SEALED,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32CompareExchange(
		&pWorker->ShutdownFailed,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		__xrtNetEngineStatError(&pWorker->Stats.ShutdownStalls, 1);
	}
}



/* 封口后不再产生新任务，因此剩余快照必然有限。 */
static void __xrtNetEngineShutdownDrainSealed(xnetworker* pWorker)
{
	for ( ;; ) {
		size_t iInternal = __xrtNetEngineInternalDrain(
			pWorker,
			SIZE_MAX
		);
		size_t iCommands = __xrtNetEngineCommandsDrain(
			pWorker,
			SIZE_MAX
		);

		if ( (iInternal == 0) && (iCommands == 0) ) {
			return;
		}
	}
}



/* Worker 主循环公平地交替处理命令、Timer 和端口事件。 */
static int32 __xrtNetEngineWorkerMain(ptr pData)
{
	xnetworker* pWorker = (xnetworker*)pData;

	if ( !__xrtNetPortThreadClaim(pWorker->Port) ) {
		return 1;
	}
	xrtAtomic64Store(
		&pWorker->ThreadId,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&pWorker->Running, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pWorker->Stop, XMEMORY_ACQUIRE) == 0 ) {
		size_t iEventCount = 0;
		xnetresult Result;

		(void)__xrtNetEngineInternalDrain(
			pWorker,
			XRT_NET_ENGINE_COMMAND_BUDGET
		);
		(void)__xrtNetEngineCommandsDrain(
			pWorker,
			XRT_NET_ENGINE_COMMAND_BUDGET
		);
		__xrtNetEngineTimersExpire(pWorker);
		Result = xrtNetPortWait(
			pWorker->Port,
			pWorker->Events,
			pWorker->Engine->Config.EventBatch,
			__xrtNetEngineNextDeadline(pWorker),
			&iEventCount
		);
		if ( Result == XNET_RESULT_OK ) {
			__xrtNetEngineDispatch(
				pWorker,
				pWorker->Events,
				iEventCount
			);
		} else if ( Result == XNET_RESULT_ERROR ) {
			uint64 iDelay = pWorker->Engine->Config.IdleWait;

			#if XRT_NET_STATS_LEVEL >= XNET_STATS_BASIC
				const xerror* pError = xrtGetError();
				uint32 iCode = XNET_ERROR_PORT_WAIT;

				if ( (pError != NULL) &&
					 (strcmp(xrtErrorDomain(pError), "xrt.net") == 0) ) {
					iCode = (uint32)xrtErrorCode(pError);
				}
				__xrtNetStatBasicStore32(
					&pWorker->Stats.LastWaitError,
					iCode
				);
				__xrtNetStatBasicStore32(
					&pWorker->Stats.LastWaitSystemCode,
					(uint32)((pError != NULL) ?
						xrtErrorSystemCode(pError) : 0)
				);
			#endif
			__xrtNetEngineStatError(&pWorker->Stats.WaitErrors, 1);
			xrtClearError();
			if ( iDelay < 1000u ) {
				iDelay = 1000u;
			} else if ( iDelay > 10000u ) {
				iDelay = 10000u;
			}
			xrtSleepUs(iDelay);
		}
		__xrtNetEngineTimersExpire(pWorker);
	}
	xrtAtomic32Store(
		&pWorker->ShutdownPhase,
		XRT_NET_ENGINE_SHUTDOWN_DRAINING,
		XMEMORY_RELEASE
	);
	if ( !__xrtNetEngineShutdownDrain(pWorker) ) {
		__xrtNetEngineShutdownStall(pWorker);
		__xrtNetEngineShutdownDrainSealed(pWorker);
	}
	__xrtNetEngineTimersClose(pWorker);
	if ( (xrtAtomic32Load(
		&pWorker->ShutdownPhase,
		XMEMORY_ACQUIRE
	 ) != XRT_NET_ENGINE_SHUTDOWN_SEALED) &&
		 !__xrtNetEngineShutdownDrain(pWorker) ) {
		__xrtNetEngineShutdownStall(pWorker);
	}
	xrtAtomic32Store(
		&pWorker->ShutdownPhase,
		XRT_NET_ENGINE_SHUTDOWN_SEALED,
		XMEMORY_RELEASE
	);
	__xrtNetEngineShutdownDrainSealed(pWorker);
	xrtAtomic32Store(&pWorker->Running, 0, XMEMORY_RELEASE);
	if ( !__xrtNetPortThreadRelease(pWorker->Port) ) {
		return 1;
	}
	return 0;
}



/* 释放尚未进入 Worker 的命令；正常停止时队列已经排空。 */
static void __xrtNetEngineCommandDiscard(ptr pItem, ptr pData)
{
	__xrt_net_engine_command* pCommand =
		(__xrt_net_engine_command*)pItem;
	xnetworker* pWorker = (xnetworker*)pData;

	if ( pCommand == NULL ) {
		return;
	}
	(void)xrtAtomic32FetchSub(
		&pWorker->CommandPending,
		1,
		XMEMORY_ACQ_REL
	);
	if ( pCommand->Timer != NULL ) {
		__xrtNetEngineTimerFinish(
			pWorker,
			pCommand->Timer,
			XNET_RESULT_CLOSED
		);
	}
	__xrtNetEngineCommandRecycle(pWorker, pCommand);
}



/* 释放 Worker 一次运行周期资源；Busy 缓冲池保留给重启或重试。 */
static bool __xrtNetEngineWorkerUnit(xnetworker* pWorker)
{
	xerror* pError = NULL;
	bool bResult = true;

	if ( pWorker->Port != NULL ) {
		if ( !xrtNetPortDestroy(pWorker->Port) ) {
			bResult = false;
			__xrtNetEngineErrorKeep(&pError);
		}
		pWorker->Port = NULL;
	}
	if ( pWorker->CommandsReady ) {
		xrtMPSCQueueClose(&pWorker->Commands);
		(void)xrtMPSCQueueDrain(
			&pWorker->Commands,
			__xrtNetEngineCommandDiscard,
			pWorker
		);
		xrtMPSCQueueUnit(&pWorker->Commands);
		pWorker->CommandsReady = false;
	}
	if ( pWorker->TimersReady ) {
		__xrtNetEngineTimersClose(pWorker);
		__xrtNetEngineTimersUnit(pWorker);
	}
	xrtFree(pWorker->Events);
	pWorker->Events = NULL;
	__xrtNetWorkerNodeCacheClear(pWorker);
	if ( pWorker->CacheLockReady ) {
		if ( xrtMutexUnit(&pWorker->CacheLock) ) {
			pWorker->CacheLockReady = false;
		} else {
			bResult = false;
			__xrtNetEngineErrorKeep(&pError);
		}
	}
	if ( pWorker->BufferPool != NULL ) {
		if ( xrtNetBufPoolDestroy(pWorker->BufferPool) ) {
			pWorker->BufferPool = NULL;
		} else {
			bResult = false;
			__xrtNetEngineErrorKeep(&pError);
		}
	}
	xrtAtomic32Store(&pWorker->TimerReserved, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&pWorker->CommandPending, 0, XMEMORY_RELEASE);
	if ( !bResult ) {
		__xrtNetEngineErrorRestore(
			pError,
			XNET_ERROR_ENGINE_STOP,
			"stop-worker",
			"network worker cleanup failed"
		);
	}
	return bResult;
}



/* 建立一个 Worker 的队列、Timer、端口和线程。 */
static bool __xrtNetEngineWorkerStart(xnetworker* pWorker)
{
	xnetengine* pEngine = pWorker->Engine;
	xnetportconfig PortConfig;

	if ( pWorker->Thread != NULL ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_START,
			"start-worker",
			"network worker still owns a previous thread"
		);
		return false;
	}
	xrtAtomic32Store(&pWorker->Stop, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&pWorker->Running, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(
		&pWorker->ShutdownPhase,
		XRT_NET_ENGINE_SHUTDOWN_ACTIVE,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&pWorker->ShutdownFailed, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&pWorker->CommandPending, 0, XMEMORY_RELEASE);
	xrtAtomicPtrStore(
		&pWorker->InternalCommands,
		NULL,
		XMEMORY_RELEASE
	);
	pWorker->InternalReady = NULL;
	xrtAtomic32Store(
		&pWorker->InternalPending,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(&pWorker->ThreadId, 0, XMEMORY_RELEASE);
	pWorker->NodeCacheLimit = pEngine->Config.NodeCacheBytes;
	if ( pWorker->BufferPool == NULL ) {
		pWorker->BufferPool = xrtNetBufPoolCreate(&pEngine->BufferConfig);
		if ( pWorker->BufferPool == NULL ) {
			return false;
		}
	}
	if ( !pWorker->CacheLockReady ) {
		if ( !xrtMutexInit(&pWorker->CacheLock) ) {
			(void)__xrtNetEngineWorkerUnit(pWorker);
			return false;
		}
		pWorker->CacheLockReady = true;
	}
	if ( !xrtMPSCQueueInit(
		&pWorker->Commands,
		pEngine->Config.CommandCapacity
	) ) {
		(void)__xrtNetEngineWorkerUnit(pWorker);
		return false;
	}
	pWorker->CommandsReady = true;
	if ( !__xrtNetEngineTimersInit(pWorker) ) {
		(void)__xrtNetEngineWorkerUnit(pWorker);
		return false;
	}
	pWorker->TimersReady = true;
	pWorker->Events = (xnetportevent*)xrtCalloc(
		pEngine->Config.EventBatch,
		sizeof(*pWorker->Events)
	);
	if ( pWorker->Events == NULL ) {
		(void)__xrtNetEngineWorkerUnit(pWorker);
		return false;
	}
	xrtNetPortConfigInit(&PortConfig);
	PortConfig.Backend = pEngine->Config.Backend;
	PortConfig.PostLimit = pEngine->Config.PortPostLimit;
	PortConfig.WatchLimit = pEngine->Config.PortWatchLimit;
	PortConfig.OperationLimit = pEngine->Config.PortOperationLimit;
	PortConfig.OperationCache = pEngine->Config.PortOperationCache;
	pWorker->Port = xrtNetPortCreate(&PortConfig);
	if ( pWorker->Port == NULL ) {
		(void)__xrtNetEngineWorkerUnit(pWorker);
		return false;
	}
	if ( !__xrtNetPortThreadRelease(pWorker->Port) ) {
		(void)__xrtNetEngineWorkerUnit(pWorker);
		return false;
	}
	pWorker->Thread = xrtThreadCreate(
		__xrtNetEngineWorkerMain,
		pWorker,
		pEngine->Config.ThreadStack
	);
	if ( pWorker->Thread == NULL ) {
		(void)__xrtNetPortThreadClaim(pWorker->Port);
		(void)__xrtNetEngineWorkerUnit(pWorker);
		return false;
	}
	while ( xrtAtomic32Load(&pWorker->Running, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtThreadState(pWorker->Thread) == XTHREAD_FINISHED ) {
			(void)xrtThreadWait(pWorker->Thread);
			xrtThreadDestroy(pWorker->Thread);
			pWorker->Thread = NULL;
			(void)__xrtNetPortThreadClaim(pWorker->Port);
			(void)__xrtNetEngineWorkerUnit(pWorker);
			__xrtNetEngineError(
				XERR_INTERNAL,
				XNET_ERROR_ENGINE_START,
				"start-worker",
				"network worker exited during startup"
			);
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 请求一个 Worker 停止；端口唤醒失败时仍可由有限空闲等待退出。 */
static void __xrtNetEngineWorkerStopRequest(xnetworker* pWorker)
{
	if ( pWorker->Thread == NULL ) {
		return;
	}
	xrtAtomic32Store(&pWorker->Stop, 1, XMEMORY_RELEASE);
	if ( pWorker->CommandsReady ) {
		xrtMPSCQueueClose(&pWorker->Commands);
	}
	if ( pWorker->Port != NULL ) {
		__xrtNetEngineWake(pWorker);
	}
}



/* 等待一个已请求停止的 Worker 排空任务并释放运行资源。 */
static bool __xrtNetEngineWorkerStop(xnetworker* pWorker)
{
	bool bResult;
	bool bShutdownFailed;

	if ( pWorker->Thread == NULL ) {
		return __xrtNetEngineWorkerUnit(pWorker);
	}
	if ( xrtThreadWait(pWorker->Thread) != XWAIT_OK ) {
		return false;
	}
	bShutdownFailed = xrtAtomic32Load(
		&pWorker->ShutdownFailed,
		XMEMORY_ACQUIRE
	) != 0;
	xrtThreadDestroy(pWorker->Thread);
	pWorker->Thread = NULL;
	xrtAtomic64Store(&pWorker->ThreadId, 0, XMEMORY_RELEASE);
	if ( !__xrtNetPortThreadClaim(pWorker->Port) ) {
		return false;
	}
	bResult = __xrtNetEngineWorkerUnit(pWorker);
	if ( bShutdownFailed ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"stop-worker",
			"network worker shutdown tasks did not converge"
		);
		return false;
	}
	return bResult;
}



/* 停止一段已经成功启动的 Worker。 */
static bool __xrtNetEngineWorkersStop(
	xnetengine* pEngine,
	uint32 iCount
)
{
	xerror* pError = NULL;
	bool bResult = true;

	/* 先同时关门，避免等待前一个 Worker 时后面的 Worker 继续受理。 */
	for ( uint32 i = 0; i < iCount; i++ ) {
		(void)xrtAtomic32FetchOr(
			&pEngine->Workers[i].Submitters,
			XRT_NET_ENGINE_SUBMIT_CLOSED,
			XMEMORY_ACQ_REL
		);
	}
	for ( uint32 i = 0; i < iCount; i++ ) {
		while ( xrtAtomic32Load(
			&pEngine->Workers[i].Submitters,
			XMEMORY_ACQUIRE
		) != XRT_NET_ENGINE_SUBMIT_CLOSED ) {
			xrtThreadYield();
		}
	}
	for ( uint32 i = 0; i < iCount; i++ ) {
		__xrtNetEngineWorkerStopRequest(&pEngine->Workers[i]);
	}
	for ( uint32 i = 0; i < iCount; i++ ) {
		if ( !__xrtNetEngineWorkerStop(&pEngine->Workers[i]) ) {
			bResult = false;
			__xrtNetEngineErrorKeep(&pError);
		}
	}
	if ( !bResult ) {
		__xrtNetEngineErrorRestore(
			pError,
			XNET_ERROR_ENGINE_STOP,
			"stop-engine",
			"network worker shutdown failed"
		);
	}
	return bResult;
}



/* 占用 Worker 提交侧，关门后不再允许新生产者进入。 */
static bool __xrtNetEngineSubmitGateEnter(xnetworker* pWorker)
{
	uint32 iGate = xrtAtomic32Load(
		&pWorker->Submitters,
		XMEMORY_ACQUIRE
	);

	for ( ;; ) {
		uint32 iExpected = iGate;

		if ( ((iGate & XRT_NET_ENGINE_SUBMIT_CLOSED) != 0) ||
			 ((iGate & XRT_NET_ENGINE_SUBMIT_COUNT) ==
			 XRT_NET_ENGINE_SUBMIT_COUNT) ) {
			return false;
		}
		if ( xrtAtomic32CompareExchange(
			&pWorker->Submitters,
			&iExpected,
			iGate + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		iGate = iExpected;
	}
	return true;
}



/* 在 Engine 仍运行时占用 Worker 提交侧，防止 Stop 提前释放队列。 */
static bool __xrtNetEngineSubmitEnter(xnetworker* pWorker)
{
	if ( xrtNetEngineState(pWorker->Engine) != XNET_ENGINE_RUNNING ) {
		return false;
	}
	if ( !__xrtNetEngineSubmitGateEnter(pWorker) ) {
		return false;
	}
	if ( xrtNetEngineState(pWorker->Engine) != XNET_ENGINE_RUNNING ) {
		(void)xrtAtomic32FetchSub(
			&pWorker->Submitters,
			1,
			XMEMORY_ACQ_REL
		);
		return false;
	}
	return true;
}



/* 释放 Worker 提交侧占用。 */
static void __xrtNetEngineSubmitLeave(xnetworker* pWorker)
{
	(void)xrtAtomic32FetchSub(
		&pWorker->Submitters,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 在硬上限内原子预留一个 Timer 槽。 */
static bool __xrtNetEngineTimerReserve(xnetworker* pWorker)
{
	uint32 iReserved = xrtAtomic32Load(
		&pWorker->TimerReserved,
		XMEMORY_ACQUIRE
	);
	uint32 iLimit = (uint32)pWorker->Engine->Config.TimerLimit;

	while ( iReserved < iLimit ) {
		uint32 iExpected = iReserved;

		if ( xrtAtomic32CompareExchange(
			&pWorker->TimerReserved,
			&iExpected,
			iReserved + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			return true;
		}
		iReserved = iExpected;
	}
	return false;
}



/* 为指定 Worker 分配可反向定位的非零 Timer ID。 */
static uint64 __xrtNetEngineTimerId(xnetworker* pWorker)
{
	xnetengine* pEngine = pWorker->Engine;

	for ( uint32 i = 0; i < 8u; i++ ) {
		uint64 iSerial = xrtAtomic64FetchAdd(
			&pEngine->NextTimer,
			1,
			XMEMORY_RELAXED
		);
		uint64 Id = (iSerial * (uint64)pEngine->WorkerCount) +
			(uint64)pWorker->Index + 1u;

		if ( (Id != 0) &&
			 (((Id - 1u) % pEngine->WorkerCount) == pWorker->Index) ) {
			return Id;
		}
	}
	return 0;
}



/* 检查 Engine 配置的硬边界和平台无关枚举。 */
static bool __xrtNetEngineConfigValid(const xnetengineconfig* pConfig)
{
	if ( (pConfig->BufferPool != NULL) &&
		 !__xrtNetBufPoolConfigValid(pConfig->BufferPool) ) {
		return false;
	}
	if ( (pConfig->Backend < XNET_PORT_AUTO) ||
		 (pConfig->Backend > XNET_PORT_SELECT) ||
		 (pConfig->Workers > XRT_NET_ENGINE_WORKERS_MAX) ||
		 (pConfig->CommandCapacity < 2u) ||
		 (pConfig->CommandCapacity > XRT_QUEUE_MAX_CAPACITY) ||
		 (pConfig->TimerLimit == 0) ||
		 (pConfig->TimerLimit > UINT32_MAX) ||
		 (pConfig->TimerLimit >
			(SIZE_MAX / sizeof(__xrt_net_engine_timer*))) ||
		 (pConfig->EventBatch == 0) ||
		 (pConfig->EventBatch > XRT_NET_ENGINE_EVENT_BATCH_MAX) ||
		 (pConfig->PortPostLimit == 0) ) {
		__xrtNetEngineError(
			XERR_ARGUMENT,
			XNET_ERROR_ENGINE_CREATE,
			"create-engine",
			"invalid network engine configuration"
		);
		return false;
	}
	return true;
}



/* 初始化兼顾吞吐与内存占用的 Engine 默认配置。 */
XRT_API void xrtNetEngineConfigInit(xnetengineconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Backend = XNET_PORT_AUTO;
	pConfig->BufferPool = NULL;
	pConfig->CommandCapacity = XRT_NET_ENGINE_COMMAND_DEFAULT;
	pConfig->NodeCacheBytes = XRT_NET_ENGINE_NODE_CACHE_DEFAULT;
	pConfig->TimerLimit = XRT_NET_ENGINE_TIMER_DEFAULT;
	pConfig->EventBatch = XRT_NET_ENGINE_EVENT_BATCH_DEFAULT;
	pConfig->PortPostLimit = 4096u;
	pConfig->PortWatchLimit = 0;
	pConfig->PortOperationLimit = 0;
	pConfig->PortOperationCache = 64u;
	pConfig->IdleWait = XRT_NET_ENGINE_IDLE_WAIT;
}



/* 初始化一个借用过程和数据的端口 Completion。 */
XRT_API void xrtNetCompletionInit(
	xnetcompletion* pCompletion,
	xnetcompletionproc pProc,
	ptr pData
)
{
	if ( pCompletion == NULL ) {
		return;
	}
	pCompletion->Proc = pProc;
	pCompletion->Data = pData;
}



/* 创建停止状态的 Engine 和固定 Worker 描述数组。 */
XRT_API xnetengine* xrtNetEngineCreate(const xnetengineconfig* pConfig)
{
	xnetengineconfig Config;
	xnetbufpoolconfig BufferConfig;
	xnetengine* pEngine;
	uint32 iWorkers;

	xrtNetEngineConfigInit(&Config);
	xrtNetBufPoolConfigInit(&BufferConfig);
	if ( pConfig != NULL ) {
		Config = *pConfig;
		if ( pConfig->BufferPool != NULL ) {
			BufferConfig = *pConfig->BufferPool;
		}
	}
	if ( !__xrtNetEngineConfigValid(&Config) ) {
		return NULL;
	}
	iWorkers = Config.Workers;
	if ( iWorkers == 0 ) {
		iWorkers = __xrtProcessorCount();
		if ( iWorkers > XRT_NET_ENGINE_AUTO_WORKERS_MAX ) {
			iWorkers = XRT_NET_ENGINE_AUTO_WORKERS_MAX;
		}
		if ( iWorkers == 0 ) {
			iWorkers = 1;
		}
	}
	pEngine = (xnetengine*)xrtCalloc(1, sizeof(*pEngine));
	if ( pEngine == NULL ) {
		return NULL;
	}
	pEngine->Workers = (xnetworker*)xrtCalloc(
		iWorkers,
		sizeof(*pEngine->Workers)
	);
	if ( pEngine->Workers == NULL ) {
		xrtFree(pEngine);
		return NULL;
	}
	if ( !xrtMutexInit(&pEngine->Lifecycle) ) {
		xrtFree(pEngine->Workers);
		xrtFree(pEngine);
		return NULL;
	}
	pEngine->Config = Config;
	pEngine->Config.BufferPool = NULL;
	pEngine->BufferConfig = BufferConfig;
	pEngine->WorkerCount = iWorkers;
	xrtAtomic32Init(&pEngine->State, XNET_ENGINE_STOPPED);
	xrtAtomic32Init(&pEngine->LiveObjects, 0);
	xrtAtomic64Init(&pEngine->NextTimer, 0);
	xrtAtomic64Init(&pEngine->NextOperation, 0);
	for ( uint32 i = 0; i < iWorkers; i++ ) {
		xnetworker* pWorker = &pEngine->Workers[i];

		pWorker->Engine = pEngine;
		pWorker->Index = i;
		xrtAtomic32Init(&pWorker->Running, 0);
		xrtAtomic32Init(&pWorker->Stop, 0);
		xrtAtomic32Init(
			&pWorker->ShutdownPhase,
			XRT_NET_ENGINE_SHUTDOWN_ACTIVE
		);
		xrtAtomic32Init(&pWorker->ShutdownFailed, 0);
		xrtAtomic32Init(
			&pWorker->Submitters,
			XRT_NET_ENGINE_SUBMIT_CLOSED
		);
		xrtAtomic32Init(&pWorker->CommandPending, 0);
		xrtAtomic32Init(&pWorker->TimerReserved, 0);
		xrtAtomic64Init(&pWorker->ThreadId, 0);
		xrtAtomicPtrInit(&pWorker->InternalCommands, NULL);
		xrtAtomic32Init(&pWorker->InternalPending, 0);
		xrtAtomic64Init(&pWorker->NodeCachedBytes, 0);
		__xrtNetEngineStatsInit(&pWorker->Stats);
	}
	return pEngine;
}



/* 建立全部 Worker、端口和线程。 */
XRT_API bool xrtNetEngineStart(xnetengine* pEngine)
{
	uint32 iStarted = 0;
	xnetenginestate State;

	if ( pEngine == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	State = xrtNetEngineState(pEngine);
	if ( State == XNET_ENGINE_RUNNING ) {
		return true;
	}
	if ( State != XNET_ENGINE_STOPPED ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_START,
			"start-engine",
			"network engine is changing state"
		);
		return false;
	}
	if ( !xrtMutexLock(&pEngine->Lifecycle) ) {
		return false;
	}
	if ( xrtNetEngineState(pEngine) != XNET_ENGINE_STOPPED ) {
		(void)xrtMutexUnlock(&pEngine->Lifecycle);
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_START,
			"start-engine",
			"network engine is no longer stopped"
		);
		return false;
	}
	xrtAtomic32Store(
		&pEngine->State,
		XNET_ENGINE_STARTING,
		XMEMORY_RELEASE
	);
	for ( ; iStarted < pEngine->WorkerCount; iStarted++ ) {
		if ( !__xrtNetEngineWorkerStart(&pEngine->Workers[iStarted]) ) {
			xerror* pError = xrtTakeError();

			(void)__xrtNetEngineWorkersStop(pEngine, iStarted);
			xrtAtomic32Store(
				&pEngine->State,
				XNET_ENGINE_STOPPED,
				XMEMORY_RELEASE
			);
			(void)xrtMutexUnlock(&pEngine->Lifecycle);
			if ( pError != NULL ) {
				xrtSetError(pError);
				xrtErrorFree(pError);
			} else {
				__xrtNetEngineError(
					XERR_INTERNAL,
					XNET_ERROR_ENGINE_START,
					"start-engine",
					"network worker start failed"
				);
			}
			return false;
		}
	}
	for ( uint32 i = 0; i < pEngine->WorkerCount; i++ ) {
		xrtAtomic32Store(
			&pEngine->Workers[i].Submitters,
			0,
			XMEMORY_RELEASE
		);
	}
	xrtAtomic32Store(
		&pEngine->State,
		XNET_ENGINE_RUNNING,
		XMEMORY_RELEASE
	);
	(void)xrtMutexUnlock(&pEngine->Lifecycle);
	return true;
}



/* 停止全部 Worker，排空任务并关闭 Timer。 */
XRT_API bool xrtNetEngineStop(xnetengine* pEngine)
{
	bool bResult;
	xnetenginestate State;

	if ( pEngine == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtNetEngineCurrent(pEngine) != NULL ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"stop-engine",
			"a network worker cannot stop its own engine"
		);
		return false;
	}
	State = xrtNetEngineState(pEngine);
	if ( (State != XNET_ENGINE_STOPPED) &&
		 (State != XNET_ENGINE_RUNNING) ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"stop-engine",
			"network engine is changing state"
		);
		return false;
	}
	if ( !xrtMutexLock(&pEngine->Lifecycle) ) {
		return false;
	}
	State = xrtNetEngineState(pEngine);
	if ( (State != XNET_ENGINE_STOPPED) &&
		 (State != XNET_ENGINE_RUNNING) ) {
		(void)xrtMutexUnlock(&pEngine->Lifecycle);
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"stop-engine",
			"network engine is changing state"
		);
		return false;
	}
	xrtAtomic32Store(
		&pEngine->State,
		XNET_ENGINE_STOPPING,
		XMEMORY_RELEASE
	);
	if ( (State == XNET_ENGINE_RUNNING) &&
		 (xrtAtomic32Load(
			&pEngine->LiveObjects,
			XMEMORY_ACQUIRE
		 ) != 0) ) {
		xrtAtomic32Store(
			&pEngine->State,
			State,
			XMEMORY_RELEASE
		);
		(void)xrtMutexUnlock(&pEngine->Lifecycle);
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"stop-engine",
			"network engine still owns live network objects"
		);
		return false;
	}
	(void)xrtMutexUnlock(&pEngine->Lifecycle);
	bResult = __xrtNetEngineWorkersStop(
		pEngine,
		pEngine->WorkerCount
	);
	xrtAtomic32Store(
		&pEngine->State,
		XNET_ENGINE_STOPPED,
		XMEMORY_RELEASE
	);
	return bResult;
}



/* 停止并销毁 Engine；活动高层对象会阻止该操作。 */
XRT_API bool xrtNetEngineDestroy(xnetengine* pEngine)
{
	bool bResult;
	xnetenginestate State;

	if ( pEngine == NULL ) {
		return true;
	}
	if ( xrtNetEngineCurrent(pEngine) != NULL ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"destroy-engine",
			"a network worker cannot destroy its own engine"
		);
		return false;
	}
	if ( !xrtMutexLock(&pEngine->Lifecycle) ) {
		return false;
	}
	State = xrtNetEngineState(pEngine);
	if ( (State != XNET_ENGINE_STOPPED) &&
		 (State != XNET_ENGINE_RUNNING) ) {
		(void)xrtMutexUnlock(&pEngine->Lifecycle);
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"destroy-engine",
			"network engine is changing state"
		);
		return false;
	}
	xrtAtomic32Store(
		&pEngine->State,
		XNET_ENGINE_DESTROYING,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(&pEngine->LiveObjects, XMEMORY_ACQUIRE) != 0 ) {
		xrtAtomic32Store(&pEngine->State, State, XMEMORY_RELEASE);
		(void)xrtMutexUnlock(&pEngine->Lifecycle);
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_STOP,
			"destroy-engine",
			"network engine still owns live network objects"
		);
		return false;
	}
	(void)xrtMutexUnlock(&pEngine->Lifecycle);
	bResult = __xrtNetEngineWorkersStop(
		pEngine,
		pEngine->WorkerCount
	);
	if ( !bResult ) {
		xrtAtomic32Store(
			&pEngine->State,
			XNET_ENGINE_STOPPED,
			XMEMORY_RELEASE
		);
		return false;
	}
	if ( !xrtMutexUnit(&pEngine->Lifecycle) ) {
		xrtAtomic32Store(
			&pEngine->State,
			XNET_ENGINE_STOPPED,
			XMEMORY_RELEASE
		);
		return false;
	}
	xrtFree(pEngine->Workers);
	xrtFree(pEngine);
	return true;
}



/* 返回当前生命周期状态。 */
XRT_API xnetenginestate xrtNetEngineState(const xnetengine* pEngine)
{
	if ( pEngine == NULL ) {
		return XNET_ENGINE_DESTROYING;
	}
	return (xnetenginestate)xrtAtomic32Load(
		&pEngine->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回 Engine 固定的 Worker 数量。 */
XRT_API uint32 xrtNetEngineWorkerCount(const xnetengine* pEngine)
{
	return pEngine != NULL ? pEngine->WorkerCount : 0;
}



/* 返回借用的指定 Worker。 */
XRT_API xnetworker* xrtNetEngineWorker(
	xnetengine* pEngine,
	uint32 iIndex
)
{
	if ( (pEngine == NULL) || (iIndex >= pEngine->WorkerCount) ) {
		__xrtErrorSetRange();
		return NULL;
	}
	return &pEngine->Workers[iIndex];
}



/* 返回当前线程所属的 Worker。 */
XRT_API xnetworker* xrtNetEngineCurrent(xnetengine* pEngine)
{
	uint64 iThreadId;

	if ( pEngine == NULL ) {
		return NULL;
	}
	iThreadId = xrtThreadCurrentId();
	for ( uint32 i = 0; i < pEngine->WorkerCount; i++ ) {
		xnetworker* pWorker = &pEngine->Workers[i];

		if ( (iThreadId != 0) && (iThreadId == xrtAtomic64Load(
			&pWorker->ThreadId,
			XMEMORY_ACQUIRE
		)) ) {
			return pWorker;
		}
	}
	return NULL;
}



/* 返回 Worker 所属的借用 Engine。 */
XRT_API xnetengine* xrtNetWorkerEngine(const xnetworker* pWorker)
{
	return pWorker != NULL ? pWorker->Engine : NULL;
}



/* 返回 Worker 在 Engine 内的稳定索引。 */
XRT_API uint32 xrtNetWorkerIndex(const xnetworker* pWorker)
{
	if ( pWorker == NULL ) {
		__xrtErrorSetInvalidArgument();
		return UINT32_MAX;
	}
	return pWorker->Index;
}



/* 判断调用线程是否正是指定 Worker。 */
XRT_API bool xrtNetWorkerIsCurrent(const xnetworker* pWorker)
{
	uint64 iThreadId;

	if ( pWorker == NULL ) {
		return false;
	}
	iThreadId = xrtAtomic64Load(&pWorker->ThreadId, XMEMORY_ACQUIRE);
	return (iThreadId != 0) && (iThreadId == xrtThreadCurrentId());
}



/* 返回运行期间借用的 Worker 端口。 */
XRT_API xnetport* xrtNetWorkerPort(xnetworker* pWorker)
{
	if ( (pWorker == NULL) ||
		 (xrtAtomic32Load(&pWorker->Running, XMEMORY_ACQUIRE) == 0) ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_POST,
			"worker-port",
			"network worker is not running"
		);
		return NULL;
	}
	return pWorker->Port;
}



/* 返回仅能由所属 Worker 线程操作的共享自适应缓冲池。 */
XRT_API xnetbufpool* xrtNetWorkerBufPool(xnetworker* pWorker)
{
	if ( pWorker == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtNetWorkerIsCurrent(pWorker) ||
		 (pWorker->BufferPool == NULL) ) {
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_POST,
			"worker-buffer-pool",
			"network worker buffer pool is only available on its worker"
		);
		return NULL;
	}
	return pWorker->BufferPool;
}



/* 分配跨协议共享的端口操作 ID，避免同一端口上的活动 ID 冲突。 */
XRT_API uint64 xrtNetWorkerOperationId(xnetworker* pWorker)
{
	if ( pWorker == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	for ( uint32 i = 0; i < 2u; i++ ) {
		uint64 Id = xrtAtomic64FetchAdd(
			&pWorker->Engine->NextOperation,
			1,
			XMEMORY_RELAXED
		) + 1u;

		if ( Id != 0 ) {
			return Id;
		}
	}
	__xrtNetEngineError(
		XERR_INTERNAL,
		XNET_ERROR_ENGINE_POST,
		"allocate-operation-id",
		"network operation identity space is exhausted"
	);
	return 0;
}



/* 有限重试端口唤醒；命令已经入队时失败只影响调度延迟。 */
static void __xrtNetEngineWake(xnetworker* pWorker)
{
	bool bFailed = false;

	for ( uint32 i = 0; i < XRT_NET_ENGINE_WAKE_RETRIES; i++ ) {
		if ( xrtNetPortWake(pWorker->Port) ) {
			if ( bFailed ) {
				xrtClearError();
			}
			return;
		}
		bFailed = true;
		if ( (i + 1u) < XRT_NET_ENGINE_WAKE_RETRIES ) {
			xrtThreadYield();
		}
	}
	__xrtNetEngineStatError(&pWorker->Stats.WakeErrors, 1);
	xrtClearError();
}



/* 把一个已构造命令提交到 Worker 的有界队列。 */
static bool __xrtNetEngineCommandPush(
	xnetworker* pWorker,
	__xrt_net_engine_command* pCommand,
	xneterror Code,
	cstr sOperation
)
{
	xqueueresult Result;

	(void)xrtAtomic32FetchAdd(
		&pWorker->CommandPending,
		1,
		XMEMORY_ACQ_REL
	);
	Result = xrtMPSCQueueTryPush(
		&pWorker->Commands,
		pCommand
	);

	if ( Result == XQUEUE_OK ) {
		__xrtNetEngineWake(pWorker);
		return true;
	}
	(void)xrtAtomic32FetchSub(
		&pWorker->CommandPending,
		1,
		XMEMORY_ACQ_REL
	);
	__xrtNetEngineCommandRecycle(pWorker, pCommand);
	if ( Result == XQUEUE_FULL ) {
		__xrtNetEngineError(
			XERR_AGAIN,
			Code,
			sOperation,
			"network worker command queue is full"
		);
	} else if ( Result == XQUEUE_CLOSED ) {
		__xrtNetEngineError(
			XERR_CLOSED,
			Code,
			sOperation,
			"network worker command queue is closed"
		);
	} else {
		__xrtNetEngineError(
			XERR_INTERNAL,
			Code,
			sOperation,
			"network worker command queue rejected the command"
		);
	}
	return false;
}



/* 有界投递任务；受理后必在亲和 Worker 上执行一次。 */
XRT_API bool xrtNetEnginePost(
	xnetengine* pEngine,
	uint64 iAffinity,
	xnettaskproc pProc,
	ptr pData
)
{
	xnetworker* pWorker;
	__xrt_net_engine_command* pCommand;

	if ( (pEngine == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWorker = &pEngine->Workers[iAffinity % pEngine->WorkerCount];
	if ( !__xrtNetEngineSubmitEnter(pWorker) ) {
		__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
		__xrtNetEngineError(
			XERR_CLOSED,
			XNET_ERROR_ENGINE_POST,
			"post-engine",
			"network engine is not running"
		);
		return false;
	}
	pCommand = __xrtNetEngineCommandAlloc(pWorker);
	if ( pCommand == NULL ) {
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
		return false;
	}
	pCommand->Type = XRT_NET_ENGINE_COMMAND_TASK;
	pCommand->Task = pProc;
	pCommand->Data = pData;
	if ( !__xrtNetEngineCommandPush(
		pWorker,
		pCommand,
		XNET_ERROR_ENGINE_POST,
		"post-engine"
	) ) {
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
		return false;
	}
	__xrtNetEngineSubmitLeave(pWorker);
	__xrtNetEngineStatAdd(&pWorker->Stats.PostsAccepted, 1);
	return true;
}



/* 投递一个不受公开命令容量和分配器影响的内部生命周期命令。 */
bool __xrtNetEnginePostInternal(
	xnetworker* pWorker,
	__xrt_net_engine_internal* pCommand,
	xnettaskproc pProc,
	ptr pData
)
{
	ptr pHead;
	bool bCurrent;
	bool bEntered = false;

	if ( (pWorker == NULL) || (pCommand == NULL) || (pProc == NULL) ) {
		return false;
	}
	bCurrent = xrtNetWorkerIsCurrent(pWorker);
	if ( !bCurrent ) {
		bEntered = __xrtNetEngineSubmitGateEnter(pWorker);
		if ( !bEntered ) {
			return false;
		}
	}
	if ( (xrtAtomic32Load(
		&pWorker->Running,
		XMEMORY_ACQUIRE
	 ) == 0) || (xrtAtomic32Load(
		&pWorker->ShutdownPhase,
		XMEMORY_ACQUIRE
	 ) == XRT_NET_ENGINE_SHUTDOWN_SEALED) ) {
		if ( bEntered ) {
			__xrtNetEngineSubmitLeave(pWorker);
		}
		return false;
	}
	pHead = xrtAtomicPtrLoad(
		&pWorker->InternalCommands,
		XMEMORY_ACQUIRE
	);

	pCommand->Task = pProc;
	pCommand->Data = pData;
	(void)xrtAtomic32FetchAdd(
		&pWorker->InternalPending,
		1,
		XMEMORY_ACQ_REL
	);
	for ( ;; ) {
		ptr pExpected = pHead;

		pCommand->Next = (__xrt_net_engine_internal*)pHead;
		if ( xrtAtomicPtrCompareExchange(
			&pWorker->InternalCommands,
			&pExpected,
			pCommand,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		pHead = pExpected;
	}
	__xrtNetEngineWake(pWorker);
	if ( bEntered ) {
		__xrtNetEngineSubmitLeave(pWorker);
	}
	return true;
}



/* 执行公开嵌入式 Post，并在进入用户过程前允许下一次投递。 */
static void __xrtNetPostTask(xnetworker* pWorker, ptr pData)
{
	xrt_net_post_impl* pImpl = (xrt_net_post_impl*)pData;
	xnettaskproc pTask = pImpl->Task;
	ptr pTaskData = pImpl->Data;

	pImpl->Task = NULL;
	pImpl->Data = NULL;
	xrtAtomic32Store(&pImpl->Pending, 0, XMEMORY_RELEASE);
	__xrtNetEngineStatAdd(&pWorker->Stats.PostsExecuted, 1);
	pTask(pWorker, pTaskData);
}



/* 初始化一个调用方持有的嵌入式 Post。 */
XRT_API bool xrtNetPostInit(xnetpost* pPost)
{
	xrt_net_post_impl* pImpl;

	if ( !__xrtRangeValid(pPost, sizeof(*pPost)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pPost, 0, sizeof(*pPost));
	pImpl = __xrtNetPostImpl(pPost);
	xrtAtomic32Init(&pImpl->Pending, 0);
	pImpl->Magic = XRT_NET_POST_MAGIC;
	return true;
}



/* 返回嵌入式 Post 的排队状态。 */
XRT_API bool xrtNetPostPending(const xnetpost* pPost)
{
	const xrt_net_post_impl* pImpl;

	if ( !__xrtRangeValid(pPost, sizeof(*pPost)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pImpl = (const xrt_net_post_impl*)pPost;
	if ( pImpl->Magic != XRT_NET_POST_MAGIC ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return xrtAtomic32Load(
		&pImpl->Pending,
		XMEMORY_ACQUIRE
	) != 0;
}



/* 无分配地把调用方嵌入节点投递到指定 Worker。 */
XRT_API bool xrtNetPost(
	xnetworker* pWorker,
	xnetpost* pPost,
	xnettaskproc pProc,
	ptr pData
)
{
	xrt_net_post_impl* pImpl;
	uint32 iExpected = 0;
	bool bCurrent;
	bool bEntered = false;

	if ( (pWorker == NULL) ||
		!__xrtRangeValid(pPost, sizeof(*pPost)) ||
		(pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pImpl = __xrtNetPostImpl(pPost);
	bCurrent = xrtNetWorkerIsCurrent(pWorker);
	if ( pImpl->Magic != XRT_NET_POST_MAGIC ) {
		__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( xrtAtomic32Load(
		&pWorker->Running,
		XMEMORY_ACQUIRE
	) == 0 ) {
		__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
		__xrtNetEngineError(
			XERR_CLOSED,
			XNET_ERROR_ENGINE_POST,
			"post-worker",
			"network worker is not running"
		);
		return false;
	}
	if ( !bCurrent ) {
		bEntered = __xrtNetEngineSubmitEnter(pWorker);
		if ( !bEntered ) {
			__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
			__xrtNetEngineError(
				XERR_CLOSED,
				XNET_ERROR_ENGINE_POST,
				"post-worker",
				"network worker is not accepting posts"
			);
			return false;
		}
	}
	if ( !xrtAtomic32CompareExchange(
		&pImpl->Pending,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		if ( bEntered ) {
			__xrtNetEngineSubmitLeave(pWorker);
		}
		__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
		__xrtNetEngineError(
			XERR_STATE,
			XNET_ERROR_ENGINE_POST,
			"post-worker",
			"embedded network Post is already pending"
		);
		return false;
	}
	pImpl->Task = pProc;
	pImpl->Data = pData;
	if ( !__xrtNetEnginePostInternal(
		pWorker,
		&pImpl->Internal,
		__xrtNetPostTask,
		pImpl
	) ) {
		pImpl->Task = NULL;
		pImpl->Data = NULL;
		xrtAtomic32Store(&pImpl->Pending, 0, XMEMORY_RELEASE);
		if ( bEntered ) {
			__xrtNetEngineSubmitLeave(pWorker);
		}
		__xrtNetEngineStatError(&pWorker->Stats.PostsRejected, 1);
		__xrtNetEngineError(
			XERR_CLOSED,
			XNET_ERROR_ENGINE_POST,
			"post-worker",
			"network worker shutdown is sealed"
		);
		return false;
	}
	if ( bEntered ) {
		__xrtNetEngineSubmitLeave(pWorker);
	}
	__xrtNetEngineStatAdd(&pWorker->Stats.PostsAccepted, 1);
	return true;
}



/* 按单调截止时间调度一个具有唯一终态的 Timer。 */
XRT_API uint64 xrtNetEngineSchedule(
	xnetengine* pEngine,
	uint64 iAffinity,
	xdeadline iDeadline,
	xnettimerproc pProc,
	ptr pData
)
{
	xnetworker* pWorker;
	__xrt_net_engine_timer* pTimer;
	__xrt_net_engine_command* pCommand;
	uint64 Id;
	bool bCurrent;

	if ( (pEngine == NULL) || (pProc == NULL) ||
		 (iDeadline == XRT_DEADLINE_NEVER) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	pWorker = &pEngine->Workers[iAffinity % pEngine->WorkerCount];
	if ( !__xrtNetEngineSubmitEnter(pWorker) ) {
		__xrtNetEngineStatError(&pWorker->Stats.TimersRejected, 1);
		__xrtNetEngineError(
			XERR_CLOSED,
			XNET_ERROR_ENGINE_TIMER,
			"schedule-timer",
			"network engine is not running"
		);
		return 0;
	}
	if ( !__xrtNetEngineTimerReserve(pWorker) ) {
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatError(&pWorker->Stats.TimersRejected, 1);
		__xrtNetEngineError(
			XERR_AGAIN,
			XNET_ERROR_ENGINE_TIMER,
			"schedule-timer",
			"network worker timer limit reached"
		);
		return 0;
	}
	pTimer = __xrtNetEngineTimerAlloc(pWorker);
	if ( pTimer == NULL ) {
		(void)xrtAtomic32FetchSub(
			&pWorker->TimerReserved,
			1,
			XMEMORY_ACQ_REL
		);
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatError(&pWorker->Stats.TimersRejected, 1);
		return 0;
	}
	Id = __xrtNetEngineTimerId(pWorker);
	if ( Id == 0 ) {
		__xrtNetEngineTimerRecycle(pWorker, pTimer);
		(void)xrtAtomic32FetchSub(
			&pWorker->TimerReserved,
			1,
			XMEMORY_ACQ_REL
		);
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatError(&pWorker->Stats.TimersRejected, 1);
		__xrtNetEngineError(
			XERR_RANGE,
			XNET_ERROR_ENGINE_TIMER,
			"schedule-timer",
			"network timer identifier space exhausted"
		);
		return 0;
	}
	memset(pTimer, 0, sizeof(*pTimer));
	pTimer->Id = Id;
	pTimer->Deadline = iDeadline;
	pTimer->Proc = pProc;
	pTimer->Data = pData;
	pTimer->HeapIndex = SIZE_MAX;

	/*
		所属 Worker 可直接入堆，避免同一回调内完成后仍需分配
		取消命令。先确保容量，保证成功返回前不会内联失败回调。
	*/
	bCurrent = xrtNetWorkerIsCurrent(pWorker);
	if ( bCurrent ) {
		if ( !__xrtNetEngineTimerEnsure(
			pWorker,
			pWorker->Timers.Count + 1u
		) ) {
			__xrtNetEngineTimerRecycle(pWorker, pTimer);
			(void)xrtAtomic32FetchSub(
				&pWorker->TimerReserved,
				1,
				XMEMORY_ACQ_REL
			);
			__xrtNetEngineSubmitLeave(pWorker);
			__xrtNetEngineStatError(
				&pWorker->Stats.TimersRejected,
				1
			);
			return 0;
		}
		__xrtNetEngineTimerInsert(pWorker, pTimer);
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatAdd(
			&pWorker->Stats.TimersAccepted,
			1
		);
		return Id;
	}

	pCommand = __xrtNetEngineCommandAlloc(pWorker);
	if ( pCommand == NULL ) {
		__xrtNetEngineTimerRecycle(pWorker, pTimer);
		(void)xrtAtomic32FetchSub(
			&pWorker->TimerReserved,
			1,
			XMEMORY_ACQ_REL
		);
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatError(&pWorker->Stats.TimersRejected, 1);
		return 0;
	}
	pCommand->Type = XRT_NET_ENGINE_COMMAND_TIMER_ADD;
	pCommand->Timer = pTimer;
	if ( !__xrtNetEngineCommandPush(
		pWorker,
		pCommand,
		XNET_ERROR_ENGINE_TIMER,
		"schedule-timer"
	) ) {
		__xrtNetEngineTimerRecycle(pWorker, pTimer);
		(void)xrtAtomic32FetchSub(
			&pWorker->TimerReserved,
			1,
			XMEMORY_ACQ_REL
		);
		__xrtNetEngineSubmitLeave(pWorker);
		__xrtNetEngineStatError(&pWorker->Stats.TimersRejected, 1);
		return 0;
	}
	__xrtNetEngineSubmitLeave(pWorker);
	__xrtNetEngineStatAdd(&pWorker->Stats.TimersAccepted, 1);
	return Id;
}



/* 按相对微秒数调度 Timer。 */
XRT_API uint64 xrtNetEngineAfter(
	xnetengine* pEngine,
	uint64 iAffinity,
	uint64 iTimeout,
	xnettimerproc pProc,
	ptr pData
)
{
	return xrtNetEngineSchedule(
		pEngine,
		iAffinity,
		xrtDeadlineAfter(iTimeout),
		pProc,
		pData
	);
}



/* 异步请求目标 Worker 取消 Timer。 */
XRT_API bool xrtNetEngineTimerCancel(xnetengine* pEngine, uint64 Id)
{
	xnetworker* pWorker;
	__xrt_net_engine_command* pCommand;
	uint32 iWorker;

	if ( (pEngine == NULL) || (Id == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iWorker = (uint32)((Id - 1u) % pEngine->WorkerCount);
	pWorker = &pEngine->Workers[iWorker];
	if ( !__xrtNetEngineSubmitEnter(pWorker) ) {
		__xrtNetEngineError(
			XERR_CLOSED,
			XNET_ERROR_ENGINE_TIMER,
			"cancel-timer",
			"network engine is not running"
		);
		return false;
	}
	pCommand = __xrtNetEngineCommandAlloc(pWorker);
	if ( pCommand == NULL ) {
		__xrtNetEngineSubmitLeave(pWorker);
		return false;
	}
	pCommand->Type = XRT_NET_ENGINE_COMMAND_TIMER_CANCEL;
	pCommand->TimerId = Id;
	{
		bool bResult = __xrtNetEngineCommandPush(
			pWorker,
			pCommand,
			XNET_ERROR_ENGINE_TIMER,
			"cancel-timer"
		);

		__xrtNetEngineSubmitLeave(pWorker);
		return bResult;
	}
}



/* 在所属 Worker 上无分配地立即移除已经入堆的 Timer。 */
XRT_API bool xrtNetEngineTimerCancelCurrent(
	xnetengine* pEngine,
	uint64 Id
)
{
	xnetworker* pWorker;
	__xrt_net_engine_timer* pTimer;
	uint32 iWorker;

	if ( (pEngine == NULL) || (Id == 0) ||
		(pEngine->WorkerCount == 0) ) {
		return false;
	}
	iWorker = (uint32)((Id - 1u) %
		pEngine->WorkerCount);
	pWorker = &pEngine->Workers[iWorker];
	if ( !xrtNetWorkerIsCurrent(pWorker) ) {
		return false;
	}
	pTimer = __xrtNetEngineTimerFind(pWorker, Id);
	if ( pTimer == NULL ) {
		return false;
	}
	pTimer = __xrtNetEngineTimerRemove(
		pWorker,
		pTimer->HeapIndex
	);
	__xrtNetEngineTimerFinish(
		pWorker,
		pTimer,
		XNET_RESULT_CANCELLED
	);
	return true;
}



/*
	优先从所属 Worker 无分配地终结生命周期 Timer，
	跨线程或尚未入堆时仍使用有序的公开取消命令。
*/
bool __xrtNetEngineTimerCancelLifecycle(
	xnetengine* pEngine,
	uint64 Id
)
{
	if ( xrtNetEngineTimerCancelCurrent(pEngine, Id) ) {
		return true;
	}
	return xrtNetEngineTimerCancel(pEngine, Id);
}



/* 读取一个 Worker 的统计快照。 */
XRT_API bool xrtNetWorkerStats(
	const xnetworker* pWorker,
	xnetworkerstats* pStats
)
{
	if ( (pWorker == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pStats, 0, sizeof(*pStats));
	pStats->PostsAccepted =
		__xrtNetEngineStatLoad(&pWorker->Stats.PostsAccepted);
	pStats->PostsRejected =
		__xrtNetEngineStatLoad(&pWorker->Stats.PostsRejected);
	pStats->PostsExecuted =
		__xrtNetEngineStatLoad(&pWorker->Stats.PostsExecuted);
	pStats->TimersAccepted =
		__xrtNetEngineStatLoad(&pWorker->Stats.TimersAccepted);
	pStats->TimersRejected =
		__xrtNetEngineStatLoad(&pWorker->Stats.TimersRejected);
	pStats->TimersFired =
		__xrtNetEngineStatLoad(&pWorker->Stats.TimersFired);
	pStats->TimersCancelled =
		__xrtNetEngineStatLoad(&pWorker->Stats.TimersCancelled);
	pStats->TimersClosed =
		__xrtNetEngineStatLoad(&pWorker->Stats.TimersClosed);
	pStats->TimerErrors =
		__xrtNetEngineStatLoad(&pWorker->Stats.TimerErrors);
	pStats->Events = __xrtNetEngineStatLoad(&pWorker->Stats.Events);
	pStats->WaitErrors =
		__xrtNetEngineStatLoad(&pWorker->Stats.WaitErrors);
	pStats->WakeErrors =
		__xrtNetEngineStatLoad(&pWorker->Stats.WakeErrors);
	pStats->ShutdownStalls =
		__xrtNetEngineStatLoad(&pWorker->Stats.ShutdownStalls);
	pStats->LastWaitError = (xneterror)xrtAtomic32Load(
		&pWorker->Stats.LastWaitError,
		XMEMORY_ACQUIRE
	);
	pStats->LastWaitSystemCode = (int32)xrtAtomic32Load(
		&pWorker->Stats.LastWaitSystemCode,
		XMEMORY_ACQUIRE
	);
	pStats->NodeCacheHits =
		__xrtNetEngineStatLoad(&pWorker->Stats.NodeCacheHits);
	pStats->NodeCacheMisses =
		__xrtNetEngineStatLoad(&pWorker->Stats.NodeCacheMisses);
	pStats->PendingCommands = xrtAtomic32Load(
		&pWorker->CommandPending,
		XMEMORY_ACQUIRE
	);
	pStats->PendingCommands += xrtAtomic32Load(
		&pWorker->InternalPending,
		XMEMORY_ACQUIRE
	);
	pStats->ActiveTimers = xrtAtomic32Load(
		&pWorker->TimerReserved,
		XMEMORY_ACQUIRE
	);
	pStats->NodeCachedBytes = (size_t)xrtAtomic64Load(
		&pWorker->NodeCachedBytes,
		XMEMORY_RELAXED
	);
	return true;
}



/* 聚合全部 Worker 的统计快照。 */
XRT_API bool xrtNetEngineStats(
	const xnetengine* pEngine,
	xnetenginestats* pStats
)
{
	if ( (pEngine == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pStats, 0, sizeof(*pStats));
	pStats->State = xrtNetEngineState(pEngine);
	pStats->Workers = pEngine->WorkerCount;
	for ( uint32 i = 0; i < pEngine->WorkerCount; i++ ) {
		xnetworkerstats WorkerStats;

		(void)xrtNetWorkerStats(&pEngine->Workers[i], &WorkerStats);
		pStats->PostsAccepted += WorkerStats.PostsAccepted;
		pStats->PostsRejected += WorkerStats.PostsRejected;
		pStats->PostsExecuted += WorkerStats.PostsExecuted;
		pStats->TimersAccepted += WorkerStats.TimersAccepted;
		pStats->TimersRejected += WorkerStats.TimersRejected;
		pStats->TimersFired += WorkerStats.TimersFired;
		pStats->TimersCancelled += WorkerStats.TimersCancelled;
		pStats->TimersClosed += WorkerStats.TimersClosed;
		pStats->TimerErrors += WorkerStats.TimerErrors;
		pStats->Events += WorkerStats.Events;
		pStats->WaitErrors += WorkerStats.WaitErrors;
		pStats->WakeErrors += WorkerStats.WakeErrors;
		pStats->ShutdownStalls += WorkerStats.ShutdownStalls;
		pStats->NodeCacheHits += WorkerStats.NodeCacheHits;
		pStats->NodeCacheMisses += WorkerStats.NodeCacheMisses;
		pStats->PendingCommands += WorkerStats.PendingCommands;
		pStats->ActiveTimers += WorkerStats.ActiveTimers;
		pStats->NodeCachedBytes += WorkerStats.NodeCachedBytes;
	}
	pStats->LiveObjects = (size_t)xrtAtomic32Load(
		&pEngine->LiveObjects,
		XMEMORY_ACQUIRE
	);
	return true;
}



/* 为扩展组合对象占用一个正在运行的 Engine 生命周期。 */
XRT_API bool xrtNetEnginePin(xnetengine* pEngine)
{
	return __xrtNetEngineObjectHold(pEngine);
}



/* 安全释放一份由公开 Pin 取得的 Engine 生命周期占用。 */
XRT_API bool xrtNetEngineUnpin(xnetengine* pEngine)
{
	uint32 iObjects;

	if ( pEngine == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iObjects = xrtAtomic32Load(
		&pEngine->LiveObjects,
		XMEMORY_ACQUIRE
	);
	for ( ;; ) {
		uint32 iExpected = iObjects;

		if ( iObjects == 0 ) {
			__xrtErrorSetInvalidState();
			return false;
		}
		if ( xrtAtomic32CompareExchange(
			&pEngine->LiveObjects,
			&iExpected,
			iObjects - 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			return true;
		}
		iObjects = iExpected;
	}
}



/* 高层网络对象占用 Engine 生命周期。 */
bool __xrtNetEngineObjectHold(xnetengine* pEngine)
{
	uint32 iObjects;

	if ( pEngine == NULL ) {
		return false;
	}
	if ( !xrtMutexLock(&pEngine->Lifecycle) ) {
		return false;
	}
	if ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
		(void)xrtMutexUnlock(&pEngine->Lifecycle);
		__xrtNetEngineError(
			XERR_CLOSED,
			XNET_ERROR_ENGINE_POST,
			"hold-engine",
			"network engine is not running"
		);
		return false;
	}
	iObjects = xrtAtomic32Load(&pEngine->LiveObjects, XMEMORY_ACQUIRE);
	for ( ;; ) {
		uint32 iExpected = iObjects;

		if ( iObjects == UINT32_MAX ) {
			(void)xrtMutexUnlock(&pEngine->Lifecycle);
			__xrtNetEngineError(
				XERR_RANGE,
				XNET_ERROR_ENGINE_POST,
				"hold-engine",
				"network engine live object limit reached"
			);
			return false;
		}
		if ( xrtAtomic32CompareExchange(
			&pEngine->LiveObjects,
			&iExpected,
			iObjects + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		iObjects = iExpected;
	}
	(void)xrtMutexUnlock(&pEngine->Lifecycle);
	return true;
}



/* 释放高层网络对象对 Engine 的生命周期占用。 */
void __xrtNetEngineObjectRelease(xnetengine* pEngine)
{
	if ( pEngine != NULL ) {
		(void)xrtAtomic32FetchSub(
			&pEngine->LiveObjects,
			1,
			XMEMORY_ACQ_REL
		);
	}
}

#endif
