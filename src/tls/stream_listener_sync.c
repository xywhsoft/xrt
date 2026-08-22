#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC)

/* 把 Future 终态映射到 TLS 错误域。 */
static bool __xrtTlsListenerSyncResult(
	xfuture* pFuture,
	xfutureresult* pResult
)
{
	xerror* pError;

	if ( !xrtFutureResult(pFuture, pResult) ) {
		return false;
	}
	if ( pResult->State == XFUTURE_RESOLVED ) {
		return true;
	}
	if ( pResult->State == XFUTURE_FAILED ) {
		pError = xrtErrorRef(pResult->Error);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		} else {
			__xrtTlsError(
				XERR_IO,
				XTLS_ERROR_INTERNAL,
				"accept-tls-listener",
				"TLS listener Future failed without an error",
				SIZE_MAX
			);
		}
		return false;
	}
	__xrtTlsError(
		pResult->State == XFUTURE_CANCELLED ?
			XERR_CANCELLED : XERR_CLOSED,
		XTLS_ERROR_CLOSED,
		"accept-tls-listener",
		"TLS listener accept did not complete",
		SIZE_MAX
	);
	return false;
}



/* 等待 Future 的首个线性化终态，并保持超时与取消优先级。 */
static bool __xrtTlsListenerSyncWait(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel,
	xfutureresult* pResult
)
{
	xwaitresult Wait = xrtFutureWaitUntilCancel(
		pFuture,
		iDeadline,
		pCancel
	);

	if ( Wait == XWAIT_ERROR ) {
		(void)xrtFutureCancel(pFuture);
		return false;
	}
	if ( Wait != XWAIT_OK ) {
		(void)xrtFutureCancel(pFuture);
		__xrtTlsError(
			Wait == XWAIT_TIMEOUT ? XERR_TIMEOUT : XERR_CANCELLED,
			XTLS_ERROR_CLOSED,
			"accept-tls-listener",
			"TLS listener accept did not complete",
			SIZE_MAX
		);
		return false;
	}
	return __xrtTlsListenerSyncResult(pFuture, pResult);
}

/* 阻塞等待一个完成握手的 TLS Stream，并克隆 Future 持有的引用。 */
XRT_API xtlsstream* xrtTlsListenerAcceptWait(
	xtlslistener* pListener,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xtlsstream* pStream = NULL;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN) &&
		(xrtNetEngineCurrent(pListener->Engine) != NULL) ) {
		__xrtTlsError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"accept-tls-listener",
			"an Engine Worker cannot block on a TLS listener",
			SIZE_MAX
		);
		return NULL;
	}
	pFuture = xrtTlsListenerAcceptAsync(pListener);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtTlsListenerSyncWait(
		pFuture,
		iDeadline,
		pCancel,
		&Result
	) ) {
		pStream = xrtTlsStreamRef((xtlsstream*)Result.Value);
	}
	xrtFutureDestroy(pFuture);
	return pStream;
}

#endif
