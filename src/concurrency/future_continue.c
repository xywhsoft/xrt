#include "../internal/xrt_future.h"



#if defined(XRT_FEATURE_FUTURE_CONTINUE)

/* 延续模式决定回调命中的源终态，以及未命中时是否自动透传。 */
typedef enum xrt_future_continue_mode {
	XRT_FUTURE_CONTINUE_ALL = 0,
	XRT_FUTURE_CONTINUE_SUCCESS = 1,
	XRT_FUTURE_CONTINUE_FAILED = 2,
	XRT_FUTURE_CONTINUE_FINALLY = 3
} xrt_future_continue_mode;



/* 每个延续节点只占一次分配，并由源 Future 的等待节点生命周期管理。 */
typedef struct xrt_future_continue {
	xrt_future_waiter Waiter;
	xfuture* Source;
	xpromise* Promise;
	xcancel* Cancel;
	xcancelwatch* CancelSourceWatch;
	xfuturecontinueproc Proc;
	xfuturefinallyproc Finally;
	ptr Data;
	xfuturefreeproc Destroy;
	ptr DestroyData;
	xrt_future_continue_mode Mode;
} xrt_future_continue;



/* 独占生产链的输出被取消时，立即把协作取消请求传给源 Future。 */
static void __xrtFutureContinueCancelSource(ptr pData)
{
	xrt_future_continue* pContinue =
		(xrt_future_continue*)pData;

	(void)xrtFutureCancel(pContinue->Source);
}



/* 判断当前源终态是否需要进入可改变输出结果的延续过程。 */
static bool __xrtFutureContinueShouldRun(
	const xrt_future_continue* pContinue,
	xfuturestate State
)
{
	if ( pContinue->Mode == XRT_FUTURE_CONTINUE_ALL ) {
		return true;
	}
	if ( pContinue->Mode == XRT_FUTURE_CONTINUE_SUCCESS ) {
		return State == XFUTURE_RESOLVED;
	}
	return (pContinue->Mode == XRT_FUTURE_CONTINUE_FAILED) &&
		(State == XFUTURE_FAILED);
}



/* 源完成后执行短延续；未完成的输出 Promise 在最后引用离开时自动关闭。 */
static void __xrtFutureContinueRun(ptr pData)
{
	xrt_future_continue* pContinue = (xrt_future_continue*)pData;
	xfutureresult tInput;
	bool bCancelled = xrtCancelRequested(pContinue->Cancel);

	if ( !xrtFutureResult(pContinue->Source, &tInput) ) {
		(void)xrtPromiseClose(pContinue->Promise);
	} else if ( pContinue->Mode == XRT_FUTURE_CONTINUE_FINALLY ) {
		pContinue->Finally(&tInput, pContinue->Data);
		if ( xrtCancelRequested(pContinue->Cancel) ) {
			(void)xrtPromiseCancel(pContinue->Promise);
		} else {
			(void)xrtPromiseForward(
				pContinue->Promise,
				pContinue->Source
			);
		}
	} else if ( bCancelled ) {
		(void)xrtPromiseCancel(pContinue->Promise);
	} else if ( __xrtFutureContinueShouldRun(pContinue, tInput.State) ) {
		pContinue->Proc(&tInput, pContinue->Promise, pContinue->Data);
	} else {
		(void)xrtPromiseForward(pContinue->Promise, pContinue->Source);
	}
	xrtPromiseDestroy(pContinue->Promise);
	pContinue->Promise = NULL;
}



/* 完成批次释放节点持有的数据、取消令牌和源 Future。 */
static void __xrtFutureContinueRelease(ptr pData)
{
	xrt_future_continue* pContinue = (xrt_future_continue*)pData;
	xcancelwatch* pWatch = pContinue->CancelSourceWatch;

	pContinue->CancelSourceWatch = NULL;
	xrtCancelUnwatch(pWatch);
	if ( pContinue->Promise != NULL ) {
		xrtPromiseDestroy(pContinue->Promise);
	}
	if ( pContinue->Destroy != NULL ) {
		pContinue->Destroy(pContinue->Data, pContinue->DestroyData);
	}
	xrtCancelDestroy(pContinue->Cancel);
	xrtFutureDestroy(pContinue->Source);
	xrtFree(pContinue);
}



/* 统一创建四类延续，保证并发完成、立即完成和失败回滚使用同一条路径。 */
static xfuture* __xrtFutureContinueCreate(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	xfuturefinallyproc pFinally,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData,
	bool bCancelSource,
	xrt_future_continue_mode Mode
)
{
	xrt_future_continue* pContinue;
	xfuture* pOutput;
	xpromise* pPromise;

	if ( (pSource == NULL) ||
		((Mode == XRT_FUTURE_CONTINUE_FINALLY) && (pFinally == NULL)) ||
		((Mode != XRT_FUTURE_CONTINUE_FINALLY) && (pProc == NULL)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pSource = xrtFutureRef(pSource);
	if ( pSource == NULL ) {
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pOutput, NULL);
	if ( pPromise == NULL ) {
		xrtFutureDestroy(pSource);
		return NULL;
	}
	pContinue = (xrt_future_continue*)xrtCalloc(
		1,
		sizeof(xrt_future_continue)
	);
	if ( pContinue == NULL ) {
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pOutput);
		xrtFutureDestroy(pSource);
		return NULL;
	}
	pContinue->Source = pSource;
	pContinue->Promise = pPromise;
	pContinue->Cancel = xrtPromiseCancelToken(pPromise);
	pContinue->Proc = pProc;
	pContinue->Finally = pFinally;
	pContinue->Data = pData;
	pContinue->Destroy = pDestroy;
	pContinue->DestroyData = pDestroyData;
	pContinue->Mode = Mode;
	pContinue->Waiter.Proc = __xrtFutureContinueRun;
	pContinue->Waiter.Release = __xrtFutureContinueRelease;
	pContinue->Waiter.Data = pContinue;
	if ( pContinue->Cancel == NULL ) {
		pContinue->Destroy = NULL;
		__xrtFutureContinueRelease(pContinue);
		xrtFutureDestroy(pOutput);
		return NULL;
	}
	if ( bCancelSource ) {
		pContinue->CancelSourceWatch = xrtCancelWatch(
			pContinue->Cancel,
			__xrtFutureContinueCancelSource,
			pContinue
		);
		if ( pContinue->CancelSourceWatch == NULL ) {
			pContinue->Destroy = NULL;
			__xrtFutureContinueRelease(pContinue);
			xrtFutureDestroy(pOutput);
			return NULL;
		}
	}
	if ( !__xrtFutureWaiterAdd(pSource, &pContinue->Waiter) ) {
		if ( !xrtFutureDone(pSource) ) {
			pContinue->Destroy = NULL;
			__xrtFutureContinueRelease(pContinue);
			xrtFutureDestroy(pOutput);
			return NULL;
		}
		__xrtFutureContinueRun(pContinue);
		__xrtFutureContinueRelease(pContinue);
	}
	return pOutput;
}



/* 对源的任意终态执行借用数据延续。 */
XRT_API xfuture* xrtFutureContinue(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
)
{
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		NULL,
		NULL,
		false,
		XRT_FUTURE_CONTINUE_ALL
	);
}



/* 对源的任意终态执行延续并接管回调数据。 */
XRT_API xfuture* xrtFutureContinueOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( pDestroy == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		pDestroy,
		pDestroyData,
		false,
		XRT_FUTURE_CONTINUE_ALL
	);
}



/* 仅对成功源执行借用数据延续。 */
XRT_API xfuture* xrtFutureThen(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
)
{
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		NULL,
		NULL,
		false,
		XRT_FUTURE_CONTINUE_SUCCESS
	);
}



/* 仅对成功源执行延续并接管回调数据。 */
XRT_API xfuture* xrtFutureThenOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( pDestroy == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		pDestroy,
		pDestroyData,
		false,
		XRT_FUTURE_CONTINUE_SUCCESS
	);
}



/*
	组合层明确独占源生产链时使用该延续。
	普通延续仍保持单向观察语义，避免一个分支取消共享源。
*/
XRT_API xfuture* xrtFutureThenOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( pDestroy == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		pDestroy,
		pDestroyData,
		true,
		XRT_FUTURE_CONTINUE_SUCCESS
	);
}



/*
	独占生产链需要检查失败或取消终态时使用该延续。
	输出取消仍只影响这一条显式组合链。
*/
XRT_API xfuture* xrtFutureContinueOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( pDestroy == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		pDestroy,
		pDestroyData,
		true,
		XRT_FUTURE_CONTINUE_ALL
	);
}



/* 仅对失败源执行借用数据延续。 */
XRT_API xfuture* xrtFutureCatch(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
)
{
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		NULL,
		NULL,
		false,
		XRT_FUTURE_CONTINUE_FAILED
	);
}



/* 仅对失败源执行延续并接管回调数据。 */
XRT_API xfuture* xrtFutureCatchOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( pDestroy == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtFutureContinueCreate(
		pSource,
		pProc,
		NULL,
		pData,
		pDestroy,
		pDestroyData,
		false,
		XRT_FUTURE_CONTINUE_FAILED
	);
}



/* 观察任意源终态并安全透传原结果。 */
XRT_API xfuture* xrtFutureFinally(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData
)
{
	return __xrtFutureContinueCreate(
		pSource,
		NULL,
		pProc,
		pData,
		NULL,
		NULL,
		false,
		XRT_FUTURE_CONTINUE_FINALLY
	);
}



/* 观察任意源终态、安全透传原结果并接管回调数据。 */
XRT_API xfuture* xrtFutureFinallyOwned(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( pDestroy == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtFutureContinueCreate(
		pSource,
		NULL,
		pProc,
		pData,
		pDestroy,
		pDestroyData,
		false,
		XRT_FUTURE_CONTINUE_FINALLY
	);
}

#endif
