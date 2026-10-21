#include <xrt/detail/wait.h>
#include "../internal/xrt_tcp_server.h"
#include "../internal/xrt_net_sync.h"



#if defined(XRT_FEATURE_NET_TCP_SERVER_SYNC)

/* 阻塞接受一个聚合连接，并把 Future 持有转换为调用方引用。 */
static xnetstream* __xrtNetServerAcceptWaitBody(
	xnetserver* pServer,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return NULL; }

	xfuture* pFuture;
	xfutureresult Result;
	xnetstream* pStream = NULL;

	if ( pServer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	/* A closed shell may outlive its Engine after real retirement. Do not
	 * inspect its borrowed Engine pointer on that path. Active admission keeps
	 * the real Engine edge alive across every OPEN-path blocking call. */
	if (xrtNetServerState(pServer) != XNET_SERVER_OPEN) {
		__xrtNetSetError(XERR_CLOSED, XNET_ERROR_SERVER_ACCEPT, "accept-server", "TCP server is closed", 0);
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

XRT_API xnetstream* __xrtNetServerAcceptWait(xnetserver* pServer, double iDeadline, xcancel* pCancel)
{
	if (!__xrtNetServerEnter(pServer, true)) return NULL;
	xnetstream* pResult = __xrtNetServerAcceptWaitBody(pServer, iDeadline, pCancel);
	__xrtNetServerLeave(pServer);return pResult;
}

#endif

#if (defined(XRT_FEATURE_NET_TCP_SERVER)) && (defined(XRT_FEATURE_NET_TCP_SERVER_SYNC))
XRT_API xnetstream* xrtNetServerAcceptWait(
	xnetserver* pServer,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtNetServerAcceptWait(pServer, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
