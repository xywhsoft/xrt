#include "../internal/xrt_coroutine.h"



#if defined(XRT_FEATURE_COROUTINE_EVENT)

#define XRT_CO_EVENT_MAGIC UINT32_C(0x58544345)



typedef struct xrt_co_event_waiter xrt_co_event_waiter;



/* 单次等待全部使用协程栈存储，并由事件锁保护链表状态。 */
struct xrt_co_event_waiter {
	xrt_co_wait Wait;
	xrt_co_event_waiter* Previous;
	xrt_co_event_waiter* Next;
	bool Linked;
	bool Signaled;
};



/* 协程事件把状态、活动等待计数和 FIFO 队列收拢到单一锁域。 */
typedef struct xrt_co_event_impl {
	uint32 Magic;
	bool ManualReset;
	bool Signaled;
	size_t ActiveWaits;
	xmutex Lock;
	xrt_co_event_waiter* WaitHead;
	xrt_co_event_waiter* WaitTail;
} xrt_co_event_impl;



typedef char xrt_co_event_storage_check[
	(sizeof(xrt_co_event_impl) <= XRT_CO_EVENT_STORAGE_SIZE) ? 1 : -1
];



/* 读取公开固定存储中的协程事件实现。 */
static xrt_co_event_impl* __xrtCoEventImpl(xcoevent* pEvent)
{
	return (xrt_co_event_impl*)pEvent;
}



/* 验证协程事件已经初始化。 */
static xrt_co_event_impl* __xrtCoEventRequire(xcoevent* pEvent)
{
	xrt_co_event_impl* pImpl;

	if ( pEvent == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtCoEventImpl(pEvent);
	if ( pImpl->Magic != XRT_CO_EVENT_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 将等待节点追加到 FIFO 队列。 */
static void __xrtCoEventWaitPush(
	xrt_co_event_impl* pImpl,
	xrt_co_event_waiter* pWaiter
)
{
	pWaiter->Previous = pImpl->WaitTail;
	pWaiter->Next = NULL;
	pWaiter->Linked = true;
	if ( pImpl->WaitTail != NULL ) {
		pImpl->WaitTail->Next = pWaiter;
	} else {
		pImpl->WaitHead = pWaiter;
	}
	pImpl->WaitTail = pWaiter;
}



/* 从 FIFO 队列摘除仍在等待的节点。 */
static void __xrtCoEventWaitRemove(
	xrt_co_event_impl* pImpl,
	xrt_co_event_waiter* pWaiter
)
{
	if ( !pWaiter->Linked ) {
		return;
	}
	if ( pWaiter->Previous != NULL ) {
		pWaiter->Previous->Next = pWaiter->Next;
	} else {
		pImpl->WaitHead = pWaiter->Next;
	}
	if ( pWaiter->Next != NULL ) {
		pWaiter->Next->Previous = pWaiter->Previous;
	} else {
		pImpl->WaitTail = pWaiter->Previous;
	}
	pWaiter->Previous = NULL;
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
}



/* 取出最早等待的节点。 */
static xrt_co_event_waiter* __xrtCoEventWaitPop(
	xrt_co_event_impl* pImpl
)
{
	xrt_co_event_waiter* pWaiter = pImpl->WaitHead;

	if ( pWaiter != NULL ) {
		__xrtCoEventWaitRemove(pImpl, pWaiter);
	}
	return pWaiter;
}



/* 标记一个等待者已经获得信号，并在锁内完成线程安全通知。 */
static void __xrtCoEventSignalWaiter(xrt_co_event_waiter* pWaiter)
{
	pWaiter->Signaled = true;
	__xrtCoWaitWake(&pWaiter->Wait);
}



/* 初始化自动或手动复位协程事件。 */
XRT_API bool xrtCoEventInit(
	xcoevent* pEvent,
	bool bManualReset,
	bool bSignaled
)
{
	xrt_co_event_impl* pImpl;

	if ( pEvent == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pEvent, 0, sizeof(xcoevent));
	pImpl = __xrtCoEventImpl(pEvent);
	if ( !xrtMutexInit(&pImpl->Lock) ) {
		return false;
	}
	pImpl->ManualReset = bManualReset;
	pImpl->Signaled = bSignaled;
	pImpl->Magic = XRT_CO_EVENT_MAGIC;
	return true;
}



/* 释放没有活动等待者的协程事件。 */
XRT_API bool xrtCoEventUnit(xcoevent* pEvent)
{
	xrt_co_event_impl* pImpl = __xrtCoEventRequire(pEvent);

	if ( pImpl == NULL ) {
		return false;
	}
	if ( !xrtMutexLock(&pImpl->Lock) ) {
		return false;
	}
	if ( pImpl->ActiveWaits != 0 ) {
		(void)xrtMutexUnlock(&pImpl->Lock);
		__xrtErrorSetInvalidState();
		return false;
	}
	pImpl->Magic = 0;
	(void)xrtMutexUnlock(&pImpl->Lock);
	if ( !xrtMutexUnit(&pImpl->Lock) ) {
		pImpl->Magic = XRT_CO_EVENT_MAGIC;
		return false;
	}
	memset(pEvent, 0, sizeof(xcoevent));
	return true;
}



/* 创建自动或手动复位协程事件。 */
XRT_API xcoevent* xrtCoEventCreate(
	bool bManualReset,
	bool bSignaled
)
{
	xcoevent* pEvent = (xcoevent*)xrtMalloc(sizeof(xcoevent));

	if ( pEvent == NULL ) {
		return NULL;
	}
	if ( !xrtCoEventInit(pEvent, bManualReset, bSignaled) ) {
		xrtFree(pEvent);
		return NULL;
	}
	return pEvent;
}



/* 释放 Create 返回的协程事件。 */
XRT_API bool xrtCoEventDestroy(xcoevent* pEvent)
{
	if ( pEvent == NULL ) {
		return true;
	}
	if ( !xrtCoEventUnit(pEvent) ) {
		return false;
	}
	xrtFree(pEvent);
	return true;
}



/* 置位事件并按照复位模式通知等待者。 */
XRT_API bool xrtCoEventSet(xcoevent* pEvent)
{
	xrt_co_event_impl* pImpl = __xrtCoEventRequire(pEvent);
	xrt_co_event_waiter* pWaiter;

	if ( pImpl == NULL ) {
		return false;
	}
	if ( !xrtMutexLock(&pImpl->Lock) ) {
		return false;
	}
	if ( pImpl->ManualReset ) {
		pImpl->Signaled = true;
		while ( (pWaiter = __xrtCoEventWaitPop(pImpl)) != NULL ) {
			__xrtCoEventSignalWaiter(pWaiter);
		}
	} else {
		pWaiter = __xrtCoEventWaitPop(pImpl);
		if ( pWaiter != NULL ) {
			__xrtCoEventSignalWaiter(pWaiter);
		} else {
			pImpl->Signaled = true;
		}
	}
	return xrtMutexUnlock(&pImpl->Lock);
}



/* 清除事件的信号态。 */
XRT_API bool xrtCoEventReset(xcoevent* pEvent)
{
	xrt_co_event_impl* pImpl = __xrtCoEventRequire(pEvent);

	if ( pImpl == NULL ) {
		return false;
	}
	if ( !xrtMutexLock(&pImpl->Lock) ) {
		return false;
	}
	pImpl->Signaled = false;
	return xrtMutexUnlock(&pImpl->Lock);
}



/* 等待事件置位、协程取消或截止时间。 */
XRT_API xwaitresult xrtCoEventAwaitUntil(
	xcoevent* pEvent,
	xdeadline iDeadline
)
{
	xrt_co_event_impl* pImpl = __xrtCoEventRequire(pEvent);
	xrt_co_event_waiter tWaiter;
	xcoro* pCo;
	xwaitresult Result;

	if ( pImpl == NULL ) {
		return XWAIT_ERROR;
	}
	pCo = xrtCoCurrent();
	if ( (pCo == NULL) || (xrtCoSchedCurrent() == NULL) ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	memset(&tWaiter, 0, sizeof(tWaiter));
	if ( !__xrtCoWaitOpen(pCo, &tWaiter.Wait) ) {
		return XWAIT_ERROR;
	}
	if ( xrtCancelRequested(pCo->Cancel) ) {
		__xrtCoWaitClose(&tWaiter.Wait);
		return XWAIT_CANCELLED;
	}
	if ( !xrtMutexLock(&pImpl->Lock) ) {
		__xrtCoWaitClose(&tWaiter.Wait);
		return XWAIT_ERROR;
	}
	if ( pImpl->Signaled ) {
		if ( !pImpl->ManualReset ) {
			pImpl->Signaled = false;
		}
		(void)xrtMutexUnlock(&pImpl->Lock);
		__xrtCoWaitClose(&tWaiter.Wait);
		return XWAIT_OK;
	}
	if ( xrtDeadlineExpired(iDeadline) ) {
		(void)xrtMutexUnlock(&pImpl->Lock);
		__xrtCoWaitClose(&tWaiter.Wait);
		return XWAIT_TIMEOUT;
	}
	if ( pImpl->ActiveWaits == SIZE_MAX ) {
		(void)xrtMutexUnlock(&pImpl->Lock);
		__xrtCoWaitClose(&tWaiter.Wait);
		__xrtErrorSetSizeOverflow();
		return XWAIT_ERROR;
	}
	pImpl->ActiveWaits++;
	__xrtCoEventWaitPush(pImpl, &tWaiter);
	(void)xrtMutexUnlock(&pImpl->Lock);

	for ( ;; ) {
		Result = __xrtCoWaitParkUntil(&tWaiter.Wait, iDeadline);
		if ( !xrtMutexLock(&pImpl->Lock) ) {
			Result = XWAIT_ERROR;
			break;
		}
		if ( tWaiter.Signaled ) {
			Result = XWAIT_OK;
			(void)xrtMutexUnlock(&pImpl->Lock);
			break;
		}
		if ( Result != XWAIT_OK ) {
			__xrtCoEventWaitRemove(pImpl, &tWaiter);
			(void)xrtMutexUnlock(&pImpl->Lock);
			break;
		}
		(void)xrtMutexUnlock(&pImpl->Lock);
	}

	if ( xrtMutexLock(&pImpl->Lock) ) {
		__xrtCoEventWaitRemove(pImpl, &tWaiter);
		pImpl->ActiveWaits--;
		(void)xrtMutexUnlock(&pImpl->Lock);
	} else {
		Result = XWAIT_ERROR;
	}
	__xrtCoWaitClose(&tWaiter.Wait);
	return Result;
}



/* 无限期等待事件置位。 */
XRT_API xwaitresult xrtCoEventAwait(xcoevent* pEvent)
{
	return xrtCoEventAwaitUntil(pEvent, XRT_DEADLINE_NEVER);
}



/* 非阻塞地检查并消费自动复位事件。 */
XRT_API xwaitresult xrtCoEventTryAwait(xcoevent* pEvent)
{
	return xrtCoEventAwaitUntil(pEvent, xrtClock());
}



/* 在相对微秒数内等待事件置位。 */
XRT_API xwaitresult xrtCoEventAwaitFor(
	xcoevent* pEvent,
	uint64 iTimeout
)
{
	return xrtCoEventAwaitUntil(pEvent, xrtDeadlineAfter(iTimeout));
}

#endif
