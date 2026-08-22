#include "../internal/xrt_net.h"
#include "../internal/xrt_net_sync.h"



#if defined(XRT_FEATURE_NET_SYNC)

/* 为同步等待的控制流终态建立稳定的网络错误。 */
static void __xrtNetSyncSetError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtNetSetError(
		Kind,
		iCode,
		sOperation,
		sMessage,
		0
	);
}



/* 把 Future 终态复制为同步结果；失败原因仍由 Future 稳定持有。 */
static bool __xrtNetSyncResult(
	xfuture* pFuture,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
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
			__xrtNetSyncSetError(
				XERR_IO,
				iCode,
				sOperation,
				"network Future failed without an error"
			);
		}
		return false;
	}
	if ( pResult->State == XFUTURE_CANCELLED ) {
		__xrtNetSyncSetError(
			XERR_CANCELLED,
			iCode,
			sOperation,
			sMessage
		);
		return false;
	}
	__xrtNetSyncSetError(
		XERR_CLOSED,
		iCode,
		sOperation,
		sMessage
	);
	return false;
}



/*
	阻塞调用线程等待现有网络 Future，不创建隐藏 Engine 或辅助线程。
	Worker 内只允许读取已经完成的结果，避免阻塞自身事件循环。
*/
bool __xrtNetSyncWait(
	xfuture* pFuture,
	const xnetworker* pWorker,
	xdeadline iDeadline,
	xcancel* pCancel,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	xfutureresult* pResult
)
{
	xwaitresult Wait;

	if ( (pFuture == NULL) || (pResult == NULL) ||
		 (sOperation == NULL) || (sMessage == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtFutureDone(pFuture) &&
		 (pWorker != NULL) && xrtNetWorkerIsCurrent(pWorker) ) {
		(void)xrtFutureCancel(pFuture);
		__xrtNetSyncSetError(
			XERR_STATE,
			iCode,
			sOperation,
			"a network worker cannot block on its own operation"
		);
		return false;
	}
	Wait = xrtFutureWaitUntilCancel(
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
		__xrtNetSyncSetError(
			Wait == XWAIT_TIMEOUT ? XERR_TIMEOUT : XERR_CANCELLED,
			iCode,
			sOperation,
			sMessage
		);
		return false;
	}
	return __xrtNetSyncResult(
		pFuture,
		iCode,
		sOperation,
		sMessage,
		pResult
	);
}

#endif
