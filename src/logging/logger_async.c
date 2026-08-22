#include "../internal/xrt_internal.h"
#include "../internal/xrt_logger.h"
#include <xrt/logger.h>
#include <xrt/thread.h>



#if defined(XRT_FEATURE_LOGGER_ASYNC)

/* 队列节点只保存链表和动态字节开销，具体数据由记录或栅栏节点扩展。 */
typedef enum xlogasyncitemkind {
	XLOG_ASYNC_ITEM_RECORD = 0,
	XLOG_ASYNC_ITEM_FLUSH
} xlogasyncitemkind;



/* 公共队列头允许记录和 Flush 栅栏共享同一条严格 FIFO 链。 */
typedef struct xlogasyncitem {
	struct xlogasyncitem* Next;
	xlogasyncitemkind Kind;
	size_t Bytes;
} xlogasyncitem;



/* 一次分配同时保存完整记录、字段数组和全部字符串副本。 */
typedef struct xlogasyncrecord {
	xlogasyncitem Item;
	xlogrecord Record;
	xlogfield Fields[];
} xlogasyncrecord;



/* Flush 栅栏由调用线程和工作线程各持有一个引用。 */
typedef struct xlogasyncflush {
	xlogasyncitem Item;
	volatile int32 RefCount;
	xevent Done;
	bool Result;
	xerror* Error;
} xlogasyncflush;



/* Async Sink 状态集中管理队列、工作线程、目标引用和观测数据。 */
typedef struct xlogasyncstate {
	volatile int32 RefCount;
	xmutex Lock;
	xcond NotEmpty;
	xcond NotFull;
	xatomic64 WorkerId;
	xlogasyncconfig Config;
	xlogsink* Target;
	xthread* Thread;
	xlogasyncitem* Head;
	xlogasyncitem* Tail;
	xerror* LastError;
	bool Closing;
	size_t Queued;
	size_t QueueBytes;
	size_t PeakQueued;
	size_t PeakBytes;
	uint64 Enqueued;
	uint64 Processed;
	uint64 Written;
	uint64 Skipped;
	uint64 DroppedNewest;
	uint64 DroppedOldest;
	uint64 DroppedTarget;
	uint64 ReentrantDrops;
	uint64 Discarded;
	uint64 Failed;
	uint64 Flushes;
} xlogasyncstate;



/* 状态引用可能由 Sink 析构线程或工作线程最后释放。 */
static void __xrtLogAsyncStateRelease(xlogasyncstate* pState);



/* 建立一个拥有下层原因引用的稳定异步错误。 */
static xerror* __xrtLogAsyncErrorCreate(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
)
{
	return __xrtLogErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 取走当前错误并把异步上下文设置回调用线程。 */
static void __xrtLogAsyncErrorSet(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtLogErrorSet(
		Kind,
		Code,
		sOperation,
		sMessage
	);
}



/* 在状态锁下替换后台最近一次错误，调用方仍保留传入引用。 */
static void __xrtLogAsyncStoreError(
	xlogasyncstate* pState,
	const xerror* pError
)
{
	xerror* pNew = xrtErrorRef(pError);
	xerror* pOld;

	if ( (pError != NULL) && (pNew == NULL) ) {
		return;
	}
	(void)xrtMutexLock(&pState->Lock);
	pOld = pState->LastError;
	pState->LastError = pNew;
	(void)xrtMutexUnlock(&pState->Lock);
	xrtErrorFree(pOld);
}



/* 计算异步队列节点和拥有型记录所需的唯一分配大小。 */
static bool __xrtLogAsyncRecordSize(
	const xlogrecord* pRecord,
	size_t* pSize
)
{
	size_t iOwned;

	if ( !__xrtLogOwnedSize(pRecord, &iOwned) ) {
		return false;
	}
	if ( iOwned > (SIZE_MAX - offsetof(xlogasyncrecord, Record)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize = offsetof(xlogasyncrecord, Record) + iOwned;
	return true;
}



/* 释放记录持有的错误引用和唯一动态分配。 */
static void __xrtLogAsyncRecordFree(xlogasyncrecord* pItem)
{
	__xrtLogOwnedClear(&pItem->Record);
	xrtFree(pItem);
}



/* 深拷贝借用记录，任何失败都不会留下部分拥有状态。 */
static xlogasyncrecord* __xrtLogAsyncRecordCopy(
	const xlogrecord* pRecord,
	size_t iSize
)
{
	xlogasyncrecord* pItem = (xlogasyncrecord*)xrtMalloc(iSize);

	if ( pItem == NULL ) {
		return NULL;
	}
	memset(pItem, 0, offsetof(xlogasyncrecord, Record));
	pItem->Item.Kind = XLOG_ASYNC_ITEM_RECORD;
	pItem->Item.Bytes = iSize;
	if ( !__xrtLogOwnedCopy(
		pRecord,
		&pItem->Record,
		iSize - offsetof(xlogasyncrecord, Record)
	) ) {
		xrtFree(pItem);
		return NULL;
	}
	return pItem;
}



/* 释放 Flush 节点引用，并由最后一个持有者回收事件。 */
static void __xrtLogAsyncFlushRelease(xlogasyncflush* pFlush)
{
	if ( xrtRefRelease(&pFlush->RefCount) != 0 ) {
		return;
	}
	xrtErrorFree(pFlush->Error);
	(void)xrtEventUnit(&pFlush->Done);
	xrtFree(pFlush);
}



/* 创建调用线程和工作线程共同拥有的 Flush 栅栏。 */
static xlogasyncflush* __xrtLogAsyncFlushCreate(void)
{
	xlogasyncflush* pFlush = (xlogasyncflush*)xrtMalloc(
		sizeof(xlogasyncflush)
	);

	if ( pFlush == NULL ) {
		return NULL;
	}
	memset(pFlush, 0, sizeof(xlogasyncflush));
	pFlush->Item.Kind = XLOG_ASYNC_ITEM_FLUSH;
	pFlush->RefCount = 2;
	if ( !xrtEventInit(&pFlush->Done, true, false) ) {
		xrtFree(pFlush);
		return NULL;
	}
	return pFlush;
}



/* 判断下一条记录是否同时满足记录数和真实字节上限。 */
static bool __xrtLogAsyncFits(
	const xlogasyncstate* pState,
	size_t iBytes
)
{
	return
		(pState->Queued < pState->Config.Capacity) &&
		(iBytes <= (pState->Config.ByteLimit - pState->QueueBytes));
}



/* 从队首移除一条记录，并同步队列统计。 */
static xlogasyncitem* __xrtLogAsyncPopLocked(xlogasyncstate* pState)
{
	xlogasyncitem* pItem = pState->Head;

	if ( pItem == NULL ) {
		return NULL;
	}
	pState->Head = pItem->Next;
	if ( pState->Head == NULL ) {
		pState->Tail = NULL;
	}
	pItem->Next = NULL;
	pState->Queued--;
	pState->QueueBytes -= pItem->Bytes;
	(void)xrtCondBroadcast(&pState->NotFull);
	return pItem;
}



/* 为一条尚未分配的记录预留槽位和真实字节预算。 */
static void __xrtLogAsyncReserveLocked(
	xlogasyncstate* pState,
	size_t iBytes
)
{
	pState->Queued++;
	pState->QueueBytes += iBytes;
	if ( pState->Queued > pState->PeakQueued ) {
		pState->PeakQueued = pState->Queued;
	}
	if ( pState->QueueBytes > pState->PeakBytes ) {
		pState->PeakBytes = pState->QueueBytes;
	}
}



/* 释放未能提交到队列的记录预算，并唤醒背压提交者。 */
static void __xrtLogAsyncUnreserve(
	xlogasyncstate* pState,
	size_t iBytes
)
{
	(void)xrtMutexLock(&pState->Lock);
	pState->Queued--;
	pState->QueueBytes -= iBytes;
	(void)xrtCondBroadcast(&pState->NotFull);
	(void)xrtMutexUnlock(&pState->Lock);
}



/* 把已经完整拥有且预留预算的节点追加到严格 FIFO 队尾。 */
static void __xrtLogAsyncPushLocked(
	xlogasyncstate* pState,
	xlogasyncitem* pItem
)
{
	pItem->Next = NULL;
	if ( pState->Tail == NULL ) {
		pState->Head = pItem;
	} else {
		pState->Tail->Next = pItem;
	}
	pState->Tail = pItem;
	(void)xrtCondSignal(&pState->NotEmpty);
}



/* 在任何记录分配前按满载策略取得严格有界的队列预算。 */
static xlogresult __xrtLogAsyncReserveRecord(
	xlogasyncstate* pState,
	size_t iBytes
)
{
	xlogasyncitem* pDropped;
	xwaitresult Wait;

	if ( !xrtMutexLock(&pState->Lock) ) {
		__xrtLogAsyncErrorSet(
			XERR_IO,
			XLOG_ERROR_ASYNC_QUEUE,
			"async-enqueue",
			"failed to lock the asynchronous log queue"
		);
		return XLOG_RESULT_ERROR;
	}
	while ( !__xrtLogAsyncFits(pState, iBytes) ) {
		if ( pState->Closing ) {
			(void)xrtMutexUnlock(&pState->Lock);
			__xrtLogAsyncErrorSet(
				XERR_CLOSED,
				XLOG_ERROR_ASYNC_CLOSED,
				"async-enqueue",
				"asynchronous log sink is closed"
			);
			return XLOG_RESULT_ERROR;
		}
		if ( pState->Config.Full == XLOG_ASYNC_DROP_NEWEST ) {
			pState->DroppedNewest++;
			(void)xrtMutexUnlock(&pState->Lock);
			return XLOG_RESULT_DROPPED;
		}
		if ( pState->Config.Full == XLOG_ASYNC_DROP_OLDEST ) {
			if (
				(pState->Head == NULL) ||
				(pState->Head->Kind != XLOG_ASYNC_ITEM_RECORD)
			) {
				pState->DroppedNewest++;
				(void)xrtMutexUnlock(&pState->Lock);
				return XLOG_RESULT_DROPPED;
			}
			pDropped = __xrtLogAsyncPopLocked(pState);
			pState->DroppedOldest++;
			__xrtLogAsyncRecordFree((xlogasyncrecord*)pDropped);
			continue;
		}

		Wait = xrtCondWait(&pState->NotFull, &pState->Lock);
		if ( Wait != XWAIT_OK ) {
			(void)xrtMutexUnlock(&pState->Lock);
			__xrtLogAsyncErrorSet(
				XERR_IO,
				XLOG_ERROR_ASYNC_QUEUE,
				"async-enqueue",
				"failed while waiting for asynchronous log queue space"
			);
			return XLOG_RESULT_ERROR;
		}
	}
	if ( pState->Closing ) {
		(void)xrtMutexUnlock(&pState->Lock);
		__xrtLogAsyncErrorSet(
			XERR_CLOSED,
			XLOG_ERROR_ASYNC_CLOSED,
			"async-enqueue",
			"asynchronous log sink is closed"
		);
		return XLOG_RESULT_ERROR;
	}
	__xrtLogAsyncReserveLocked(pState, iBytes);
	(void)xrtMutexUnlock(&pState->Lock);
	return XLOG_RESULT_WRITTEN;
}



/* 把完成深拷贝的记录提交到已经预留的严格 FIFO 位置。 */
static xlogresult __xrtLogAsyncCommitRecord(
	xlogasyncstate* pState,
	xlogasyncrecord* pRecord
)
{
	if ( !xrtMutexLock(&pState->Lock) ) {
		__xrtLogAsyncUnreserve(pState, pRecord->Item.Bytes);
		__xrtLogAsyncRecordFree(pRecord);
		__xrtLogAsyncErrorSet(
			XERR_IO,
			XLOG_ERROR_ASYNC_QUEUE,
			"async-enqueue",
			"failed to commit asynchronous log record"
		);
		return XLOG_RESULT_ERROR;
	}
	if ( pState->Closing ) {
		pState->Queued--;
		pState->QueueBytes -= pRecord->Item.Bytes;
		(void)xrtCondBroadcast(&pState->NotFull);
		(void)xrtMutexUnlock(&pState->Lock);
		__xrtLogAsyncRecordFree(pRecord);
		__xrtLogAsyncErrorSet(
			XERR_CLOSED,
			XLOG_ERROR_ASYNC_CLOSED,
			"async-enqueue",
			"asynchronous log sink closed while copying a record"
		);
		return XLOG_RESULT_ERROR;
	}
	__xrtLogAsyncPushLocked(pState, &pRecord->Item);
	pState->Enqueued++;
	(void)xrtMutexUnlock(&pState->Lock);
	return XLOG_RESULT_WRITTEN;
}



/* 提交线程先完成完整深拷贝，再把记录交给有界队列。 */
static xlogresult __xrtLogAsyncWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	xlogasyncstate* pState = (xlogasyncstate*)pUserData;
	xlogasyncrecord* pCopy;
	xlogresult Reserve;
	size_t iSize;
	uint64 iWorker = xrtAtomic64Load(
		&pState->WorkerId,
		XMEMORY_ACQUIRE
	);

	if ( (iWorker != 0u) && (iWorker == __xrtCurrentThreadId()) ) {
		(void)xrtMutexLock(&pState->Lock);
		pState->ReentrantDrops++;
		(void)xrtMutexUnlock(&pState->Lock);
		return XLOG_RESULT_DROPPED;
	}
	if ( !__xrtLogAsyncRecordSize(pRecord, &iSize) ) {
		__xrtLogAsyncErrorSet(
			XERR_RANGE,
			XLOG_ERROR_ASYNC_RECORD,
			"async-copy",
			"asynchronous log record size overflowed"
		);
		return XLOG_RESULT_ERROR;
	}
	if (
		(iSize > pState->Config.RecordLimit) ||
		(iSize > pState->Config.ByteLimit)
	) {
		xrtClearError();
		__xrtLogAsyncErrorSet(
			XERR_RANGE,
			XLOG_ERROR_ASYNC_RECORD,
			"async-copy",
			"asynchronous log record exceeds the configured byte limit"
		);
		return XLOG_RESULT_ERROR;
	}

	Reserve = __xrtLogAsyncReserveRecord(pState, iSize);
	if ( Reserve != XLOG_RESULT_WRITTEN ) {
		return Reserve;
	}

	pCopy = __xrtLogAsyncRecordCopy(pRecord, iSize);
	if ( pCopy == NULL ) {
		__xrtLogAsyncUnreserve(pState, iSize);
		__xrtLogAsyncErrorSet(
			XERR_MEMORY,
			XLOG_ERROR_ASYNC_RECORD,
			"async-copy",
			"failed to copy asynchronous log record"
		);
		return XLOG_RESULT_ERROR;
	}
	return __xrtLogAsyncCommitRecord(pState, pCopy);
}



/* 统计一次目标 Sink 处理结果，并保存后台错误所有权。 */
static void __xrtLogAsyncRecordResult(
	xlogasyncstate* pState,
	xlogresult Result,
	xerror* pError
)
{
	xerror* pOld = NULL;

	(void)xrtMutexLock(&pState->Lock);
	pState->Processed++;
	if ( Result == XLOG_RESULT_WRITTEN ) {
		pState->Written++;
	} else if ( Result == XLOG_RESULT_SKIPPED ) {
		pState->Skipped++;
	} else if ( Result == XLOG_RESULT_DROPPED ) {
		pState->DroppedTarget++;
	} else {
		pState->Failed++;
		pOld = pState->LastError;
		pState->LastError = pError;
		pError = NULL;
	}
	(void)xrtMutexUnlock(&pState->Lock);
	xrtErrorFree(pOld);
	xrtErrorFree(pError);
}



/* 在工作线程上向目标 Sink 提交一条拥有记录。 */
static void __xrtLogAsyncProcessRecord(
	xlogasyncstate* pState,
	xlogasyncrecord* pItem
)
{
	xlogresult Result;
	xerror* pError = NULL;

	xrtClearError();
	Result = xrtLogSinkSubmit(pState->Target, &pItem->Record);
	if ( Result == XLOG_RESULT_ERROR ) {
		pError = __xrtLogAsyncErrorCreate(
			XERR_IO,
			XLOG_ERROR_ASYNC_TARGET,
			"async-target",
			"asynchronous log target rejected a record",
			xrtTakeError()
		);
	}
	__xrtLogAsyncRecordResult(pState, Result, pError);
	__xrtLogAsyncRecordFree(pItem);
}



/* 在工作线程上完成 Flush 栅栏，并把目标错误传回等待线程。 */
static void __xrtLogAsyncProcessFlush(
	xlogasyncstate* pState,
	xlogasyncflush* pFlush
)
{
	xerror* pError = NULL;

	xrtClearError();
	pFlush->Result = xrtLogSinkFlush(pState->Target);
	if ( !pFlush->Result ) {
		pError = __xrtLogAsyncErrorCreate(
			XERR_IO,
			XLOG_ERROR_ASYNC_FLUSH,
			"async-flush",
			"asynchronous log target flush failed",
			xrtTakeError()
		);
		pFlush->Error = xrtErrorRef(pError);
		__xrtLogAsyncStoreError(pState, pError);
	}
	(void)xrtMutexLock(&pState->Lock);
	pState->Flushes++;
	if ( !pFlush->Result ) {
		pState->Failed++;
	}
	(void)xrtMutexUnlock(&pState->Lock);
	xrtErrorFree(pError);
	(void)xrtEventSet(&pFlush->Done);
	__xrtLogAsyncFlushRelease(pFlush);
}



/* 从工作线程执行最终目标 Flush，并只通过后台错误快照报告失败。 */
static void __xrtLogAsyncFinalFlush(xlogasyncstate* pState)
{
	xerror* pError;

	xrtClearError();
	if ( xrtLogSinkFlush(pState->Target) ) {
		(void)xrtMutexLock(&pState->Lock);
		pState->Flushes++;
		(void)xrtMutexUnlock(&pState->Lock);
		return;
	}
	pError = __xrtLogAsyncErrorCreate(
		XERR_IO,
		XLOG_ERROR_ASYNC_FLUSH,
		"async-shutdown",
		"asynchronous log target shutdown flush failed",
		xrtTakeError()
	);
	__xrtLogAsyncStoreError(pState, pError);
	(void)xrtMutexLock(&pState->Lock);
	pState->Flushes++;
	pState->Failed++;
	(void)xrtMutexUnlock(&pState->Lock);
	xrtErrorFree(pError);
}



/* 丢弃队列时唤醒 Flush 调用方，并释放全部拥有节点。 */
static void __xrtLogAsyncDiscardItems(xlogasyncitem* pItem)
{
	while ( pItem != NULL ) {
		xlogasyncitem* pNext = pItem->Next;

		if ( pItem->Kind == XLOG_ASYNC_ITEM_RECORD ) {
			__xrtLogAsyncRecordFree((xlogasyncrecord*)pItem);
		} else {
			xlogasyncflush* pFlush = (xlogasyncflush*)pItem;

			pFlush->Result = false;
			pFlush->Error = __xrtLogAsyncErrorCreate(
				XERR_CLOSED,
				XLOG_ERROR_ASYNC_CLOSED,
				"async-flush",
				"asynchronous log sink closed before flush",
				NULL
			);
			(void)xrtEventSet(&pFlush->Done);
			__xrtLogAsyncFlushRelease(pFlush);
		}
		pItem = pNext;
	}
}



/* 工作线程严格按队列顺序处理记录和 Flush 栅栏。 */
static int32 __xrtLogAsyncWorker(ptr pData)
{
	xlogasyncstate* pState = (xlogasyncstate*)pData;
	xlogasyncitem* pItem;
	xerror* pWaitError = NULL;
	xwaitresult Wait;

	xrtAtomic64Store(
		&pState->WorkerId,
		__xrtCurrentThreadId(),
		XMEMORY_RELEASE
	);
	for ( ;; ) {
		(void)xrtMutexLock(&pState->Lock);
		while ( (pState->Head == NULL) && !pState->Closing ) {
			Wait = xrtCondWait(&pState->NotEmpty, &pState->Lock);
			if ( Wait != XWAIT_OK ) {
				pWaitError = __xrtLogAsyncErrorCreate(
					XERR_IO,
					XLOG_ERROR_ASYNC_QUEUE,
					"async-worker",
					"asynchronous log worker wait failed",
					xrtTakeError()
				);
				pState->Closing = true;
				(void)xrtCondBroadcast(&pState->NotFull);
				break;
			}
		}
		if ( (pState->Head == NULL) && pState->Closing ) {
			(void)xrtMutexUnlock(&pState->Lock);
			break;
		}
		pItem = __xrtLogAsyncPopLocked(pState);
		(void)xrtMutexUnlock(&pState->Lock);

		if ( pItem->Kind == XLOG_ASYNC_ITEM_RECORD ) {
			__xrtLogAsyncProcessRecord(
				pState,
				(xlogasyncrecord*)pItem
			);
		} else {
			__xrtLogAsyncProcessFlush(
				pState,
				(xlogasyncflush*)pItem
			);
		}
	}
	if ( pWaitError != NULL ) {
		__xrtLogAsyncStoreError(pState, pWaitError);
		xrtErrorFree(pWaitError);
	}
	__xrtLogAsyncFinalFlush(pState);
	__xrtLogAsyncStateRelease(pState);
	return 0;
}



/* 最后一个状态引用负责释放同步对象、目标和残余防御性队列。 */
static void __xrtLogAsyncStateRelease(xlogasyncstate* pState)
{
	xlogasyncitem* pItems;

	if ( xrtRefRelease(&pState->RefCount) != 0 ) {
		return;
	}
	pItems = pState->Head;
	pState->Head = NULL;
	pState->Tail = NULL;
	__xrtLogAsyncDiscardItems(pItems);
	xrtErrorFree(pState->LastError);
	xrtLogSinkFree(pState->Target);
	(void)xrtCondUnit(&pState->NotFull);
	(void)xrtCondUnit(&pState->NotEmpty);
	(void)xrtMutexUnit(&pState->Lock);
	xrtFree(pState);
}



/* 关闭发送侧，并按配置保留或摘除当前未处理队列。 */
static void __xrtLogAsyncClose(xlogasyncstate* pState)
{
	xlogasyncitem* pDiscard = NULL;
	xlogasyncitem* pCursor;

	(void)xrtMutexLock(&pState->Lock);
	pState->Closing = true;
	if (
		(pState->Config.Shutdown == XLOG_ASYNC_DISCARD) &&
		(pState->Head != NULL)
	) {
		pDiscard = pState->Head;
		pState->Head = NULL;
		pState->Tail = NULL;
		pCursor = pDiscard;
		while ( pCursor != NULL ) {
			if ( pCursor->Kind == XLOG_ASYNC_ITEM_RECORD ) {
				pState->Discarded++;
			}
			pState->Queued--;
			pState->QueueBytes -= pCursor->Bytes;
			pCursor = pCursor->Next;
		}
	}
	(void)xrtCondBroadcast(&pState->NotEmpty);
	(void)xrtCondBroadcast(&pState->NotFull);
	(void)xrtMutexUnlock(&pState->Lock);
	__xrtLogAsyncDiscardItems(pDiscard);
}



/* 最后一个 Async Sink 引用关闭队列并与工作线程完成生命周期交接。 */
static void __xrtLogAsyncDrop(ptr pUserData)
{
	xlogasyncstate* pState = (xlogasyncstate*)pUserData;
	xthread* pThread = pState->Thread;
	uint64 iWorker = xrtAtomic64Load(
		&pState->WorkerId,
		XMEMORY_ACQUIRE
	);

	__xrtLogAsyncClose(pState);
	if (
		(pThread != NULL) &&
		((iWorker == 0u) || (iWorker != __xrtCurrentThreadId()))
	) {
		(void)xrtThreadWait(pThread);
	}
	if ( pThread != NULL ) {
		xrtThreadDestroy(pThread);
	}
	pState->Thread = NULL;
	__xrtLogAsyncStateRelease(pState);
}



/* Flush 回调把栅栏排到已有记录之后，并同步等待目标完成。 */
static bool __xrtLogAsyncFlush(ptr pUserData)
{
	xlogasyncstate* pState = (xlogasyncstate*)pUserData;
	xlogasyncflush* pFlush;
	xerror* pError;
	xwaitresult Wait;
	bool bResult;
	uint64 iWorker = xrtAtomic64Load(
		&pState->WorkerId,
		XMEMORY_ACQUIRE
	);

	if ( (iWorker != 0u) && (iWorker == __xrtCurrentThreadId()) ) {
		__xrtLogAsyncErrorSet(
			XERR_STATE,
			XLOG_ERROR_ASYNC_FLUSH,
			"async-flush",
			"asynchronous log target cannot flush its own queue"
		);
		return false;
	}
	pFlush = __xrtLogAsyncFlushCreate();
	if ( pFlush == NULL ) {
		__xrtLogAsyncErrorSet(
			XERR_MEMORY,
			XLOG_ERROR_ASYNC_FLUSH,
			"async-flush",
			"failed to create asynchronous log flush barrier"
		);
		return false;
	}

	(void)xrtMutexLock(&pState->Lock);
	while ( (pState->Queued >= pState->Config.Capacity) && !pState->Closing ) {
		Wait = xrtCondWait(&pState->NotFull, &pState->Lock);
		if ( Wait != XWAIT_OK ) {
			(void)xrtMutexUnlock(&pState->Lock);
			__xrtLogAsyncFlushRelease(pFlush);
			__xrtLogAsyncFlushRelease(pFlush);
			__xrtLogAsyncErrorSet(
				XERR_IO,
				XLOG_ERROR_ASYNC_FLUSH,
				"async-flush",
				"failed while waiting to enqueue asynchronous flush"
			);
			return false;
		}
	}
	if ( pState->Closing ) {
		(void)xrtMutexUnlock(&pState->Lock);
		__xrtLogAsyncFlushRelease(pFlush);
		__xrtLogAsyncFlushRelease(pFlush);
		__xrtLogAsyncErrorSet(
			XERR_CLOSED,
			XLOG_ERROR_ASYNC_CLOSED,
			"async-flush",
			"asynchronous log sink is closed"
		);
		return false;
	}
	__xrtLogAsyncReserveLocked(pState, 0u);
	__xrtLogAsyncPushLocked(pState, &pFlush->Item);
	(void)xrtMutexUnlock(&pState->Lock);

	Wait = xrtEventWait(&pFlush->Done);
	if ( Wait != XWAIT_OK ) {
		__xrtLogAsyncFlushRelease(pFlush);
		__xrtLogAsyncErrorSet(
			XERR_IO,
			XLOG_ERROR_ASYNC_FLUSH,
			"async-flush",
			"failed while waiting for asynchronous flush completion"
		);
		return false;
	}
	bResult = pFlush->Result;
	pError = xrtErrorRef(pFlush->Error);
	__xrtLogAsyncFlushRelease(pFlush);
	if ( !bResult ) {
		if ( pError != NULL ) {
			__xrtErrorSetOwned(pError);
		} else {
			__xrtLogAsyncErrorSet(
				XERR_IO,
				XLOG_ERROR_ASYNC_FLUSH,
				"async-flush",
				"asynchronous log flush failed"
			);
		}
		return false;
	}
	xrtErrorFree(pError);
	return true;
}



/* 校验异步配置的枚举、视图和双重有界队列。 */
static bool __xrtLogAsyncConfigValid(const xlogasyncconfig* pConfig)
{
	return
		(pConfig != NULL) &&
		((pConfig->Name.Data != NULL) || (pConfig->Name.Size == 0)) &&
		(pConfig->Level >= XLOG_TRACE) &&
		(pConfig->Level <= XLOG_OFF) &&
		(pConfig->Full >= XLOG_ASYNC_BLOCK) &&
		(pConfig->Full <= XLOG_ASYNC_DROP_OLDEST) &&
		(pConfig->Shutdown >= XLOG_ASYNC_DRAIN) &&
		(pConfig->Shutdown <= XLOG_ASYNC_DISCARD) &&
		(pConfig->Capacity != 0u) &&
		(pConfig->RecordLimit >= sizeof(xlogasyncrecord)) &&
		(pConfig->ByteLimit >= pConfig->RecordLimit);
}



/* 初始化 Async Sink 默认配置。 */
XRT_API bool xrtLogAsyncConfigInit(xlogasyncconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pConfig, 0, sizeof(xlogasyncconfig));
	pConfig->Level = XLOG_TRACE;
	pConfig->Full = XLOG_ASYNC_DROP_NEWEST;
	pConfig->Shutdown = XLOG_ASYNC_DRAIN;
	pConfig->Capacity = XLOG_ASYNC_CAPACITY_DEFAULT;
	pConfig->RecordLimit = XLOG_ASYNC_RECORD_LIMIT_DEFAULT;
	pConfig->ByteLimit = XLOG_ASYNC_BYTE_LIMIT_DEFAULT;
	return true;
}



/* 创建有界异步包装器并启动唯一的顺序工作线程。 */
XRT_API xlogsink* xrtLogAsync(
	xlogsink* pTarget,
	const xlogasyncconfig* pConfig
)
{
	xlogasyncconfig Default;
	xlogasyncstate* pState;
	xlogsinkconfig SinkConfig;
	xlogsink* pSink;
	xstrview Name;
	xerror* pCause;

	if ( pConfig == NULL ) {
		(void)xrtLogAsyncConfigInit(&Default);
		pConfig = &Default;
	}
	if ( (pTarget == NULL) || !__xrtLogAsyncConfigValid(pConfig) ) {
		xrtClearError();
		__xrtLogAsyncErrorSet(
			XERR_ARGUMENT,
			XLOG_ERROR_ASYNC_CONFIG,
			"async-create",
			"invalid asynchronous log sink configuration"
		);
		return NULL;
	}

	pState = (xlogasyncstate*)xrtCalloc(1u, sizeof(xlogasyncstate));
	if ( pState == NULL ) {
		__xrtLogAsyncErrorSet(
			XERR_MEMORY,
			XLOG_ERROR_ASYNC_CONFIG,
			"async-create",
			"failed to allocate asynchronous log state"
		);
		return NULL;
	}
	pState->RefCount = 1;
	pState->Config = *pConfig;
	xrtAtomic64Init(&pState->WorkerId, 0u);
	pState->Target = xrtLogSinkRef(pTarget);
	if ( pState->Target == NULL ) {
		pCause = xrtTakeError();
		xrtFree(pState);
		__xrtErrorSetOwned(__xrtLogAsyncErrorCreate(
			XERR_STATE,
			XLOG_ERROR_ASYNC_CONFIG,
			"async-create",
			"failed to retain asynchronous log target",
			pCause
		));
		return NULL;
	}
	if ( !xrtMutexInit(&pState->Lock) ) {
		pCause = xrtTakeError();
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		__xrtErrorSetOwned(__xrtLogAsyncErrorCreate(
			XERR_IO,
			XLOG_ERROR_ASYNC_CONFIG,
			"async-create",
			"failed to initialize asynchronous log mutex",
			pCause
		));
		return NULL;
	}
	if ( !xrtCondInit(&pState->NotEmpty) ) {
		pCause = xrtTakeError();
		(void)xrtMutexUnit(&pState->Lock);
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		__xrtErrorSetOwned(__xrtLogAsyncErrorCreate(
			XERR_IO,
			XLOG_ERROR_ASYNC_CONFIG,
			"async-create",
			"failed to initialize asynchronous log wait condition",
			pCause
		));
		return NULL;
	}
	if ( !xrtCondInit(&pState->NotFull) ) {
		pCause = xrtTakeError();
		(void)xrtCondUnit(&pState->NotEmpty);
		(void)xrtMutexUnit(&pState->Lock);
		xrtLogSinkFree(pState->Target);
		xrtFree(pState);
		__xrtErrorSetOwned(__xrtLogAsyncErrorCreate(
			XERR_IO,
			XLOG_ERROR_ASYNC_CONFIG,
			"async-create",
			"failed to initialize asynchronous log space condition",
			pCause
		));
		return NULL;
	}

	(void)xrtRefRetain(&pState->RefCount);
	pState->Thread = xrtThreadCreate(
		__xrtLogAsyncWorker,
		pState,
		pConfig->StackSize
	);
	if ( pState->Thread == NULL ) {
		pCause = xrtTakeError();
		(void)xrtRefRelease(&pState->RefCount);
		__xrtLogAsyncStateRelease(pState);
		__xrtErrorSetOwned(__xrtLogAsyncErrorCreate(
			XERR_IO,
			XLOG_ERROR_ASYNC_THREAD,
			"async-create",
			"failed to start asynchronous log worker",
			pCause
		));
		return NULL;
	}

	Name = pConfig->Name.Size == 0
		? xrtLogSinkName(pTarget)
		: pConfig->Name;
	memset(&SinkConfig, 0, sizeof(SinkConfig));
	SinkConfig.Name = Name;
	SinkConfig.Level = pConfig->Level;
	SinkConfig.Write = __xrtLogAsyncWrite;
	SinkConfig.Flush = __xrtLogAsyncFlush;
	SinkConfig.Drop = __xrtLogAsyncDrop;
	SinkConfig.UserData = pState;
	pSink = xrtLogSinkCreate(&SinkConfig);
	if ( pSink != NULL ) {
		return pSink;
	}

	pCause = xrtTakeError();
	__xrtLogAsyncClose(pState);
	(void)xrtThreadWait(pState->Thread);
	xrtThreadDestroy(pState->Thread);
	pState->Thread = NULL;
	__xrtLogAsyncStateRelease(pState);
	__xrtErrorSetOwned(__xrtLogAsyncErrorCreate(
		XERR_MEMORY,
		XLOG_ERROR_ASYNC_CONFIG,
		"async-create",
		"failed to create asynchronous log sink",
		pCause
	));
	return NULL;
}



/* 创建异步包装器并把唯一调用方引用交给 Logger。 */
XRT_API bool xrtLogAddAsync(
	xlogger* pLogger,
	xlogsink* pTarget,
	const xlogasyncconfig* pConfig
)
{
	xlogsink* pSink;
	bool bResult;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSink = xrtLogAsync(pTarget, pConfig);
	if ( pSink == NULL ) {
		return false;
	}
	bResult = xrtLogAttach(pLogger, pSink);
	xrtLogSinkFree(pSink);
	return bResult;
}



/* 验证 Async Sink 身份并返回内部状态。 */
static xlogasyncstate* __xrtLogAsyncState(const xlogsink* pSink)
{
	ptr pData = NULL;

	if ( !__xrtLogSinkData(pSink, __xrtLogAsyncWrite, &pData) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return (xlogasyncstate*)pData;
}



/* 返回 Async Sink 借用的目标引用。 */
XRT_API xlogsink* xrtLogAsyncTarget(const xlogsink* pSink)
{
	xlogasyncstate* pState = __xrtLogAsyncState(pSink);

	return pState == NULL ? NULL : pState->Target;
}



/* 读取队列和后台处理的同锁统计快照。 */
XRT_API bool xrtLogAsyncStats(
	const xlogsink* pSink,
	xlogasyncstats* pStats
)
{
	xlogasyncstate* pState = __xrtLogAsyncState(pSink);

	if ( (pState == NULL) || (pStats == NULL) ) {
		if ( pStats == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	(void)xrtMutexLock(&pState->Lock);
	pStats->Enqueued = pState->Enqueued;
	pStats->Processed = pState->Processed;
	pStats->Written = pState->Written;
	pStats->Skipped = pState->Skipped;
	pStats->DroppedNewest = pState->DroppedNewest;
	pStats->DroppedOldest = pState->DroppedOldest;
	pStats->DroppedTarget = pState->DroppedTarget;
	pStats->ReentrantDrops = pState->ReentrantDrops;
	pStats->Discarded = pState->Discarded;
	pStats->Failed = pState->Failed;
	pStats->Flushes = pState->Flushes;
	pStats->Queued = pState->Queued;
	pStats->QueueBytes = pState->QueueBytes;
	pStats->PeakQueued = pState->PeakQueued;
	pStats->PeakBytes = pState->PeakBytes;
	(void)xrtMutexUnlock(&pState->Lock);
	return true;
}



/* 返回后台最近一次错误的新引用。 */
XRT_API xerror* xrtLogAsyncLastError(const xlogsink* pSink)
{
	xlogasyncstate* pState = __xrtLogAsyncState(pSink);
	xerror* pError;

	if ( pState == NULL ) {
		return NULL;
	}
	(void)xrtMutexLock(&pState->Lock);
	pError = xrtErrorRef(pState->LastError);
	(void)xrtMutexUnlock(&pState->Lock);
	return pError;
}

#endif
