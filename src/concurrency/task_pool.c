#include "../internal/xrt_task.h"



#if defined(XRT_FEATURE_TASK_POOL)

typedef struct xrt_task_worker {
	struct xtaskpool* Pool;
	xthread* Thread;
	bool Parked;
	bool Exited;
} xrt_task_worker;



/* 任务池用一把锁统一保护队列、运行链、统计和生命周期状态。 */
struct xtaskpool {
	xmutex Lock;
	xmutex OwnershipLock;
	xcond Work;
	xcond Idle;
	xcond Space;
	xrt_task_worker* Workers;
	uint32 ThreadCount;
	uint32 StartedThreads;
	size_t QueueLimit;
	size_t StackSize;
	size_t QueueDepth;
	size_t Queued;
	size_t Running;
	size_t FinalizerQueued;
	size_t FinalizerRunning;
	uint64 Submitted;
	uint64 Completed;
	uint64 Succeeded;
	uint64 Failed;
	uint64 Cancelled;
	uint64 Rejected;
	bool Closed;
	bool Cancelling;
	bool Shutdown;
	xrt_task_job* Head;
	xrt_task_job* Tail;
	xrt_task_job* RunningHead;
	xrt_task_finalizer* FinalizerHead;
	xrt_task_finalizer* FinalizerTail;
	size_t References;
	size_t Entries;
	const void* OwnershipClaim;
	bool Initialized;
	bool OwnerHeld;
	bool Destroying;
	bool Joining;
	bool Joined;
	bool Retiring;
	bool Retired;
	bool OwnershipCleared;
};

/* OwnershipLock never encloses a queue lock, a wait, or a user callback. Native
 * bodies keep an actual reference/entry while their old queue-lock protocol is
 * in use. Count refuses that whole interval, including detached cancellation
 * lists, cancel-watch removal and user cleanup tails. */
static void __xrtTaskPoolOwnershipBegin(xtaskpool* pPool, xrtownershipscope* pScope)
{
	if (!xrtOwnershipMutationBegin(pScope) || !xrtMutexLock(&pPool->OwnershipLock)) abort();
}
static void __xrtTaskPoolOwnershipEnd(xtaskpool* pPool, xrtownershipscope* pScope)
{
	if (!xrtMutexUnlock(&pPool->OwnershipLock) || !xrtOwnershipScopeEnd(pScope)) abort();
}
static bool __xrtTaskPoolEnter(xtaskpool* pPool, bool bTerminal)
{
	xrtownershipscope Mutation = {0}; bool bEntered = false;
	if (pPool == NULL) { __xrtErrorSetInvalidArgument(); return false; }
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	if (pPool->References && pPool->References < SIZE_MAX && pPool->Entries < SIZE_MAX &&
		!pPool->Retiring && !pPool->Joining && (!pPool->Retired || bTerminal)) {
		++pPool->References; ++pPool->Entries; bEntered = true;
	}
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
	if (!bEntered) __xrtErrorSetInvalidState();
	return bEntered;
}
static void __xrtTaskPoolDropLocked(xtaskpool* pPool, xrtownershipscope* pScope)
{
	if (!pPool->References) abort();
	bool bLast = --pPool->References == 0;
	if (bLast && (!pPool->Retired || pPool->OwnerHeld || pPool->Entries || pPool->Workers)) abort();
	if (!xrtMutexUnlock(&pPool->OwnershipLock)) abort();
	if (bLast) {
		if (!xrtCondUnit(&pPool->Space) || !xrtCondUnit(&pPool->Idle) || !xrtCondUnit(&pPool->Work) ||
			!xrtMutexUnit(&pPool->Lock) || !xrtMutexUnit(&pPool->OwnershipLock)) abort();
		xrtFree(pPool);
	}
	if (!xrtOwnershipScopeEnd(pScope)) abort();
}
static void __xrtTaskPoolRelease(const void* pData)
{
	xtaskpool* pPool = (xtaskpool*)pData; xrtownershipscope Mutation = {0};
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation); __xrtTaskPoolDropLocked(pPool, &Mutation);
}
static void __xrtTaskPoolLeave(xtaskpool* pPool)
{
	xrtownershipscope Mutation = {0}; __xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	if (!pPool->Entries) abort();
	--pPool->Entries; __xrtTaskPoolDropLocked(pPool, &Mutation);
}
static void __xrtTaskPoolWorkerBegin(xtaskpool* pPool, xrtownershipscope* pScope)
{
	if (!xrtOwnershipMutationBegin(pScope) || !xrtMutexLock(&pPool->Lock)) abort();
}
static void __xrtTaskPoolWorkerEnd(xtaskpool* pPool, xrtownershipscope* pScope)
{
	if (!xrtMutexUnlock(&pPool->Lock) || !xrtOwnershipScopeEnd(pScope)) abort();
}



/* 判断普通任务与资源回收过程是否都已经排空。 */
static bool __xrtTaskPoolIdleLocked(const xtaskpool* pPool)
{
	return (pPool->Queued == 0) &&
		(pPool->Running == 0) &&
		(pPool->FinalizerQueued == 0) &&
		(pPool->FinalizerRunning == 0);
}



/* 判断调用线程是否属于指定任务池。 */
static bool __xrtTaskPoolIsWorker(const xtaskpool* pPool)
{
	xthread* pCurrent = xrtThreadCurrent();

	if ( pCurrent == NULL ) {
		return false;
	}
	for ( uint32 i = 0; i < pPool->StartedThreads; i++ ) {
		if ( pPool->Workers[i].Thread == pCurrent ) {
			return true;
		}
	}
	return false;
}



/* 调用方取消容量或排空等待时，唤醒两类条件变量重新检查状态。 */
static void __xrtTaskPoolWakeWaiters(ptr pData)
{
	xtaskpool* pPool = (xtaskpool*)pData;

	(void)xrtMutexLock(&pPool->Lock);
	(void)xrtCondBroadcast(&pPool->Space);
	(void)xrtCondBroadcast(&pPool->Idle);
	(void)xrtMutexUnlock(&pPool->Lock);
}



/* 在锁内按 Future 终态更新任务完成统计。 */
static void __xrtTaskPoolRecordLocked(xtaskpool* pPool, xfuturestate State)
{
	pPool->Completed++;
	if ( State == XFUTURE_RESOLVED ) {
		pPool->Succeeded++;
	} else if ( State == XFUTURE_FAILED ) {
		pPool->Failed++;
	} else {
		pPool->Cancelled++;
	}
}



/* 从双向运行链中以固定成本移除指定作业。 */
static void __xrtTaskPoolRunningRemoveLocked(
	xtaskpool* pPool,
	xrt_task_job* pJob
)
{
	if ( pJob->RunningPrevious != NULL ) {
		pJob->RunningPrevious->RunningNext = pJob->RunningNext;
	} else if ( pPool->RunningHead == pJob ) {
		pPool->RunningHead = pJob->RunningNext;
	}
	if ( pJob->RunningNext != NULL ) {
		pJob->RunningNext->RunningPrevious = pJob->RunningPrevious;
	}
	pJob->RunningPrevious = NULL;
	pJob->RunningNext = NULL;
}



/* 队列满时摘出已经请求取消的作业，为新任务及时腾出硬上限槽位。 */
static xrt_task_job* __xrtTaskPoolPruneCancelledLocked(xtaskpool* pPool)
{
	xrt_task_job* pCancelledHead = NULL;
	xrt_task_job* pCancelledTail = NULL;
	xrt_task_job* pPrevious = NULL;
	xrt_task_job** ppJob = &pPool->Head;

	while ( *ppJob != NULL ) {
		xrt_task_job* pJob = *ppJob;

		if ( xrtCancelRequested(pJob->Cancel) ) {
			*ppJob = pJob->Next;
			pJob->Next = NULL;
			if ( pCancelledTail != NULL ) {
				pCancelledTail->Next = pJob;
			} else {
				pCancelledHead = pJob;
			}
			pCancelledTail = pJob;
			pPool->QueueDepth--;
		} else {
			pPrevious = pJob;
			ppJob = &pJob->Next;
		}
	}
	pPool->Tail = pPrevious;
	if ( pCancelledHead != NULL ) {
		(void)xrtCondBroadcast(&pPool->Space);
	}
	return pCancelledHead;
}



/* 在池锁外完成一组已经安全摘除的取消任务。 */
static void __xrtTaskPoolFinishCancelled(
	xtaskpool* pPool,
	xrt_task_job* pList
)
{
	while ( pList != NULL ) {
		xrt_task_job* pJob = pList;

		pList = pJob->Next;
		pJob->Next = NULL;
		__xrtTaskCancel(pJob);
		__xrtTaskDestroy(pJob, true);
		(void)xrtMutexLock(&pPool->Lock);
		pPool->Queued--;
		__xrtTaskPoolRecordLocked(pPool, XFUTURE_CANCELLED);
		if ( __xrtTaskPoolIdleLocked(pPool) ) {
			(void)xrtCondBroadcast(&pPool->Idle);
		}
		(void)xrtMutexUnlock(&pPool->Lock);
	}
}



/* 工作线程持续消费普通任务和 finalizer，只在 Destroy 发出 Shutdown 后退出。 */
static int32 __xrtTaskPoolWorker(ptr pData)
{
	xrt_task_worker* pWorker = (xrt_task_worker*)pData;
	xtaskpool* pPool = pWorker->Pool;

	for ( ;; ) {
		xrt_task_job* pJob;
		xrt_task_finalizer* pFinalizer;
		xfuturestate State;
		xrtownershipscope Mutation = {0};

		__xrtTaskPoolWorkerBegin(pPool, &Mutation);
		while (
			(pPool->Head == NULL) &&
			(pPool->FinalizerHead == NULL) &&
			!pPool->Shutdown
		) {
			/* Publish the real worker state before sleeping. CondWait may return
			 * with the queue lock held while a collector owns freeze: release
			 * that lock BEFORE reacquiring mutation to avoid lock inversion. */
			pWorker->Parked = true;
			if (!xrtOwnershipScopeEnd(&Mutation)) abort();
			(void)xrtCondWait(&pPool->Work, &pPool->Lock);
			(void)xrtMutexUnlock(&pPool->Lock);
			__xrtTaskPoolWorkerBegin(pPool, &Mutation);
			pWorker->Parked = false;
		}

		/*
			优先回收已关闭资源，避免普通任务洪峰长期占用文件和套接字。
			节点属于资源自身，回调返回后不能再访问节点。
		*/
		pFinalizer = pPool->FinalizerHead;
		if ( pFinalizer != NULL ) {
			pPool->FinalizerHead = pFinalizer->Next;
			if ( pPool->FinalizerHead == NULL ) {
				pPool->FinalizerTail = NULL;
			}
			pPool->FinalizerQueued--;
			pPool->FinalizerRunning++;
			__xrtTaskPoolWorkerEnd(pPool, &Mutation);

			pFinalizer->Proc(pFinalizer->Data);

			__xrtTaskPoolWorkerBegin(pPool, &Mutation);
			pPool->FinalizerRunning--;
			if ( __xrtTaskPoolIdleLocked(pPool) ) {
				(void)xrtCondBroadcast(&pPool->Idle);
			}
			__xrtTaskPoolWorkerEnd(pPool, &Mutation);
			continue;
		}
		if ( pPool->Head == NULL ) {
			pWorker->Exited = true;
			__xrtTaskPoolWorkerEnd(pPool, &Mutation);
			break;
		}
		pJob = pPool->Head;
		pPool->Head = pJob->Next;
		if ( pPool->Head == NULL ) {
			pPool->Tail = NULL;
		}
		pJob->Next = NULL;
		pJob->RunningPrevious = NULL;
		pJob->RunningNext = pPool->RunningHead;
		if ( pPool->RunningHead != NULL ) {
			pPool->RunningHead->RunningPrevious = pJob;
		}
		pPool->RunningHead = pJob;
		pPool->QueueDepth--;
		pPool->Queued--;
		pPool->Running++;
		(void)xrtCondSignal(&pPool->Space);
		__xrtTaskPoolWorkerEnd(pPool, &Mutation);

		/* Future 取消只请求协作；在真正执行前再次检查即可跳过过程。 */
		__xrtTaskRun(pJob);
		State = __xrtTaskState(pJob);

		__xrtTaskPoolWorkerBegin(pPool, &Mutation);
		__xrtTaskPoolRunningRemoveLocked(pPool, pJob);
		__xrtTaskPoolWorkerEnd(pPool, &Mutation);
		/* The worker's physical Job reference and cleanup tail remain active
		 * until Destroy returns. Wait/retirement must not observe false idle. */
		__xrtTaskDestroy(pJob, true);
		__xrtTaskPoolWorkerBegin(pPool, &Mutation);
		pPool->Running--;
		__xrtTaskPoolRecordLocked(pPool, State);
		if ( __xrtTaskPoolIdleLocked(pPool) ) {
			(void)xrtCondBroadcast(&pPool->Idle);
		}
		__xrtTaskPoolWorkerEnd(pPool, &Mutation);
	}
	return 0;
}



/* 停止创建失败的半成品任务池并释放已启动线程。 */
static void __xrtTaskPoolCreateCleanup(xtaskpool* pPool)
{
	(void)xrtMutexLock(&pPool->Lock);
	pPool->Closed = true;
	pPool->Shutdown = true;
	(void)xrtCondBroadcast(&pPool->Work);
	(void)xrtMutexUnlock(&pPool->Lock);
	for ( uint32 i = 0; i < pPool->StartedThreads; i++ ) {
		(void)xrtThreadWait(pPool->Workers[i].Thread);
		xrtThreadDestroy(pPool->Workers[i].Thread);
	}
	(void)xrtCondUnit(&pPool->Space);
	(void)xrtCondUnit(&pPool->Idle);
	(void)xrtCondUnit(&pPool->Work);
	(void)xrtMutexUnit(&pPool->Lock);
	(void)xrtMutexUnit(&pPool->OwnershipLock);
	xrtFree(pPool->Workers);
	xrtFree(pPool);
}



/* 创建有界任务池并启动全部工作线程。 */
XRT_API xtaskpool* xrtTaskPoolCreate(const xtaskpoolconfig* pConfig)
{
	xtaskpoolconfig tConfig;
	xtaskpool* pPool;
	xerror* pError;
	bool bDefaultThreads;

	memset(&tConfig, 0, sizeof(tConfig));
	if ( pConfig != NULL ) {
		tConfig = *pConfig;
	}
	bDefaultThreads = tConfig.Threads == 0;
	if ( bDefaultThreads ) {
		tConfig.Threads = __xrtProcessorCount();
		if ( tConfig.Threads > XRT_TASK_POOL_THREAD_LIMIT ) {
			tConfig.Threads = XRT_TASK_POOL_THREAD_LIMIT;
		}
	}
	if ( tConfig.QueueLimit == 0 ) {
		tConfig.QueueLimit = XRT_TASK_POOL_QUEUE_LIMIT_DEFAULT;
	}
	if ( tConfig.Threads > XRT_TASK_POOL_THREAD_LIMIT ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pPool = (xtaskpool*)xrtCalloc(1, sizeof(xtaskpool));
	if ( pPool == NULL ) {
		return NULL;
	}
	pPool->ThreadCount = tConfig.Threads;
	pPool->References = 1;
	pPool->OwnerHeld = true;
	pPool->QueueLimit = tConfig.QueueLimit;
	pPool->StackSize = tConfig.StackSize;
	if ( !xrtMutexInit(&pPool->Lock) ) {
		xrtFree(pPool);
		return NULL;
	}
	if ( !xrtMutexInit(&pPool->OwnershipLock) ) {
		(void)xrtMutexUnit(&pPool->Lock);
		xrtFree(pPool);
		return NULL;
	}
	if ( !xrtCondInit(&pPool->Work) ) {
		(void)xrtMutexUnit(&pPool->OwnershipLock);
		(void)xrtMutexUnit(&pPool->Lock);
		xrtFree(pPool);
		return NULL;
	}
	if ( !xrtCondInit(&pPool->Idle) ) {
		(void)xrtMutexUnit(&pPool->OwnershipLock);
		(void)xrtCondUnit(&pPool->Work);
		(void)xrtMutexUnit(&pPool->Lock);
		xrtFree(pPool);
		return NULL;
	}
	if ( !xrtCondInit(&pPool->Space) ) {
		(void)xrtMutexUnit(&pPool->OwnershipLock);
		(void)xrtCondUnit(&pPool->Idle);
		(void)xrtCondUnit(&pPool->Work);
		(void)xrtMutexUnit(&pPool->Lock);
		xrtFree(pPool);
		return NULL;
	}
	pPool->Workers = (xrt_task_worker*)xrtCalloc(
		tConfig.Threads,
		sizeof(xrt_task_worker)
	);
	if ( pPool->Workers == NULL ) {
		(void)xrtMutexUnit(&pPool->OwnershipLock);
		(void)xrtCondUnit(&pPool->Space);
		(void)xrtCondUnit(&pPool->Idle);
		(void)xrtCondUnit(&pPool->Work);
		(void)xrtMutexUnit(&pPool->Lock);
		xrtFree(pPool);
		return NULL;
	}

	/* 持有池锁发布线程对象，确保工作入口不会观察到半初始化槽位。 */
	for ( uint32 i = 0; i < tConfig.Threads; i++ ) {
		xrt_task_worker* pWorker = &pPool->Workers[i];

		pWorker->Pool = pPool;
		(void)xrtMutexLock(&pPool->Lock);
		pWorker->Thread = xrtThreadCreate(
			__xrtTaskPoolWorker,
			pWorker,
			tConfig.StackSize
		);
		if ( pWorker->Thread != NULL ) {
			pPool->StartedThreads++;
		}
		(void)xrtMutexUnlock(&pPool->Lock);
		if ( pWorker->Thread == NULL ) {
			pError = xrtTakeError();
			if ( pError == NULL ) {
				__xrtErrorSetInternal();
				pError = xrtTakeError();
			}
			__xrtTaskPoolCreateCleanup(pPool);
			xrtSetError(pError);
			xrtErrorFree(pError);
			return NULL;
		}
	}
	xrtownershipscope Mutation = {0};
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	pPool->Initialized = true;
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
	return pPool;
}



typedef enum xrt_task_submit_stop {
	XRT_TASK_SUBMIT_NONE = 0,
	XRT_TASK_SUBMIT_CLOSED,
	XRT_TASK_SUBMIT_FULL,
	XRT_TASK_SUBMIT_TIMEOUT,
	XRT_TASK_SUBMIT_CANCELLED,
	XRT_TASK_SUBMIT_WORKER,
	XRT_TASK_SUBMIT_ERROR
} xrt_task_submit_stop;



/* 按立即或可等待模式提交，并且只在受理成功后取得任务数据所有权。 */
static xfuture* __xrtTaskPoolSubmitBody(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xfutureownershiptrace pResultTrace,
	const xfuturepayloadownershipv1* pResultPolicy,
	const xtaskdataownershipv1* pDataPolicy,
	bool bWait,
	xdeadline iDeadline,
	xcancel* pWaitCancel
)
{
	xrt_task_job* pJob;
	xfuture* pFuture;
	xcancelwatch* pTaskWatch = NULL;
	xcancelwatch* pWaitWatch = NULL;
	xerror* pWaitError = NULL;
	xrt_task_job* pCancelledList = NULL;
	xrt_task_submit_stop Stop = XRT_TASK_SUBMIT_NONE;
	bool bCancelled = false;
	bool bWorker = false;
	bool bWorkerChecked = false;

	if ( (pPool == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pJob = __xrtTaskCreateOwned(pProc, pData, pArgs, pDataPolicy, &pFuture);
	if ( pJob == NULL ) {
		return NULL;
	}
	pJob->ResultTrace = pResultTrace;
	pJob->ResultPolicy = pResultPolicy;
	if ( bWait && (pArgs != NULL) && (pArgs->Cancel != NULL) ) {
		pTaskWatch = xrtCancelWatch(
			pJob->Cancel,
			__xrtTaskPoolWakeWaiters,
			pPool
		);
		if ( pTaskWatch == NULL ) {
			__xrtTaskDestroy(pJob, false);
			return NULL;
		}
	}
	if ( bWait && (pWaitCancel != NULL) ) {
		pWaitWatch = xrtCancelWatch(
			pWaitCancel,
			__xrtTaskPoolWakeWaiters,
			pPool
		);
		if ( pWaitWatch == NULL ) {
			pWaitError = xrtTakeError();
			xrtCancelUnwatch(pTaskWatch);
			__xrtTaskDestroy(pJob, false);
			if ( pWaitError != NULL ) {
				xrtSetError(pWaitError);
				xrtErrorFree(pWaitError);
			} else {
				__xrtErrorSetInternal();
			}
			return NULL;
		}
	}

	(void)xrtMutexLock(&pPool->Lock);
	for ( ;; ) {
		if ( pPool->Closed ) {
			Stop = XRT_TASK_SUBMIT_CLOSED;
			break;
		}
		bCancelled = xrtCancelRequested(pJob->Cancel);
		if ( bCancelled || (pPool->QueueDepth < pPool->QueueLimit) ) {
			break;
		}

		pCancelledList = __xrtTaskPoolPruneCancelledLocked(pPool);
		if ( pCancelledList != NULL ) {
			(void)xrtMutexUnlock(&pPool->Lock);
			__xrtTaskPoolFinishCancelled(pPool, pCancelledList);
			(void)xrtMutexLock(&pPool->Lock);
			continue;
		}
		if ( !bWait ) {
			Stop = XRT_TASK_SUBMIT_FULL;
			break;
		}
		if ( !bWorkerChecked ) {
			bWorker = __xrtTaskPoolIsWorker(pPool);
			bWorkerChecked = true;
		}
		if ( bWorker ) {
			Stop = XRT_TASK_SUBMIT_WORKER;
			break;
		}
		if ( (pWaitCancel != NULL) && xrtCancelRequested(pWaitCancel) ) {
			Stop = XRT_TASK_SUBMIT_CANCELLED;
			break;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			Stop = XRT_TASK_SUBMIT_TIMEOUT;
			break;
		}
		if ( xrtCondWaitUntil(
			&pPool->Space,
			&pPool->Lock,
			iDeadline
		) == XWAIT_ERROR ) {
			Stop = XRT_TASK_SUBMIT_ERROR;
			pWaitError = xrtTakeError();
			break;
		}
	}

	if ( Stop == XRT_TASK_SUBMIT_NONE ) {
		__xrtTaskAccept(pJob);
		pPool->Submitted++;
	}
	if ( (Stop == XRT_TASK_SUBMIT_NONE) && !bCancelled ) {
		if ( pPool->Tail != NULL ) {
			pPool->Tail->Next = pJob;
		} else {
			pPool->Head = pJob;
		}
		pPool->Tail = pJob;
		pPool->QueueDepth++;
		pPool->Queued++;
		(void)xrtCondSignal(&pPool->Work);
	} else if ( Stop != XRT_TASK_SUBMIT_NONE ) {
		pPool->Rejected++;
	}
	(void)xrtMutexUnlock(&pPool->Lock);
	xrtCancelUnwatch(pWaitWatch);
	xrtCancelUnwatch(pTaskWatch);

	if ( Stop != XRT_TASK_SUBMIT_NONE ) {
		__xrtTaskDestroy(pJob, false);
		if ( Stop == XRT_TASK_SUBMIT_CLOSED ) {
			__xrtErrorSetClosed();
		} else if ( Stop == XRT_TASK_SUBMIT_FULL ) {
			__xrtErrorSetAgain();
		} else if ( Stop == XRT_TASK_SUBMIT_TIMEOUT ) {
			__xrtErrorSetTimeout();
		} else if ( Stop == XRT_TASK_SUBMIT_CANCELLED ) {
			__xrtErrorSetCancelled();
		} else if ( Stop == XRT_TASK_SUBMIT_WORKER ) {
			__xrtErrorSetInvalidState();
		} else if ( pWaitError != NULL ) {
			xrtSetError(pWaitError);
		} else {
			__xrtErrorSetInternal();
		}
		xrtErrorFree(pWaitError);
		return NULL;
	}
	if ( !bCancelled ) {
		return pFuture;
	}

	/* 已经取消的父上下文仍返回一个有效且立即完成的任务 Future。 */
	__xrtTaskCancel(pJob);
	__xrtTaskDestroy(pJob, true);
	(void)xrtMutexLock(&pPool->Lock);
	__xrtTaskPoolRecordLocked(pPool, XFUTURE_CANCELLED);
	if ( __xrtTaskPoolIdleLocked(pPool) ) {
		(void)xrtCondBroadcast(&pPool->Idle);
	}
	(void)xrtMutexUnlock(&pPool->Lock);
	return pFuture;
}

static xfuture* __xrtTaskPoolSubmit(xtaskpool* pPool, xtaskproc pProc, ptr pData,
	const xtaskargs* pArgs, xfutureownershiptrace pResultTrace,
	const xfuturepayloadownershipv1* pResultPolicy, const xtaskdataownershipv1* pDataPolicy,
	bool bWait, xdeadline iDeadline, xcancel* pWaitCancel)
{
	if (!__xrtTaskPoolEnter(pPool, false)) return NULL;
	xfuture* pFuture = __xrtTaskPoolSubmitBody(pPool, pProc, pData, pArgs,
		pResultTrace, pResultPolicy, pDataPolicy, bWait, iDeadline, pWaitCancel);
	__xrtTaskPoolLeave(pPool); return pFuture;
}



/* 立即尝试提交；队列已满时返回 AGAIN。 */
XRT_API xfuture* xrtTaskSubmit(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
)
{
	return __xrtTaskPoolSubmit(
		pPool,
		pProc,
		pData,
		pArgs,
		NULL,
		NULL,
		NULL,
		false,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



XRT_API xfuture* xrtTaskSubmitTraced(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xfutureownershiptrace pResultTrace
)
{
	if ( pResultTrace == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtTaskPoolSubmit(pPool, pProc, pData, pArgs,
		pResultTrace, NULL, NULL, false, XRT_DEADLINE_NEVER, NULL);
}

XRT_API xfuture* xrtTaskSubmitOwnedPolicyV1(xtaskpool* pPool, xtaskproc pProc,
	ptr pData, const xtaskargs* pArgs, const xfuturepayloadownershipv1* pPolicy)
{
	if (pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) || pPolicy->Drop == NULL || pPolicy->Trace == NULL) {
		__xrtErrorSetInvalidArgument(); return NULL;
	}
	return __xrtTaskPoolSubmit(pPool, pProc, pData, pArgs,
		pPolicy->Trace, pPolicy, NULL, false, XRT_DEADLINE_NEVER, NULL);
}

XRT_API xfuture* xrtTaskSubmitOwnedJobV1(xtaskpool* pPool, ptr pData, xcancel* pCancel,
	const xtaskdataownershipv1* pDataPolicy, const xfuturepayloadownershipv1* pResultPolicy)
{
	if (pData == NULL || pDataPolicy == NULL || pDataPolicy->size != sizeof(*pDataPolicy) ||
		pDataPolicy->Proc == NULL || pDataPolicy->Drop == NULL || pDataPolicy->Ops == NULL ||
		pDataPolicy->Ops->Count == NULL || pDataPolicy->Ops->Trace == NULL ||
		pResultPolicy == NULL || pResultPolicy->size != sizeof(*pResultPolicy) ||
		pResultPolicy->Drop == NULL || pResultPolicy->Trace == NULL) {
		__xrtErrorSetInvalidArgument(); return NULL;
	}
	xtaskargs Args = {pCancel, pDataPolicy->Drop, NULL};
	return __xrtTaskPoolSubmit(pPool, pDataPolicy->Proc, pData, &Args,
		pResultPolicy->Trace, pResultPolicy, pDataPolicy, false, XRT_DEADLINE_NEVER, NULL);
}



/* 永久等待队列槽位后提交。 */
XRT_API xfuture* xrtTaskSubmitWait(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
)
{
	return xrtTaskSubmitUntilCancel(
		pPool,
		pProc,
		pData,
		pArgs,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 在相对超时内等待队列槽位后提交。 */
XRT_API xfuture* xrtTaskSubmitFor(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
)
{
	return xrtTaskSubmitUntilCancel(
		pPool,
		pProc,
		pData,
		pArgs,
		xrtDeadlineAfter(iTimeout),
		NULL
	);
}



/* 等待到指定截止时间后提交。 */
XRT_API xfuture* xrtTaskSubmitUntil(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
)
{
	return xrtTaskSubmitUntilCancel(
		pPool,
		pProc,
		pData,
		pArgs,
		iDeadline,
		NULL
	);
}



/* 等待队列槽位、截止时间或调用方取消中的首个事件。 */
XRT_API xfuture* xrtTaskSubmitUntilCancel(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtTaskPoolSubmit(
		pPool,
		pProc,
		pData,
		pArgs,
		NULL,
		NULL,
		NULL,
		true,
		iDeadline,
		pCancel
	);
}



/*
	投递一个无分配资源回收过程。
	该内部通道不受普通队列上限和 Closed 状态影响，但调用方必须保证池仍存活。
*/
static void __xrtTaskPoolFinalizeBody(
	xtaskpool* pPool,
	xrt_task_finalizer* pFinalizer,
	xrt_task_finalizer_proc pProc,
	ptr pData
)
{
	/*
		内部调用点都在资源所有权已转移后到达这里。
		参数失效时同步回收是最后一道防泄漏保护，不属于正常执行路径。
	*/
	if ( (pPool == NULL) ||
		(pFinalizer == NULL) ||
		(pProc == NULL) ) {
		if ( pProc != NULL ) {
			pProc(pData);
		}
		return;
	}

	pFinalizer->Next = NULL;
	pFinalizer->Proc = pProc;
	pFinalizer->Data = pData;
	(void)xrtMutexLock(&pPool->Lock);
	if ( pPool->FinalizerTail != NULL ) {
		pPool->FinalizerTail->Next = pFinalizer;
	} else {
		pPool->FinalizerHead = pFinalizer;
	}
	pPool->FinalizerTail = pFinalizer;
	pPool->FinalizerQueued++;
	(void)xrtCondSignal(&pPool->Work);
	(void)xrtMutexUnlock(&pPool->Lock);
}



/* 停止接收普通任务；工作线程保留到 Destroy，以便回收已受理资源。 */
static bool __xrtTaskPoolCloseBody(xtaskpool* pPool)
{
	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pPool->Lock);
	pPool->Closed = true;
	(void)xrtCondBroadcast(&pPool->Space);
	if ( __xrtTaskPoolIdleLocked(pPool) ) {
		(void)xrtCondBroadcast(&pPool->Idle);
	}
	(void)xrtMutexUnlock(&pPool->Lock);
	return true;
}

void __xrtTaskPoolFinalize(xtaskpool* pPool, xrt_task_finalizer* pFinalizer,
	xrt_task_finalizer_proc pProc, ptr pData)
{
	if (!pPool || !pFinalizer || !pProc) {
		__xrtTaskPoolFinalizeBody(pPool, pFinalizer, pProc, pData); return;
	}
	/* Internal submitters already own accepted resource-lifetime credit. A
	 * joined/retired pool is a violated caller lifetime, not permission to run
	 * its resource destructor on an arbitrary thread. */
	if (!__xrtTaskPoolEnter(pPool, false)) abort();
	__xrtTaskPoolFinalizeBody(pPool, pFinalizer, pProc, pData);
	__xrtTaskPoolLeave(pPool);
}

XRT_API bool xrtTaskPoolClose(xtaskpool* pPool)
{
	if (!__xrtTaskPoolEnter(pPool, false)) return false;
	bool bClosed = __xrtTaskPoolCloseBody(pPool);
	__xrtTaskPoolLeave(pPool); return bClosed;
}



/* 取出一个尚未通知取消的运行任务令牌。 */
static xcancel* __xrtTaskPoolNextRunningCancel(xtaskpool* pPool)
{
	xcancel* pCancel = NULL;

	(void)xrtMutexLock(&pPool->Lock);
	for ( xrt_task_job* pJob = pPool->RunningHead;
		pJob != NULL; pJob = pJob->RunningNext ) {
		if ( !pJob->CancelPosted ) {
			pJob->CancelPosted = true;
			pCancel = xrtCancelRef(pJob->Cancel);
			break;
		}
	}
	(void)xrtMutexUnlock(&pPool->Lock);
	return pCancel;
}



/* 取消排队任务，并在池锁外逐个通知运行任务。 */
static bool __xrtTaskPoolCancelBody(xtaskpool* pPool)
{
	xrt_task_job* pList;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pPool->Lock);
	pPool->Closed = true;
	pPool->Cancelling = true;
	pList = pPool->Head;
	pPool->Head = NULL;
	pPool->Tail = NULL;
	pPool->QueueDepth = 0;
	(void)xrtCondBroadcast(&pPool->Work);
	(void)xrtCondBroadcast(&pPool->Space);
	(void)xrtMutexUnlock(&pPool->Lock);

	/* 排队计数直到对应 Future 完成后才减少，保持 Wait 的严格含义。 */
	__xrtTaskPoolFinishCancelled(pPool, pList);

	for ( ;; ) {
		xcancel* pCancel = __xrtTaskPoolNextRunningCancel(pPool);

		if ( pCancel == NULL ) {
			break;
		}
		(void)xrtCancelRequest(pCancel);
		xrtCancelDestroy(pCancel);
	}
	return true;
}

XRT_API bool xrtTaskPoolCancel(xtaskpool* pPool)
{
	if (!__xrtTaskPoolEnter(pPool, false)) return false;
	bool bCancelled = __xrtTaskPoolCancelBody(pPool);
	__xrtTaskPoolLeave(pPool); return bCancelled;
}



/* 永久等待已关闭任务池排空。 */
XRT_API xwaitresult xrtTaskPoolWait(xtaskpool* pPool)
{
	return xrtTaskPoolWaitUntilCancel(
		pPool,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 在相对微秒数内等待已关闭任务池排空。 */
XRT_API xwaitresult xrtTaskPoolWaitFor(xtaskpool* pPool, uint64 iTimeout)
{
	return xrtTaskPoolWaitUntilCancel(
		pPool,
		xrtDeadlineAfter(iTimeout),
		NULL
	);
}



/* 等待已关闭任务池中的全部任务完成到指定截止时间。 */
XRT_API xwaitresult xrtTaskPoolWaitUntil(xtaskpool* pPool, xdeadline iDeadline)
{
	return xrtTaskPoolWaitUntilCancel(pPool, iDeadline, NULL);
}



/* 等待排空、截止时间或调用方取消；已经排空时完成优先。 */
static xwaitresult __xrtTaskPoolWaitUntilCancelBody(
	xtaskpool* pPool,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xcancelwatch* pWatch = NULL;
	xerror* pWaitError = NULL;
	xwaitresult Result = XWAIT_OK;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XWAIT_ERROR;
	}
	if ( __xrtTaskPoolIsWorker(pPool) ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	if ( pCancel != NULL ) {
		pWatch = xrtCancelWatch(
			pCancel,
			__xrtTaskPoolWakeWaiters,
			pPool
		);
		if ( pWatch == NULL ) {
			return XWAIT_ERROR;
		}
	}
	(void)xrtMutexLock(&pPool->Lock);
	if ( !pPool->Closed ) {
		(void)xrtMutexUnlock(&pPool->Lock);
		xrtCancelUnwatch(pWatch);
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	while ( !__xrtTaskPoolIdleLocked(pPool) ) {
		if ( (pCancel != NULL) && xrtCancelRequested(pCancel) ) {
			Result = XWAIT_CANCELLED;
			break;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			Result = XWAIT_TIMEOUT;
			break;
		}
		Result = xrtCondWaitUntil(&pPool->Idle, &pPool->Lock, iDeadline);
		if ( Result == XWAIT_ERROR ) {
			pWaitError = xrtTakeError();
			break;
		}
		if ( Result == XWAIT_TIMEOUT ) {
			if ( __xrtTaskPoolIdleLocked(pPool) ) {
				Result = XWAIT_OK;
			}
			break;
		}
	}
	(void)xrtMutexUnlock(&pPool->Lock);
	xrtCancelUnwatch(pWatch);
	if ( pWaitError != NULL ) {
		xrtSetError(pWaitError);
		xrtErrorFree(pWaitError);
	} else if ( Result == XWAIT_ERROR ) {
		__xrtErrorSetInternal();
	}
	return Result;
}

XRT_API xwaitresult xrtTaskPoolWaitUntilCancel(xtaskpool* pPool, xdeadline iDeadline, xcancel* pCancel)
{
	if (!__xrtTaskPoolEnter(pPool, false)) return XWAIT_ERROR;
	xwaitresult Result = __xrtTaskPoolWaitUntilCancelBody(pPool, iDeadline, pCancel);
	__xrtTaskPoolLeave(pPool); return Result;
}



/* 复制任务池统计快照。 */
static bool __xrtTaskPoolGetBody(const xtaskpool* pPool, xtaskpoolstats* pStats)
{
	if ( (pPool == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock((xmutex*)&pPool->Lock);
	pStats->Threads = pPool->ThreadCount;
	pStats->QueueLimit = pPool->QueueLimit;
	pStats->Queued = pPool->Queued;
	pStats->Running = pPool->Running;
	pStats->Submitted = pPool->Submitted;
	pStats->Completed = pPool->Completed;
	pStats->Succeeded = pPool->Succeeded;
	pStats->Failed = pPool->Failed;
	pStats->Cancelled = pPool->Cancelled;
	pStats->Rejected = pPool->Rejected;
	pStats->Closed = pPool->Closed;
	pStats->Cancelling = pPool->Cancelling;
	(void)xrtMutexUnlock((xmutex*)&pPool->Lock);
	return true;
}

XRT_API bool xrtTaskPoolGet(const xtaskpool* pPool, xtaskpoolstats* pStats)
{
	if (!__xrtTaskPoolEnter((xtaskpool*)pPool, false)) return false;
	bool bGot = __xrtTaskPoolGetBody(pPool, pStats);
	__xrtTaskPoolLeave((xtaskpool*)pPool); return bGot;
}



/* Joined control/thread allocations are uniquely contained resources. Keep
 * the locks/conditions with the shell until the last actual reference drops. */
static bool __xrtTaskPoolRetire(xtaskpool* pPool, size_t iEntries)
{
	xrtownershipscope Mutation = {0};
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	if (pPool->Retired) { __xrtTaskPoolOwnershipEnd(pPool, &Mutation); return true; }
	if (!pPool->Joined || pPool->Joining || pPool->Retiring || pPool->Entries != iEntries) {
		__xrtTaskPoolOwnershipEnd(pPool, &Mutation); return false;
	}
	pPool->Retiring = true;
	xrt_task_worker* pWorkers = pPool->Workers; uint32 iCount = pPool->StartedThreads;
	pPool->Workers = NULL; pPool->StartedThreads = 0;
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
	for (uint32 i = 0; i < iCount; ++i) {
		if (!pWorkers[i].Exited || pWorkers[i].Parked) abort();
		xrtThreadDestroy(pWorkers[i].Thread);
	}
	xrtFree(pWorkers);
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	pPool->Retired = true; pPool->Retiring = false;
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation); return true;
}

/* Close and drain with every accepted cleanup tail still accounted for. The
 * caller must stop other concurrent access (the existing public contract).
 * A reentrant Destroy refuses before closing or freeing an outer entry's pool. */
XRT_API bool xrtTaskPoolDestroy(xtaskpool* pPool)
{
	if (!pPool) return true;
	if (!__xrtTaskPoolEnter(pPool, true)) return false;
	xrtownershipscope Mutation = {0}; __xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	bool bReady = pPool->Entries == 1 && !pPool->Destroying && !pPool->Joining && !pPool->Retiring &&
		(!pPool->OwnershipClaim || pPool->OwnershipCleared) && !__xrtTaskPoolIsWorker(pPool);
	bool bRetired = pPool->Retired;
	if (bReady) pPool->Destroying = true;
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
	if (!bReady) { __xrtTaskPoolLeave(pPool); __xrtErrorSetInvalidState(); return false; }
	if (!bRetired) {
		bReady = __xrtTaskPoolCloseBody(pPool) &&
			__xrtTaskPoolWaitUntilCancelBody(pPool, XRT_DEADLINE_NEVER, NULL) == XWAIT_OK;
		__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
		bReady = bReady && pPool->Entries == 1;
		if (bReady) pPool->Joining = true;
		__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
		if (bReady) {
			__xrtTaskPoolWorkerBegin(pPool, &Mutation);
			pPool->Shutdown = true; (void)xrtCondBroadcast(&pPool->Work);
			__xrtTaskPoolWorkerEnd(pPool, &Mutation);
			for (uint32 i = 0; i < pPool->StartedThreads; ++i)
				if (xrtThreadWait(pPool->Workers[i].Thread) != XWAIT_OK) bReady = false;
			__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
			pPool->Joining = false; pPool->Joined = bReady;
			__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
			if (bReady) bReady = __xrtTaskPoolRetire(pPool, 1);
		}
	}
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	bool bOwner = bReady && pPool->OwnerHeld;
	if (bOwner) pPool->OwnerHeld = false;
	pPool->Destroying = false;
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
	if (bOwner) __xrtTaskPoolRelease(pPool);
	__xrtTaskPoolLeave(pPool); return bReady;
}

static bool __xrtTaskPoolOwnershipCount(const void* pData, size_t* pCount)
{
	const xtaskpool* pPool = pData;
	if (!pPool || !pCount || !pPool->Initialized || !pPool->References || pPool->Entries ||
		pPool->Joining || pPool->Destroying || pPool->Retiring || pPool->OwnershipCleared) return false;
	if (pPool->Running || pPool->RunningHead || pPool->FinalizerQueued || pPool->FinalizerRunning ||
		pPool->FinalizerHead || pPool->FinalizerTail || pPool->Queued != pPool->QueueDepth) return false;
	if (pPool->Retired) {
		if (!pPool->Joined || !pPool->Closed || pPool->Workers || pPool->StartedThreads) return false;
	} else {
		if (!pPool->Workers || pPool->StartedThreads != pPool->ThreadCount) return false;
		for (uint32 i = 0; i < pPool->StartedThreads; ++i) {
			const xrt_task_worker* pWorker = &pPool->Workers[i];
			if (pWorker->Pool != pPool || !pWorker->Thread ||
				(pWorker->Exited ? pWorker->Parked : (!pWorker->Parked || pPool->Joined))) return false;
		}
	}
	size_t iJobs = 0; const xrt_task_job* pTail = NULL;
	for (const xrt_task_job* pJob = pPool->Head; pJob; pJob = pJob->Next) {
		if (iJobs++ == pPool->Queued || !pJob->Accepted || pJob->Active || pJob->Destroyed ||
			pJob->RunningPrevious || pJob->RunningNext) return false;
		pTail = pJob;
	}
	if (iJobs != pPool->Queued || pTail != pPool->Tail) return false;
	*pCount = pPool->References; return true;
}
static bool __xrtTaskPoolOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xtaskpool* pPool = pData; size_t iCount;
	if (!pVisit || !__xrtTaskPoolOwnershipCount(pData, &iCount)) return false;
	/* One actual accepted executor reference per queued Job. Tail/Next are
	 * non-owning links; do not report duplicates or invent worker RC nodes. */
	for (const xrt_task_job* pJob = pPool->Head; pJob; pJob = pJob->Next)
		if (!pVisit(__xrtTaskOwnership(pJob), pContext)) return false;
	return true;
}
static const xrtownershipops __xrtTaskPoolOwnershipOps = {
	__xrtTaskPoolOwnershipCount, __xrtTaskPoolOwnershipTrace
};
XRT_API xrtownershipref xrtTaskPoolOwnership(const xtaskpool* pPool)
{ return (xrtownershipref){pPool, pPool ? &__xrtTaskPoolOwnershipOps : NULL}; }
static bool __xrtTaskPoolHold(const void* pData)
{
	xtaskpool* pPool = (xtaskpool*)pData; xrtownershipscope Mutation = {0}; bool bHeld = false;
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	if (pPool->References && pPool->References < SIZE_MAX && !pPool->OwnershipCleared) {
		++pPool->References; bHeld = true;
	}
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation); return bHeld;
}
static bool __xrtTaskPoolClaim(const void* pData, const void* pToken)
{
	xtaskpool* pPool = (xtaskpool*)pData;
	if (!pToken || pPool->OwnershipCleared || (pPool->OwnershipClaim && pPool->OwnershipClaim != pToken)) return false;
	pPool->OwnershipClaim = pToken; return true;
}
static void __xrtTaskPoolRestore(const void* pData, const void* pToken)
{
	xtaskpool* pPool = (xtaskpool*)pData;
	if (!pToken || pPool->OwnershipClaim != pToken || pPool->OwnershipCleared) abort();
	pPool->OwnershipClaim = NULL;
}
static bool __xrtTaskPoolPreparationReady(const void* pData)
{
	const xtaskpool* pPool = pData;
	return pPool->Closed && pPool->Joined && !pPool->Joining && !pPool->Destroying &&
		!pPool->Retiring && !pPool->Entries && __xrtTaskPoolIdleLocked(pPool);
}
static xrtownershipprepareresult __xrtTaskPoolPrepare(const void* pData, const void* pToken)
{
	xtaskpool* pPool = (xtaskpool*)pData; xrtownershipscope Freeze = {0}, Mutation = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pPool->OwnershipClaim != pToken || pPool->OwnershipCleared) abort();
	bool bReady = __xrtTaskPoolPreparationReady(pData);
	bool bJoin = !pPool->Entries && !pPool->Joining && !pPool->Destroying && !pPool->Retiring;
	if (!bReady && bJoin) {
		/* No native body can hold Lock while waiting for mutation: Entries is
		 * zero. Worker wakeups release Lock before requesting mutation. */
		(void)xrtMutexLock(&pPool->Lock);
		pPool->Closed = true; (void)xrtCondBroadcast(&pPool->Space);
		bJoin = __xrtTaskPoolIdleLocked(pPool);
		if (bJoin) { pPool->Shutdown = true; (void)xrtCondBroadcast(&pPool->Work); }
		for (uint32 i = 0; i < pPool->StartedThreads; ++i)
			if (!pPool->Workers[i].Exited || pPool->Workers[i].Parked) bJoin = false;
		if (bJoin) pPool->Joining = true;
		(void)xrtMutexUnlock(&pPool->Lock);
	}
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	if (bReady) return XRT_OWNERSHIP_PREPARE_READY;
	if (!bJoin) return XRT_OWNERSHIP_PREPARE_BUSY;
	bool bJoined = true, bFailed = false;
	for (uint32 i = 0; i < pPool->StartedThreads; ++i) {
		xwaitresult Result = xrtThreadWaitFor(pPool->Workers[i].Thread, 0);
		if (Result != XWAIT_OK) { bJoined = false; if (Result != XWAIT_TIMEOUT) bFailed = true; }
	}
	__xrtTaskPoolOwnershipBegin(pPool, &Mutation);
	pPool->Joining = false; pPool->Joined = bJoined;
	__xrtTaskPoolOwnershipEnd(pPool, &Mutation);
	return bFailed ? XRT_OWNERSHIP_PREPARE_FAILED : bJoined ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTaskPoolClear(const void* pData, const void* pToken)
{
	xtaskpool* pPool = (xtaskpool*)pData;
	if (!pToken || pPool->OwnershipClaim != pToken || pPool->OwnershipCleared || !__xrtTaskPoolPreparationReady(pData)) abort();
	pPool->OwnershipCleared = true;
}
static bool __xrtTaskPoolFinish(const void* pData, const void* pToken)
{
	xtaskpool* pPool = (xtaskpool*)pData;
	if (!pToken || pPool->OwnershipClaim != pToken || !pPool->OwnershipCleared) abort();
	return __xrtTaskPoolRetire(pPool, 0);
}
XRT_API const xrtownershipadapterv1* xrtTaskPoolOwnershipAdapterV1(
	xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtTaskPoolHold, __xrtTaskPoolRelease,
		__xrtTaskPoolClaim, __xrtTaskPoolRestore, NULL, __xrtTaskPoolClear, __xrtTaskPoolFinish};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtTaskPoolPreparationReady, __xrtTaskPoolPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtTaskPoolOwnershipOps ||
		!__xrtTaskPoolOwnershipCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

#endif
