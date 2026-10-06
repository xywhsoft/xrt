#include <xrt/detail/wait.h>
#include "../internal/xrt_future.h"
#include "../internal/xrt_coroutine.h"



#if defined(XRT_FEATURE_FUTURE_COROUTINE)

/* 挂起当前调度协程，直到 Future 完成、超时或协程取消。 */
XRT_API xwaitresult __xrtFutureAwaitUntil(xfuture* pFuture, double iDeadline)
{
    if ( !__xrtWaitValid(iDeadline) ) { return XWAIT_ERROR; }

	xrt_future_waiter tWaiter;
	xrt_co_wait tWait;
	xcoro* pCurrent;
	xwaitresult Result = XWAIT_OK;

	pFuture = xrtFutureRef(pFuture);
	if ( pFuture == NULL ) {
		return XWAIT_ERROR;
	}
	pCurrent = xrtCoCurrent();
	if ( (pCurrent == NULL) || (xrtCoSchedCurrent() == NULL) ) {
		xrtFutureDestroy(pFuture);
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	if ( xrtFutureDone(pFuture) ) {
		xrtFutureDestroy(pFuture);
		return XWAIT_OK;
	}
	if ( !__xrtCoWaitOpen(pCurrent, &tWait) ) {
		xrtFutureDestroy(pFuture);
		return XWAIT_ERROR;
	}
	memset(&tWaiter, 0, sizeof(tWaiter));
	tWaiter.Proc = __xrtCoWaitWake;
	tWaiter.Data = &tWait;
	if ( !__xrtFutureWaiterAdd(pFuture, &tWaiter) ) {
		if ( xrtFutureDone(pFuture) ) {
			__xrtCoWaitClose(&tWait);
			xrtFutureDestroy(pFuture);
			return XWAIT_OK;
		}
		__xrtCoWaitClose(&tWait);
		xrtFutureDestroy(pFuture);
		return XWAIT_ERROR;
	}
	while ( !xrtFutureDone(pFuture) ) {
		Result = __xrtCoWaitParkUntil(&tWait, iDeadline);
		if ( Result != XWAIT_OK ) {
			break;
		}
	}
	__xrtFutureWaiterRemove(pFuture, &tWaiter);
	__xrtCoWaitClose(&tWait);
	if ( xrtFutureDone(pFuture) ) {
		Result = XWAIT_OK;
	}
	xrtFutureDestroy(pFuture);
	return Result;
}



/* 永久挂起当前调度协程等待 Future。 */
XRT_API xwaitresult xrtFutureAwait(xfuture* pFuture)
{
	return __xrtFutureAwaitUntil(pFuture, INFINITY);
}



/* 在相对毫秒数内挂起当前调度协程等待 Future。 */
XRT_API xwaitresult xrtFutureAwaitFor(xfuture* pFuture, int64 iTimeout)
{
	return __xrtFutureAwaitUntil(pFuture, __xrtWaitAfter(iTimeout));
}

#endif
