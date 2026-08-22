#include "../internal/xrt_task.h"



#if defined(XRT_FEATURE_TASK_POOL)

typedef struct xrt_task_worker {
	struct xtaskpool* Pool;
	xthread* Thread;
} xrt_task_worker;



/* 任务池用一把锁统一保护队列、运行链、统计和生命周期状态。 */
struct xtaskpool {
	xmutex Lock;
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
};



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
		(void)xrtMutexLock(&pPool->Lock);
		pPool->Queued--;
		__xrtTaskPoolRecordLocked(pPool, XFUTURE_CANCELLED);
		if ( __xrtTaskPoolIdleLocked(pPool) ) {
			(void)xrtCondBroadcast(&pPool->Idle);
		}
		(void)xrtMutexUnlock(&pPool->Lock);
		__xrtTaskDestroy(pJob, true);
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

		(void)xrtMutexLock(&pPool->Lock);
		while (
			(pPool->Head == NULL) &&
			(pPool->FinalizerHead == NULL) &&
			!pPool->Shutdown
		) {
			(void)xrtCondWait(&pPool->Work, &pPool->Lock);
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
			(void)xrtMutexUnlock(&pPool->Lock);

			pFinalizer->Proc(pFinalizer->Data);

			(void)xrtMutexLock(&pPool->Lock);
			pPool->FinalizerRunning--;
			if ( __xrtTaskPoolIdleLocked(pPool) ) {
				(void)xrtCondBroadcast(&pPool->Idle);
			}
			(void)xrtMutexUnlock(&pPool->Lock);
			continue;
		}
		if ( pPool->Head == NULL ) {
			(void)xrtMutexUnlock(&pPool->Lock);
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
		(void)xrtMutexUnlock(&pPool->Lock);

		/* Future 取消只请求协作；在真正执行前再次检查即可跳过过程。 */
		__xrtTaskRun(pJob);
		State = __xrtTaskState(pJob);

		(void)xrtMutexLock(&pPool->Lock);
		__xrtTaskPoolRunningRemoveLocked(pPool, pJob);
		pPool->Running--;
		__xrtTaskPoolRecordLocked(pPool, State);
		if ( __xrtTaskPoolIdleLocked(pPool) ) {
			(void)xrtCondBroadcast(&pPool->Idle);
		}
		(void)xrtMutexUnlock(&pPool->Lock);
		__xrtTaskDestroy(pJob, true);
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
	pPool->QueueLimit = tConfig.QueueLimit;
	pPool->StackSize = tConfig.StackSize;
	if ( !xrtMutexInit(&pPool->Lock) ) {
		xrtFree(pPool);
		return NULL;
	}
	if ( !xrtCondInit(&pPool->Work) ) {
		(void)xrtMutexUnit(&pPool->Lock);
		xrtFree(pPool);
		return NULL;
	}
	if ( !xrtCondInit(&pPool->Idle) ) {
		(void)xrtCondUnit(&pPool->Work);
		(void)xrtMutexUnit(&pPool->Lock);
		xrtFree(pPool);
		return NULL;
	}
	if ( !xrtCondInit(&pPool->Space) ) {
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
static xfuture* __xrtTaskPoolSubmit(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
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
	pJob = __xrtTaskCreate(pProc, pData, pArgs, &pFuture);
	if ( pJob == NULL ) {
		return NULL;
	}
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
	(void)xrtMutexLock(&pPool->Lock);
	__xrtTaskPoolRecordLocked(pPool, XFUTURE_CANCELLED);
	if ( __xrtTaskPoolIdleLocked(pPool) ) {
		(void)xrtCondBroadcast(&pPool->Idle);
	}
	(void)xrtMutexUnlock(&pPool->Lock);
	__xrtTaskDestroy(pJob, true);
	return pFuture;
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
		false,
		XRT_DEADLINE_NEVER,
		NULL
	);
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
		true,
		iDeadline,
		pCancel
	);
}



/*
	投递一个无分配资源回收过程。
	该内部通道不受普通队列上限和 Closed 状态影响，但调用方必须保证池仍存活。
*/
void __xrtTaskPoolFinalize(
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
XRT_API bool xrtTaskPoolClose(xtaskpool* pPool)
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
XRT_API bool xrtTaskPoolCancel(xtaskpool* pPool)
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
XRT_API xwaitresult xrtTaskPoolWaitUntilCancel(
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



/* 复制任务池统计快照。 */
XRT_API bool xrtTaskPoolGet(const xtaskpool* pPool, xtaskpoolstats* pStats)
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



/* 关闭、排空、终止工作线程并释放任务池。 */
XRT_API bool xrtTaskPoolDestroy(xtaskpool* pPool)
{
	if ( pPool == NULL ) {
		return true;
	}
	if ( __xrtTaskPoolIsWorker(pPool) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !xrtTaskPoolClose(pPool) ) {
		return false;
	}

	/*
		Shutdown 只控制空闲线程退出。
		已经运行的任务仍可在析构阶段投递嵌入式 finalizer。
	*/
	(void)xrtMutexLock(&pPool->Lock);
	pPool->Shutdown = true;
	(void)xrtCondBroadcast(&pPool->Work);
	(void)xrtMutexUnlock(&pPool->Lock);
	if ( xrtTaskPoolWait(pPool) != XWAIT_OK ) {
		return false;
	}
	for ( uint32 i = 0; i < pPool->StartedThreads; i++ ) {
		if ( xrtThreadWait(pPool->Workers[i].Thread) != XWAIT_OK ) {
			return false;
		}
	}
	for ( uint32 i = 0; i < pPool->StartedThreads; i++ ) {
		xrtThreadDestroy(pPool->Workers[i].Thread);
	}
	(void)xrtCondUnit(&pPool->Space);
	(void)xrtCondUnit(&pPool->Idle);
	(void)xrtCondUnit(&pPool->Work);
	(void)xrtMutexUnit(&pPool->Lock);
	xrtFree(pPool->Workers);
	xrtFree(pPool);
	return true;
}

#endif
