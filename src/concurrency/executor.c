#include "../internal/xrt_temp.h"



#if defined(XRT_FEATURE_EXECUTOR)

typedef struct xrt_executor_worker xrt_executor_worker;



/* 作业槽在创建时预分配，提交和执行热路径不再申请通用堆内存。 */
typedef struct xrt_executor_job {
	struct xrt_executor_job* Previous;
	struct xrt_executor_job* Next;
	xrt_executor_worker* Owner;
	xexecutorproc Proc;
	ptr Data;
	xexecutorfreeproc Destroy;
	ptr DestroyContext;
} xrt_executor_job;



/* 每个 Worker 独占一个有界双端队列；其他 Worker 只从队首窃取。 */
struct xrt_executor_worker {
	struct xexecutor* Executor;
	xthread* Thread;
	xrt_spinlock Lock;
	xrt_executor_job* Free;
	xrt_executor_job* Head;
	xrt_executor_job* Tail;
	size_t FreeCount;
	size_t Queued;
	uint32 Index;
};



/* 唤醒和排空条件使用冷锁，提交队列自身只经过目标 Worker 的短自旋锁。 */
struct xexecutor {
	xmutex SleepLock;
	xcond Work;
	xcond Idle;
	xrt_executor_worker* Workers;
	xrt_executor_job* Jobs;
	uint32 ThreadCount;
	uint32 StartedThreads;
	size_t QueueLimit;
	size_t StackSize;
	xatomic32 Closed;
	xatomic32 Cancelling;
	xatomic32 Shutdown;
	xatomic32 Submitters;
	xatomic32 Sleepers;
	xatomic32 Queued;
	xatomic32 Running;
	xatomic64 NextWorker;
	xatomic64 Submitted;
	xatomic64 Completed;
	xatomic64 Executed;
	xatomic64 Stolen;
	xatomic64 Cancelled;
	xatomic64 Rejected;
};



#if defined(XRT_THREAD_LOCAL)
static XRT_THREAD_LOCAL xrt_executor_worker* __xrtExecutorCurrentWorker;
#endif



/* 返回当前线程是否属于指定执行器，并按需返回本地 Worker。 */
static xrt_executor_worker* __xrtExecutorCurrent(xexecutor* pExecutor)
{
	#if defined(XRT_THREAD_LOCAL)
		xrt_executor_worker* pWorker = __xrtExecutorCurrentWorker;

		return (pWorker != NULL) &&
			(pWorker->Executor == pExecutor) ? pWorker : NULL;
	#else
		xthread* pCurrent = xrtThreadCurrent();

		if ( pCurrent == NULL ) {
			return NULL;
		}
		for ( uint32 i = 0; i < pExecutor->StartedThreads; i++ ) {
			if ( pExecutor->Workers[i].Thread == pCurrent ) {
				return &pExecutor->Workers[i];
			}
		}
		return NULL;
	#endif
}



/* 从所属 Worker 的预分配空闲链取出一个作业槽。 */
static xrt_executor_job* __xrtExecutorJobTakeLocked(
	xrt_executor_worker* pWorker
)
{
	xrt_executor_job* pJob = pWorker->Free;

	if ( pJob != NULL ) {
		pWorker->Free = pJob->Next;
		pWorker->FreeCount--;
		pJob->Previous = NULL;
		pJob->Next = NULL;
	}
	return pJob;
}



/* 把已经完成析构的作业槽归还给创建它的 Worker。 */
static void __xrtExecutorJobReturn(xrt_executor_job* pJob)
{
	xrt_executor_worker* pOwner = pJob->Owner;

	pJob->Proc = NULL;
	pJob->Data = NULL;
	pJob->Destroy = NULL;
	pJob->DestroyContext = NULL;
	pJob->Previous = NULL;
	__xrtSpinLock(&pOwner->Lock);
	pJob->Next = pOwner->Free;
	pOwner->Free = pJob;
	pOwner->FreeCount++;
	__xrtSpinUnlock(&pOwner->Lock);
}



/* 本地 Worker 从队尾取工作，保持嵌套提交的缓存局部性。 */
static xrt_executor_job* __xrtExecutorPopLocal(
	xrt_executor_worker* pWorker
)
{
	xrt_executor_job* pJob;

	__xrtSpinLock(&pWorker->Lock);
	pJob = pWorker->Tail;
	if ( pJob != NULL ) {
		pWorker->Tail = pJob->Previous;
		if ( pWorker->Tail == NULL ) {
			pWorker->Head = NULL;
		} else {
			pWorker->Tail->Next = NULL;
		}
		pJob->Previous = NULL;
		pJob->Next = NULL;
		pWorker->Queued--;
	}
	__xrtSpinUnlock(&pWorker->Lock);
	return pJob;
}



/* 空闲 Worker 从其他队列的最旧端窃取，避免与所有者竞争同一端。 */
static xrt_executor_job* __xrtExecutorSteal(
	xrt_executor_worker* pWorker
)
{
	xexecutor* pExecutor = pWorker->Executor;

	for ( uint32 i = 1; i < pExecutor->ThreadCount; i++ ) {
		uint32 iIndex = (pWorker->Index + i) % pExecutor->ThreadCount;
		xrt_executor_worker* pVictim = &pExecutor->Workers[iIndex];
		xrt_executor_job* pJob;

		__xrtSpinLock(&pVictim->Lock);
		pJob = pVictim->Head;
		if ( pJob != NULL ) {
			pVictim->Head = pJob->Next;
			if ( pVictim->Head == NULL ) {
				pVictim->Tail = NULL;
			} else {
				pVictim->Head->Previous = NULL;
			}
			pJob->Previous = NULL;
			pJob->Next = NULL;
			pVictim->Queued--;
		}
		__xrtSpinUnlock(&pVictim->Lock);
		if ( pJob != NULL ) {
			(void)xrtAtomic64FetchAdd(
				&pExecutor->Stolen,
				1,
				XMEMORY_RELAXED
			);
			return pJob;
		}
	}
	return NULL;
}



/* 优先取得本地工作，本地为空时再遍历其他 Worker。 */
static xrt_executor_job* __xrtExecutorTake(
	xrt_executor_worker* pWorker
)
{
	xrt_executor_job* pJob = __xrtExecutorPopLocal(pWorker);

	if ( pJob == NULL ) {
		pJob = __xrtExecutorSteal(pWorker);
	}
	if ( pJob != NULL ) {
		(void)xrtAtomic32FetchSub(
			&pWorker->Executor->Queued,
			1,
			XMEMORY_ACQ_REL
		);
		(void)xrtAtomic32FetchAdd(
			&pWorker->Executor->Running,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return pJob;
}



/* 工作排空后在冷路径唤醒全部等待者。 */
static void __xrtExecutorNotifyIdle(xexecutor* pExecutor)
{
	if ( (xrtAtomic32Load(
		&pExecutor->Queued,
		XMEMORY_ACQUIRE
	) != 0) || (xrtAtomic32Load(
		&pExecutor->Running,
		XMEMORY_ACQUIRE
	) != 0) ) {
		return;
	}
	(void)xrtMutexLock(&pExecutor->SleepLock);
	(void)xrtCondBroadcast(&pExecutor->Idle);
	(void)xrtMutexUnlock(&pExecutor->SleepLock);
}



/* 唤醒一个或全部休眠 Worker；批量工作允许多个 Worker 立即窃取。 */
static void __xrtExecutorWake(xexecutor* pExecutor, bool bBatch)
{
	if ( xrtAtomic32Load(
		&pExecutor->Sleepers,
		XMEMORY_ACQUIRE
	) == 0 ) {
		return;
	}
	(void)xrtMutexLock(&pExecutor->SleepLock);
	if ( bBatch ) {
		(void)xrtCondBroadcast(&pExecutor->Work);
	} else {
		(void)xrtCondSignal(&pExecutor->Work);
	}
	(void)xrtMutexUnlock(&pExecutor->SleepLock);
}



/* 在隔离的错误和临时内存上下文中执行一项 detached 工作。 */
static void __xrtExecutorRun(
	xrt_executor_worker* pWorker,
	xrt_executor_job* pJob
)
{
	xexecutor* pExecutor = pWorker->Executor;

	pJob->Proc(pJob->Data);
	if ( pJob->Destroy != NULL ) {
		pJob->Destroy(pJob->Data, pJob->DestroyContext);
	}
	xrtClearError();
	(void)xrtTempClear();
	__xrtExecutorJobReturn(pJob);
	(void)xrtAtomic32FetchSub(
		&pExecutor->Running,
		1,
		XMEMORY_ACQ_REL
	);
	(void)xrtAtomic64FetchAdd(
		&pExecutor->Executed,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pExecutor->Completed,
		1,
		XMEMORY_RELEASE
	);
	__xrtExecutorNotifyIdle(pExecutor);
}



/* Worker 持续消费本地或窃取工作，只在所有队列为空时进入条件等待。 */
static int32 __xrtExecutorWorker(ptr pData)
{
	xrt_executor_worker* pWorker = (xrt_executor_worker*)pData;
	xexecutor* pExecutor = pWorker->Executor;

	#if defined(XRT_THREAD_LOCAL)
		__xrtExecutorCurrentWorker = pWorker;
	#endif
	for ( ;; ) {
		xrt_executor_job* pJob = __xrtExecutorTake(pWorker);

		if ( pJob != NULL ) {
			__xrtExecutorRun(pWorker, pJob);
			continue;
		}
		(void)xrtMutexLock(&pExecutor->SleepLock);
		(void)xrtAtomic32FetchAdd(
			&pExecutor->Sleepers,
			1,
			XMEMORY_RELEASE
		);
		pJob = __xrtExecutorTake(pWorker);
		while ( (pJob == NULL) && !xrtAtomic32Load(
			&pExecutor->Shutdown,
			XMEMORY_ACQUIRE
		) ) {
			(void)xrtCondWait(
				&pExecutor->Work,
				&pExecutor->SleepLock
			);
			pJob = __xrtExecutorTake(pWorker);
		}
		(void)xrtAtomic32FetchSub(
			&pExecutor->Sleepers,
			1,
			XMEMORY_RELEASE
		);
		(void)xrtMutexUnlock(&pExecutor->SleepLock);
		if ( pJob != NULL ) {
			__xrtExecutorRun(pWorker, pJob);
			continue;
		}
		if ( xrtAtomic32Load(
			&pExecutor->Shutdown,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	#if defined(XRT_THREAD_LOCAL)
		__xrtExecutorCurrentWorker = NULL;
	#endif
	return 0;
}



/* 停止并回收创建失败的半成品执行器。 */
static void __xrtExecutorCreateCleanup(xexecutor* pExecutor)
{
	xrtAtomic32Store(
		&pExecutor->Shutdown,
		1,
		XMEMORY_RELEASE
	);
	__xrtExecutorWake(pExecutor, true);
	for ( uint32 i = 0; i < pExecutor->StartedThreads; i++ ) {
		(void)xrtThreadWait(pExecutor->Workers[i].Thread);
		xrtThreadDestroy(pExecutor->Workers[i].Thread);
	}
	for ( uint32 i = 0; i < pExecutor->ThreadCount; i++ ) {
		__xrtSpinUnit(&pExecutor->Workers[i].Lock);
	}
	(void)xrtCondUnit(&pExecutor->Idle);
	(void)xrtCondUnit(&pExecutor->Work);
	(void)xrtMutexUnit(&pExecutor->SleepLock);
	xrtFree(pExecutor);
}



/* 创建预分配作业槽、Worker 本地队列和工作窃取线程。 */
XRT_API xexecutor* xrtExecutorCreate(const xexecutorconfig* pConfig)
{
	xexecutorconfig Config;
	xexecutor* pExecutor;
	size_t iWorkerBytes;
	size_t iSlotsPerWorker;
	size_t iJobCount;
	size_t iJobBytes;
	size_t iTotal;

	memset(&Config, 0, sizeof(Config));
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( Config.Threads == 0 ) {
		Config.Threads = __xrtProcessorCount();
	}
	if ( Config.QueueLimit == 0 ) {
		Config.QueueLimit = XRT_EXECUTOR_QUEUE_LIMIT_DEFAULT;
	}
	if ( (Config.Threads == 0) ||
		(Config.Threads > XRT_EXECUTOR_THREAD_LIMIT) ||
		(Config.QueueLimit > UINT32_MAX) ||
		(Config.QueueLimit > (SIZE_MAX / Config.Threads)) ||
		(Config.QueueLimit > (UINT32_MAX / Config.Threads)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( Config.QueueLimit > (SIZE_MAX - Config.Threads) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSlotsPerWorker = Config.QueueLimit + Config.Threads;
	if ( iSlotsPerWorker > (SIZE_MAX / Config.Threads) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iJobCount = iSlotsPerWorker * Config.Threads;
	if ( iJobCount > (SIZE_MAX / sizeof(xrt_executor_job)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iWorkerBytes = Config.Threads * sizeof(xrt_executor_worker);
	iJobBytes = iJobCount * sizeof(xrt_executor_job);
	if ( (iWorkerBytes > (SIZE_MAX - sizeof(*pExecutor))) ||
		(iJobBytes > (SIZE_MAX - sizeof(*pExecutor) - iWorkerBytes)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = sizeof(*pExecutor) + iWorkerBytes + iJobBytes;
	pExecutor = (xexecutor*)xrtCalloc(1, iTotal);
	if ( pExecutor == NULL ) {
		return NULL;
	}
	pExecutor->Workers = (xrt_executor_worker*)(pExecutor + 1);
	pExecutor->Jobs = (xrt_executor_job*)(
		(uint8*)pExecutor->Workers + iWorkerBytes
	);
	pExecutor->ThreadCount = Config.Threads;
	pExecutor->QueueLimit = Config.QueueLimit;
	pExecutor->StackSize = Config.StackSize;
	xrtAtomic32Init(&pExecutor->Closed, 0);
	xrtAtomic32Init(&pExecutor->Cancelling, 0);
	xrtAtomic32Init(&pExecutor->Shutdown, 0);
	xrtAtomic32Init(&pExecutor->Submitters, 0);
	xrtAtomic32Init(&pExecutor->Sleepers, 0);
	xrtAtomic32Init(&pExecutor->Queued, 0);
	xrtAtomic32Init(&pExecutor->Running, 0);
	xrtAtomic64Init(&pExecutor->NextWorker, 0);
	xrtAtomic64Init(&pExecutor->Submitted, 0);
	xrtAtomic64Init(&pExecutor->Completed, 0);
	xrtAtomic64Init(&pExecutor->Executed, 0);
	xrtAtomic64Init(&pExecutor->Stolen, 0);
	xrtAtomic64Init(&pExecutor->Cancelled, 0);
	xrtAtomic64Init(&pExecutor->Rejected, 0);
	if ( !xrtMutexInit(&pExecutor->SleepLock) ) {
		xrtFree(pExecutor);
		return NULL;
	}
	if ( !xrtCondInit(&pExecutor->Work) ) {
		(void)xrtMutexUnit(&pExecutor->SleepLock);
		xrtFree(pExecutor);
		return NULL;
	}
	if ( !xrtCondInit(&pExecutor->Idle) ) {
		(void)xrtCondUnit(&pExecutor->Work);
		(void)xrtMutexUnit(&pExecutor->SleepLock);
		xrtFree(pExecutor);
		return NULL;
	}

	/* 每个 Worker 只管理自己的固定槽，避免提交热路径争用全局空闲表。 */
	for ( uint32 i = 0; i < Config.Threads; i++ ) {
		xrt_executor_worker* pWorker = &pExecutor->Workers[i];

		pWorker->Executor = pExecutor;
		pWorker->Index = i;
		pWorker->FreeCount = iSlotsPerWorker;
		__xrtSpinInit(&pWorker->Lock);
		for ( size_t j = 0; j < iSlotsPerWorker; j++ ) {
			xrt_executor_job* pJob = &pExecutor->Jobs[
				((size_t)i * iSlotsPerWorker) + j
			];

			pJob->Owner = pWorker;
			pJob->Next = pWorker->Free;
			pWorker->Free = pJob;
		}
	}

	/* 全部同步对象和槽位发布后再启动线程。 */
	for ( uint32 i = 0; i < Config.Threads; i++ ) {
		xrt_executor_worker* pWorker = &pExecutor->Workers[i];

		pWorker->Thread = xrtThreadCreate(
			__xrtExecutorWorker,
			pWorker,
			Config.StackSize
		);
		if ( pWorker->Thread == NULL ) {
			__xrtExecutorCreateCleanup(pExecutor);
			return NULL;
		}
		pExecutor->StartedThreads++;
	}
	return pExecutor;
}



/* 尝试把一组工作原子放入指定 Worker 的本地队列。 */
static bool __xrtExecutorSubmitTo(
	xexecutor* pExecutor,
	xrt_executor_worker* pWorker,
	const xexecutoritem* pItems,
	size_t iCount
)
{
	xrt_executor_job* pFirst = NULL;
	xrt_executor_job* pLast = NULL;

	__xrtSpinLock(&pWorker->Lock);
	if ( (pWorker->Queued >
		(pExecutor->QueueLimit - iCount)) ||
		(pWorker->FreeCount < iCount) ) {
		__xrtSpinUnlock(&pWorker->Lock);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		xrt_executor_job* pJob = __xrtExecutorJobTakeLocked(pWorker);

		pJob->Proc = pItems[i].Proc;
		pJob->Data = pItems[i].Data;
		pJob->Destroy = pItems[i].Destroy;
		pJob->DestroyContext = pItems[i].DestroyContext;
		pJob->Previous = pLast;
		if ( pLast == NULL ) {
			pFirst = pJob;
		} else {
			pLast->Next = pJob;
		}
		pLast = pJob;
	}
	pLast->Next = NULL;
	if ( pWorker->Tail == NULL ) {
		pWorker->Head = pFirst;
	} else {
		pFirst->Previous = pWorker->Tail;
		pWorker->Tail->Next = pFirst;
	}
	pWorker->Tail = pLast;
	pWorker->Queued += iCount;
	(void)xrtAtomic32FetchAdd(
		&pExecutor->Queued,
		(uint32)iCount,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic64FetchAdd(
		&pExecutor->Submitted,
		(uint64)iCount,
		XMEMORY_RELAXED
	);
	__xrtSpinUnlock(&pWorker->Lock);
	return true;
}



/* 进入提交线性化区；Close 会等待已经进入的提交发布或回滚。 */
static bool __xrtExecutorSubmitEnter(xexecutor* pExecutor)
{
	(void)xrtAtomic32FetchAdd(
		&pExecutor->Submitters,
		1,
		XMEMORY_SEQ_CST
	);
	if ( xrtAtomic32Load(
		&pExecutor->Closed,
		XMEMORY_SEQ_CST
	) ) {
		(void)xrtAtomic32FetchSub(
			&pExecutor->Submitters,
			1,
			XMEMORY_SEQ_CST
		);
		return false;
	}
	return true;
}



/* 离开提交线性化区。 */
static void __xrtExecutorSubmitLeave(xexecutor* pExecutor)
{
	(void)xrtAtomic32FetchSub(
		&pExecutor->Submitters,
		1,
		XMEMORY_SEQ_CST
	);
}



/* 验证批量描述符并选择本地或轮转起始 Worker。 */
static bool __xrtExecutorSubmitItems(
	xexecutor* pExecutor,
	const xexecutoritem* pItems,
	size_t iCount
)
{
	xrt_executor_worker* pCurrent;
	uint32 iStart;

	if ( (pExecutor == NULL) || (pItems == NULL) ||
		(iCount == 0) || (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / sizeof(*pItems))) ||
		!__xrtRangeValid(pItems, iCount * sizeof(*pItems)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pItems[i].Proc == NULL ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	if ( iCount > pExecutor->QueueLimit ) {
		(void)xrtAtomic64FetchAdd(
			&pExecutor->Rejected,
			(uint64)iCount,
			XMEMORY_RELAXED
		);
		__xrtErrorSetAgain();
		return false;
	}
	if ( !__xrtExecutorSubmitEnter(pExecutor) ) {
		(void)xrtAtomic64FetchAdd(
			&pExecutor->Rejected,
			(uint64)iCount,
			XMEMORY_RELAXED
		);
		__xrtErrorSetClosed();
		return false;
	}
	pCurrent = __xrtExecutorCurrent(pExecutor);
	iStart = pCurrent != NULL ? pCurrent->Index :
		(uint32)(xrtAtomic64FetchAdd(
			&pExecutor->NextWorker,
			1,
			XMEMORY_RELAXED
		) % pExecutor->ThreadCount);
	for ( uint32 i = 0; i < pExecutor->ThreadCount; i++ ) {
		xrt_executor_worker* pWorker = &pExecutor->Workers[
			(iStart + i) % pExecutor->ThreadCount
		];

		if ( __xrtExecutorSubmitTo(
			pExecutor,
			pWorker,
			pItems,
			iCount
		) ) {
			__xrtExecutorSubmitLeave(pExecutor);
			__xrtExecutorWake(pExecutor, iCount > 1);
			return true;
		}
	}
	(void)xrtAtomic64FetchAdd(
		&pExecutor->Rejected,
		(uint64)iCount,
		XMEMORY_RELAXED
	);
	__xrtExecutorSubmitLeave(pExecutor);
	__xrtErrorSetAgain();
	return false;
}



/* 提交一个 detached 工作。 */
XRT_API bool xrtExecutorSubmit(
	xexecutor* pExecutor,
	xexecutorproc pProc,
	ptr pData,
	xexecutorfreeproc pDestroy,
	ptr pDestroyContext
)
{
	const xexecutoritem Item = {
		pProc,
		pData,
		pDestroy,
		pDestroyContext
	};

	return __xrtExecutorSubmitItems(pExecutor, &Item, 1);
}



/* 原子提交一组 detached 工作。 */
XRT_API bool xrtExecutorSubmitBatch(
	xexecutor* pExecutor,
	const xexecutoritem* pItems,
	size_t iCount
)
{
	return __xrtExecutorSubmitItems(pExecutor, pItems, iCount);
}



/* 停止受理新工作，并让已经受理的工作自然排空。 */
XRT_API bool xrtExecutorClose(xexecutor* pExecutor)
{
	if ( pExecutor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtAtomic32Store(&pExecutor->Closed, 1, XMEMORY_SEQ_CST);
	while ( xrtAtomic32Load(
		&pExecutor->Submitters,
		XMEMORY_SEQ_CST
	) != 0 ) {
		xrtThreadYield();
	}
	__xrtExecutorWake(pExecutor, true);
	__xrtExecutorNotifyIdle(pExecutor);
	return true;
}



/* 在锁内摘除所有尚未开始的工作，锁外执行数据析构。 */
XRT_API bool xrtExecutorCancel(xexecutor* pExecutor)
{
	xrt_executor_job* pHead = NULL;
	xrt_executor_job* pTail = NULL;
	uint32 iCancelled = 0;

	if ( pExecutor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtExecutorClose(pExecutor);
	xrtAtomic32Store(&pExecutor->Cancelling, 1, XMEMORY_RELEASE);
	for ( uint32 i = 0; i < pExecutor->ThreadCount; i++ ) {
		xrt_executor_worker* pWorker = &pExecutor->Workers[i];

		__xrtSpinLock(&pWorker->Lock);
		if ( pWorker->Head != NULL ) {
			if ( pTail == NULL ) {
				pHead = pWorker->Head;
			} else {
				pTail->Next = pWorker->Head;
				pWorker->Head->Previous = pTail;
			}
			pTail = pWorker->Tail;
			for ( xrt_executor_job* pJob = pWorker->Head;
				pJob != NULL; pJob = pJob->Next ) {
				iCancelled++;
			}
			pWorker->Head = NULL;
			pWorker->Tail = NULL;
			pWorker->Queued = 0;
		}
		__xrtSpinUnlock(&pWorker->Lock);
	}
	if ( iCancelled != 0 ) {
		(void)xrtAtomic32FetchSub(
			&pExecutor->Queued,
			iCancelled,
			XMEMORY_ACQ_REL
		);
	}
	while ( pHead != NULL ) {
		xrt_executor_job* pJob = pHead;

		pHead = pJob->Next;
		if ( pJob->Destroy != NULL ) {
			pJob->Destroy(pJob->Data, pJob->DestroyContext);
		}
		__xrtExecutorJobReturn(pJob);
	}
	if ( iCancelled != 0 ) {
		(void)xrtAtomic64FetchAdd(
			&pExecutor->Cancelled,
			iCancelled,
			XMEMORY_RELAXED
		);
		(void)xrtAtomic64FetchAdd(
			&pExecutor->Completed,
			iCancelled,
			XMEMORY_RELEASE
		);
	}
	__xrtExecutorWake(pExecutor, true);
	__xrtExecutorNotifyIdle(pExecutor);
	return true;
}



/* 等待已经关闭的执行器排空到指定截止时间。 */
XRT_API xwaitresult xrtExecutorWaitUntil(
	xexecutor* pExecutor,
	xdeadline iDeadline
)
{
	xwaitresult Result = XWAIT_OK;

	if ( pExecutor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XWAIT_ERROR;
	}
	if ( __xrtExecutorCurrent(pExecutor) != NULL ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	if ( !xrtAtomic32Load(&pExecutor->Closed, XMEMORY_ACQUIRE) ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	(void)xrtMutexLock(&pExecutor->SleepLock);
	while ( (xrtAtomic32Load(
		&pExecutor->Queued,
		XMEMORY_ACQUIRE
	) != 0) || (xrtAtomic32Load(
		&pExecutor->Running,
		XMEMORY_ACQUIRE
	) != 0) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			Result = XWAIT_TIMEOUT;
			break;
		}
		Result = xrtCondWaitUntil(
			&pExecutor->Idle,
			&pExecutor->SleepLock,
			iDeadline
		);
		if ( Result == XWAIT_ERROR ) {
			break;
		}
		if ( Result == XWAIT_TIMEOUT ) {
			if ( (xrtAtomic32Load(
				&pExecutor->Queued,
				XMEMORY_ACQUIRE
			) == 0) && (xrtAtomic32Load(
				&pExecutor->Running,
				XMEMORY_ACQUIRE
			) == 0) ) {
				Result = XWAIT_OK;
			}
			break;
		}
	}
	(void)xrtMutexUnlock(&pExecutor->SleepLock);
	return Result;
}



/* 永久等待已经关闭的执行器排空。 */
XRT_API xwaitresult xrtExecutorWait(xexecutor* pExecutor)
{
	return xrtExecutorWaitUntil(pExecutor, XRT_DEADLINE_NEVER);
}



/* 在相对超时内等待已经关闭的执行器排空。 */
XRT_API xwaitresult xrtExecutorWaitFor(
	xexecutor* pExecutor,
	uint64 iTimeout
)
{
	return xrtExecutorWaitUntil(
		pExecutor,
		xrtDeadlineAfter(iTimeout)
	);
}



/* 复制当前负载和累计统计快照。 */
XRT_API bool xrtExecutorGet(
	const xexecutor* pExecutor,
	xexecutorstats* pStats
)
{
	if ( (pExecutor == NULL) ||
		!__xrtRangeValid(pStats, sizeof(*pStats)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pStats->Threads = pExecutor->ThreadCount;
	pStats->QueueLimit = pExecutor->QueueLimit;
	pStats->Queued = xrtAtomic32Load(
		&pExecutor->Queued,
		XMEMORY_ACQUIRE
	);
	pStats->Running = xrtAtomic32Load(
		&pExecutor->Running,
		XMEMORY_ACQUIRE
	);
	pStats->Submitted = xrtAtomic64Load(
		&pExecutor->Submitted,
		XMEMORY_RELAXED
	);
	pStats->Completed = xrtAtomic64Load(
		&pExecutor->Completed,
		XMEMORY_RELAXED
	);
	pStats->Executed = xrtAtomic64Load(
		&pExecutor->Executed,
		XMEMORY_RELAXED
	);
	pStats->Stolen = xrtAtomic64Load(
		&pExecutor->Stolen,
		XMEMORY_RELAXED
	);
	pStats->Cancelled = xrtAtomic64Load(
		&pExecutor->Cancelled,
		XMEMORY_RELAXED
	);
	pStats->Rejected = xrtAtomic64Load(
		&pExecutor->Rejected,
		XMEMORY_RELAXED
	);
	pStats->Closed = xrtAtomic32Load(
		&pExecutor->Closed,
		XMEMORY_ACQUIRE
	) != 0;
	pStats->Cancelling = xrtAtomic32Load(
		&pExecutor->Cancelling,
		XMEMORY_ACQUIRE
	) != 0;
	return true;
}



/* 关闭、排空、停止 Worker 并释放执行器。 */
XRT_API bool xrtExecutorDestroy(xexecutor* pExecutor)
{
	if ( pExecutor == NULL ) {
		return true;
	}
	if ( __xrtExecutorCurrent(pExecutor) != NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !xrtExecutorClose(pExecutor) ||
		(xrtExecutorWait(pExecutor) != XWAIT_OK) ) {
		return false;
	}
	xrtAtomic32Store(&pExecutor->Shutdown, 1, XMEMORY_RELEASE);
	__xrtExecutorWake(pExecutor, true);
	for ( uint32 i = 0; i < pExecutor->StartedThreads; i++ ) {
		if ( xrtThreadWait(pExecutor->Workers[i].Thread) != XWAIT_OK ) {
			return false;
		}
	}
	for ( uint32 i = 0; i < pExecutor->StartedThreads; i++ ) {
		xrtThreadDestroy(pExecutor->Workers[i].Thread);
		__xrtSpinUnit(&pExecutor->Workers[i].Lock);
	}
	(void)xrtCondUnit(&pExecutor->Idle);
	(void)xrtCondUnit(&pExecutor->Work);
	(void)xrtMutexUnit(&pExecutor->SleepLock);
	xrtFree(pExecutor);
	return true;
}

#endif
