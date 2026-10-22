#include "../internal/xrt_udp.h"



#if defined(XRT_FEATURE_NET_UDP)

_Static_assert(XNET_UDP_VECTOR_MAX == XRT_NET_SOCKET_VECTOR_LIMIT, "UDP vector bound differs from native socket bound");
_Static_assert(offsetof(__xrt_net_udp_send, Tail) % _Alignof(__xrt_net_udp_vector) == 0, "UDP vector tail alignment");

/* 可裁剪地推进 UDP 发送侧 Future 条件。 */
static void __xrtNetUdpSendNotifyFutures(xnetudp* pUdp)
{
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		__xrtNetUdpFutureNotify(pUdp);
	#else
		(void)pUdp;
	#endif
}

/* 原子占用一个发送数据报的字节和包数预算。 */
static bool __xrtNetUdpReserveSend(xnetudp* pUdp, size_t iSize)
{
	uint64 iPackets = xrtAtomic64Load(
		&pUdp->QueuedPackets,
		XMEMORY_ACQUIRE
	);
	uint64 iBytes;

	for ( ;; ) {
		uint64 iExpected = iPackets;

		if ( iPackets >= (uint64)pUdp->Config.SendPacketLimit ) {
			goto Rejected;
		}
		if ( xrtAtomic64CompareExchange(
			&pUdp->QueuedPackets,
			&iExpected,
			iPackets + 1,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		iPackets = iExpected;
	}

	iBytes = xrtAtomic64Load(&pUdp->QueuedBytes, XMEMORY_ACQUIRE);
	for ( ;; ) {
		uint64 iExpected = iBytes;

		if ( (uint64)iSize >
			 ((uint64)pUdp->Config.SendLimit - iBytes) ) {
			(void)xrtAtomic64FetchSub(
				&pUdp->QueuedPackets,
				1,
				XMEMORY_ACQ_REL
			);
			goto Rejected;
		}
		if ( xrtAtomic64CompareExchange(
			&pUdp->QueuedBytes,
			&iExpected,
			iBytes + (uint64)iSize,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		iBytes = iExpected;
	}
	__xrtNetUdpPeak(&pUdp->PeakQueuedPackets, iPackets + 1);
	__xrtNetUdpPeak(&pUdp->PeakQueuedBytes, iBytes + (uint64)iSize);
	return true;

Rejected:
	__xrtNetStatBasicAdd(&pUdp->SendRejected, 1);
	return false;
}



/* 归还一个尚未进入终态的发送预算。 */
static void __xrtNetUdpUnreserveSend(xnetudp* pUdp, size_t iSize)
{
	(void)xrtAtomic64FetchSub(
		&pUdp->QueuedPackets,
		1,
		XMEMORY_ACQ_REL
	);
	(void)xrtAtomic64FetchSub(
		&pUdp->QueuedBytes,
		(uint64)iSize,
		XMEMORY_ACQ_REL
	);
	__xrtNetUdpSendNotifyFutures(pUdp);
}



/* 进入跨线程发送提交区，阻止关闭越过已经开始的提交。 */
static bool __xrtNetUdpBeginSend(xnetudp* pUdp)
{
	if (!__xrtNetUdpOwnershipEnter(pUdp, true)) return false;
	if (xrtNetUdpState(pUdp) == XNET_UDP_CLOSED) { __xrtNetUdpOwnershipLeave(pUdp); return false; }
	bool bWorker = xrtNetWorkerIsCurrent(pUdp->Worker);
	xnetudpstate State;

	if ( !bWorker ) {
		(void)xrtAtomic32FetchAdd(
			&pUdp->SendSubmitters,
			1,
			XMEMORY_ACQ_REL
		);
	}
	State = xrtNetUdpState(pUdp);
	if ( ((State != XNET_UDP_OPENING) && (State != XNET_UDP_OPEN)) ||
		 xrtAtomic32Load(&pUdp->SendGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pUdp->AbortGate, XMEMORY_ACQUIRE) ) {
		if ( !bWorker ) {
			(void)xrtAtomic32FetchSub(
				&pUdp->SendSubmitters,
				1,
				XMEMORY_RELEASE
			);
		}
		__xrtNetUdpOwnershipLeave(pUdp);
		return false;
	}
	return true;
}



/* 离开跨线程发送提交区。 */
static void __xrtNetUdpEndSend(xnetudp* pUdp)
{
	if ( !xrtNetWorkerIsCurrent(pUdp->Worker) ) {
		uint32 iPrevious = xrtAtomic32FetchSub(
			&pUdp->SendSubmitters,
			1,
			XMEMORY_ACQ_REL
		);

		if ( iPrevious == 1 ) {
			__xrtNetUdpWakeLifecycle(pUdp);
		}
	}
	__xrtNetUdpOwnershipLeave(pUdp);
}



/* 解析并验证一次发送的目标地址。 */
static bool __xrtNetUdpRemote(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	xnetaddr* pResult
)
{
	if ( pRemote == NULL ) {
		if ( !pUdp->Connected ) {
			__xrtNetUdpSetError(
				XERR_ARGUMENT,
				XNET_ERROR_UDP_SEND,
				"send-udp",
				"unconnected UDP requires a remote address"
			);
			return false;
		}
		*pResult = pUdp->Peer;
		return true;
	}
	if ( (pRemote->Family != pUdp->Local.Family) ||
		 (pRemote->Port == 0) ||
		 (pUdp->Connected &&
		  !xrtNetAddrEqual(pRemote, &pUdp->Peer)) ) {
		__xrtNetUdpSetError(
			XERR_ARGUMENT,
			XNET_ERROR_UDP_SEND,
			"send-udp",
			"invalid UDP remote address"
		);
		return false;
	}
	*pResult = *pRemote;
	return true;
}



/* 释放由 SendTake 接管的数据。 */
static void __xrtNetUdpFreeTaken(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 返回发送节点创建时使用的精确请求大小。 */
static size_t __xrtNetUdpSendAllocation(
	const __xrt_net_udp_send* pSend
)
{
	size_t iAllocation = offsetof(__xrt_net_udp_send, Tail);

	if ( pSend->Controlled ) {
		iAllocation += sizeof(xnetdgramcontrol);
	}
	if ( pSend->Copied ) {
		iAllocation += pSend->Size;
	}
	if (pSend->PayloadOwnership == XRT_NET_UDP_SEND_VECTOR) {
		const __xrt_net_udp_vector* pVector = __xrtNetUdpSendVector(pSend);
		iAllocation += __xrtNetUdpVectorPrefix(pSend->Controlled) - (pSend->Controlled ? sizeof(xnetdgramcontrol) : 0) +
			offsetof(__xrt_net_udp_vector, Spans) +
			pVector->Capacity * (sizeof(xnetspan) + sizeof(xnetbytes*));
	}
	return iAllocation;
}



/* 分配一个已经占用硬预算的发送节点。 */
static __xrt_net_udp_send* __xrtNetUdpCreateSend(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl,
	size_t iSize,
	bool bCopy,
	size_t iExtra,
	xnetresult* pResult
)
{
	__xrt_net_udp_send* pSend;
	size_t iAllocation = offsetof(__xrt_net_udp_send, Tail);

	if ( iSize > XNET_UDP_PAYLOAD_MAX ) {
		__xrtNetUdpSetError(
			XERR_RANGE,
			XNET_ERROR_UDP_SEND,
			"send-udp",
			"UDP payload exceeds the supported maximum"
		);
		*pResult = XNET_RESULT_ERROR;
		return NULL;
	}
	if ( !__xrtNetUdpReserveSend(pUdp, iSize) ) {
		*pResult = XNET_RESULT_AGAIN;
		return NULL;
	}
	if ( bCopy ) {
		if ( iSize > (SIZE_MAX - iAllocation) ) {
			__xrtNetUdpUnreserveSend(pUdp, iSize);
			__xrtErrorSetSizeOverflow();
			*pResult = XNET_RESULT_ERROR;
			return NULL;
		}
		iAllocation += iSize;
	}
	if (iExtra > SIZE_MAX - iAllocation) {
		__xrtNetUdpUnreserveSend(pUdp, iSize); __xrtErrorSetSizeOverflow();
		*pResult = XNET_RESULT_ERROR; return NULL;
	}
	iAllocation += iExtra;
	if ( pControl != NULL ) {
		if ( sizeof(*pControl) > (SIZE_MAX - iAllocation) ) {
			__xrtNetUdpUnreserveSend(pUdp, iSize);
			__xrtErrorSetSizeOverflow();
			*pResult = XNET_RESULT_ERROR;
			return NULL;
		}
		iAllocation += sizeof(*pControl);
	}
	pSend = (__xrt_net_udp_send*)__xrtNetWorkerNodeAlloc(
		pUdp->Worker,
		iAllocation
	);
	if ( pSend == NULL ) {
		__xrtNetUdpUnreserveSend(pUdp, iSize);
		*pResult = XNET_RESULT_ERROR;
		return NULL;
	}
	pSend->Udp = xrtNetUdpRef(pUdp);
	if (!pSend->Udp) {
		__xrtNetWorkerNodeRecycle(pUdp->Worker, pSend, iAllocation);
		__xrtNetUdpUnreserveSend(pUdp, iSize); *pResult = XNET_RESULT_ERROR; return NULL;
	}
	pSend->Next = NULL;
	pSend->Previous = NULL;
	pSend->Slot = NULL;
	pSend->Submitted = false;
	pSend->Remote = *pRemote;
	pSend->Size = iSize;
	pSend->Copied = bCopy;
	pSend->Controlled = pControl != NULL;
	pSend->PayloadOwnership = bCopy ? XRT_NET_UDP_SEND_COPY : XRT_NET_UDP_SEND_OPAQUE;
	pSend->Data = NULL; pSend->Release = NULL; pSend->ReleaseContext = NULL; pSend->OwnsExternal = false;
	if ( pControl != NULL ) {
		*__xrtNetUdpSendControl(pSend) = *pControl;
	}
	*pResult = XNET_RESULT_OK;
	return pSend;
}



/* 丢弃一个尚未完成的发送节点。 */
void __xrtNetUdpDiscardSend(
	__xrt_net_udp_send* pSend,
	bool bReleaseExternal
)
{
	xnetudp* pUdp = pSend->Udp;
	xnetworker* pWorker = pUdp->Worker;
	size_t iAllocation = __xrtNetUdpSendAllocation(pSend);

	/* Typed factories already own their retained references, even before
	 * submission succeeds. Raw Take/Ref still transfer only on acceptance. */
	if ( (bReleaseExternal || __xrtNetUdpSendRetained(pSend)) && pSend->OwnsExternal &&
		 (pSend->Release != NULL) ) {
		pSend->Release(
			pSend->ReleaseContext,
			pSend->Data,
			pSend->Size
		);
	}
	__xrtNetUdpUnreserveSend(pUdp, pSend->Size);
	pSend->Next = NULL;
	pSend->Previous = NULL;
	pSend->Slot = NULL;
	pSend->Submitted = false;
	__xrtNetWorkerNodeRecycle(pWorker, pSend, iAllocation);
	xrtNetUdpDestroy(pUdp);
}



/* 发送节点终态释放预算、外部数据和对象引用。 */
void __xrtNetUdpReleaseSend(__xrt_net_udp_send* pSend)
{
	xnetudp* pUdp = pSend->Udp;
	xnetworker* pWorker = pUdp->Worker;
	size_t iAllocation = __xrtNetUdpSendAllocation(pSend);
	uint64 iBytes;
	uint64 iPackets;

	if ( pSend->OwnsExternal && (pSend->Release != NULL) ) {
		pSend->Release(
			pSend->ReleaseContext,
			pSend->Data,
			pSend->Size
		);
	}
	iBytes = xrtAtomic64FetchSub(
		&pUdp->QueuedBytes,
		(uint64)pSend->Size,
		XMEMORY_ACQ_REL
	) - (uint64)pSend->Size;
	iPackets = xrtAtomic64FetchSub(
		&pUdp->QueuedPackets,
		1,
		XMEMORY_ACQ_REL
	) - 1;
	if ( !pUdp->AbortRequested &&
		 !xrtAtomic32Load(&pUdp->AbortGate, XMEMORY_ACQUIRE) ) {
		if ( pUdp->HighWater &&
			 (iBytes <= (uint64)pUdp->Config.SendLowWater) ) {
			pUdp->HighWater = false;
			if ( pUdp->Events.LowWater != NULL ) {
				pUdp->Events.LowWater(
					pUdp,
					(size_t)iBytes,
					(size_t)iPackets,
					__xrtNetUdpDataCurrent(pUdp)
				);
			}
		}
		if ( (iPackets == 0) && (pUdp->Events.Drain != NULL) ) {
			pUdp->Events.Drain(
				pUdp,
				__xrtNetUdpDataCurrent(pUdp)
			);
		}
	}
	__xrtNetUdpSendNotifyFutures(pUdp);
	pSend->Next = NULL;
	pSend->Previous = NULL;
	pSend->Slot = NULL;
	pSend->Submitted = false;
	__xrtNetWorkerNodeRecycle(pWorker, pSend, iAllocation);
	xrtNetUdpDestroy(pUdp);
}



/* 将发送节点挂入 Worker 独占队列。 */
static bool __xrtNetUdpAttachSend(__xrt_net_udp_send* pSend)
{
	xnetudp* pUdp = pSend->Udp;
	xnetudpstate State = xrtNetUdpState(pUdp);

	if ( pUdp->AbortRequested ||
		 xrtAtomic32Load(&pUdp->AbortGate, XMEMORY_ACQUIRE) ||
		 (State == XNET_UDP_CLOSED) ) {
		return false;
	}
	pSend->Next = NULL;
	pSend->Previous = pUdp->SendTail;
	pSend->Slot = NULL;
	pSend->Submitted = false;
	if ( pUdp->SendTail != NULL ) {
		pUdp->SendTail->Next = pSend;
	} else {
		pUdp->SendHead = pSend;
	}
	pUdp->SendTail = pSend;
	if ( pUdp->SendReady == NULL ) {
		pUdp->SendReady = pSend;
	}
	if ( !pUdp->HighWater &&
		 (xrtAtomic64Load(&pUdp->QueuedBytes, XMEMORY_ACQUIRE) >=
		  (uint64)pUdp->Config.SendHighWater) ) {
		pUdp->HighWater = true;
		if ( pUdp->Events.HighWater != NULL ) {
			pUdp->Events.HighWater(
				pUdp,
				(size_t)xrtAtomic64Load(
					&pUdp->QueuedBytes,
					XMEMORY_RELAXED
				),
				(size_t)xrtAtomic64Load(
					&pUdp->QueuedPackets,
					XMEMORY_RELAXED
				),
				__xrtNetUdpDataCurrent(pUdp)
			);
		}
	}
	return true;
}



/* 在 Worker 上挂入一个发送节点链，并收敛被关闭入口拒绝的节点。 */
static void __xrtNetUdpAttachSendsImpl(__xrt_net_udp_send* pSend)
{
	xnetudp* pUdp = pSend->Udp;

	while ( pSend != NULL ) {
		__xrt_net_udp_send* pNext = pSend->Next;

		if ( !__xrtNetUdpAttachSend(pSend) ) {
			__xrtNetUdpDiscardSend(pSend, true);
		}
		pSend = pNext;
	}
	__xrtNetUdpDriveWrite(pUdp);
	__xrtNetUdpTryFinish(pUdp);
}

static void __xrtNetUdpAttachSends(__xrt_net_udp_send* pSend)
{
	xnetudp* pUdp = pSend->Udp;
	if (!xrtNetUdpRef(pUdp)) {
		while (pSend) { __xrt_net_udp_send* pNext = pSend->Next;
			__xrtNetUdpDiscardSend(pSend, true); pSend = pNext; }
		return;
	}
	__xrtNetUdpActiveEnter(pUdp);
	__xrtNetUdpAttachSendsImpl(pSend); __xrtNetUdpActiveLeave(pUdp);
	xrtNetUdpDestroy(pUdp);
}



/* 在 Worker 上接收一个跨线程发送节点链。 */
static void __xrtNetUdpQueueSend(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_net_udp_send* pSend = (__xrt_net_udp_send*)pData;
	xnetudp* pUdp = pSend->Udp;
	uint32 iCommands;

	(void)pWorker;
	iCommands = xrtAtomic32FetchSub(
		&pUdp->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( iCommands == 1 ) {
		__xrtNetUdpWakeLifecycle(pUdp);
	}
	__xrtNetUdpAttachSends(pSend);
}



/* 提交一个已经准备好的发送节点链。 */
static xnetresult __xrtNetUdpSubmitSend(
	xnetudp* pUdp,
	__xrt_net_udp_send* pHead
)
{
	if ( xrtNetWorkerIsCurrent(pUdp->Worker) ) {
		__xrtNetUdpAttachSends(pHead);
		return XNET_RESULT_OK;
	}
	(void)xrtAtomic32FetchAdd(
		&pUdp->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !__xrtNetEnginePostRuntime(
		pUdp->Engine,
		xrtNetWorkerIndex(pUdp->Worker),
		__xrtNetUdpQueueSend,
		pHead
	) ) {
		(void)xrtAtomic32FetchSub(
			&pUdp->SendCommands,
			1,
			XMEMORY_ACQ_REL
		);
		while ( pHead != NULL ) {
			__xrt_net_udp_send* pNext = pHead->Next;

			/* 未受理失败保持 Take/Ref 数据的调用方所有权。 */
			__xrtNetUdpDiscardSend(pHead, false);
			pHead = pNext;
		}
		return XNET_RESULT_ERROR;
	}
	return XNET_RESULT_OK;
}



/* 建立一种所有权的单数据报发送。 */
static xnetresult __xrtNetUdpSend(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext,
	uint32 iOwnership
)
{
	__xrt_net_udp_send* pSend;
	xnetaddr Remote;
	xnetresult Result;
	bool bCopy = iOwnership == XRT_NET_UDP_SEND_COPY;
	bool bOwnsExternal = !bCopy;

	if ( (pUdp == NULL) || ((pData == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetUdpRemote(pUdp, pRemote, &Remote) ) {
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetUdpBeginSend(pUdp) ) {
		return XNET_RESULT_CLOSED;
	}
	if ( pControl != NULL ) {
		union {
			uint64 Align;
			unsigned char Data[XRT_NET_SOCKET_DGRAM_CONTROL_SIZE];
		} ControlBuffer;
		size_t iControlSize;

		if ( !__xrtNetSocketDgramControlBuild(
			pUdp->Socket,
			pControl,
			iSize,
			ControlBuffer.Data,
			sizeof(ControlBuffer.Data),
			&iControlSize,
			XNET_ERROR_UDP_SEND,
			"send-udp-message"
		) ) {
			__xrtNetUdpEndSend(pUdp);
			return XNET_RESULT_ERROR;
		}
		(void)iControlSize;
	}
	pSend = __xrtNetUdpCreateSend(
		pUdp,
		&Remote,
		pControl,
		iSize,
		bCopy,
		0,
		&Result
	);
	if ( pSend == NULL ) {
		__xrtNetUdpEndSend(pUdp);
		return Result;
	}
	pSend->Release = pRelease;
	pSend->ReleaseContext = pContext;
	pSend->OwnsExternal = bOwnsExternal;
	pSend->PayloadOwnership = iOwnership;
	if ( bCopy ) {
		if ( iSize != 0 ) {
			memcpy(__xrtNetUdpSendCopy(pSend), pData, iSize);
		}
		pSend->Data = __xrtNetUdpSendCopy(pSend);
	} else {
		pSend->Data = (cbytes)pData;
	}
	Result = __xrtNetUdpSubmitSend(pUdp, pSend);
	__xrtNetUdpEndSend(pUdp);
	return Result;
}



/* 复制发送一个数据报。 */
XRT_API xnetresult xrtNetUdpSendTo(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const void* pData,
	size_t iSize
)
{
	return __xrtNetUdpSend(
		pUdp,
		pRemote,
		NULL,
		pData,
		iSize,
		NULL,
		NULL,
		XRT_NET_UDP_SEND_COPY
	);
}



/* 复制发送到连接式 UDP 的固定 Peer。 */
XRT_API xnetresult xrtNetUdpSend(
	xnetudp* pUdp,
	const void* pData,
	size_t iSize
)
{
	return xrtNetUdpSendTo(pUdp, NULL, pData, iSize);
}



/* 聚集复制为一个数据报。 */
XRT_API xnetresult xrtNetUdpSendVecTo(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const xnetspan* pSpans,
	size_t iCount
)
{
	__xrt_net_udp_send* pSend;
	xnetaddr Remote;
	xnetresult Result;
	size_t iTotal = 0;
	size_t iOffset = 0;

	if ( (pUdp == NULL) || ((pSpans == NULL) && (iCount != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (pSpans[i].Data == NULL) && (pSpans[i].Size != 0) ) {
			__xrtErrorSetInvalidArgument();
			return XNET_RESULT_ERROR;
		}
		if ( pSpans[i].Size > (SIZE_MAX - iTotal) ) {
			__xrtErrorSetSizeOverflow();
			return XNET_RESULT_ERROR;
		}
		iTotal += pSpans[i].Size;
	}
	if ( !__xrtNetUdpRemote(pUdp, pRemote, &Remote) ) {
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetUdpBeginSend(pUdp) ) {
		return XNET_RESULT_CLOSED;
	}
	pSend = __xrtNetUdpCreateSend(
		pUdp,
		&Remote,
		NULL,
		iTotal,
		true,
		0,
		&Result
	);
	if ( pSend == NULL ) {
		__xrtNetUdpEndSend(pUdp);
		return Result;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pSpans[i].Size != 0 ) {
			memcpy(
				__xrtNetUdpSendCopy(pSend) + iOffset,
				pSpans[i].Data,
				pSpans[i].Size
			);
			iOffset += pSpans[i].Size;
		}
	}
	pSend->Data = __xrtNetUdpSendCopy(pSend);
	Result = __xrtNetUdpSubmitSend(pUdp, pSend);
	__xrtNetUdpEndSend(pUdp);
	return Result;
}



/* 聚集复制到固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendVec(
	xnetudp* pUdp,
	const xnetspan* pSpans,
	size_t iCount
)
{
	return xrtNetUdpSendVecTo(pUdp, NULL, pSpans, iCount);
}



/* 引用发送一个数据报。 */
XRT_API xnetresult xrtNetUdpSendRefTo(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	if ( pRelease == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	return __xrtNetUdpSend(
		pUdp,
		pRemote,
		NULL,
		pData,
		iSize,
		pRelease,
		pContext,
		XRT_NET_UDP_SEND_OPAQUE
	);
}



/* 引用发送到固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendRef(
	xnetudp* pUdp,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	return xrtNetUdpSendRefTo(
		pUdp,
		NULL,
		pData,
		iSize,
		pRelease,
		pContext
	);
}



/* 接管发送一个数据报。 */
XRT_API xnetresult xrtNetUdpSendTakeTo(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	ptr pData,
	size_t iSize
)
{
	return __xrtNetUdpSend(
		pUdp,
		pRemote,
		NULL,
		pData,
		iSize,
		__xrtNetUdpFreeTaken,
		NULL,
		XRT_NET_UDP_SEND_TAKE
	);
}



/* 接管发送到固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendTake(
	xnetudp* pUdp,
	ptr pData,
	size_t iSize
)
{
	return xrtNetUdpSendTakeTo(pUdp, NULL, pData, iSize);
}



/* 复制发送一个带逐包控制的数据报。 */
XRT_API xnetresult xrtNetUdpSendMsg(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl,
	const void* pData,
	size_t iSize
)
{
	if ( (pControl == NULL) || (pControl->Flags == 0) ) {
		return xrtNetUdpSendTo(pUdp, pRemote, pData, iSize);
	}
	return __xrtNetUdpSend(
		pUdp,
		pRemote,
		pControl,
		pData,
		iSize,
		NULL,
		NULL,
		XRT_NET_UDP_SEND_COPY
	);
}



/* 引用发送一个带逐包控制的数据报。 */
XRT_API xnetresult xrtNetUdpSendMsgRef(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	if ( pRelease == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( (pControl == NULL) || (pControl->Flags == 0) ) {
		return xrtNetUdpSendRefTo(
			pUdp, pRemote, pData, iSize, pRelease, pContext
		);
	}
	return __xrtNetUdpSend(
		pUdp,
		pRemote,
		pControl,
		pData,
		iSize,
		pRelease,
		pContext,
		XRT_NET_UDP_SEND_OPAQUE
	);
}



/* 接管发送一个带逐包控制的数据报。 */
XRT_API xnetresult xrtNetUdpSendMsgTake(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl,
	ptr pData,
	size_t iSize
)
{
	if ( (pControl == NULL) || (pControl->Flags == 0) ) {
		return xrtNetUdpSendTakeTo(pUdp, pRemote, pData, iSize);
	}
	return __xrtNetUdpSend(
		pUdp,
		pRemote,
		pControl,
		pData,
		iSize,
		__xrtNetUdpFreeTaken,
		NULL,
		XRT_NET_UDP_SEND_TAKE
	);
}



/* 按前缀构造一个只需单次跨线程投递的发送链。 */
XRT_API xnetresult xrtNetUdpSendBatch(
	xnetudp* pUdp,
	const xnetdgramsend* pItems,
	size_t iCount,
	size_t* pAccepted
)
{
	__xrt_net_udp_send* pHead = NULL;
	__xrt_net_udp_send* pTail = NULL;
	xnetresult Result = XNET_RESULT_OK;
	size_t iAccepted = 0;

	if ( pAccepted != NULL ) {
		*pAccepted = 0;
	}
	if ( (pUdp == NULL) || (pAccepted == NULL) ||
		 ((pItems == NULL) && (iCount != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( iCount == 0 ) {
		return XNET_RESULT_OK;
	}
	if ( !__xrtNetUdpBeginSend(pUdp) ) {
		return XNET_RESULT_CLOSED;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrt_net_udp_send* pSend;
		xnetaddr Remote;

		if ( (pItems[i].Data == NULL) && (pItems[i].Size != 0) ) {
			__xrtErrorSetInvalidArgument();
			Result = XNET_RESULT_ERROR;
			break;
		}
		if ( !__xrtNetUdpRemote(
			pUdp,
			pItems[i].Remote,
			&Remote
		) ) {
			Result = XNET_RESULT_ERROR;
			break;
		}
		pSend = __xrtNetUdpCreateSend(
			pUdp,
			&Remote,
			NULL,
			pItems[i].Size,
			true,
			0,
			&Result
		);
		if ( pSend == NULL ) {
			break;
		}
		if ( pItems[i].Size != 0 ) {
			memcpy(
				__xrtNetUdpSendCopy(pSend),
				pItems[i].Data,
				pItems[i].Size
			);
		}
		pSend->Data = __xrtNetUdpSendCopy(pSend);
		if ( pTail != NULL ) {
			pTail->Next = pSend;
		} else {
			pHead = pSend;
		}
		pTail = pSend;
		iAccepted++;
	}
	if ( pHead != NULL ) {
		xnetresult SubmitResult = __xrtNetUdpSubmitSend(pUdp, pHead);

		if ( SubmitResult != XNET_RESULT_OK ) {
			iAccepted = 0;
			Result = SubmitResult;
		}
	}
	__xrtNetUdpEndSend(pUdp);
	*pAccepted = iAccepted;
	return Result;
}

static void __xrtNetUdpReleaseBytes(ptr pContext, cbytes pData, size_t iSize)
{ (void)pData; (void)iSize; xrtNetBytesDestroy(pContext); }
static void __xrtNetUdpReleasePacket(ptr pContext, cbytes pData, size_t iSize)
{ (void)pData; (void)iSize; xrtNetUdpPacketDestroy(pContext); }
static void __xrtNetUdpReleaseVector(ptr pContext, cbytes pData, size_t iSize)
{
	(void)pData; (void)iSize;
	__xrt_net_udp_vector* pVector = __xrtNetUdpSendVector(pContext);
	xnetbytes** ppOwners = __xrtNetUdpVectorOwners(pVector);
	for (size_t i = 0; i < pVector->Count; ++i) xrtNetBytesDestroy(ppOwners[i]);
	pVector->Count = 0;
}
static bool __xrtNetUdpSlice(xbytesview View, size_t iOffset, size_t iSize)
{
	if (iOffset > View.Size || iSize > View.Size - iOffset) { __xrtErrorSetRange(); return false; }
	return true;
}

/* A factory owns every reference it retains, independent of submission.
 * The original descriptor/reference stays caller-owned on ALL outcomes. */
static __xrt_net_udp_send* __xrtNetUdpCreateOwnedSend(xnetudp* pUdp,
	const xnetudpownedmessage* pItem, xnetresult* pResult)
{
	xnetaddr Remote; xbytesview View = {0}; size_t iOffset = 0, iSize = 0, iUsed = 0, iExtra = 0;
	const xnetdgramcontrol* pControl = pItem->Control && pItem->Control->Flags ? pItem->Control : NULL;
	if (!__xrtNetUdpRemote(pUdp, pItem->Remote, &Remote)) goto Failed;
	switch (pItem->Kind) {
	case XNET_UDP_OWNED_BYTES:
		if (!pItem->Payload.Bytes.Bytes) { __xrtErrorSetInvalidArgument(); goto Failed; }
		View = xrtNetBytesView(pItem->Payload.Bytes.Bytes); iOffset = pItem->Payload.Bytes.Offset; iSize = pItem->Payload.Bytes.Size;
		if (!__xrtNetUdpSlice(View, iOffset, iSize)) goto Failed;
		break;
	case XNET_UDP_OWNED_PACKET:
		if (!pItem->Payload.Packet.Packet) { __xrtErrorSetInvalidArgument(); goto Failed; }
		View.Data = xrtNetUdpPacketData(pItem->Payload.Packet.Packet); View.Size = xrtNetUdpPacketSize(pItem->Payload.Packet.Packet);
		iOffset = pItem->Payload.Packet.Offset; iSize = pItem->Payload.Packet.Size;
		if (!__xrtNetUdpSlice(View, iOffset, iSize)) goto Failed;
		break;
	case XNET_UDP_OWNED_VECTOR: {
		const xnetbytesref* pRefs = pItem->Payload.Vector.Items; size_t iCount = pItem->Payload.Vector.Count;
		if ((!pRefs && iCount) || iCount > XNET_UDP_VECTOR_MAX) { __xrtErrorSetInvalidArgument(); goto Failed; }
		for (size_t i = 0; i < iCount; ++i) {
			if (!pRefs[i].Bytes) { __xrtErrorSetInvalidArgument(); goto Failed; }
			xbytesview Part = xrtNetBytesView(pRefs[i].Bytes);
			if (!__xrtNetUdpSlice(Part, pRefs[i].Offset, pRefs[i].Size)) goto Failed;
			if (pRefs[i].Size > XNET_UDP_PAYLOAD_MAX - iSize) { __xrtErrorSetRange(); goto Failed; }
			iSize += pRefs[i].Size; if (pRefs[i].Size) ++iUsed;
		}
		if (iUsed) iExtra = __xrtNetUdpVectorPrefix(pControl != NULL) - (pControl ? sizeof(*pControl) : 0) +
			offsetof(__xrt_net_udp_vector, Spans) + iUsed * (sizeof(xnetspan) + sizeof(xnetbytes*));
		break;
	}
	default: __xrtErrorSetInvalidArgument(); goto Failed;
	}
	if (pControl) {
		union {uint64 Align; unsigned char Data[XRT_NET_SOCKET_DGRAM_CONTROL_SIZE];} ControlBuffer;
		size_t iControlSize;
		if (!__xrtNetSocketDgramControlBuild(pUdp->Socket, pControl, iSize, ControlBuffer.Data,
			sizeof(ControlBuffer.Data), &iControlSize, XNET_ERROR_UDP_SEND, "send-udp-owned")) goto Failed;
	}
	__xrt_net_udp_send* pSend = __xrtNetUdpCreateSend(pUdp, &Remote, pControl, iSize, !iSize, iExtra, pResult);
	if (!pSend) return NULL;
	if (!iSize) { pSend->Data = __xrtNetUdpSendCopy(pSend); return pSend; }
	if (pItem->Kind == XNET_UDP_OWNED_BYTES) {
		xnetbytes* pOwner = xrtNetBytesRef(pItem->Payload.Bytes.Bytes);
		if (!pOwner) { __xrtNetUdpDiscardSend(pSend, false); goto Failed; }
		pSend->PayloadOwnership = XRT_NET_UDP_SEND_BYTES;
		pSend->Release = __xrtNetUdpReleaseBytes; pSend->ReleaseContext = pOwner;
		pSend->Data = View.Data + iOffset; pSend->OwnsExternal = true;
	} else if (pItem->Kind == XNET_UDP_OWNED_PACKET) {
		xnetudppacket* pOwner = xrtNetUdpPacketRef(pItem->Payload.Packet.Packet);
		if (!pOwner) { __xrtNetUdpDiscardSend(pSend, false); goto Failed; }
		pSend->PayloadOwnership = XRT_NET_UDP_SEND_PACKET;
		pSend->Release = __xrtNetUdpReleasePacket; pSend->ReleaseContext = pOwner;
		pSend->Data = View.Data + iOffset; pSend->OwnsExternal = true;
	} else {
		__xrt_net_udp_vector* pVector = __xrtNetUdpSendVector(pSend);
		pVector->Capacity = iUsed; pVector->Count = 0;
		pSend->PayloadOwnership = XRT_NET_UDP_SEND_VECTOR;
		pSend->Release = __xrtNetUdpReleaseVector; pSend->ReleaseContext = pSend;
		pSend->Spans = pVector->Spans; pSend->OwnsExternal = true;
		xnetbytes** ppOwners = __xrtNetUdpVectorOwners(pVector);
		for (size_t i = 0; i < pItem->Payload.Vector.Count; ++i) {
			const xnetbytesref* pRef = &pItem->Payload.Vector.Items[i];
			if (!pRef->Size) continue;
			xnetbytes* pOwner = xrtNetBytesRef(pRef->Bytes);
			if (!pOwner) { __xrtNetUdpDiscardSend(pSend, false); goto Failed; }
			xbytesview Part = xrtNetBytesView(pOwner); size_t iSlot = pVector->Count;
			ppOwners[iSlot] = pOwner;
			pVector->Spans[iSlot].Data = Part.Data + pRef->Offset; pVector->Spans[iSlot].Size = pRef->Size;
			++pVector->Count;
		}
	}
	return pSend;
Failed:
	*pResult = XNET_RESULT_ERROR; return NULL;
}

XRT_API xnetresult xrtNetUdpSendOwnedBatch(xnetudp* pUdp,
	const xnetudpownedmessage* pItems, size_t iCount, size_t* pAccepted)
{
	if (pAccepted) *pAccepted = 0;
	if (!pUdp || !pAccepted || (!pItems && iCount)) { __xrtErrorSetInvalidArgument(); return XNET_RESULT_ERROR; }
	if (!iCount) return XNET_RESULT_OK;
	if (!__xrtNetUdpBeginSend(pUdp)) return XNET_RESULT_CLOSED;
	__xrt_net_udp_send *pHead = NULL, *pTail = NULL; size_t iAccepted = 0; xnetresult Result = XNET_RESULT_OK;
	for (size_t i = 0; i < iCount; ++i) {
		__xrt_net_udp_send* pSend = __xrtNetUdpCreateOwnedSend(pUdp, &pItems[i], &Result);
		if (!pSend) break;
		if (pTail) pTail->Next = pSend; else pHead = pSend;
		pTail = pSend; ++iAccepted;
	}
	if (pHead) {
		xnetresult Submitted = __xrtNetUdpSubmitSend(pUdp, pHead);
		if (Submitted != XNET_RESULT_OK) { Result = Submitted; iAccepted = 0; }
	}
	__xrtNetUdpEndSend(pUdp); *pAccepted = iAccepted; return Result;
}
XRT_API xnetresult xrtNetUdpSendMsgBytes(xnetudp* pUdp, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, xnetbytes* pBytes, size_t iOffset, size_t iSize)
{
	xnetudpownedmessage Item = {0}; Item.Remote = pRemote; Item.Control = pControl; Item.Kind = XNET_UDP_OWNED_BYTES;
	Item.Payload.Bytes = (xnetbytesref){pBytes, iOffset, iSize}; size_t iAccepted;
	return xrtNetUdpSendOwnedBatch(pUdp, &Item, 1, &iAccepted);
}
XRT_API xnetresult xrtNetUdpSendMsgPacket(xnetudp* pUdp, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, xnetudppacket* pPacket, size_t iOffset, size_t iSize)
{
	xnetudpownedmessage Item = {0}; Item.Remote = pRemote; Item.Control = pControl; Item.Kind = XNET_UDP_OWNED_PACKET;
	Item.Payload.Packet = (xnetudppacketref){pPacket, iOffset, iSize}; size_t iAccepted;
	return xrtNetUdpSendOwnedBatch(pUdp, &Item, 1, &iAccepted);
}
XRT_API xnetresult xrtNetUdpSendMsgBytesVec(xnetudp* pUdp, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, const xnetbytesref* pRefs, size_t iCount)
{
	xnetudpownedmessage Item = {0}; Item.Remote = pRemote; Item.Control = pControl; Item.Kind = XNET_UDP_OWNED_VECTOR;
	Item.Payload.Vector.Items = pRefs; Item.Payload.Vector.Count = iCount; size_t iAccepted;
	return xrtNetUdpSendOwnedBatch(pUdp, &Item, 1, &iAccepted);
}

static bool __xrtNetUdpRetainedSpan(xbytesview View, const void* pData, size_t iSize)
{
	uintptr_t iFirst = (uintptr_t)View.Data, iData = (uintptr_t)pData;
	return iSize && iData >= iFirst && iData - iFirst <= View.Size && iSize <= View.Size - (iData - iFirst);
}

/* Contained send records own one actual UDP reference each. Slots only borrow
 * their send record; they do not create a second graph node/reference. */
bool __xrtNetUdpSendsOwnership(const xnetudp* pUdp, xrtownershipvisitor pVisit, ptr pContext)
{
	uint64 iCount = 0, iBytes = 0; size_t iSubmitted = 0;
	const __xrt_net_udp_send* pPrevious = NULL; bool bReadySeen = pUdp->SendReady == NULL;
	uint64 iExpected = xrtAtomic64Load(&pUdp->QueuedPackets, XMEMORY_ACQUIRE);
	const __xrt_net_udp_send *pSlow = pUdp->SendHead, *pFast = pSlow;
	while (pFast && pFast->Next) {
		pSlow = pSlow->Next; pFast = pFast->Next->Next; if (pSlow == pFast) return false;
	}
	for (const __xrt_net_udp_send* p = pUdp->SendHead; p; p = p->Next) {
		if (++iCount > iExpected || p->Udp != pUdp || p->Previous != pPrevious || p->Size > UINT64_MAX - iBytes) return false;
		iBytes += p->Size; pPrevious = p;
		if (p == pUdp->SendReady) bReadySeen = true;
		if (p->PayloadOwnership == XRT_NET_UDP_SEND_COPY) {
			if (!p->Copied || p->OwnsExternal || p->Release || p->ReleaseContext ||
				p->Data != __xrtNetUdpSendCopy((__xrt_net_udp_send*)p)) return false;
		} else if (p->PayloadOwnership == XRT_NET_UDP_SEND_TAKE) {
			if (p->Copied || !p->OwnsExternal || p->Release != __xrtNetUdpFreeTaken || p->ReleaseContext || (!p->Data && p->Size)) return false;
		} else if (p->PayloadOwnership == XRT_NET_UDP_SEND_BYTES) {
			if (p->Copied || !p->OwnsExternal || p->Release != __xrtNetUdpReleaseBytes || !p->ReleaseContext ||
				!__xrtNetUdpRetainedSpan(xrtNetBytesView(p->ReleaseContext), p->Data, p->Size)) return false;
			if (pVisit && !pVisit(xrtNetBytesOwnership(p->ReleaseContext), pContext)) return false;
		} else if (p->PayloadOwnership == XRT_NET_UDP_SEND_PACKET) {
			xnetudppacket* pPacket = p->ReleaseContext;
			if (p->Copied || !p->OwnsExternal || p->Release != __xrtNetUdpReleasePacket || !pPacket ||
				!__xrtNetUdpRetainedSpan((xbytesview){xrtNetUdpPacketData(pPacket), xrtNetUdpPacketSize(pPacket)}, p->Data, p->Size)) return false;
			if (pVisit && !pVisit(xrtNetUdpPacketOwnership(pPacket), pContext)) return false;
		} else if (p->PayloadOwnership == XRT_NET_UDP_SEND_VECTOR) {
			const __xrt_net_udp_vector* pVector = __xrtNetUdpSendVector(p);
			if (p->Copied || !p->OwnsExternal || p->Release != __xrtNetUdpReleaseVector || p->ReleaseContext != p ||
				!pVector->Count || pVector->Count != pVector->Capacity || pVector->Count > XNET_UDP_VECTOR_MAX || p->Spans != pVector->Spans) return false;
			xnetbytes** ppOwners = __xrtNetUdpVectorOwners((__xrt_net_udp_vector*)pVector); size_t iTotal = 0;
			for (size_t j = 0; j < pVector->Count; ++j) {
				if (!ppOwners[j] || !__xrtNetUdpRetainedSpan(xrtNetBytesView(ppOwners[j]), p->Spans[j].Data, p->Spans[j].Size) ||
					p->Spans[j].Size > XNET_UDP_PAYLOAD_MAX - iTotal) return false;
				iTotal += p->Spans[j].Size;
				if (pVisit && !pVisit(xrtNetBytesOwnership(ppOwners[j]), pContext)) return false;
			}
			if (iTotal != p->Size) return false;
		} else return false;
		if (p->Submitted) {
			uintptr_t slot = (uintptr_t)p->Slot, first = (uintptr_t)pUdp->SendSlots;
			if (!p->Slot || slot < first || (slot - first) % sizeof(*p->Slot) ||
				(slot - first) / sizeof(*p->Slot) >= pUdp->SendSlotCount || p->Slot->Send != p || !p->Slot->Id) return false;
			++iSubmitted;
		} else if (p->Slot) return false;
		if (pVisit && !pVisit(xrtNetUdpOwnership(pUdp), pContext)) return false;
	}
	if (iCount != iExpected || iBytes != xrtAtomic64Load(&pUdp->QueuedBytes, XMEMORY_ACQUIRE) ||
		pPrevious != pUdp->SendTail || !bReadySeen || iSubmitted != xrtAtomic64Load(&pUdp->ActiveSends, XMEMORY_ACQUIRE)) return false;
	size_t iSlots = 0;
	for (uint32 i = 0; i < pUdp->SendSlotCount; ++i) {
		const __xrt_net_udp_send_slot* s = &pUdp->SendSlots[i];
		if (s->Udp != pUdp || s->Completion.Data != s || s->Completion.Proc != __xrtNetUdpSendCompletion ||
			(!s->Send && s->Id)) return false;
		if (s->Send) { if (s->Send->Slot != s || !s->Send->Submitted || !s->Id) return false; ++iSlots; }
	}
	return iSlots == iSubmitted;
}

#endif
