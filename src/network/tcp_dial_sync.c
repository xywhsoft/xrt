#include "../internal/xrt_tcp.h"
#include "../internal/xrt_net_sync.h"



#if defined(XRT_FEATURE_NET_TCP_DIAL_SYNC)

/* 阻塞完成受管 TCP Dial，并把成功 Stream 转换为调用方引用。 */
XRT_API xnetstream* xrtNetConnect(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xnetstream* pStream = NULL;

	pFuture = xrtNetDialAsync(
		pEngine,
		pResolver,
		sHost,
		iPort,
		pConfig,
		pStreamEvents,
		pStreamData
	);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtNetSyncWait(
		pFuture,
		xrtNetEngineCurrent(pEngine),
		iDeadline,
		pCancel,
		XNET_ERROR_STREAM_CONNECT,
		"connect-stream",
		"TCP connection did not complete",
		&Result
	) ) {
		pStream = xrtNetStreamRef((xnetstream*)Result.Value);
	}
	xrtFutureDestroy(pFuture);
	return pStream;
}

#endif
