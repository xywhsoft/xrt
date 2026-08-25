#include "../internal/xrt_coroutine.h"



#if defined(XRT_FEATURE_COROUTINE_SCHEDULER)

#define XRT_CO_TIMER_INITIAL 16u



/* 通用投递节点独立于协程唤醒节点，允许从任意线程提交短调度过程。 */
struct xrt_co_post {
	struct xrt_co_post* Next;
	xcoschedpostproc Proc;
	ptr Data;
	xcocleanupproc Destroy;
};



/* 验证调度器归属于当前原生线程。 */
static bool __xrtCoSchedCheckOwner(const xcosched* pSched)
{
	if ( pSched == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtCoSchedIsOwner(pSched) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 判断当前线程是否拥有调度器。 */
bool __xrtCoSchedIsOwner(const xcosched* pSched)
{
	return (pSched != NULL) &&
		(pSched->OwnerThreadId == xrtThreadCurrentId());
}



/* 将协程加入活跃链。 */
static void __xrtCoActivePush(xcosched* pSched, xcoro* pCo)
{
	pCo->ActivePrevious = pSched->ActiveTail;
	pCo->ActiveNext = NULL;
	if ( pSched->ActiveTail != NULL ) {
		pSched->ActiveTail->ActiveNext = pCo;
	} else {
		pSched->ActiveHead = pCo;
	}
	pSched->ActiveTail = pCo;
	pCo->ActiveQueued = true;
}



/* 从活跃链移除协程。 */
static void __xrtCoActiveRemove(xcosched* pSched, xcoro* pCo)
{
	if ( !pCo->ActiveQueued ) {
		return;
	}
	if ( pCo->ActivePrevious != NULL ) {
		pCo->ActivePrevious->ActiveNext = pCo->ActiveNext;
	} else {
		pSched->ActiveHead = pCo->ActiveNext;
	}
	if ( pCo->ActiveNext != NULL ) {
		pCo->ActiveNext->ActivePrevious = pCo->ActivePrevious;
	} else {
		pSched->ActiveTail = pCo->ActivePrevious;
	}
	pCo->ActivePrevious = NULL;
	pCo->ActiveNext = NULL;
	pCo->ActiveQueued = false;
}



/* 将保留句柄的完成协程加入完成链。 */
static void __xrtCoCompletePush(xcosched* pSched, xcoro* pCo)
{
	pCo->CompletePrevious = pSched->CompleteTail;
	pCo->CompleteNext = NULL;
	if ( pSched->CompleteTail != NULL ) {
		pSched->CompleteTail->CompleteNext = pCo;
	} else {
		pSched->CompleteHead = pCo;
	}
	pSched->CompleteTail = pCo;
	pCo->CompleteQueued = true;
}



/* 从完成链移除协程。 */
static void __xrtCoCompleteRemove(xcosched* pSched, xcoro* pCo)
{
	if ( !pCo->CompleteQueued ) {
		return;
	}
	if ( pCo->CompletePrevious != NULL ) {
		pCo->CompletePrevious->CompleteNext = pCo->CompleteNext;
	} else {
		pSched->CompleteHead = pCo->CompleteNext;
	}
	if ( pCo->CompleteNext != NULL ) {
		pCo->CompleteNext->CompletePrevious = pCo->CompletePrevious;
	} else {
		pSched->CompleteTail = pCo->CompletePrevious;
	}
	pCo->CompletePrevious = NULL;
	pCo->CompleteNext = NULL;
	pCo->CompleteQueued = false;
}



/* 将可运行协程放入就绪队列尾部。 */
static void __xrtCoReadyPush(xcosched* pSched, xcoro* pCo)
{
	if ( pCo->ReadyQueued || (xrtCoState(pCo) == XCORO_DONE) ) {
		return;
	}
	pCo->ReadyPrevious = pSched->ReadyTail;
	pCo->ReadyNext = NULL;
	if ( pSched->ReadyTail != NULL ) {
		pSched->ReadyTail->ReadyNext = pCo;
	} else {
		pSched->ReadyHead = pCo;
	}
	pSched->ReadyTail = pCo;
	pCo->ReadyQueued = true;
}



/* 从就绪队列移除指定协程。 */
static void __xrtCoReadyRemove(xcosched* pSched, xcoro* pCo)
{
	if ( !pCo->ReadyQueued ) {
		return;
	}
	if ( pCo->ReadyPrevious != NULL ) {
		pCo->ReadyPrevious->ReadyNext = pCo->ReadyNext;
	} else {
		pSched->ReadyHead = pCo->ReadyNext;
	}
	if ( pCo->ReadyNext != NULL ) {
		pCo->ReadyNext->ReadyPrevious = pCo->ReadyPrevious;
	} else {
		pSched->ReadyTail = pCo->ReadyPrevious;
	}
	pCo->ReadyPrevious = NULL;
	pCo->ReadyNext = NULL;
	pCo->ReadyQueued = false;
}



/* 取出最早加入的就绪协程。 */
static xcoro* __xrtCoReadyPop(xcosched* pSched)
{
	xcoro* pCo = pSched->ReadyHead;

	if ( pCo != NULL ) {
		__xrtCoReadyRemove(pSched, pCo);
	}
	return pCo;
}



/* 将等待协程挂到目标的 join 链尾部。 */
static void __xrtCoJoinPush(xcoro* pTarget, xcoro* pWaiter)
{
	pWaiter->JoinPrevious = pTarget->JoinTail;
	pWaiter->JoinNext = NULL;
	if ( pTarget->JoinTail != NULL ) {
		pTarget->JoinTail->JoinNext = pWaiter;
	} else {
		pTarget->JoinHead = pWaiter;
	}
	pTarget->JoinTail = pWaiter;
	pWaiter->JoinTarget = pTarget;
}



/* 从目标的 join 链移除等待协程。 */
static void __xrtCoJoinRemove(xcoro* pWaiter)
{
	xcoro* pTarget = pWaiter->JoinTarget;

	if ( pTarget == NULL ) {
		return;
	}
	if ( pWaiter->JoinPrevious != NULL ) {
		pWaiter->JoinPrevious->JoinNext = pWaiter->JoinNext;
	} else {
		pTarget->JoinHead = pWaiter->JoinNext;
	}
	if ( pWaiter->JoinNext != NULL ) {
		pWaiter->JoinNext->JoinPrevious = pWaiter->JoinPrevious;
	} else {
		pTarget->JoinTail = pWaiter->JoinPrevious;
	}
	pWaiter->JoinTarget = NULL;
	pWaiter->JoinPrevious = NULL;
	pWaiter->JoinNext = NULL;
}



/* 扩充定时器堆，保证运行期挂起不再分配。 */
static bool __xrtCoTimerEnsure(xcosched* pSched, size_t iNeed)
{
	size_t iCapacity = pSched->TimerCapacity;
	xcoro** pTimers;

	if ( iNeed <= iCapacity ) {
		return true;
	}
	if ( iCapacity == 0 ) {
		iCapacity = XRT_CO_TIMER_INITIAL;
	}
	while ( iCapacity < iNeed ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(xcoro*)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pTimers = (xcoro**)xrtRealloc(
		pSched->Timers,
		iCapacity * sizeof(xcoro*)
	);
	if ( pTimers == NULL ) {
		return false;
	}
	pSched->Timers = pTimers;
	pSched->TimerCapacity = iCapacity;
	return true;
}



/* 交换定时器堆节点并同步反向索引。 */
static void __xrtCoTimerSwap(xcosched* pSched, size_t iLeft, size_t iRight)
{
	xcoro* pSwap = pSched->Timers[iLeft];

	pSched->Timers[iLeft] = pSched->Timers[iRight];
	pSched->Timers[iRight] = pSwap;
	pSched->Timers[iLeft]->TimerIndex = iLeft;
	pSched->Timers[iRight]->TimerIndex = iRight;
}



/* 向上恢复定时器最小堆。 */
static void __xrtCoTimerUp(xcosched* pSched, size_t iIndex)
{
	while ( iIndex != 0 ) {
		size_t iParent = (iIndex - 1u) / 2u;

		if ( pSched->Timers[iParent]->Deadline <= pSched->Timers[iIndex]->Deadline ) {
			break;
		}
		__xrtCoTimerSwap(pSched, iParent, iIndex);
		iIndex = iParent;
	}
}



/* 向下恢复定时器最小堆。 */
static void __xrtCoTimerDown(xcosched* pSched, size_t iIndex)
{
	for ( ;; ) {
		size_t iLeft = (iIndex * 2u) + 1u;
		size_t iRight = iLeft + 1u;
		size_t iSmallest = iIndex;

		if ( (iLeft < pSched->TimerCount) &&
			 (pSched->Timers[iLeft]->Deadline < pSched->Timers[iSmallest]->Deadline) ) {
			iSmallest = iLeft;
		}
		if ( (iRight < pSched->TimerCount) &&
			 (pSched->Timers[iRight]->Deadline < pSched->Timers[iSmallest]->Deadline) ) {
			iSmallest = iRight;
		}
		if ( iSmallest == iIndex ) {
			break;
		}
		__xrtCoTimerSwap(pSched, iIndex, iSmallest);
		iIndex = iSmallest;
	}
}



/* 将有限截止时间挂入定时器堆。 */
static void __xrtCoTimerAttach(xcosched* pSched, xcoro* pCo)
{
	if ( (pCo->Deadline == XRT_DEADLINE_NEVER) ||
		 (pCo->TimerIndex != SIZE_MAX) ) {
		return;
	}
	pCo->TimerIndex = pSched->TimerCount;
	pSched->Timers[pSched->TimerCount++] = pCo;
	__xrtCoTimerUp(pSched, pCo->TimerIndex);
}



/* 从定时器堆移除指定协程。 */
static void __xrtCoTimerRemove(xcosched* pSched, xcoro* pCo)
{
	size_t iIndex = pCo->TimerIndex;
	size_t iLast;

	if ( iIndex == SIZE_MAX ) {
		return;
	}
	iLast = --pSched->TimerCount;
	pCo->TimerIndex = SIZE_MAX;
	if ( iIndex == iLast ) {
		return;
	}
	pSched->Timers[iIndex] = pSched->Timers[iLast];
	pSched->Timers[iIndex]->TimerIndex = iIndex;
	if ( (iIndex != 0) &&
		 (pSched->Timers[iIndex]->Deadline <
		  pSched->Timers[(iIndex - 1u) / 2u]->Deadline) ) {
		__xrtCoTimerUp(pSched, iIndex);
	} else {
		__xrtCoTimerDown(pSched, iIndex);
	}
}



/* 在已经持有投递锁时从唤醒链移除指定协程。 */
static void __xrtCoPostRemoveLocked(xcosched* pSched, xcoro* pCo)
{
	xcoro* pPrevious = NULL;
	xcoro* pCurrent;

	for ( pCurrent = pSched->PostHead;
		  pCurrent != NULL;
		  pCurrent = pCurrent->PostNext ) {
		if ( pCurrent == pCo ) {
			if ( pPrevious != NULL ) {
				pPrevious->PostNext = pCurrent->PostNext;
			} else {
				pSched->PostHead = pCurrent->PostNext;
			}
			if ( pSched->PostTail == pCurrent ) {
				pSched->PostTail = pPrevious;
			}
			pCurrent->PostNext = NULL;
			pCurrent->PostQueued = 0;
			break;
		}
		pPrevious = pCurrent;
	}
}



/* 在已经持有投递锁时把协程至多一次加入唤醒链。 */
static void __xrtCoPostPushLocked(xcosched* pSched, xcoro* pCo)
{
	if ( pCo->PostQueued != 0 ) {
		return;
	}
	pCo->PostNext = NULL;
	if ( pSched->PostTail != NULL ) {
		pSched->PostTail->PostNext = pCo;
	} else {
		pSched->PostHead = pCo;
	}
	pSched->PostTail = pCo;
	pCo->PostQueued = 1;
}



/* 在线程安全投递链中移除即将销毁的协程。 */
static void __xrtCoPostRemove(xcosched* pSched, xcoro* pCo)
{
	(void)xrtMutexLock(&pSched->PostLock);
	__xrtCoPostRemoveLocked(pSched, pCo);
	(void)xrtMutexUnlock(&pSched->PostLock);
}



/* 在已经持有投递锁时判断当前资源代际是否收到通知。 */
static bool __xrtCoResourceWakePending(const xcoro* pCo)
{
	return
		(pCo->WaitTokenArmed != 0) &&
		(pCo->WaitTokenPending == pCo->WaitTokenArmed);
}



/* 原子消费通用唤醒或当前资源代际的一次通知。 */
static bool __xrtCoTakeWaitWake(xcoro* pCo)
{
	xcosched* pSched = pCo->Scheduler;
	bool bPending;

	(void)xrtMutexLock(&pSched->PostLock);
	bPending =
		(pCo->WakePending != 0) ||
		__xrtCoResourceWakePending(pCo);
	pCo->WakePending = 0;
	if ( __xrtCoResourceWakePending(pCo) ) {
		pCo->WaitTokenPending = 0;
	}
	if (
		(pCo->PostQueued != 0) &&
		(pCo->WakePending == 0) &&
		!__xrtCoResourceWakePending(pCo)
	) {
		__xrtCoPostRemoveLocked(pSched, pCo);
	}
	(void)xrtMutexUnlock(&pSched->PostLock);
	return bPending;
}



/* 在等待节点发布前为当前协程开启资源代际，不需要进入投递锁。 */
bool __xrtCoWaitOpen(xcoro* pCo, xrt_co_wait* pWait)
{
	xcosched* pSched;

	if ( (pCo == NULL) || (pWait == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSched = pCo->Scheduler;
	if (
		(pSched == NULL) ||
		(xrtCoCurrent() != pCo) ||
		!__xrtCoSchedIsOwner(pSched) ||
		pCo->InCleanup ||
		pCo->InFinalize
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pCo->WaitTokenArmed != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pCo->WaitTokenNext++;
	if ( pCo->WaitTokenNext == 0 ) {
		pCo->WaitTokenNext++;
	}
	pCo->WaitTokenArmed = pCo->WaitTokenNext;
	pCo->WaitTokenPending = 0;
	pWait->Coroutine = pCo;
	pWait->Token = pCo->WaitTokenArmed;
	return true;
}



/* 线程安全地通知仍处于当前代际的资源等待。 */
void __xrtCoWaitWake(ptr pData)
{
	xrt_co_wait* pWait = (xrt_co_wait*)pData;
	xcoro* pCo;
	xcosched* pSched;

	if ( (pWait == NULL) || (pWait->Coroutine == NULL) ) {
		return;
	}
	pCo = pWait->Coroutine;
	pSched = pCo->Scheduler;
	if ( pSched == NULL ) {
		return;
	}

	(void)xrtMutexLock(&pSched->PostLock);
	if (
		(pCo->Scheduler == pSched) &&
		(xrtCoState(pCo) != XCORO_DONE) &&
		(pCo->WaitTokenArmed == pWait->Token)
	) {
		pCo->WaitTokenPending = pWait->Token;

		/*
		 * 当前协程仍在注册或检查资源，不需要把自身挂入跨线程投递链；
		 * 它会在进入 park 前直接消费本代际通知。
		 */
		if ( xrtCoCurrent() != pCo ) {
			__xrtCoPostPushLocked(pSched, pCo);
		}
		(void)xrtCondSignal(&pSched->PostReady);
	}
	(void)xrtMutexUnlock(&pSched->PostLock);
}



/* 在等待节点同步摘除后关闭资源代际，并按需清理残留投递。 */
void __xrtCoWaitClose(xrt_co_wait* pWait)
{
	xcoro* pCo;
	xcosched* pSched;

	if ( (pWait == NULL) || (pWait->Coroutine == NULL) ) {
		return;
	}
	pCo = pWait->Coroutine;
	pSched = pCo->Scheduler;
	if ( pSched == NULL ) {
		pWait->Coroutine = NULL;
		pWait->Token = 0;
		return;
	}

	/*
	 * 调用方已经同步摘除全部回调；本代际没有待处理通知时，
	 * 令牌字段只由当前协程访问，可以直接完成最常见的关闭路径。
	 */
	if (
		(xrtCoCurrent() == pCo) &&
		__xrtCoSchedIsOwner(pSched) &&
		(pCo->WaitTokenArmed == pWait->Token) &&
		(pCo->WaitTokenPending != pWait->Token)
	) {
		pCo->WaitTokenArmed = 0;
		pWait->Coroutine = NULL;
		pWait->Token = 0;
		return;
	}

	(void)xrtMutexLock(&pSched->PostLock);
	if ( pCo->WaitTokenArmed == pWait->Token ) {
		pCo->WaitTokenArmed = 0;
		if ( pCo->WaitTokenPending == pWait->Token ) {
			pCo->WaitTokenPending = 0;
		}
	}
	if (
		(pCo->PostQueued != 0) &&
		(pCo->WakePending == 0) &&
		!__xrtCoResourceWakePending(pCo)
	) {
		__xrtCoPostRemoveLocked(pSched, pCo);
	}
	(void)xrtMutexUnlock(&pSched->PostLock);
	pWait->Coroutine = NULL;
	pWait->Token = 0;
}



/* 完成一次 park 或 join，并重新加入就绪队列。 */
static void __xrtCoWaitFinish(xcoro* pCo, xwaitresult Result)
{
	xcosched* pSched = pCo->Scheduler;

	__xrtCoTimerRemove(pSched, pCo);
	if ( pCo->WaitKind == XRT_CO_WAIT_JOIN ) {
		__xrtCoJoinRemove(pCo);
	}
	pCo->WaitKind = XRT_CO_WAIT_NONE;
	pCo->Deadline = XRT_DEADLINE_NEVER;
	pCo->WaitResult = Result;
	__xrtCoReadyPush(pSched, pCo);
}



/* 唤醒目标完成时挂接的全部 join 等待者。 */
static void __xrtCoJoinWakeAll(xcoro* pTarget, xwaitresult Result)
{
	while ( pTarget->JoinHead != NULL ) {
		xcoro* pWaiter = pTarget->JoinHead;

		__xrtCoJoinRemove(pWaiter);
		__xrtCoTimerRemove(pWaiter->Scheduler, pWaiter);
		pWaiter->WaitKind = XRT_CO_WAIT_NONE;
		pWaiter->Deadline = XRT_DEADLINE_NEVER;
		pWaiter->WaitResult = Result;
		__xrtCoReadyPush(pWaiter->Scheduler, pWaiter);
	}
}



/* 根据取消、唤醒和期限决定挂起协程的下一状态。 */
static void __xrtCoWaitPrepare(xcoro* pCo)
{
	if ( pCo->WaitKind == XRT_CO_WAIT_NONE ) {
		__xrtCoReadyPush(pCo->Scheduler, pCo);
		return;
	}
	if ( xrtCancelRequested(pCo->Cancel) ) {
		(void)__xrtCoTakeWaitWake(pCo);
		__xrtCoWaitFinish(pCo, XWAIT_CANCELLED);
		return;
	}
	if ( pCo->WaitKind == XRT_CO_WAIT_PARK ) {
		if ( __xrtCoTakeWaitWake(pCo) ) {
			__xrtCoWaitFinish(pCo, XWAIT_OK);
			return;
		}
	} else if ( (pCo->JoinTarget == NULL) ||
				(xrtCoState(pCo->JoinTarget) == XCORO_DONE) ) {
		__xrtCoWaitFinish(pCo, XWAIT_OK);
		return;
	}
	if ( xrtDeadlineExpired(pCo->Deadline) ) {
		__xrtCoWaitFinish(pCo, XWAIT_TIMEOUT);
		return;
	}
	__xrtCoTimerAttach(pCo->Scheduler, pCo);
}



/* 将已经结束的协程移出活跃体系并处理句柄所有权。 */
static void __xrtCoSchedComplete(xcosched* pSched, xcoro* pCo)
{
	xcancelwatch* pWatch = pCo->CancelWatch;

	__xrtCoReadyRemove(pSched, pCo);
	__xrtCoTimerRemove(pSched, pCo);
	__xrtCoActiveRemove(pSched, pCo);
	if ( pSched->Alive != 0 ) {
		pSched->Alive--;
	}
	pCo->CancelWatch = NULL;
	xrtCancelUnwatch(pWatch);
	__xrtCoPostRemove(pSched, pCo);
	__xrtCoJoinWakeAll(pCo, XWAIT_OK);
	if ( pCo->Detached ) {
		pCo->Scheduler = NULL;
		__xrtCoFree(pCo);
	} else {
		__xrtCoCompletePush(pSched, pCo);
	}
}



/* 取消监听只投递协程，不在请求线程操作调度队列。 */
static void __xrtCoCancelWake(ptr pData)
{
	(void)__xrtCoSchedWake((xcoro*)pData);
}



/* 线程安全地投递一次唤醒，协程自身即为无分配投递节点。 */
bool __xrtCoSchedWake(xcoro* pCo)
{
	xcosched* pSched;

	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSched = pCo->Scheduler;
	if ( pSched == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	(void)xrtMutexLock(&pSched->PostLock);
	if ( (pCo->Scheduler != pSched) || (xrtCoState(pCo) == XCORO_DONE) ) {
		(void)xrtMutexUnlock(&pSched->PostLock);
		return true;
	}
	pCo->WakePending = 1;
	__xrtCoPostPushLocked(pSched, pCo);
	(void)xrtCondSignal(&pSched->PostReady);
	(void)xrtMutexUnlock(&pSched->PostLock);
	return true;
}



/* 消费跨线程投递链，并只在所属线程修改执行结构。 */
static void __xrtCoSchedDrainPosts(xcosched* pSched)
{
	for ( ;; ) {
		xcoro* pCo;

		(void)xrtMutexLock(&pSched->PostLock);
		pCo = pSched->PostHead;
		if ( pCo != NULL ) {
			pSched->PostHead = pCo->PostNext;
			if ( pSched->PostTail == pCo ) {
				pSched->PostTail = NULL;
			}
			pCo->PostNext = NULL;
			pCo->PostQueued = 0;
		}
		(void)xrtMutexUnlock(&pSched->PostLock);
		if ( pCo == NULL ) {
			break;
		}
		pCo->PostNext = NULL;
		if ( (pCo->Scheduler != pSched) ||
			 (xrtCoState(pCo) == XCORO_DONE) ) {
			continue;
		}
		if ( pCo->WaitKind != XRT_CO_WAIT_NONE ) {
			__xrtCoWaitPrepare(pCo);
		} else if ( xrtCoState(pCo) == XCORO_SUSPENDED ) {
			__xrtCoReadyPush(pSched, pCo);
		}
	}
}



/* 从线程安全投递队列取出一个通用过程。 */
static xrt_co_post* __xrtCoSchedWorkTake(xcosched* pSched)
{
	xrt_co_post* pPost;

	(void)xrtMutexLock(&pSched->PostLock);
	pPost = pSched->WorkHead;
	if ( pPost != NULL ) {
		pSched->WorkHead = pPost->Next;
		if ( pSched->WorkTail == pPost ) {
			pSched->WorkTail = NULL;
		}
		pPost->Next = NULL;
		pSched->WorkCount--;
	}
	(void)xrtMutexUnlock(&pSched->PostLock);
	return pPost;
}



/* 在所属线程执行一个通用投递，并在回调返回后释放受理的数据。 */
static bool __xrtCoSchedWorkRun(xcosched* pSched)
{
	xrt_co_post* pPost = __xrtCoSchedWorkTake(pSched);

	if ( pPost == NULL ) {
		return false;
	}
	pPost->Proc(pSched, pPost->Data);
	if ( pPost->Destroy != NULL ) {
		pPost->Destroy(pPost->Data);
	}
	xrtFree(pPost);
	return true;
}



/* 判断调度器是否仍有已经受理但尚未执行的通用投递。 */
static bool __xrtCoSchedWorkPending(xcosched* pSched)
{
	bool bPending;

	(void)xrtMutexLock(&pSched->PostLock);
	bPending = pSched->WorkCount != 0;
	(void)xrtMutexUnlock(&pSched->PostLock);
	return bPending;
}



/* 把所有已经到期的挂起协程转回就绪队列。 */
static void __xrtCoSchedExpireTimers(xcosched* pSched)
{
	uint64 iNow = xrtClock();

	while ( (pSched->TimerCount != 0) &&
			(pSched->Timers[0]->Deadline <= iNow) ) {
		xcoro* pCo = pSched->Timers[0];

		__xrtCoTimerRemove(pSched, pCo);
		if ( xrtCancelRequested(pCo->Cancel) ) {
			(void)__xrtCoTakeWaitWake(pCo);
			__xrtCoWaitFinish(pCo, XWAIT_CANCELLED);
		} else if ( (pCo->WaitKind == XRT_CO_WAIT_PARK) &&
					__xrtCoTakeWaitWake(pCo) ) {
			__xrtCoWaitFinish(pCo, XWAIT_OK);
		} else {
			__xrtCoWaitFinish(pCo, XWAIT_TIMEOUT);
		}
	}
}



/* 返回用户期限和最近协程定时器中的较早者。 */
static xdeadline __xrtCoSchedWaitDeadline(
	const xcosched* pSched,
	xdeadline iDeadline
)
{
	if ( (pSched->TimerCount != 0) &&
		 (pSched->Timers[0]->Deadline < iDeadline) ) {
		return pSched->Timers[0]->Deadline;
	}
	return iDeadline;
}



/* 等待跨线程投递或当前最近期限。 */
static xwaitresult __xrtCoSchedWait(
	xcosched* pSched,
	xdeadline iDeadline
)
{
	xwaitresult Result = XWAIT_OK;

	(void)xrtMutexLock(&pSched->PostLock);
	while ( (pSched->PostHead == NULL) && (pSched->WorkHead == NULL) ) {
		Result = xrtCondWaitUntil(
			&pSched->PostReady,
			&pSched->PostLock,
			iDeadline
		);
		if ( Result != XWAIT_OK ) {
			break;
		}
	}
	(void)xrtMutexUnlock(&pSched->PostLock);
	return Result;
}



/* 创建当前线程拥有的协程调度器。 */
XRT_API xcosched* xrtCoSchedCreate(void)
{
	xcosched* pSched = (xcosched*)xrtCalloc(1, sizeof(xcosched));

	if ( pSched == NULL ) {
		return NULL;
	}
	pSched->OwnerThreadId = xrtThreadCurrentId();
	if ( !xrtMutexInit(&pSched->PostLock) ) {
		xrtFree(pSched);
		return NULL;
	}
	if ( !xrtCondInit(&pSched->PostReady) ) {
		(void)xrtMutexUnit(&pSched->PostLock);
		xrtFree(pSched);
		return NULL;
	}
	return pSched;
}



/* 销毁空调度器，并回收仍由调用方保留的完成句柄。 */
XRT_API bool xrtCoSchedDestroy(xcosched* pSched)
{
	if ( pSched == NULL ) {
		return true;
	}
	if ( !__xrtCoSchedCheckOwner(pSched) ) {
		return false;
	}
	if ( pSched->Running || (pSched->Alive != 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	(void)xrtMutexLock(&pSched->PostLock);
	if ( (pSched->PostHead != NULL) || (pSched->WorkHead != NULL) ) {
		(void)xrtMutexUnlock(&pSched->PostLock);
		__xrtErrorSetInvalidState();
		return false;
	}
	pSched->Closed = true;
	(void)xrtMutexUnlock(&pSched->PostLock);
	while ( pSched->CompleteHead != NULL ) {
		xcoro* pCo = pSched->CompleteHead;

		__xrtCoCompleteRemove(pSched, pCo);
		pCo->Scheduler = NULL;
		__xrtCoFree(pCo);
	}
	(void)xrtCondUnit(&pSched->PostReady);
	(void)xrtMutexUnit(&pSched->PostLock);
	xrtFree(pSched->Timers);
	xrtFree(pSched);
	return true;
}



/* 返回当前协程所属调度器。 */
XRT_API xcosched* xrtCoSchedCurrent(void)
{
	xcoro* pCo = xrtCoCurrent();

	return pCo != NULL ? pCo->Scheduler : NULL;
}



/* 统一投递借用或受理所有权的数据。 */
static bool __xrtCoSchedPost(
	xcosched* pSched,
	xcoschedpostproc pProc,
	ptr pData,
	xcocleanupproc pDestroy
)
{
	xrt_co_post* pPost;

	if ( (pSched == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pPost = (xrt_co_post*)xrtMalloc(sizeof(xrt_co_post));
	if ( pPost == NULL ) {
		return false;
	}
	pPost->Next = NULL;
	pPost->Proc = pProc;
	pPost->Data = pData;
	pPost->Destroy = pDestroy;

	/* 关闭与受理在同一把锁内线性化，失败时数据所有权仍归调用方。 */
	(void)xrtMutexLock(&pSched->PostLock);
	if ( pSched->Closed ) {
		(void)xrtMutexUnlock(&pSched->PostLock);
		xrtFree(pPost);
		__xrtErrorSetClosed();
		return false;
	}
	if ( pSched->WorkCount == SIZE_MAX ) {
		(void)xrtMutexUnlock(&pSched->PostLock);
		xrtFree(pPost);
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( pSched->WorkTail != NULL ) {
		pSched->WorkTail->Next = pPost;
	} else {
		pSched->WorkHead = pPost;
	}
	pSched->WorkTail = pPost;
	pSched->WorkCount++;
	(void)xrtCondSignal(&pSched->PostReady);
	(void)xrtMutexUnlock(&pSched->PostLock);
	return true;
}



/* 从任意线程投递借用数据过程。 */
XRT_API bool xrtCoSchedPost(
	xcosched* pSched,
	xcoschedpostproc pProc,
	ptr pData
)
{
	return __xrtCoSchedPost(pSched, pProc, pData, NULL);
}



/* 从任意线程投递并接管过程数据。 */
XRT_API bool xrtCoSchedPostOwned(
	xcosched* pSched,
	xcoschedpostproc pProc,
	ptr pData,
	xcocleanupproc pDestroy
)
{
	if ( pDestroy == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtCoSchedPost(pSched, pProc, pData, pDestroy);
}



/* 创建并挂入一个指定句柄所有权的调度协程。 */
static xcoro* __xrtCoSchedSpawn(
	xcosched* pSched,
	xcoroproc pProc,
	ptr pData,
	const xcoroargs* pArgs,
	bool bDetached
)
{
	xcoro* pCo;

	if ( !__xrtCoSchedCheckOwner(pSched) ) {
		return NULL;
	}
	if ( pSched->Closed ) {
		__xrtErrorSetClosed();
		return NULL;
	}
	if ( pSched->Alive == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( !__xrtCoTimerEnsure(pSched, pSched->Alive + 1u) ) {
		return NULL;
	}
	pCo = xrtCoCreate(pProc, pData, pArgs);
	if ( pCo == NULL ) {
		return NULL;
	}
	pCo->Scheduler = pSched;
	pCo->Detached = bDetached;
	pCo->Deadline = XRT_DEADLINE_NEVER;
	pCo->TimerIndex = SIZE_MAX;
	__xrtCoActivePush(pSched, pCo);
	__xrtCoReadyPush(pSched, pCo);
	pSched->Alive++;
	pCo->CancelWatch = xrtCancelWatch(
		pCo->Cancel,
		__xrtCoCancelWake,
		pCo
	);
	if ( pCo->CancelWatch == NULL ) {
		__xrtCoReadyRemove(pSched, pCo);
		__xrtCoActiveRemove(pSched, pCo);
		pSched->Alive--;
		pCo->Scheduler = NULL;
		__xrtCoFree(pCo);
		return NULL;
	}
	return pCo;
}



/* 创建完成后保留句柄的调度协程。 */
XRT_API xcoro* xrtCoSpawn(
	xcosched* pSched,
	xcoroproc pProc,
	ptr pData,
	const xcoroargs* pArgs
)
{
	return __xrtCoSchedSpawn(pSched, pProc, pData, pArgs, false);
}



/* 创建完成后自动回收的分离协程。 */
XRT_API bool xrtCoGo(
	xcosched* pSched,
	xcoroproc pProc,
	ptr pData,
	const xcoroargs* pArgs
)
{
	return __xrtCoSchedSpawn(pSched, pProc, pData, pArgs, true) != NULL;
}



/* 关闭调度器并协作取消全部活跃协程。 */
XRT_API bool xrtCoSchedClose(xcosched* pSched)
{
	xcoro* pCo;

	if ( !__xrtCoSchedCheckOwner(pSched) ) {
		return false;
	}
	(void)xrtMutexLock(&pSched->PostLock);
	if ( pSched->Closed ) {
		(void)xrtMutexUnlock(&pSched->PostLock);
		return true;
	}
	pSched->Closed = true;
	(void)xrtCondBroadcast(&pSched->PostReady);
	(void)xrtMutexUnlock(&pSched->PostLock);
	for ( pCo = pSched->ActiveHead; pCo != NULL; pCo = pCo->ActiveNext ) {
		(void)xrtCoCancel(pCo);
	}
	return true;
}



/* 执行一次有截止时间的调度轮询。 */
XRT_API xwaitresult xrtCoSchedPollUntil(
	xcosched* pSched,
	xdeadline iDeadline
)
{
	xwaitresult Result;

	if ( !__xrtCoSchedCheckOwner(pSched) ) {
		return XWAIT_ERROR;
	}
	if ( pSched->Running || (xrtCoCurrent() != NULL) ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	pSched->Running = true;
	for ( ;; ) {
		xcoro* pCo;
		xdeadline iWaitDeadline;
		bool bWorked;

		bWorked = __xrtCoSchedWorkRun(pSched);
		__xrtCoSchedDrainPosts(pSched);
		__xrtCoSchedExpireTimers(pSched);
		pCo = __xrtCoReadyPop(pSched);
		if ( pCo != NULL ) {
			if ( !xrtCoResume(pCo) ) {
				__xrtCoReadyPush(pSched, pCo);
				Result = XWAIT_ERROR;
				break;
			}
			if ( xrtCoState(pCo) == XCORO_DONE ) {
				__xrtCoSchedComplete(pSched, pCo);
			} else {
				__xrtCoWaitPrepare(pCo);
			}
			Result = XWAIT_OK;
			break;
		}
		if ( bWorked ) {
			Result = XWAIT_OK;
			break;
		}
		if ( pSched->Alive == 0 ) {
			if ( __xrtCoSchedWorkPending(pSched) ) {
				continue;
			}
			Result = XWAIT_CLOSED;
			break;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			Result = XWAIT_TIMEOUT;
			break;
		}
		iWaitDeadline = __xrtCoSchedWaitDeadline(pSched, iDeadline);
		Result = __xrtCoSchedWait(pSched, iWaitDeadline);
		if ( Result == XWAIT_ERROR ) {
			break;
		}
	}
	pSched->Running = false;
	return Result;
}



/* 非阻塞执行至多一个就绪协程。 */
XRT_API xwaitresult xrtCoSchedStep(xcosched* pSched)
{
	return xrtCoSchedPollUntil(pSched, xrtClock());
}



/* 在相对期限内等待并执行至多一个就绪协程。 */
XRT_API xwaitresult xrtCoSchedPollFor(
	xcosched* pSched,
	uint64 iTimeout
)
{
	return xrtCoSchedPollUntil(pSched, xrtDeadlineAfter(iTimeout));
}



/* 运行调度器直到全部协程完成。 */
XRT_API bool xrtCoSchedRun(xcosched* pSched)
{
	if ( !__xrtCoSchedCheckOwner(pSched) ) {
		return false;
	}
	while ( (pSched->Alive != 0) || __xrtCoSchedWorkPending(pSched) ) {
		if ( xrtCoSchedPollUntil(pSched, XRT_DEADLINE_NEVER) == XWAIT_ERROR ) {
			return false;
		}
	}
	return true;
}



/* 返回调度器活跃协程数。 */
XRT_API size_t xrtCoSchedAlive(const xcosched* pSched)
{
	if ( !__xrtCoSchedCheckOwner(pSched) ) {
		return 0;
	}
	return pSched->Alive;
}



/* 公开线程安全唤醒入口。 */
XRT_API bool xrtCoWake(xcoro* pCo)
{
	return __xrtCoSchedWake(pCo);
}



/* 返回当前调度协程并验证执行上下文。 */
static xcoro* __xrtCoWaitCurrent(void)
{
	xcoro* pCo = xrtCoCurrent();

	if (
		(pCo == NULL) ||
		(pCo->Scheduler == NULL) ||
		pCo->InCleanup ||
		pCo->InFinalize
	) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pCo;
}



/* 挂起当前协程到唤醒、取消或截止时间。 */
XRT_API xwaitresult xrtCoParkUntil(xdeadline iDeadline)
{
	xcoro* pCo = __xrtCoWaitCurrent();

	if ( pCo == NULL ) {
		return XWAIT_ERROR;
	}
	if ( xrtCancelRequested(pCo->Cancel) ) {
		return XWAIT_CANCELLED;
	}
	if ( __xrtCoTakeWaitWake(pCo) ) {
		return XWAIT_OK;
	}
	if ( xrtDeadlineExpired(iDeadline) ) {
		return XWAIT_TIMEOUT;
	}
	pCo->WaitKind = XRT_CO_WAIT_PARK;
	pCo->WaitResult = XWAIT_ERROR;
	pCo->Deadline = iDeadline;
	if ( xrtCoYield() == XWAIT_ERROR ) {
		pCo->WaitKind = XRT_CO_WAIT_NONE;
		return XWAIT_ERROR;
	}
	return pCo->WaitResult;
}



/* 挂起当前资源等待到通知、通用唤醒、取消或截止时间。 */
xwaitresult __xrtCoWaitParkUntil(ptr pData, xdeadline iDeadline)
{
	xrt_co_wait* pWait = (xrt_co_wait*)pData;
	xcoro* pCo = __xrtCoWaitCurrent();

	if (
		(pCo == NULL) ||
		(pWait == NULL) ||
		(pWait->Coroutine != pCo) ||
		(pWait->Token == 0) ||
		(pCo->WaitTokenArmed != pWait->Token)
	) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	return xrtCoParkUntil(iDeadline);
}



/* 无限期挂起当前调度协程。 */
XRT_API xwaitresult xrtCoPark(void)
{
	return xrtCoParkUntil(XRT_DEADLINE_NEVER);
}



/* 在相对期限内挂起当前调度协程。 */
XRT_API xwaitresult xrtCoParkFor(uint64 iTimeout)
{
	return xrtCoParkUntil(xrtDeadlineAfter(iTimeout));
}



/* 让当前调度协程睡眠到指定截止时间。 */
XRT_API xwaitresult xrtCoSleepUntil(xdeadline iDeadline)
{
	xwaitresult Result = xrtCoParkUntil(iDeadline);

	return Result == XWAIT_TIMEOUT ? XWAIT_OK : Result;
}



/* 让当前调度协程睡眠指定微秒数，零值执行一次公平让出。 */
XRT_API xwaitresult xrtCoSleep(uint64 iTimeout)
{
	if ( iTimeout == 0 ) {
		return xrtCoYield();
	}
	return xrtCoSleepUntil(xrtDeadlineAfter(iTimeout));
}



/* 检测 join 依赖链中是否出现当前协程。 */
static bool __xrtCoJoinCycle(xcoro* pCurrent, xcoro* pTarget)
{
	while ( pTarget != NULL ) {
		if ( pTarget == pCurrent ) {
			return true;
		}
		pTarget = pTarget->JoinTarget;
	}
	return false;
}



/* 在当前协程中等待同一调度器目标到指定截止时间。 */
XRT_API xwaitresult xrtCoJoinUntil(
	xcoro* pTarget,
	xdeadline iDeadline
)
{
	xcoro* pCurrent = __xrtCoWaitCurrent();

	if ( pCurrent == NULL ) {
		return XWAIT_ERROR;
	}
	if ( pTarget == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XWAIT_ERROR;
	}
	if ( (pTarget->Scheduler != pCurrent->Scheduler) ||
		 pTarget->Detached || __xrtCoJoinCycle(pCurrent, pTarget) ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	if ( xrtCoState(pTarget) == XCORO_DONE ) {
		return XWAIT_OK;
	}
	if ( xrtCancelRequested(pCurrent->Cancel) ) {
		return XWAIT_CANCELLED;
	}
	if ( xrtDeadlineExpired(iDeadline) ) {
		return XWAIT_TIMEOUT;
	}
	pCurrent->WaitKind = XRT_CO_WAIT_JOIN;
	pCurrent->WaitResult = XWAIT_ERROR;
	pCurrent->Deadline = iDeadline;
	__xrtCoJoinPush(pTarget, pCurrent);
	if ( xrtCoYield() == XWAIT_ERROR ) {
		__xrtCoJoinRemove(pCurrent);
		pCurrent->WaitKind = XRT_CO_WAIT_NONE;
		return XWAIT_ERROR;
	}
	return pCurrent->WaitResult;
}



/* 无限期等待同一调度器目标。 */
XRT_API xwaitresult xrtCoJoin(xcoro* pCo)
{
	return xrtCoJoinUntil(pCo, XRT_DEADLINE_NEVER);
}



/* 在相对期限内等待同一调度器目标。 */
XRT_API xwaitresult xrtCoJoinFor(xcoro* pCo, uint64 iTimeout)
{
	return xrtCoJoinUntil(pCo, xrtDeadlineAfter(iTimeout));
}



/* 从调度器链中移除并销毁一个 READY 或 DONE 协程。 */
bool __xrtCoSchedDestroyCoroutine(xcoro* pCo)
{
	xcosched* pSched = pCo->Scheduler;
	xcancelwatch* pWatch;
	xcorostate State = xrtCoState(pCo);

	if ( !__xrtCoSchedCheckOwner(pSched) ) {
		return false;
	}
	pWatch = pCo->CancelWatch;
	pCo->CancelWatch = NULL;
	xrtCancelUnwatch(pWatch);
	__xrtCoPostRemove(pSched, pCo);
	__xrtCoReadyRemove(pSched, pCo);
	__xrtCoTimerRemove(pSched, pCo);
	if ( pCo->ActiveQueued ) {
		__xrtCoJoinWakeAll(pCo, XWAIT_CLOSED);
		__xrtCoActiveRemove(pSched, pCo);
		if ( pSched->Alive != 0 ) {
			pSched->Alive--;
		}
	}
	__xrtCoCompleteRemove(pSched, pCo);
	pCo->Scheduler = NULL;
	if ( State == XCORO_READY ) {
		__xrtCoFinishHost(pCo, XCORO_TERM_CANCELLED);
	}
	__xrtCoFree(pCo);
	return true;
}

#endif
