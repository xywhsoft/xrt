#include "../internal/xrt_tcp.h"
#include "../internal/xrt_net_sync.h"



#if defined(XRT_FEATURE_NET_TCP_SYNC)

/* 把 Stream 等待条件映射到最接近的稳定错误码。 */
static int32 __xrtNetStreamWaitCode(xnetstreamwait Wait)
{
	if ( Wait == XNET_STREAM_WAIT_OPEN ) {
		return XNET_ERROR_STREAM_CONNECT;
	}
	if ( Wait == XNET_STREAM_WAIT_READ ) {
		return XNET_ERROR_STREAM_READ;
	}
	if ( (Wait == XNET_STREAM_WAIT_WRITE) ||
		 (Wait == XNET_STREAM_WAIT_DRAIN) ) {
		return XNET_ERROR_STREAM_WRITE;
	}
	return XNET_ERROR_STREAM_CLOSE;
}



/* 阻塞等待一个 Stream 条件，不复制底层状态机。 */
XRT_API bool xrtNetStreamWait(
	xnetstream* pStream,
	xnetstreamwait Wait,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	bool bReady;

	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pFuture = xrtNetStreamWaitAsync(pStream, Wait);
	if ( pFuture == NULL ) {
		return false;
	}
	bReady = __xrtNetSyncWait(
		pFuture,
		xrtNetStreamWorker(pStream),
		iDeadline,
		pCancel,
		__xrtNetStreamWaitCode(Wait),
		"wait-stream",
		"TCP stream wait did not complete",
		&Result
	);
	xrtFutureDestroy(pFuture);
	return bReady;
}



/* 阻塞等待拉取缓冲增长到指定字节数，不复制或消费现有前缀。 */
XRT_API bool xrtNetStreamWaitAvailable(
	xnetstream* pStream,
	size_t iMinimum,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	bool bReady;

	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pFuture = xrtNetStreamWaitAvailableAsync(pStream, iMinimum);
	if ( pFuture == NULL ) {
		return false;
	}
	bReady = __xrtNetSyncWait(
		pFuture,
		xrtNetStreamWorker(pStream),
		iDeadline,
		pCancel,
		XNET_ERROR_STREAM_READ,
		"wait-stream-available",
		"TCP stream readable byte wait did not complete",
		&Result
	);
	xrtFutureDestroy(pFuture);
	return bReady;
}



/* 阻塞接受一个连接，并把 Future 持有转换为调用方引用。 */
XRT_API xnetstream* xrtNetListenerAcceptWait(
	xnetlistener* pListener,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xnetstream* pStream = NULL;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFuture = xrtNetListenerAcceptAsync(pListener);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtNetSyncWait(
		pFuture,
		xrtNetListenerWorker(pListener),
		iDeadline,
		pCancel,
		XNET_ERROR_LISTENER_ACCEPT,
		"accept-listener",
		"TCP listener accept did not complete",
		&Result
	) ) {
		pStream = xrtNetStreamRef((xnetstream*)Result.Value);
	}
	xrtFutureDestroy(pFuture);
	return pStream;
}



/* 阻塞接收一段拥有型字节，并把 Future 持有转换为调用方引用。 */
XRT_API xnetbytes* xrtNetStreamRecv(
	xnetstream* pStream,
	size_t iMaxBytes,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xnetbytes* pBytes = NULL;

	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFuture = xrtNetStreamRecvAsync(pStream, iMaxBytes);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtNetSyncWait(
		pFuture,
		xrtNetStreamWorker(pStream),
		iDeadline,
		pCancel,
		XNET_ERROR_STREAM_READ,
		"receive-stream",
		"TCP stream receive did not complete",
		&Result
	) ) {
		pBytes = xrtNetBytesRef((xnetbytes*)Result.Value);
	}
	xrtFutureDestroy(pFuture);
	return pBytes;
}

#endif
