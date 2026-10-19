#include <xrt/detail/wait.h>
#include "../internal/xrt_executor.h"



#if defined(XRT_FEATURE_EXECUTOR)

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
		if (pJob->Active || pJob->Finished || __xrtAtomicRefLoad(&pJob->RefCount) <= 0) abort();
		pJob->Active = true;
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
	if ( !__xrtExecutorIdle(pExecutor) ) {
		return;
	}
	(void)xrtMutexLock(&pExecutor->SleepLock);
	(void)xrtCondBroadcast(&pExecutor->Idle);
	(void)xrtMutexUnlock(&pExecutor->SleepLock);
}



/* 唤醒一个或全部休眠 Worker；批量工作允许多个 Worker 立即窃取。 */
void __xrtExecutorWake(xexecutor* pExecutor, bool bBatch)
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



/* Completion credit survives queue/worker/cancel-list transfers AND the user
 * cleanup tail. Only the last pin may recycle the physical slot. */
static void __xrtExecutorComplete(xexecutor* pExecutor, xrt_executor_job* pJob, bool bCancelled)
{
	xrtownershipscope Mutation = {0}; __xrtExecutorMutationBegin(&Mutation);
	if (!pJob->Active || pJob->Finished) abort();
	pJob->Data = NULL; pJob->Destroy = NULL; pJob->DestroyContext = NULL;
	pJob->Active = false; pJob->Finished = true;
	(void)xrtAtomic64FetchAdd(bCancelled ? &pExecutor->Cancelled : &pExecutor->Executed, 1, XMEMORY_RELAXED);
	(void)xrtAtomic64FetchAdd(&pExecutor->Completed, 1, XMEMORY_RELAXED);
	(void)xrtAtomic32FetchSub(&pExecutor->Running, 1, XMEMORY_ACQ_REL);
	__xrtExecutorJobRelease(pJob);
	(void)xrtAtomic32FetchSub(&pExecutor->Outstanding, 1, XMEMORY_RELEASE);
	__xrtExecutorMutationEnd(&Mutation);
	__xrtExecutorNotifyIdle(pExecutor);
}

/* No mutation scope or native lock encloses Proc, Drop or thread-local cleanup. */
static void __xrtExecutorRun(xrt_executor_worker* pWorker, xrt_executor_job* pJob)
{
	pJob->Proc(pJob->Data);
	if (pJob->Destroy) pJob->Destroy(pJob->Data, pJob->DestroyContext);
	xrtClearError();
	(void)xrtTempClear();
	__xrtExecutorComplete(pWorker->Executor, pJob, false);
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
		xrtownershipscope Mutation = {0}; __xrtExecutorMutationBegin(&Mutation);
		xrt_executor_job* pJob = __xrtExecutorTake(pWorker);
		__xrtExecutorMutationEnd(&Mutation);
		if ( pJob != NULL ) {
			__xrtExecutorRun(pWorker, pJob);
			continue;
		}
		__xrtExecutorMutationBegin(&Mutation);
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
			pWorker->Parked = true;
			/* CondWait returns with SleepLock held. Release it BEFORE asking
			 * for mutation again, so an exclusive freeze never inverts locks. */
			__xrtExecutorMutationEnd(&Mutation);
			(void)xrtCondWait(
				&pExecutor->Work,
				&pExecutor->SleepLock
			);
			(void)xrtMutexUnlock(&pExecutor->SleepLock);
			__xrtExecutorMutationBegin(&Mutation);
			(void)xrtMutexLock(&pExecutor->SleepLock);
			pWorker->Parked = false;
			pJob = __xrtExecutorTake(pWorker);
		}
		(void)xrtAtomic32FetchSub(
			&pExecutor->Sleepers,
			1,
			XMEMORY_RELEASE
		);
		bool bExit = pJob == NULL && xrtAtomic32Load(&pExecutor->Shutdown, XMEMORY_ACQUIRE);
		if (bExit) pWorker->Exited = true;
		(void)xrtMutexUnlock(&pExecutor->SleepLock);
		__xrtExecutorMutationEnd(&Mutation);
		if ( pJob != NULL ) {
			__xrtExecutorRun(pWorker, pJob);
			continue;
		}
		if ( bExit ) {
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
	(void)xrtMutexUnit(&pExecutor->OwnershipLock);
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
		(Config.QueueLimit > ((UINT32_MAX - Config.Threads) / Config.Threads)) ) {
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
	pExecutor->SlotCount = iJobCount;
	pExecutor->References = 1;
	pExecutor->OwnerHeld = true;
	xrtAtomic32Init(&pExecutor->Closed, 0);
	xrtAtomic32Init(&pExecutor->Cancelling, 0);
	xrtAtomic32Init(&pExecutor->Shutdown, 0);
	xrtAtomic32Init(&pExecutor->Submitters, 0);
	xrtAtomic32Init(&pExecutor->Sleepers, 0);
	xrtAtomic32Init(&pExecutor->Queued, 0);
	xrtAtomic32Init(&pExecutor->Running, 0);
	xrtAtomic32Init(&pExecutor->Outstanding, 0);
	xrtAtomic64Init(&pExecutor->NextWorker, 0);
	xrtAtomic64Init(&pExecutor->Submitted, 0);
	xrtAtomic64Init(&pExecutor->Completed, 0);
	xrtAtomic64Init(&pExecutor->Executed, 0);
	xrtAtomic64Init(&pExecutor->Stolen, 0);
	xrtAtomic64Init(&pExecutor->Cancelled, 0);
	xrtAtomic64Init(&pExecutor->Rejected, 0);
	if ( !xrtMutexInit(&pExecutor->OwnershipLock) ) {
		xrtFree(pExecutor);
		return NULL;
	}
	if ( !xrtMutexInit(&pExecutor->SleepLock) ) {
		(void)xrtMutexUnit(&pExecutor->OwnershipLock);
		xrtFree(pExecutor);
		return NULL;
	}
	if ( !xrtCondInit(&pExecutor->Work) ) {
		(void)xrtMutexUnit(&pExecutor->SleepLock);
		(void)xrtMutexUnit(&pExecutor->OwnershipLock);
		xrtFree(pExecutor);
		return NULL;
	}
	if ( !xrtCondInit(&pExecutor->Idle) ) {
		(void)xrtCondUnit(&pExecutor->Work);
		(void)xrtMutexUnit(&pExecutor->SleepLock);
		(void)xrtMutexUnit(&pExecutor->OwnershipLock);
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
	xrtownershipscope Mutation = {0}; __xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	pExecutor->Initialized = true; __xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	return pExecutor;
}



/* 尝试把一组工作原子放入指定 Worker 的本地队列。 */
static bool __xrtExecutorSubmitTo(
	xexecutor* pExecutor,
	xrt_executor_worker* pWorker,
	const xexecutoritem* pItems,
	const xexecutorowneditemv1* pOwned,
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
		if (__xrtAtomicRefLoad(&pJob->RefCount) != 0) abort();
		pJob->RefCount = 1;
		pJob->DataPolicy = pOwned ? pOwned[i].Policy : NULL;
		pJob->Proc = pOwned ? pOwned[i].Policy->Proc : pItems[i].Proc;
		pJob->Data = pOwned ? pOwned[i].Data : pItems[i].Data;
		pJob->Destroy = pOwned ? pOwned[i].Policy->Drop : pItems[i].Destroy;
		pJob->DestroyContext = pOwned ? NULL : pItems[i].DestroyContext;
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
	(void)xrtAtomic32FetchAdd(&pExecutor->Outstanding, (uint32)iCount, XMEMORY_RELEASE);
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
static bool __xrtExecutorSubmitItemsBody(
	xexecutor* pExecutor,
	const xexecutoritem* pItems,
	const xexecutorowneditemv1* pOwned,
	size_t iCount
)
{
	xrt_executor_worker* pCurrent;
	uint32 iStart;

	size_t iItemSize = pOwned ? sizeof(*pOwned) : sizeof(*pItems);
	if ( ((pItems == NULL) == (pOwned == NULL)) ||
		(iCount == 0) || (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / iItemSize)) ||
		!__xrtRangeValid(pOwned ? (const void*)pOwned : (const void*)pItems, iCount * iItemSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xexecutordataownershipv1* pPolicy = pOwned ? pOwned[i].Policy : NULL;
		if ( pOwned ? (!pOwned[i].Data || !pPolicy || pPolicy->size != sizeof(*pPolicy) ||
			!pPolicy->Proc || !pPolicy->Drop || !pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace) :
			pItems[i].Proc == NULL ) {
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
	if (!__xrtExecutorReserveJobs(pExecutor, iCount)) {
		__xrtExecutorSubmitLeave(pExecutor); return false;
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

		xrtownershipscope Mutation = {0}; __xrtExecutorMutationBegin(&Mutation);
		bool bAccepted = __xrtExecutorSubmitTo(
			pExecutor,
			pWorker,
			pItems,
			pOwned,
			iCount
		);
		__xrtExecutorMutationEnd(&Mutation);
		if ( bAccepted ) {
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
	__xrtExecutorUnreserveJobs(pExecutor, iCount);
	__xrtExecutorSubmitLeave(pExecutor);
	__xrtErrorSetAgain();
	return false;
}

static bool __xrtExecutorSubmitItems(xexecutor* pExecutor, const xexecutoritem* pItems,
	const xexecutorowneditemv1* pOwned, size_t iCount)
{
	if (!__xrtExecutorEnter(pExecutor, false)) return false;
	bool bAccepted = __xrtExecutorSubmitItemsBody(pExecutor, pItems, pOwned, iCount);
	__xrtExecutorLeave(pExecutor); return bAccepted;
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

	return __xrtExecutorSubmitItems(pExecutor, &Item, NULL, 1);
}



/* 原子提交一组 detached 工作。 */
XRT_API bool xrtExecutorSubmitBatch(
	xexecutor* pExecutor,
	const xexecutoritem* pItems,
	size_t iCount
)
{
	return __xrtExecutorSubmitItems(pExecutor, pItems, NULL, iCount);
}

XRT_API bool xrtExecutorSubmitOwnedV1(xexecutor* pExecutor, ptr pData,
	const xexecutordataownershipv1* pPolicy)
{
	const xexecutorowneditemv1 Item = {pData, pPolicy};
	return __xrtExecutorSubmitItems(pExecutor, NULL, &Item, 1);
}
XRT_API bool xrtExecutorSubmitOwnedBatchV1(xexecutor* pExecutor,
	const xexecutorowneditemv1* pItems, size_t iCount)
{ return __xrtExecutorSubmitItems(pExecutor, NULL, pItems, iCount); }



/* 停止受理新工作，并让已经受理的工作自然排空。 */
static bool __xrtExecutorCloseBody(xexecutor* pExecutor)
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

XRT_API bool xrtExecutorClose(xexecutor* pExecutor)
{
	if (!__xrtExecutorEnter(pExecutor, false)) return false;
	bool bClosed = __xrtExecutorCloseBody(pExecutor);
	__xrtExecutorLeave(pExecutor); return bClosed;
}

static bool __xrtExecutorInCallback(xexecutor* pExecutor)
{
	if (__xrtExecutorCurrent(pExecutor)) return true;
	uint64 iThread = xrtThreadCurrentId(); bool bFound = false; xrtownershipscope Mutation = {0};
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	for (const xrt_executor_callback* pCallback = pExecutor->Callbacks; pCallback; pCallback = pCallback->Next)
		if (pCallback->Thread == iThread) { bFound = true; break; }
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation); return bFound;
}
static void __xrtExecutorCallbackBegin(xexecutor* pExecutor, xrt_executor_callback* pCallback)
{
	xrtownershipscope Mutation = {0}; pCallback->Thread = xrtThreadCurrentId();
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	pCallback->Next = pExecutor->Callbacks; pExecutor->Callbacks = pCallback;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
}
static void __xrtExecutorCallbackEnd(xexecutor* pExecutor, xrt_executor_callback* pCallback)
{
	xrtownershipscope Mutation = {0}; __xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	xrt_executor_callback** ppCallback = &pExecutor->Callbacks;
	while (*ppCallback && *ppCallback != pCallback) ppCallback = &(*ppCallback)->Next;
	if (!*ppCallback) abort();
	*ppCallback = pCallback->Next; __xrtExecutorOwnershipEnd(pExecutor, &Mutation);
}



/* 在锁内摘除所有尚未开始的工作，锁外执行数据析构。 */
static bool __xrtExecutorCancelBody(xexecutor* pExecutor)
{
	xrt_executor_job* pHead = NULL;
	xrt_executor_job* pTail = NULL;
	uint32 iCancelled = 0;

	if ( pExecutor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)__xrtExecutorCloseBody(pExecutor);
	xrtownershipscope Mutation = {0}; __xrtExecutorMutationBegin(&Mutation);
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
				if (pJob->Active || pJob->Finished) abort();
				pJob->Active = true;
				iCancelled++;
			}
			pWorker->Head = NULL;
			pWorker->Tail = NULL;
			pWorker->Queued = 0;
		}
		__xrtSpinUnlock(&pWorker->Lock);
	}
	if ( iCancelled != 0 ) {
		(void)xrtAtomic32FetchAdd(&pExecutor->Running, iCancelled, XMEMORY_ACQ_REL);
		(void)xrtAtomic32FetchSub(
			&pExecutor->Queued,
			iCancelled,
			XMEMORY_ACQ_REL
		);
	}
	__xrtExecutorMutationEnd(&Mutation);
	xrt_executor_callback Callback = {0}; __xrtExecutorCallbackBegin(pExecutor, &Callback);
	while ( pHead != NULL ) {
		xrt_executor_job* pJob = pHead;

		pHead = pJob->Next;
		if ( pJob->Destroy != NULL ) {
			pJob->Destroy(pJob->Data, pJob->DestroyContext);
		}
		__xrtExecutorComplete(pExecutor, pJob, true);
	}
	__xrtExecutorCallbackEnd(pExecutor, &Callback);
	__xrtExecutorWake(pExecutor, true);
	__xrtExecutorNotifyIdle(pExecutor);
	return true;
}

XRT_API bool xrtExecutorCancel(xexecutor* pExecutor)
{
	if (!__xrtExecutorEnter(pExecutor, false)) return false;
	bool bCancelled = __xrtExecutorCancelBody(pExecutor);
	__xrtExecutorLeave(pExecutor); return bCancelled;
}



/* 等待已经关闭的执行器排空到指定截止时间。 */
static xwaitresult __xrtExecutorWaitUntilBody(
	xexecutor* pExecutor,
	double iDeadline
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return XWAIT_ERROR; }

	xwaitresult Result = XWAIT_OK;

	if ( pExecutor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XWAIT_ERROR;
	}
	if ( __xrtExecutorInCallback(pExecutor) ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	if ( !xrtAtomic32Load(&pExecutor->Closed, XMEMORY_ACQUIRE) ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	(void)xrtMutexLock(&pExecutor->SleepLock);
	while ( !__xrtExecutorIdle(pExecutor) ) {
		if ( __xrtWaitExpired(iDeadline) ) {
			Result = XWAIT_TIMEOUT;
			break;
		}
		Result = __xrtCondWaitUntil(
			&pExecutor->Idle,
			&pExecutor->SleepLock,
			iDeadline
		);
		if ( Result == XWAIT_ERROR ) {
			break;
		}
		if ( Result == XWAIT_TIMEOUT ) {
			if ( __xrtExecutorIdle(pExecutor) ) {
				Result = XWAIT_OK;
			}
			break;
		}
	}
	(void)xrtMutexUnlock(&pExecutor->SleepLock);
	return Result;
}

XRT_API xwaitresult __xrtExecutorWaitUntil(xexecutor* pExecutor, double iDeadline)
{
	if (!__xrtWaitValid(iDeadline)) return XWAIT_ERROR;
	if (!__xrtExecutorEnter(pExecutor, false)) return XWAIT_ERROR;
	xwaitresult Result = __xrtExecutorWaitUntilBody(pExecutor, iDeadline);
	__xrtExecutorLeave(pExecutor); return Result;
}



/* 永久等待已经关闭的执行器排空。 */
XRT_API xwaitresult xrtExecutorWait(xexecutor* pExecutor)
{
	return __xrtExecutorWaitUntil(pExecutor, INFINITY);
}



/* 在相对超时内等待已经关闭的执行器排空。 */
XRT_API xwaitresult xrtExecutorWaitFor(
	xexecutor* pExecutor,
	int64 iTimeout
)
{
	return __xrtExecutorWaitUntil(
		pExecutor,
		__xrtWaitAfter(iTimeout)
	);
}



/* 复制当前负载和累计统计快照。 */
static bool __xrtExecutorGetBody(
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

XRT_API bool xrtExecutorGet(const xexecutor* pExecutor, xexecutorstats* pStats)
{
	if (!__xrtExecutorEnter((xexecutor*)pExecutor, false)) return false;
	bool bGot = __xrtExecutorGetBody(pExecutor, pStats);
	__xrtExecutorLeave((xexecutor*)pExecutor); return bGot;
}



/* 关闭、排空、停止 Worker 并释放执行器。 */
XRT_API bool xrtExecutorDestroy(xexecutor* pExecutor)
{
	if ( pExecutor == NULL ) {
		return true;
	}
	if (!__xrtExecutorEnter(pExecutor, true)) return false;
	xrtownershipscope Mutation = {0}; __xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	bool bReady = pExecutor->Entries == 1 && !pExecutor->Destroying && !pExecutor->Joining && !pExecutor->Retiring &&
		(!pExecutor->OwnershipClaim || pExecutor->OwnershipCleared) && !__xrtExecutorCurrent(pExecutor);
	bool bRetired = pExecutor->Retired;
	if (bReady) pExecutor->Destroying = true;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	if (!bReady) { __xrtExecutorLeave(pExecutor); __xrtErrorSetInvalidState(); return false; }
	if (!bRetired) {
		bReady = __xrtExecutorCloseBody(pExecutor) && __xrtExecutorWaitUntilBody(pExecutor, INFINITY) == XWAIT_OK;
		__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
		bReady = bReady && pExecutor->Entries == 1;
		if (bReady) pExecutor->Joining = true;
		__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
		if (bReady) {
			xrtAtomic32Store(&pExecutor->Shutdown, 1, XMEMORY_RELEASE);
			__xrtExecutorWake(pExecutor, true);
			for (uint32 i = 0; i < pExecutor->StartedThreads; ++i)
				if (xrtThreadWait(pExecutor->Workers[i].Thread) != XWAIT_OK) bReady = false;
			__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
			pExecutor->Joining = false; pExecutor->Joined = bReady;
			__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
			if (bReady) bReady = __xrtExecutorRetire(pExecutor, 1);
		}
	}
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	bool bOwner = bReady && pExecutor->OwnerHeld;
	if (bOwner) pExecutor->OwnerHeld = false;
	pExecutor->Destroying = false;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	if (bOwner) __xrtExecutorRelease(pExecutor);
	__xrtExecutorLeave(pExecutor); return bReady;
}

#endif
