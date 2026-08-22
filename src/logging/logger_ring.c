#include "../internal/xrt_internal.h"
#include "../internal/xrt_logger.h"
#include <xrt/logger.h>
#include <xrt/thread.h>



#if defined(XRT_FEATURE_LOGGER_RING)

#define XLOG_RING_GATE_CLOSED UINT32_C(0x80000000)
#define XLOG_RING_GATE_WRITERS UINT32_C(0x7fffffff)



/* 队列项首字段区分记录槽和唯一的预分配刷新栅栏。 */
typedef enum xlogringitemkind {
	XLOG_RING_ITEM_RECORD = 0,
	XLOG_RING_ITEM_FLUSH
} xlogringitemkind;



/* 联合体保证紧随其后的拥有型记录满足指针、整数和浮点自然对齐。 */
typedef union xlogringalign {
	ptr Pointer;
	uint64 Integer;
	double Float;
} xlogringalign;



/* 每个记录槽在创建时固定分配，热路径只覆盖其中的连续存储。 */
typedef struct xlogringslot {
	xlogringitemkind Kind;
	bool Owned;
	size_t Size;
	xlogringalign Align;
	uint8 Data[];
} xlogringslot;



/* Flush 栅栏由管理锁串行复用，不为每次刷新建立动态节点。 */
typedef struct xlogringflush {
	xlogringitemkind Kind;
	xatomic64 Ticket;
	xatomic64 Done;
	bool Result;
	xerror* Error;
} xlogringflush;



/* Ring 状态把生产者原子字段与冷路径管理锁明确分开。 */
typedef struct xlogringstate {
	volatile int32 RefCount;
	xlogringconfig Config;
	xlogsink* Target;
	xthread* Thread;
	xmpscqueue Ready;
	xmpmcqueue Free;
	ptr Storage;
	size_t Capacity;
	size_t Stride;
	xatomic32 Gate;
	xatomic64 WorkerId;
	xatomic64 Enqueued;
	xatomic64 Processed;
	xatomic64 Written;
	xatomic64 Skipped;
	xatomic64 TargetDropped;
	xatomic64 Dropped;
	xatomic64 Oversized;
	xatomic64 ReentrantDrops;
	xatomic64 Failed;
	xatomic64 Flushes;
	xatomic64 Queued;
	xatomic64 QueueBytes;
	xatomic64 PeakQueued;
	xatomic64 PeakBytes;
	xmutex ManageLock;
	xmutex ErrorLock;
	xerror* LastError;
	xlogringflush Flush;
} xlogringstate;



/* 最后一个状态引用释放目标、队列、固定槽和冷路径同步对象。 */
static void __xrtLogRingStateRelease(xlogringstate* pState);



/* 读取一个记录槽内自然对齐的拥有型记录。 */
static xlogrecord* __xrtLogRingSlotRecord(xlogringslot* pSlot)
{
	return (xlogrecord*)pSlot->Data;
}



/* 计算固定槽跨度，并检查所有加法和乘法边界。 */
static bool __xrtLogRingLayout(
	size_t iCapacity,
	size_t iRecordLimit,
	size_t* pStride,
	size_t* pTotal
)
{
	size_t iAlign = sizeof(xlogringalign);
	size_t iBase = offsetof(xlogringslot, Data);
	size_t iRaw;
	size_t iStride;

	if ( iRecordLimit > (SIZE_MAX - iBase) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRaw = iBase + iRecordLimit;
	if ( iRaw > (SIZE_MAX - (iAlign - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iStride = ((iRaw + iAlign - 1u) / iAlign) * iAlign;
	if ( iCapacity > (SIZE_MAX / iStride) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pStride = iStride;
	*pTotal = iCapacity * iStride;
	return true;
}



/* 返回固定存储中的指定记录槽。 */
static xlogringslot* __xrtLogRingSlot(
	xlogringstate* pState,
	size_t iIndex
)
{
	return (xlogringslot*)((uint8*)pState->Storage + (iIndex * pState->Stride));
}



/* 尝试进入生产者区；关闭位一旦发布，后续生产者立即失败。 */
static bool __xrtLogRingWriterEnter(xlogringstate* pState)
{
	uint32 iGate = xrtAtomic32Load(&pState->Gate, XMEMORY_ACQUIRE);

	for ( ;; ) {
		if ( (iGate & XLOG_RING_GATE_CLOSED) != 0u ) {
			return false;
		}
		if ( (iGate & XLOG_RING_GATE_WRITERS) == XLOG_RING_GATE_WRITERS ) {
			return false;
		}
		if (
			xrtAtomic32CompareExchange(
				&pState->Gate,
				&iGate,
				iGate + 1u,
				XMEMORY_ACQUIRE,
				XMEMORY_RELAXED
			)
		) {
			return true;
		}
	}
}



/* 离开生产者区，使关闭线程能够观察所有已进入写入都已完成。 */
static void __xrtLogRingWriterLeave(xlogringstate* pState)
{
	(void)xrtAtomic32FetchSub(&pState->Gate, 1u, XMEMORY_RELEASE);
}



/* 发布关闭位并等待已经进入热路径的生产者完成。 */
static void __xrtLogRingClose(xlogringstate* pState)
{
	uint32 iGate = xrtAtomic32FetchOr(
		&pState->Gate,
		XLOG_RING_GATE_CLOSED,
		XMEMORY_ACQ_REL
	);

	if ( (iGate & XLOG_RING_GATE_CLOSED) != 0u ) {
		return;
	}
	while (
		(xrtAtomic32Load(&pState->Gate, XMEMORY_ACQUIRE) &
		 XLOG_RING_GATE_WRITERS) != 0u
	) {
		xrtThreadYield();
	}
	xrtMPSCQueueClose(&pState->Ready);
}



/* 原子更新单调峰值，失败时复用比较交换写回的最新值。 */
static void __xrtLogRingPeak(xatomic64* pPeak, uint64 iValue)
{
	uint64 iPeak = xrtAtomic64Load(pPeak, XMEMORY_RELAXED);

	while ( iValue > iPeak ) {
		if (
			xrtAtomic64CompareExchange(
				pPeak,
				&iPeak,
				iValue,
				XMEMORY_RELAXED,
				XMEMORY_RELAXED
			)
		) {
			return;
		}
	}
}



/* 保存后台最近一次错误；生产者不访问此锁。 */
static void __xrtLogRingStoreError(
	xlogringstate* pState,
	const xerror* pError
)
{
	xerror* pNew = xrtErrorRef(pError);
	xerror* pOld;

	if ( (pError != NULL) && (pNew == NULL) ) {
		return;
	}
	(void)xrtMutexLock(&pState->ErrorLock);
	pOld = pState->LastError;
	pState->LastError = pNew;
	(void)xrtMutexUnlock(&pState->ErrorLock);
	xrtErrorFree(pOld);
}



/* 把目标 Sink 的记录结果计入无锁统计并保存目标错误。 */
static void __xrtLogRingProcessRecord(
	xlogringstate* pState,
	xlogringslot* pSlot
)
{
	xlogresult Result;
	xerror* pError = NULL;
	uint64 iSize = (uint64)pSlot->Size;

	xrtClearError();
	Result = xrtLogSinkSubmit(pState->Target, __xrtLogRingSlotRecord(pSlot));
	if ( Result == XLOG_RESULT_WRITTEN ) {
		(void)xrtAtomic64FetchAdd(&pState->Written, 1u, XMEMORY_RELAXED);
	} else if ( Result == XLOG_RESULT_SKIPPED ) {
		(void)xrtAtomic64FetchAdd(&pState->Skipped, 1u, XMEMORY_RELAXED);
	} else if ( Result == XLOG_RESULT_DROPPED ) {
		(void)xrtAtomic64FetchAdd(
			&pState->TargetDropped,
			1u,
			XMEMORY_RELAXED
		);
	} else {
		pError = __xrtLogErrorCreate(
			XERR_IO,
			XLOG_ERROR_RING_TARGET,
			"ring-target",
			"ring log target rejected a record",
			xrtTakeError()
		);
		__xrtLogRingStoreError(pState, pError);
		xrtErrorFree(pError);
		(void)xrtAtomic64FetchAdd(&pState->Failed, 1u, XMEMORY_RELAXED);
	}

	__xrtLogOwnedClear(__xrtLogRingSlotRecord(pSlot));
	pSlot->Owned = false;
	pSlot->Size = 0u;
	(void)xrtAtomic64FetchAdd(&pState->Processed, 1u, XMEMORY_RELEASE);
	(void)xrtAtomic64FetchSub(&pState->Queued, 1u, XMEMORY_RELAXED);
	(void)xrtAtomic64FetchSub(&pState->QueueBytes, iSize, XMEMORY_RELAXED);
	if ( xrtMPMCQueueTryPush(&pState->Free, pSlot) != XQUEUE_OK ) {
		pError = __xrtLogErrorCreate(
			XERR_STATE,
			XLOG_ERROR_RING_QUEUE,
			"ring-worker",
			"failed to return a ring log record slot",
			xrtTakeError()
		);
		__xrtLogRingStoreError(pState, pError);
		xrtErrorFree(pError);
		(void)xrtAtomic64FetchAdd(&pState->Failed, 1u, XMEMORY_RELAXED);
	}
}



/* 在工作线程上执行精确 FIFO Flush 栅栏并发布结果。 */
static void __xrtLogRingProcessFlush(xlogringstate* pState)
{
	xerror* pError = NULL;
	uint64 iTicket = xrtAtomic64Load(&pState->Flush.Ticket, XMEMORY_ACQUIRE);

	xrtClearError();
	pState->Flush.Result = xrtLogSinkFlush(pState->Target);
	if ( !pState->Flush.Result ) {
		pError = __xrtLogErrorCreate(
			XERR_IO,
			XLOG_ERROR_RING_FLUSH,
			"ring-flush",
			"ring log target flush failed",
			xrtTakeError()
		);
		pState->Flush.Error = xrtErrorRef(pError);
		__xrtLogRingStoreError(pState, pError);
		xrtErrorFree(pError);
		(void)xrtAtomic64FetchAdd(&pState->Failed, 1u, XMEMORY_RELAXED);
	}
	(void)xrtAtomic64FetchAdd(&pState->Flushes, 1u, XMEMORY_RELAXED);
	xrtAtomic64Store(&pState->Flush.Done, iTicket, XMEMORY_RELEASE);
}



/* 工作线程批量提取队列项，顺序消费记录并处理刷新栅栏。 */
static int32 __xrtLogRingWorker(ptr pData)
{
	xlogringstate* pState = (xlogringstate*)pData;
	ptr Items[XLOG_RING_BATCH_MAX];

	xrtAtomic64Store(
		&pState->WorkerId,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	for ( ;; ) {
		xqueuebatchresult Batch = xrtMPSCQueuePopBatch(
			&pState->Ready,
			Items,
			pState->Config.Batch
		);

		if ( Batch.Count != 0u ) {
			for ( size_t i = 0; i < Batch.Count; i++ ) {
				xlogringitemkind Kind = *(xlogringitemkind*)Items[i];

				if ( Kind == XLOG_RING_ITEM_RECORD ) {
					__xrtLogRingProcessRecord(
						pState,
						(xlogringslot*)Items[i]
					);
				} else {
					__xrtLogRingProcessFlush(pState);
				}
			}
			continue;
		}
		if ( Batch.Result == XQUEUE_CLOSED ) {
			break;
		}
		if ( Batch.Result == XQUEUE_ERROR ) {
			xerror* pError = __xrtLogErrorCreate(
				XERR_STATE,
				XLOG_ERROR_RING_QUEUE,
				"ring-worker",
				"ring log ready queue failed",
				xrtTakeError()
			);

			__xrtLogRingStoreError(pState, pError);
			xrtErrorFree(pError);
			(void)xrtAtomic64FetchAdd(&pState->Failed, 1u, XMEMORY_RELAXED);
			break;
		}
		if ( pState->Config.IdleWait == 0u ) {
			xrtThreadYield();
		} else {
			xrtSleepUs(pState->Config.IdleWait);
		}
	}

	xrtClearError();
	if ( xrtLogSinkFlush(pState->Target) ) {
		(void)xrtAtomic64FetchAdd(&pState->Flushes, 1u, XMEMORY_RELAXED);
	} else {
		xerror* pError = __xrtLogErrorCreate(
			XERR_IO,
			XLOG_ERROR_RING_FLUSH,
			"ring-shutdown",
			"ring log target shutdown flush failed",
			xrtTakeError()
		);

		__xrtLogRingStoreError(pState, pError);
		xrtErrorFree(pError);
		(void)xrtAtomic64FetchAdd(&pState->Flushes, 1u, XMEMORY_RELAXED);
		(void)xrtAtomic64FetchAdd(&pState->Failed, 1u, XMEMORY_RELAXED);
	}
	__xrtLogRingStateRelease(pState);
	return 0;
}



/* 释放状态；正常路径此时队列已经排空，扫描仍兜底释放错误字段引用。 */
static void __xrtLogRingStateRelease(xlogringstate* pState)
{
	if ( xrtRefRelease(&pState->RefCount) != 0 ) {
		return;
	}
	for ( size_t i = 0; i < pState->Capacity; i++ ) {
		xlogringslot* pSlot = __xrtLogRingSlot(pState, i);

		if ( pSlot->Owned ) {
			__xrtLogOwnedClear(__xrtLogRingSlotRecord(pSlot));
		}
	}
	xrtErrorFree(pState->Flush.Error);
	xrtErrorFree(pState->LastError);
	xrtLogSinkFree(pState->Target);
	xrtMPMCQueueUnit(&pState->Free);
	xrtMPSCQueueUnit(&pState->Ready);
	(void)xrtMutexUnit(&pState->ErrorLock);
	(void)xrtMutexUnit(&pState->ManageLock);
	xrtFree(pState->Storage);
	xrtFree(pState);
}



/* 热路径深拷贝到固定槽，并通过 MPSC 队列发布给唯一消费者。 */
static xlogresult __xrtLogRingWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	xlogringstate* pState = (xlogringstate*)pUserData;
	xlogringslot* pSlot = NULL;
	xqueueresult Queue;
	size_t iSize;
	uint64 iQueued;
	uint64 iBytes;
	uint64 iWorker = xrtAtomic64Load(&pState->WorkerId, XMEMORY_ACQUIRE);

	if ( (iWorker != 0u) && (iWorker == xrtThreadCurrentId()) ) {
		(void)xrtAtomic64FetchAdd(
			&pState->ReentrantDrops,
			1u,
			XMEMORY_RELAXED
		);
		return XLOG_RESULT_DROPPED;
	}
	if ( !__xrtLogRingWriterEnter(pState) ) {
		return XLOG_RESULT_DROPPED;
	}
	if ( !__xrtLogOwnedSize(pRecord, &iSize) ) {
		xrtClearError();
		(void)xrtAtomic64FetchAdd(&pState->Oversized, 1u, XMEMORY_RELAXED);
		__xrtLogRingWriterLeave(pState);
		return XLOG_RESULT_DROPPED;
	}
	if ( iSize > pState->Config.RecordLimit ) {
		(void)xrtAtomic64FetchAdd(&pState->Oversized, 1u, XMEMORY_RELAXED);
		__xrtLogRingWriterLeave(pState);
		return XLOG_RESULT_DROPPED;
	}

	Queue = xrtMPMCQueueTryPop(&pState->Free, (ptr*)&pSlot);
	if ( Queue != XQUEUE_OK ) {
		if ( Queue == XQUEUE_ERROR ) {
			__xrtLogRingWriterLeave(pState);
			return XLOG_RESULT_ERROR;
		}
		(void)xrtAtomic64FetchAdd(&pState->Dropped, 1u, XMEMORY_RELAXED);
		__xrtLogRingWriterLeave(pState);
		return XLOG_RESULT_DROPPED;
	}
	if (
		!__xrtLogOwnedCopy(
			pRecord,
			__xrtLogRingSlotRecord(pSlot),
			pState->Config.RecordLimit
		)
	) {
		(void)xrtMPMCQueueTryPush(&pState->Free, pSlot);
		__xrtLogRingWriterLeave(pState);
		return XLOG_RESULT_ERROR;
	}
	pSlot->Owned = true;
	pSlot->Size = iSize;
	(void)xrtAtomic64FetchAdd(&pState->Enqueued, 1u, XMEMORY_RELAXED);
	iQueued = xrtAtomic64FetchAdd(&pState->Queued, 1u, XMEMORY_RELAXED) + 1u;
	iBytes = xrtAtomic64FetchAdd(
		&pState->QueueBytes,
		(uint64)iSize,
		XMEMORY_RELAXED
	) + (uint64)iSize;
	Queue = xrtMPSCQueueTryPush(&pState->Ready, pSlot);
	if ( Queue != XQUEUE_OK ) {
		(void)xrtAtomic64FetchSub(&pState->Enqueued, 1u, XMEMORY_RELAXED);
		(void)xrtAtomic64FetchSub(&pState->Queued, 1u, XMEMORY_RELAXED);
		(void)xrtAtomic64FetchSub(
			&pState->QueueBytes,
			(uint64)iSize,
			XMEMORY_RELAXED
		);
		__xrtLogOwnedClear(__xrtLogRingSlotRecord(pSlot));
		pSlot->Owned = false;
		pSlot->Size = 0u;
		(void)xrtMPMCQueueTryPush(&pState->Free, pSlot);
		if ( Queue == XQUEUE_ERROR ) {
			__xrtLogRingWriterLeave(pState);
			return XLOG_RESULT_ERROR;
		}
		(void)xrtAtomic64FetchAdd(&pState->Dropped, 1u, XMEMORY_RELAXED);
		__xrtLogRingWriterLeave(pState);
		return XLOG_RESULT_DROPPED;
	}

	__xrtLogRingPeak(&pState->PeakQueued, iQueued);
	__xrtLogRingPeak(&pState->PeakBytes, iBytes);
	__xrtLogRingWriterLeave(pState);
	return XLOG_RESULT_WRITTEN;
}



/* 关闭入口并与工作线程完成生命周期交接。 */
static void __xrtLogRingDrop(ptr pUserData)
{
	xlogringstate* pState = (xlogringstate*)pUserData;
	xthread* pThread = pState->Thread;
	uint64 iWorker = xrtAtomic64Load(&pState->WorkerId, XMEMORY_ACQUIRE);

	__xrtLogRingClose(pState);
	if (
		(pThread != NULL) &&
		((iWorker == 0u) || (iWorker != xrtThreadCurrentId()))
	) {
		(void)xrtThreadWait(pThread);
	}
	if ( pThread != NULL ) {
		xrtThreadDestroy(pThread);
	}
	pState->Thread = NULL;
	__xrtLogRingStateRelease(pState);
}



/* 把预分配 Flush 栅栏排到此前记录之后并轮询等待完成。 */
static bool __xrtLogRingFlush(ptr pUserData)
{
	xlogringstate* pState = (xlogringstate*)pUserData;
	xerror* pError;
	xqueueresult Queue;
	uint64 iTicket;
	uint64 iWorker = xrtAtomic64Load(&pState->WorkerId, XMEMORY_ACQUIRE);

	if ( (iWorker != 0u) && (iWorker == xrtThreadCurrentId()) ) {
		__xrtLogErrorSet(
			XERR_STATE,
			XLOG_ERROR_RING_FLUSH,
			"ring-flush",
			"ring log target cannot flush its own queue"
		);
		return false;
	}
	(void)xrtMutexLock(&pState->ManageLock);
	if (
		(xrtAtomic32Load(&pState->Gate, XMEMORY_ACQUIRE) &
		 XLOG_RING_GATE_CLOSED) != 0u
	) {
		(void)xrtMutexUnlock(&pState->ManageLock);
		__xrtLogErrorSet(
			XERR_CLOSED,
			XLOG_ERROR_RING_CLOSED,
			"ring-flush",
			"ring log sink is closed"
		);
		return false;
	}

	xrtErrorFree(pState->Flush.Error);
	pState->Flush.Error = NULL;
	pState->Flush.Result = false;
	iTicket = xrtAtomic64Load(&pState->Flush.Ticket, XMEMORY_RELAXED) + 1u;
	xrtAtomic64Store(&pState->Flush.Ticket, iTicket, XMEMORY_RELEASE);
	for ( ;; ) {
		Queue = xrtMPSCQueueTryPush(&pState->Ready, &pState->Flush);
		if ( Queue == XQUEUE_OK ) {
			break;
		}
		if ( Queue != XQUEUE_FULL ) {
			(void)xrtMutexUnlock(&pState->ManageLock);
			__xrtLogErrorSet(
				Queue == XQUEUE_CLOSED ? XERR_CLOSED : XERR_STATE,
				Queue == XQUEUE_CLOSED
					? XLOG_ERROR_RING_CLOSED
					: XLOG_ERROR_RING_QUEUE,
				"ring-flush",
				"failed to enqueue ring log flush barrier"
			);
			return false;
		}
		xrtThreadYield();
	}
	while (
		xrtAtomic64Load(&pState->Flush.Done, XMEMORY_ACQUIRE) != iTicket
	) {
		if ( pState->Config.IdleWait == 0u ) {
			xrtThreadYield();
		} else {
			xrtSleepUs(pState->Config.IdleWait);
		}
	}

	if ( pState->Flush.Result ) {
		(void)xrtMutexUnlock(&pState->ManageLock);
		return true;
	}
	pError = xrtErrorRef(pState->Flush.Error);
	(void)xrtMutexUnlock(&pState->ManageLock);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else {
		__xrtLogErrorSet(
			XERR_IO,
			XLOG_ERROR_RING_FLUSH,
			"ring-flush",
			"ring log flush failed"
		);
	}
	return false;
}



/* 校验 Ring 配置边界和借用视图。 */
static bool __xrtLogRingConfigValid(const xlogringconfig* pConfig)
{
	return
		(pConfig != NULL) &&
		((pConfig->Name.Data != NULL) || (pConfig->Name.Size == 0u)) &&
		(pConfig->Level >= XLOG_TRACE) &&
		(pConfig->Level <= XLOG_OFF) &&
		(pConfig->Capacity != 0u) &&
		(pConfig->Capacity <= XRT_QUEUE_MAX_CAPACITY) &&
		(pConfig->RecordLimit >= sizeof(xlogrecord)) &&
		(pConfig->Batch != 0u) &&
		(pConfig->Batch <= XLOG_RING_BATCH_MAX);
}



/* 初始化固定容量、固定记录上限和批量消费的默认配置。 */
XRT_API bool xrtLogRingConfigInit(xlogringconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pConfig, 0, sizeof(xlogringconfig));
	pConfig->Level = XLOG_TRACE;
	pConfig->Capacity = XLOG_RING_CAPACITY_DEFAULT;
	pConfig->RecordLimit = XLOG_RING_RECORD_LIMIT_DEFAULT;
	pConfig->Batch = XLOG_RING_BATCH_DEFAULT;
	pConfig->IdleWait = XLOG_RING_IDLE_WAIT_DEFAULT;
	return true;
}



/* 初始化全部原子统计和唯一刷新栅栏。 */
static void __xrtLogRingAtomicsInit(xlogringstate* pState)
{
	xrtAtomic32Init(&pState->Gate, 0u);
	xrtAtomic64Init(&pState->WorkerId, 0u);
	xrtAtomic64Init(&pState->Enqueued, 0u);
	xrtAtomic64Init(&pState->Processed, 0u);
	xrtAtomic64Init(&pState->Written, 0u);
	xrtAtomic64Init(&pState->Skipped, 0u);
	xrtAtomic64Init(&pState->TargetDropped, 0u);
	xrtAtomic64Init(&pState->Dropped, 0u);
	xrtAtomic64Init(&pState->Oversized, 0u);
	xrtAtomic64Init(&pState->ReentrantDrops, 0u);
	xrtAtomic64Init(&pState->Failed, 0u);
	xrtAtomic64Init(&pState->Flushes, 0u);
	xrtAtomic64Init(&pState->Queued, 0u);
	xrtAtomic64Init(&pState->QueueBytes, 0u);
	xrtAtomic64Init(&pState->PeakQueued, 0u);
	xrtAtomic64Init(&pState->PeakBytes, 0u);
	pState->Flush.Kind = XLOG_RING_ITEM_FLUSH;
	xrtAtomic64Init(&pState->Flush.Ticket, 0u);
	xrtAtomic64Init(&pState->Flush.Done, 0u);
}



/* 创建固定槽 Ring，并在所有资源就绪后启动唯一消费者线程。 */
XRT_API xlogsink* xrtLogRing(
	xlogsink* pTarget,
	const xlogringconfig* pConfig
)
{
	xlogringconfig Default;
	xlogringstate* pState;
	xlogsinkconfig SinkConfig;
	xlogsink* pSink;
	xstrview Name;
	xerror* pCause;
	size_t iTotal;

	if ( pConfig == NULL ) {
		(void)xrtLogRingConfigInit(&Default);
		pConfig = &Default;
	}
	if ( (pTarget == NULL) || !__xrtLogRingConfigValid(pConfig) ) {
		xrtClearError();
		__xrtLogErrorSet(
			XERR_ARGUMENT,
			XLOG_ERROR_RING_CONFIG,
			"ring-create",
			"invalid ring log sink configuration"
		);
		return NULL;
	}

	pState = (xlogringstate*)xrtCalloc(1u, sizeof(xlogringstate));
	if ( pState == NULL ) {
		return NULL;
	}
	pState->RefCount = 1;
	pState->Config = *pConfig;
	pState->Capacity = xrtQueueCapacity(pConfig->Capacity);
	if (
		(pState->Capacity == 0u) ||
		!__xrtLogRingLayout(
			pState->Capacity,
			pConfig->RecordLimit,
			&pState->Stride,
			&iTotal
		)
	) {
		xrtFree(pState);
		return NULL;
	}
	pState->Config.Capacity = pState->Capacity;
	__xrtLogRingAtomicsInit(pState);
	pState->Target = xrtLogSinkRef(pTarget);
	if ( pState->Target == NULL ) {
		xrtFree(pState);
		return NULL;
	}
	if ( !xrtMutexInit(&pState->ManageLock) ) {
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		return NULL;
	}
	if ( !xrtMutexInit(&pState->ErrorLock) ) {
		(void)xrtMutexUnit(&pState->ManageLock);
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		return NULL;
	}
	if ( !xrtMPSCQueueInit(&pState->Ready, pState->Capacity) ) {
		(void)xrtMutexUnit(&pState->ErrorLock);
		(void)xrtMutexUnit(&pState->ManageLock);
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		return NULL;
	}
	if ( !xrtMPMCQueueInit(&pState->Free, pState->Capacity) ) {
		xrtMPSCQueueUnit(&pState->Ready);
		(void)xrtMutexUnit(&pState->ErrorLock);
		(void)xrtMutexUnit(&pState->ManageLock);
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		return NULL;
	}
	pState->Storage = xrtCalloc(1u, iTotal);
	if ( pState->Storage == NULL ) {
		xrtMPMCQueueUnit(&pState->Free);
		xrtMPSCQueueUnit(&pState->Ready);
		(void)xrtMutexUnit(&pState->ErrorLock);
		(void)xrtMutexUnit(&pState->ManageLock);
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		return NULL;
	}
	for ( size_t i = 0; i < pState->Capacity; i++ ) {
		xlogringslot* pSlot = __xrtLogRingSlot(pState, i);

		pSlot->Kind = XLOG_RING_ITEM_RECORD;
		if ( xrtMPMCQueueTryPush(&pState->Free, pSlot) != XQUEUE_OK ) {
			xrtFree(pState->Storage);
			xrtMPMCQueueUnit(&pState->Free);
			xrtMPSCQueueUnit(&pState->Ready);
			(void)xrtMutexUnit(&pState->ErrorLock);
			(void)xrtMutexUnit(&pState->ManageLock);
			xrtLogSinkFree(pState->Target);
			xrtFree(pState);
			return NULL;
		}
	}

	(void)xrtRefRetain(&pState->RefCount);
	pState->Thread = xrtThreadCreate(
		__xrtLogRingWorker,
		pState,
		pConfig->StackSize
	);
	if ( pState->Thread == NULL ) {
		__xrtLogRingStateRelease(pState);
		__xrtLogRingStateRelease(pState);
		return NULL;
	}

	Name = pConfig->Name.Size == 0u
		? xrtLogSinkName(pTarget)
		: pConfig->Name;
	memset(&SinkConfig, 0, sizeof(SinkConfig));
	SinkConfig.Name = Name;
	SinkConfig.Level = pConfig->Level;
	SinkConfig.Write = __xrtLogRingWrite;
	SinkConfig.Flush = __xrtLogRingFlush;
	SinkConfig.Drop = __xrtLogRingDrop;
	SinkConfig.UserData = pState;
	pSink = xrtLogSinkCreate(&SinkConfig);
	if ( pSink != NULL ) {
		return pSink;
	}

	pCause = xrtTakeError();
	__xrtLogRingClose(pState);
	(void)xrtThreadWait(pState->Thread);
	xrtThreadDestroy(pState->Thread);
	pState->Thread = NULL;
	__xrtLogRingStateRelease(pState);
	__xrtErrorSetOwned(__xrtLogErrorCreate(
		XERR_MEMORY,
		XLOG_ERROR_RING_CONFIG,
		"ring-create",
		"failed to create ring log sink",
		pCause
	));
	return NULL;
}



/* 创建 Ring 包装器并把调用方引用交给 Logger。 */
XRT_API bool xrtLogAddRing(
	xlogger* pLogger,
	xlogsink* pTarget,
	const xlogringconfig* pConfig
)
{
	xlogsink* pSink;
	bool bResult;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSink = xrtLogRing(pTarget, pConfig);
	if ( pSink == NULL ) {
		return false;
	}
	bResult = xrtLogAttach(pLogger, pSink);
	xrtLogSinkFree(pSink);
	return bResult;
}



/* 验证 Ring Sink 身份并返回内部状态。 */
static xlogringstate* __xrtLogRingState(const xlogsink* pSink)
{
	ptr pData = NULL;

	if ( !__xrtLogSinkData(pSink, __xrtLogRingWrite, &pData) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return (xlogringstate*)pData;
}



/* 返回 Ring 借用的目标 Sink。 */
XRT_API xlogsink* xrtLogRingTarget(const xlogsink* pSink)
{
	xlogringstate* pState = __xrtLogRingState(pSink);

	return pState == NULL ? NULL : pState->Target;
}



/* 读取所有原子计数的无锁近似快照。 */
XRT_API bool xrtLogRingStats(
	const xlogsink* pSink,
	xlogringstats* pStats
)
{
	xlogringstate* pState = __xrtLogRingState(pSink);

	if ( (pState == NULL) || (pStats == NULL) ) {
		if ( pStats == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pStats->Enqueued = xrtAtomic64Load(&pState->Enqueued, XMEMORY_RELAXED);
	pStats->Processed = xrtAtomic64Load(&pState->Processed, XMEMORY_RELAXED);
	pStats->Written = xrtAtomic64Load(&pState->Written, XMEMORY_RELAXED);
	pStats->Skipped = xrtAtomic64Load(&pState->Skipped, XMEMORY_RELAXED);
	pStats->TargetDropped = xrtAtomic64Load(
		&pState->TargetDropped,
		XMEMORY_RELAXED
	);
	pStats->Dropped = xrtAtomic64Load(&pState->Dropped, XMEMORY_RELAXED);
	pStats->Oversized = xrtAtomic64Load(&pState->Oversized, XMEMORY_RELAXED);
	pStats->ReentrantDrops = xrtAtomic64Load(
		&pState->ReentrantDrops,
		XMEMORY_RELAXED
	);
	pStats->Failed = xrtAtomic64Load(&pState->Failed, XMEMORY_RELAXED);
	pStats->Flushes = xrtAtomic64Load(&pState->Flushes, XMEMORY_RELAXED);
	pStats->Queued = (size_t)xrtAtomic64Load(&pState->Queued, XMEMORY_RELAXED);
	pStats->QueueBytes = (size_t)xrtAtomic64Load(
		&pState->QueueBytes,
		XMEMORY_RELAXED
	);
	pStats->PeakQueued = (size_t)xrtAtomic64Load(
		&pState->PeakQueued,
		XMEMORY_RELAXED
	);
	pStats->PeakBytes = (size_t)xrtAtomic64Load(
		&pState->PeakBytes,
		XMEMORY_RELAXED
	);
	return true;
}



/* 返回后台最近一次错误的新引用。 */
XRT_API xerror* xrtLogRingLastError(const xlogsink* pSink)
{
	xlogringstate* pState = __xrtLogRingState(pSink);
	xerror* pError;

	if ( pState == NULL ) {
		return NULL;
	}
	(void)xrtMutexLock(&pState->ErrorLock);
	pError = xrtErrorRef(pState->LastError);
	(void)xrtMutexUnlock(&pState->ErrorLock);
	return pError;
}

#endif
