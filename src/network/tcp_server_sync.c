#include "../internal/xrt_tcp_server.h"
#include "../internal/xrt_net_sync.h"



#if defined(XRT_FEATURE_NET_TCP_SERVER_SYNC)

/* 阻塞接受一个聚合连接，并把 Future 持有转换为调用方引用。 */
XRT_API xnetstream* xrtNetServerAcceptWait(
	xnetserver* pServer,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xnetstream* pStream = NULL;

	if ( pServer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtNetEngineCurrent(pServer->Engine) != NULL ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_SERVER_ACCEPT,
			"accept-server",
			"an Engine Worker cannot block on a TCP server",
			0
		);
		return NULL;
	}
	pFuture = xrtNetServerAcceptAsync(pServer);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtNetSyncWait(
		pFuture,
		NULL,
		iDeadline,
		pCancel,
		XNET_ERROR_SERVER_ACCEPT,
		"accept-server",
		"TCP server accept did not complete",
		&Result
	) ) {
		pStream = xrtNetStreamRef((xnetstream*)Result.Value);
	}
	xrtFutureDestroy(pFuture);
	return pStream;
}

#endif
