#include "../internal/xrt_udp.h"
#include "../internal/xrt_net_sync.h"



#if defined(XRT_FEATURE_NET_UDP_SYNC)

/* 把 UDP 等待条件映射到最接近的稳定错误码。 */
static int32 __xrtNetUdpWaitCode(xnetudpwait Wait)
{
	if ( Wait == XNET_UDP_WAIT_OPEN ) {
		return XNET_ERROR_UDP_CREATE;
	}
	if ( (Wait == XNET_UDP_WAIT_RECEIVE) ||
		 (Wait == XNET_UDP_WAIT_ERROR) ) {
		return XNET_ERROR_UDP_RECEIVE;
	}
	if ( Wait == XNET_UDP_WAIT_DRAIN ) {
		return XNET_ERROR_UDP_SEND;
	}
	return XNET_ERROR_UDP_CLOSE;
}



/* 阻塞等待一个 UDP 条件，不复制底层状态机。 */
XRT_API bool xrtNetUdpWait(
	xnetudp* pUdp,
	xnetudpwait Wait,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	bool bReady;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pFuture = xrtNetUdpWaitAsync(pUdp, Wait);
	if ( pFuture == NULL ) {
		return false;
	}
	bReady = __xrtNetSyncWait(
		pFuture,
		xrtNetUdpWorker(pUdp),
		iDeadline,
		pCancel,
		__xrtNetUdpWaitCode(Wait),
		"wait-udp",
		"UDP wait did not complete",
		&Result
	);
	xrtFutureDestroy(pFuture);
	return bReady;
}



/* 阻塞等待一个数据报发送预算。 */
XRT_API bool xrtNetUdpWritable(
	xnetudp* pUdp,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	bool bReady;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pFuture = xrtNetUdpWritableAsync(pUdp, iSize);
	if ( pFuture == NULL ) {
		return false;
	}
	bReady = __xrtNetSyncWait(
		pFuture,
		xrtNetUdpWorker(pUdp),
		iDeadline,
		pCancel,
		XNET_ERROR_UDP_SEND,
		"wait-udp-writable",
		"UDP writable wait did not complete",
		&Result
	);
	xrtFutureDestroy(pFuture);
	return bReady;
}



/* 阻塞接收一个数据包，并把 Future 持有转换为调用方引用。 */
XRT_API xnetudppacket* xrtNetUdpReceiveWait(
	xnetudp* pUdp,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xnetudppacket* pPacket = NULL;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFuture = xrtNetUdpReceiveAsync(pUdp);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtNetSyncWait(
		pFuture,
		xrtNetUdpWorker(pUdp),
		iDeadline,
		pCancel,
		XNET_ERROR_UDP_RECEIVE,
		"receive-udp",
		"UDP receive did not complete",
		&Result
	) ) {
		pPacket = xrtNetUdpPacketRef((xnetudppacket*)Result.Value);
	}
	xrtFutureDestroy(pFuture);
	return pPacket;
}



/* 阻塞接收一个数据报错误，并把 Future 持有转换为调用方引用。 */
XRT_API xnetudperrorpacket* xrtNetUdpReceiveErrorWait(
	xnetudp* pUdp,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xnetudperrorpacket* pPacket = NULL;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFuture = xrtNetUdpReceiveErrorAsync(pUdp);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtNetSyncWait(
		pFuture,
		xrtNetUdpWorker(pUdp),
		iDeadline,
		pCancel,
		XNET_ERROR_UDP_RECEIVE,
		"receive-udp-error",
		"UDP datagram error receive did not complete",
		&Result
	) ) {
		pPacket = xrtNetUdpErrorPacketRef(
			(xnetudperrorpacket*)Result.Value
		);
	}
	xrtFutureDestroy(pFuture);
	return pPacket;
}



/* 阻塞接收一个数据包批次，并把 Future 持有转换为调用方引用。 */
XRT_API xnetudpbatch* xrtNetUdpReceiveBatchWait(
	xnetudp* pUdp,
	size_t iCapacity,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture;
	xfutureresult Result;
	xnetudpbatch* pBatch = NULL;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFuture = xrtNetUdpReceiveBatchAsync(pUdp, iCapacity);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( __xrtNetSyncWait(
		pFuture,
		xrtNetUdpWorker(pUdp),
		iDeadline,
		pCancel,
		XNET_ERROR_UDP_RECEIVE,
		"receive-udp-batch",
		"UDP batch receive did not complete",
		&Result
	) ) {
		pBatch = xrtNetUdpBatchRef((xnetudpbatch*)Result.Value);
	}
	xrtFutureDestroy(pFuture);
	return pBatch;
}

#endif
