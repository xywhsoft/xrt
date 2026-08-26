#include "../internal/xrt_udp.h"



#if defined(__linux__)
	#include <errno.h>
#endif



#if defined(XRT_FEATURE_NET_UDP)

#define XRT_NET_UDP_SEND_BUDGET 64u



static void __xrtNetUdpDriveRead(xnetudp* pUdp);
static bool __xrtNetUdpDriveErrors(xnetudp* pUdp);
static void __xrtNetUdpFail(xnetudp* pUdp, bool bReceive);
static void __xrtNetUdpControlRequest(
	xnetudp* pUdp,
	uint32 iRequest
);



/* 可裁剪地推进统一 UDP Future 适配层。 */
static void __xrtNetUdpNotifyFutures(xnetudp* pUdp)
{
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		__xrtNetUdpFutureNotify(pUdp);
	#else
		(void)pUdp;
	#endif
}



/* 发送门建立后，最后一个提交者或命令负责唤醒挂起的生命周期请求。 */
void __xrtNetUdpWakeLifecycle(xnetudp* pUdp)
{
	if ( xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pUdp->AbortGate, XMEMORY_ACQUIRE) ) {
		__xrtNetUdpControlRequest(
			pUdp,
			XRT_NET_UDP_CONTROL_FINISH
		);
	}
}



/* 设置 UDP 层结构化错误。 */
void __xrtNetUdpSetError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
}



/* 保存第一个导致 UDP 终止的错误。 */
static void __xrtNetUdpRememberError(xnetudp* pUdp)
{
	xerror* pError = xrtTakeError();

	if ( pUdp->Error == NULL ) {
		pUdp->Error = pError;
	} else {
		xrtErrorFree(pError);
	}
}



/* 在 UDP Worker 上发布一个可恢复错误并释放临时错误对象。 */
static void __xrtNetUdpReportError(
	xnetudp* pUdp,
	bool bReceive
)
{
	xerror* pError = xrtTakeError();

	__xrtNetStatBasicAdd(
		bReceive ? &pUdp->ReceiveErrors : &pUdp->SendErrors,
		1
	);
	if ( pUdp->Events.Error != NULL ) {
		pUdp->Events.Error(
			pUdp,
			pError,
			__xrtNetUdpDataCurrent(pUdp)
		);
	}
	xrtErrorFree(pError);
}



/* 将端口终态中的平台错误转换为完整 UDP 错误。 */
static void __xrtNetUdpEventError(
	const xnetportevent* pEvent,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pEvent->SystemCode != 0 ) {
		__xrtNetSocketSetSystemError(
			Code,
			sOperation,
			sMessage,
			pEvent->SystemCode
		);
	} else {
		__xrtNetUdpSetError(XERR_IO, Code, sOperation, sMessage);
	}
}



/* 判断普通接收失败是否应由 Linux 错误队列独占发布。 */
static bool __xrtNetUdpQueuedSystemError(
	const xnetudp* pUdp,
	int iSystemCode
)
{
	#if defined(__linux__)
		if ( pUdp->Errors == NULL ) {
			return false;
		}
		return (iSystemCode == ECONNREFUSED) ||
			(iSystemCode == EMSGSIZE) ||
			(iSystemCode == EHOSTUNREACH) ||
			(iSystemCode == ENETUNREACH) ||
			(iSystemCode == EPROTO) ||
			(iSystemCode == EACCES);
	#else
		(void)pUdp;
		(void)iSystemCode;
		return false;
	#endif
}



/* 增加 UDP 引用。 */
XRT_API xnetudp* xrtNetUdpRef(xnetudp* pUdp)
{
	if ( (pUdp == NULL) ||
		 (xrtRefRetain(&pUdp->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pUdp;
}



/* 销毁一条拥有型数据包链。 */
static void __xrtNetUdpPacketListDestroy(xnetudppacket* pPacket)
{
	while ( pPacket != NULL ) {
		xnetudppacket* pNext = pPacket->Next;

		pPacket->Next = NULL;
		xrtNetUdpPacketDestroy(pPacket);
		pPacket = pNext;
	}
}



/* 销毁一条拥有型数据报错误包链。 */
static void __xrtNetUdpErrorPacketListDestroy(
	xnetudperrorpacket* pPacket
)
{
	while ( pPacket != NULL ) {
		xnetudperrorpacket* pNext = pPacket->Next;

		pPacket->Next = NULL;
		xrtNetUdpErrorPacketDestroy(pPacket);
		pPacket = pNext;
	}
}



/* 释放最后一个 UDP 引用及其 Engine 占用。 */
XRT_API void xrtNetUdpDestroy(xnetudp* pUdp)
{
	xnetengine* pEngine;
	bool bEngineHeld;

	if ( pUdp == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pUdp->References) != 0 ) {
		return;
	}
	pEngine = pUdp->Engine;
	bEngineHeld = pUdp->EngineHeld;
	__xrtNetUdpPacketListDestroy(pUdp->ReceiveHead);
	__xrtNetUdpErrorPacketListDestroy(
		pUdp->Errors != NULL ? pUdp->Errors->Head : NULL
	);
	if ( pUdp->ReceiveLockReady ) {
		__xrtSpinUnit(&pUdp->ReceiveLock);
	}
	xrtErrorFree(pUdp->Error);
	xrtFree(pUdp);
	if ( bEngineHeld ) {
		__xrtNetEngineObjectRelease(pEngine);
	}
}



/* 建立一个拥有连续载荷的数据包。 */
static xnetudppacket* __xrtNetUdpPacketCreate(
	const xnetaddr* pRemote,
	const xnetdgrammeta* pMeta,
	const void* pData,
	size_t iSize,
	uint32 iFlags
)
{
	xnetudppacket* pPacket;
	size_t iAllocation;

	if ( iSize > (SIZE_MAX - offsetof(xnetudppacket, Data)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocation = offsetof(xnetudppacket, Data) + iSize;
	pPacket = (xnetudppacket*)xrtMalloc(iAllocation);
	if ( pPacket == NULL ) {
		return NULL;
	}
	memset(pPacket, 0, offsetof(xnetudppacket, Data));
	pPacket->References = 1;
	pPacket->Remote = *pRemote;
	if ( pMeta != NULL ) {
		pPacket->Meta = *pMeta;
	}
	pPacket->Size = iSize;
	pPacket->Flags = iFlags;
	if ( iSize != 0 ) {
		memcpy(pPacket->Data, pData, iSize);
	}
	return pPacket;
}



/* 销毁拥有型 UDP 数据包。 */
XRT_API void xrtNetUdpPacketDestroy(xnetudppacket* pPacket)
{
	if ( (pPacket != NULL) &&
		 (xrtRefRelease(&pPacket->References) == 0) ) {
		xrtFree(pPacket);
	}
}



/* 增加拥有型 UDP 数据包引用。 */
XRT_API xnetudppacket* xrtNetUdpPacketRef(xnetudppacket* pPacket)
{
	if ( (pPacket == NULL) ||
		 (xrtRefRetain(&pPacket->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pPacket;
}



/* 返回数据包远端地址。 */
XRT_API const xnetaddr* xrtNetUdpPacketRemote(
	const xnetudppacket* pPacket
)
{
	return pPacket != NULL ? &pPacket->Remote : NULL;
}



/* 返回数据包接收元数据。 */
XRT_API const xnetdgrammeta* xrtNetUdpPacketMeta(
	const xnetudppacket* pPacket
)
{
	return pPacket != NULL ? &pPacket->Meta : NULL;
}



/* 返回数据包载荷。 */
XRT_API cbytes xrtNetUdpPacketData(const xnetudppacket* pPacket)
{
	return pPacket != NULL ? pPacket->Data : NULL;
}



/* 返回数据包载荷长度。 */
XRT_API size_t xrtNetUdpPacketSize(const xnetudppacket* pPacket)
{
	return pPacket != NULL ? pPacket->Size : 0;
}



/* 返回数据包截断标记。 */
XRT_API bool xrtNetUdpPacketTruncated(const xnetudppacket* pPacket)
{
	return (pPacket != NULL) &&
		((pPacket->Flags & XNET_UDP_MESSAGE_TRUNCATED) != 0);
}



/* 建立一个拥有结构化错误和连续负载前缀的错误包。 */
static xnetudperrorpacket* __xrtNetUdpErrorPacketCreate(
	const xnetdgramerror* pError,
	const void* pData,
	size_t iSize
)
{
	xnetudperrorpacket* pPacket;
	size_t iAllocation;

	if ( iSize > (SIZE_MAX - offsetof(xnetudperrorpacket, Data)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocation = offsetof(xnetudperrorpacket, Data) + iSize;
	pPacket = (xnetudperrorpacket*)xrtMalloc(iAllocation);
	if ( pPacket == NULL ) {
		return NULL;
	}
	memset(pPacket, 0, offsetof(xnetudperrorpacket, Data));
	pPacket->References = 1;
	pPacket->Error = *pError;
	pPacket->Size = iSize;
	if ( iSize != 0 ) {
		memcpy(pPacket->Data, pData, iSize);
	}
	return pPacket;
}



/* 销毁拥有型数据报错误包。 */
XRT_API void xrtNetUdpErrorPacketDestroy(xnetudperrorpacket* pPacket)
{
	if ( (pPacket != NULL) &&
		 (xrtRefRelease(&pPacket->References) == 0) ) {
		xrtFree(pPacket);
	}
}



/* 增加拥有型数据报错误包引用。 */
XRT_API xnetudperrorpacket* xrtNetUdpErrorPacketRef(
	xnetudperrorpacket* pPacket
)
{
	if ( (pPacket == NULL) ||
		 (xrtRefRetain(&pPacket->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pPacket;
}



/* 返回错误包中的结构化数据报错误。 */
XRT_API const xnetdgramerror* xrtNetUdpErrorPacketInfo(
	const xnetudperrorpacket* pPacket
)
{
	return pPacket != NULL ? &pPacket->Error : NULL;
}



/* 返回错误包中的原数据报负载前缀。 */
XRT_API cbytes xrtNetUdpErrorPacketData(
	const xnetudperrorpacket* pPacket
)
{
	return pPacket != NULL ? pPacket->Data : NULL;
}



/* 返回错误包中的原数据报负载前缀长度。 */
XRT_API size_t xrtNetUdpErrorPacketSize(
	const xnetudperrorpacket* pPacket
)
{
	return pPacket != NULL ? pPacket->Size : 0;
}



/* 判断一个数据包能否同时进入接收包数和字节硬上限。 */
static bool __xrtNetUdpReceiveFits(
	const xnetudp* pUdp,
	const xnetudppacket* pPacket,
	uint64 iQueued,
	uint64 iBytes
)
{
	uint64 iLimit = (uint64)pUdp->Config.ReceiveQueueByteLimit;

	return (iQueued < (uint64)pUdp->Config.ReceiveQueueLimit) &&
		(iBytes <= iLimit) &&
		((uint64)pPacket->Size <= (iLimit - iBytes));
}



/* 将一个拥有型数据包放入有界拉取队列。 */
static bool __xrtNetUdpQueueReceive(
	xnetudp* pUdp,
	xnetudppacket* pPacket
)
{
	xnetudppacket* pDroppedHead = NULL;
	xnetudppacket* pDroppedTail = NULL;
	uint64 iQueued;
	uint64 iBytes;
	uint64 iDropped = 0;
	bool bAccepted = false;
	bool bWaiting = false;
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		__xrt_net_udp_wait* pFinished = NULL;
	#endif

	__xrtSpinLock(&pUdp->ReceiveLock);
	iQueued = xrtAtomic64Load(&pUdp->ReceiveQueued, XMEMORY_RELAXED);
	iBytes = xrtAtomic64Load(
		&pUdp->ReceiveQueuedBytes,
		XMEMORY_RELAXED
	);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		bWaiting = (xrtAtomic64Load(
		&pUdp->ReceiveWaiters,
		XMEMORY_RELAXED
	) != 0) && !xrtAtomic32Load(
		&pUdp->CloseGate,
		XMEMORY_ACQUIRE
	);
	#endif
	if ( !bWaiting && !__xrtNetUdpReceiveFits(
		pUdp,
		pPacket,
		iQueued,
		iBytes
	) && (pUdp->Config.Overflow == XNET_UDP_DROP_OLDEST) &&
		 (pUdp->Config.ReceiveQueueLimit != 0) &&
		 (pPacket->Size <= pUdp->Config.ReceiveQueueByteLimit) ) {
		while ( (pUdp->ReceiveHead != NULL) &&
			 !__xrtNetUdpReceiveFits(
				pUdp,
				pPacket,
				iQueued,
				iBytes
			 ) ) {
			xnetudppacket* pDropped = pUdp->ReceiveHead;

			pUdp->ReceiveHead = pDropped->Next;
			pDropped->Next = NULL;
			if ( pDroppedTail != NULL ) {
				pDroppedTail->Next = pDropped;
			} else {
				pDroppedHead = pDropped;
			}
			pDroppedTail = pDropped;
			iQueued--;
			iBytes -= (uint64)pDropped->Size;
			iDropped++;
		}
		if ( pUdp->ReceiveHead == NULL ) {
			pUdp->ReceiveTail = NULL;
		}
	}
	bAccepted = bWaiting || __xrtNetUdpReceiveFits(
		pUdp,
		pPacket,
		iQueued,
		iBytes
	);
	if ( bAccepted ) {
		pPacket->Next = NULL;
		if ( pUdp->ReceiveTail != NULL ) {
			pUdp->ReceiveTail->Next = pPacket;
		} else {
			pUdp->ReceiveHead = pPacket;
		}
		pUdp->ReceiveTail = pPacket;
		iQueued++;
		iBytes += (uint64)pPacket->Size;
		xrtAtomic64Store(
			&pUdp->ReceiveQueued,
			iQueued,
			XMEMORY_RELEASE
		);
		xrtAtomic64Store(
			&pUdp->ReceiveQueuedBytes,
			iBytes,
			XMEMORY_RELEASE
		);
		#if defined(XRT_FEATURE_NET_UDP_FUTURE)
			pFinished = __xrtNetUdpFuturePairReceiveLocked(pUdp);
			iQueued = xrtAtomic64Load(
				&pUdp->ReceiveQueued,
				XMEMORY_RELAXED
			);
			iBytes = xrtAtomic64Load(
				&pUdp->ReceiveQueuedBytes,
				XMEMORY_RELAXED
			);
		#endif
		__xrtNetUdpPeak(&pUdp->PeakReceiveQueued, iQueued);
		__xrtNetUdpPeak(&pUdp->PeakReceiveQueuedBytes, iBytes);
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		__xrtNetUdpFutureFinishList(pFinished);
	#endif

	if ( pDroppedHead != NULL ) {
		__xrtNetStatBasicAdd(&pUdp->DroppedOldest, iDropped);
		__xrtNetUdpPacketListDestroy(pDroppedHead);
	} else if ( !bAccepted ) {
		__xrtNetStatBasicAdd(&pUdp->DroppedNewest, 1);
	}
	__xrtNetUdpNotifyFutures(pUdp);
	return bAccepted;
}



/* 判断一个错误包能否同时进入错误条目和字节硬上限。 */
static bool __xrtNetUdpErrorFits(
	const xnetudp* pUdp,
	const xnetudperrorpacket* pPacket,
	uint64 iQueued,
	uint64 iBytes
)
{
	const __xrt_net_udp_error_state* pState = pUdp->Errors;
	uint64 iLimit = (uint64)pState->QueueByteLimit;

	return (iQueued < (uint64)pState->QueueLimit) &&
		(iBytes <= iLimit) &&
		((uint64)pPacket->Size <= (iLimit - iBytes));
}



/* 将一个拥有型错误包放入有界拉取队列。 */
static bool __xrtNetUdpQueueError(
	xnetudp* pUdp,
	xnetudperrorpacket* pPacket
)
{
	__xrt_net_udp_error_state* pState = pUdp->Errors;
	xnetudperrorpacket* pDroppedHead = NULL;
	xnetudperrorpacket* pDroppedTail = NULL;
	uint64 iQueued;
	uint64 iBytes;
	uint64 iDropped = 0;
	bool bAccepted;
	bool bWaiting = false;
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		__xrt_net_udp_wait* pFinished = NULL;
	#endif

	__xrtSpinLock(&pUdp->ReceiveLock);
	iQueued = xrtAtomic64Load(&pState->Queued, XMEMORY_RELAXED);
		iBytes = xrtAtomic64Load(
		&pState->QueuedBytes,
		XMEMORY_RELAXED
	);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		bWaiting = (xrtAtomic64Load(
			&pState->ErrorWaiters,
			XMEMORY_RELAXED
		) != 0) && !xrtAtomic32Load(
			&pUdp->CloseGate,
			XMEMORY_ACQUIRE
		);
	#endif
	if ( !bWaiting && !__xrtNetUdpErrorFits(
		pUdp,
		pPacket,
		iQueued,
		iBytes
	) && (pState->Overflow == XNET_UDP_DROP_OLDEST) &&
		 (pState->QueueLimit != 0) &&
		 (pPacket->Size <= pState->QueueByteLimit) ) {
		while ( (pState->Head != NULL) &&
			 !__xrtNetUdpErrorFits(
				pUdp,
				pPacket,
				iQueued,
				iBytes
			 ) ) {
			xnetudperrorpacket* pDropped = pState->Head;

			pState->Head = pDropped->Next;
			pDropped->Next = NULL;
			if ( pDroppedTail != NULL ) {
				pDroppedTail->Next = pDropped;
			} else {
				pDroppedHead = pDropped;
			}
			pDroppedTail = pDropped;
			iQueued--;
			iBytes -= (uint64)pDropped->Size;
			iDropped++;
		}
		if ( pState->Head == NULL ) {
			pState->Tail = NULL;
		}
	}
	bAccepted = bWaiting || __xrtNetUdpErrorFits(
		pUdp,
		pPacket,
		iQueued,
		iBytes
	);
	if ( bAccepted ) {
		pPacket->Next = NULL;
		if ( pState->Tail != NULL ) {
			pState->Tail->Next = pPacket;
		} else {
			pState->Head = pPacket;
		}
		pState->Tail = pPacket;
		iQueued++;
		iBytes += (uint64)pPacket->Size;
		xrtAtomic64Store(
			&pState->Queued,
			iQueued,
			XMEMORY_RELEASE
		);
		xrtAtomic64Store(
			&pState->QueuedBytes,
			iBytes,
			XMEMORY_RELEASE
		);
		#if defined(XRT_FEATURE_NET_UDP_FUTURE)
			pFinished = __xrtNetUdpFuturePairErrorLocked(pUdp);
			iQueued = xrtAtomic64Load(
				&pState->Queued,
				XMEMORY_RELAXED
			);
			iBytes = xrtAtomic64Load(
				&pState->QueuedBytes,
				XMEMORY_RELAXED
			);
		#endif
		__xrtNetUdpPeak(&pState->PeakQueued, iQueued);
		__xrtNetUdpPeak(&pState->PeakQueuedBytes, iBytes);
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		__xrtNetUdpFutureFinishList(pFinished);
	#endif

	if ( pDroppedHead != NULL ) {
		__xrtNetStatBasicAdd(&pState->Dropped, iDropped);
		__xrtNetUdpErrorPacketListDestroy(pDroppedHead);
	} else if ( !bAccepted ) {
		__xrtNetStatBasicAdd(&pState->Dropped, 1);
	}
	__xrtNetUdpNotifyFutures(pUdp);
	return bAccepted;
}



/* 发布一个异步 ICMP、PMTU 或本地数据报错误。 */
static void __xrtNetUdpPublishError(
	xnetudp* pUdp,
	const xnetdgramerror* pError,
	const void* pData,
	size_t iSize
)
{
	__xrt_net_udp_error_state* pState = pUdp->Errors;

	__xrtNetStatFullAdd(&pState->Received, 1);
	if ( (pError->Flags & XNET_DGRAM_ERROR_PATH_MTU) != 0 ) {
		xrtAtomic64Store(
			&pState->PathMtu,
			(uint64)pError->PathMtu,
			XMEMORY_RELEASE
		);
		__xrtNetStatBasicAdd(&pState->PathMtuUpdates, 1);
	}

	if ( pState->Callback != NULL ) {
		xnetudperrormessage Message;

		Message.Error = *pError;
		Message.Data = (cbytes)pData;
		Message.Size = iSize;
		pState->Callback(
			pUdp,
			&Message,
			__xrtNetUdpDataCurrent(pUdp)
		);
	} else {
		xnetudperrorpacket* pPacket = __xrtNetUdpErrorPacketCreate(
			pError,
			pData,
			iSize
		);

		if ( pPacket == NULL ) {
			__xrtNetStatBasicAdd(&pState->Dropped, 1);
			__xrtNetUdpReportError(pUdp, true);
			return;
		}
		if ( !__xrtNetUdpQueueError(pUdp, pPacket) ) {
			xrtNetUdpErrorPacketDestroy(pPacket);
			if ( pState->Overflow == XNET_UDP_DROP_ERROR ) {
				__xrtNetUdpSetError(
					XERR_AGAIN,
					XNET_ERROR_UDP_RECEIVE_QUEUE,
					"queue-udp-error",
					"UDP datagram error queue limit reached"
				);
				__xrtNetUdpReportError(pUdp, true);
			}
		}
	}
}



/* 发布一个完整或被截断的数据报。 */
static void __xrtNetUdpPublish(
	xnetudp* pUdp,
	const xnetaddr* pRemote,
	const xnetdgrammeta* pMeta,
	const void* pData,
	size_t iSize,
	bool bTruncated
)
{
	uint32 iFlags = bTruncated ? XNET_UDP_MESSAGE_TRUNCATED : 0;

	__xrtNetStatFullAdd(&pUdp->ReceivedPackets, 1);
	__xrtNetStatFullAdd(&pUdp->ReceivedBytes, (uint64)iSize);
	if ( bTruncated ) {
		__xrtNetStatBasicAdd(&pUdp->Truncated, 1);
		if ( pUdp->Config.Truncation != XNET_UDP_TRUNCATE_DELIVER ) {
			__xrtNetStatBasicAdd(&pUdp->TruncatedDropped, 1);
			if ( pUdp->Config.Truncation == XNET_UDP_TRUNCATE_ERROR ) {
				__xrtNetUdpSetError(
					XERR_RANGE,
					XNET_ERROR_UDP_RECEIVE,
					"receive-udp",
					"UDP datagram exceeded the configured receive size"
				);
				__xrtNetUdpReportError(pUdp, true);
			}
			return;
		}
	}

	if ( pUdp->Events.Receive != NULL ) {
		xnetudpmessage Message;

		Message.Remote = *pRemote;
		memset(&Message.Meta, 0, sizeof(Message.Meta));
		if ( pMeta != NULL ) {
			Message.Meta = *pMeta;
		}
		Message.Data = (cbytes)pData;
		Message.Size = iSize;
		Message.Flags = iFlags;
		pUdp->Events.Receive(
			pUdp,
			&Message,
			__xrtNetUdpDataCurrent(pUdp)
		);
	} else {
		xnetudppacket* pPacket = __xrtNetUdpPacketCreate(
			pRemote,
			pMeta,
			pData,
			iSize,
			iFlags
		);

		if ( pPacket == NULL ) {
			__xrtNetStatBasicAdd(&pUdp->DroppedNewest, 1);
			__xrtNetUdpReportError(pUdp, true);
			return;
		}
		if ( !__xrtNetUdpQueueReceive(pUdp, pPacket) ) {
			xrtNetUdpPacketDestroy(pPacket);
			if ( pUdp->Config.Overflow == XNET_UDP_DROP_ERROR ) {
				__xrtNetUdpSetError(
					XERR_AGAIN,
					XNET_ERROR_UDP_RECEIVE_QUEUE,
					"queue-udp-receive",
					"UDP receive queue limit reached"
				);
				__xrtNetUdpReportError(pUdp, true);
			}
		}
	}
}



/* 提交接收缓冲，并把合法零长度数据报表示为空 Span。 */
static bool __xrtNetUdpReceiveCommit(
	xnetbuf* pBuffer,
	size_t iSize,
	xnetspan* pSpan
)
{
	if ( !xrtNetBufCommit(pBuffer, iSize) ) {
		return false;
	}
	if ( iSize == 0 ) {
		pSpan->Data = NULL;
		pSpan->Size = 0;
		return true;
	}
	return xrtNetBufFront(pBuffer, pSpan);
}



/* 非阻塞取出一个拉取数据包。 */
XRT_API xnetudppacket* xrtNetUdpReceive(xnetudp* pUdp)
{
	xnetudppacket* pPacket;
	bool bAsyncReceive = false;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pUdp->Events.Receive != NULL) || !pUdp->ReceiveLockReady ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-packet",
			"UDP is configured for push receive callbacks"
		);
		return NULL;
	}
	__xrtSpinLock(&pUdp->ReceiveLock);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		bAsyncReceive = xrtAtomic64Load(
			&pUdp->ReceiveWaiters,
			XMEMORY_RELAXED
		) != 0;
	#endif
	if ( bAsyncReceive ) {
		__xrtSpinUnlock(&pUdp->ReceiveLock);
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-packet",
			"direct UDP receive cannot race an asynchronous receive waiter"
		);
		return NULL;
	}
	pPacket = pUdp->ReceiveHead;
	if ( pPacket != NULL ) {
		pUdp->ReceiveHead = pPacket->Next;
		if ( pUdp->ReceiveHead == NULL ) {
			pUdp->ReceiveTail = NULL;
		}
		pPacket->Next = NULL;
		(void)xrtAtomic64FetchSub(
			&pUdp->ReceiveQueued,
			1,
			XMEMORY_ACQ_REL
		);
		(void)xrtAtomic64FetchSub(
			&pUdp->ReceiveQueuedBytes,
			(uint64)pPacket->Size,
			XMEMORY_ACQ_REL
		);
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	return pPacket;
}



/* 在一次锁内取出多个拉取数据包。 */
XRT_API size_t xrtNetUdpReceiveBatch(
	xnetudp* pUdp,
	xnetudppacket** pPackets,
	size_t iCapacity
)
{
	size_t iCount = 0;
	uint64 iBytes = 0;
	bool bAsyncReceive = false;

	if ( (pUdp == NULL) ||
		 ((pPackets == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( (pUdp->Events.Receive != NULL) || !pUdp->ReceiveLockReady ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-batch",
			"UDP is configured for push receive callbacks"
		);
		return 0;
	}
	__xrtSpinLock(&pUdp->ReceiveLock);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		bAsyncReceive = xrtAtomic64Load(
			&pUdp->ReceiveWaiters,
			XMEMORY_RELAXED
		) != 0;
	#endif
	if ( bAsyncReceive ) {
		__xrtSpinUnlock(&pUdp->ReceiveLock);
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-batch",
			"direct UDP receive cannot race an asynchronous receive waiter"
		);
		return 0;
	}
	while ( (iCount < iCapacity) && (pUdp->ReceiveHead != NULL) ) {
		xnetudppacket* pPacket = pUdp->ReceiveHead;

		pUdp->ReceiveHead = pPacket->Next;
		pPacket->Next = NULL;
		pPackets[iCount++] = pPacket;
		iBytes += (uint64)pPacket->Size;
	}
	if ( pUdp->ReceiveHead == NULL ) {
		pUdp->ReceiveTail = NULL;
	}
	if ( iCount != 0 ) {
		(void)xrtAtomic64FetchSub(
			&pUdp->ReceiveQueued,
			(uint64)iCount,
			XMEMORY_ACQ_REL
		);
		(void)xrtAtomic64FetchSub(
			&pUdp->ReceiveQueuedBytes,
			iBytes,
			XMEMORY_ACQ_REL
		);
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	return iCount;
}



/* 调用方持有 ReceiveLock 时取走一个错误包并同步队列统计。 */
xnetudperrorpacket* __xrtNetUdpTakeErrorLocked(xnetudp* pUdp)
{
	__xrt_net_udp_error_state* pState = pUdp->Errors;
	xnetudperrorpacket* pPacket = pState->Head;

	if ( pPacket == NULL ) {
		return NULL;
	}
	pState->Head = pPacket->Next;
	if ( pState->Head == NULL ) {
		pState->Tail = NULL;
	}
	pPacket->Next = NULL;
	(void)xrtAtomic64FetchSub(
		&pState->Queued,
		1,
		XMEMORY_ACQ_REL
	);
	(void)xrtAtomic64FetchSub(
		&pState->QueuedBytes,
		(uint64)pPacket->Size,
		XMEMORY_ACQ_REL
	);
	return pPacket;
}



/* 非阻塞取出一个拥有型数据报错误包。 */
XRT_API xnetudperrorpacket* xrtNetUdpReceiveError(xnetudp* pUdp)
{
	xnetudperrorpacket* pPacket;
	bool bAsyncReceive = false;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pUdp->Errors == NULL) ||
		 (pUdp->Errors->Callback != NULL) ||
		 !pUdp->ReceiveLockReady ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-error",
			"UDP is not configured for pulled datagram errors"
		);
		return NULL;
	}
	__xrtSpinLock(&pUdp->ReceiveLock);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		bAsyncReceive = xrtAtomic64Load(
			&pUdp->Errors->ErrorWaiters,
			XMEMORY_RELAXED
		) != 0;
	#endif
	if ( bAsyncReceive ) {
		__xrtSpinUnlock(&pUdp->ReceiveLock);
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-error",
			"direct UDP error receive cannot race an asynchronous waiter"
		);
		return NULL;
	}
	pPacket = __xrtNetUdpTakeErrorLocked(pUdp);
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	return pPacket;
}



/* 在一次锁内取出多个拥有型数据报错误包。 */
XRT_API size_t xrtNetUdpReceiveErrorBatch(
	xnetudp* pUdp,
	xnetudperrorpacket** pPackets,
	size_t iCapacity
)
{
	size_t iCount = 0;
	bool bAsyncReceive = false;

	if ( (pUdp == NULL) ||
		 ((pPackets == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( (pUdp->Errors == NULL) ||
		 (pUdp->Errors->Callback != NULL) ||
		 !pUdp->ReceiveLockReady ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-error-batch",
			"UDP is not configured for pulled datagram errors"
		);
		return 0;
	}
	__xrtSpinLock(&pUdp->ReceiveLock);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		bAsyncReceive = xrtAtomic64Load(
			&pUdp->Errors->ErrorWaiters,
			XMEMORY_RELAXED
		) != 0;
	#endif
	if ( bAsyncReceive ) {
		__xrtSpinUnlock(&pUdp->ReceiveLock);
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"receive-udp-error-batch",
			"direct UDP error receive cannot race an asynchronous waiter"
		);
		return 0;
	}
	while ( iCount < iCapacity ) {
		xnetudperrorpacket* pPacket = __xrtNetUdpTakeErrorLocked(pUdp);

		if ( pPacket == NULL ) {
			break;
		}
		pPackets[iCount++] = pPacket;
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	return iCount;
}






/* 判断 UDP 所属端口是否提供完成式 IO。 */
static bool __xrtNetUdpCompletionPort(const xnetudp* pUdp)
{
	return pUdp->CompletionPort;
}



/* 按当前收发需求重置 readiness one-shot 观察。 */
static bool __xrtNetUdpWatch(xnetudp* pUdp)
{
	xnetudpstate State = xrtNetUdpState(pUdp);
	uint32 iEvents = 0;
	uint64 Id;

	if ( __xrtNetUdpCompletionPort(pUdp) ) {
		return true;
	}
	if ( ((State == XNET_UDP_OPENING) || (State == XNET_UDP_OPEN)) &&
		 !xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ) {
		iEvents |= XNET_POLL_READ;
	}
	if ( (pUdp->SendReady != NULL) &&
		 ((State == XNET_UDP_OPEN) ||
		  ((State == XNET_UDP_CLOSING) && !pUdp->AbortRequested)) ) {
		iEvents |= XNET_POLL_WRITE;
	}
	if ( iEvents == 0 ) {
		if ( pUdp->WatchPending ) {
			if ( !xrtNetPortUnwatch(
				xrtNetWorkerPort(pUdp->Worker),
				pUdp->Socket
			) ) {
				pUdp->WatchPending = false;
				pUdp->WatchEvents = 0;
				return false;
			}
			pUdp->WatchPending = false;
			pUdp->WatchEvents = 0;
		}
		return true;
	}
	Id = xrtNetWorkerOperationId(pUdp->Worker);
	if ( (Id == 0) || !xrtNetPortWatch(
		xrtNetWorkerPort(pUdp->Worker),
		pUdp->Socket,
		Id,
		iEvents,
		&pUdp->Completion
	) ) {
		return false;
	}
	pUdp->WatchId = Id;
	pUdp->WatchEvents = iEvents;
	pUdp->WatchPending = true;
	return true;
}



/* 取消全部接收以及异常关闭时的在途发送。 */
static void __xrtNetUdpCancelOperations(xnetudp* pUdp)
{
	xnetport* pPort = xrtNetWorkerPort(pUdp->Worker);

	if ( pUdp->WatchPending ) {
		if ( !xrtNetPortUnwatch(pPort, pUdp->Socket) ) {
			xrtClearError();
		}
		pUdp->WatchPending = false;
		pUdp->WatchEvents = 0;
	}
	for ( uint32 i = 0; i < pUdp->ReceiveSlots; i++ ) {
		if ( pUdp->Receives[i].Pending &&
			 !xrtNetPortCancel(pPort, pUdp->Receives[i].Id) ) {
			xrtClearError();
		}
	}
	if ( (pUdp->Errors != NULL) &&
		 pUdp->Errors->Receive.Pending &&
		 !xrtNetPortCancel(pPort, pUdp->Errors->Receive.Id) ) {
		xrtClearError();
	}
	if ( pUdp->AbortRequested ) {
		for ( uint32 i = 0; i < pUdp->SendSlotCount; i++ ) {
			__xrt_net_udp_send_slot* pSlot = &pUdp->SendSlots[i];

			if ( (pSlot->Send != NULL) &&
				 !xrtNetPortCancel(pPort, pSlot->Id) ) {
				xrtClearError();
			}
		}
	}
}



/* 从 Worker 发送队列摘除一个指定数据报。 */
static void __xrtNetUdpRemoveSend(
	xnetudp* pUdp,
	__xrt_net_udp_send* pSend
)
{
	if ( pSend->Previous != NULL ) {
		pSend->Previous->Next = pSend->Next;
	} else {
		pUdp->SendHead = pSend->Next;
	}
	if ( pSend->Next != NULL ) {
		pSend->Next->Previous = pSend->Previous;
	} else {
		pUdp->SendTail = pSend->Previous;
	}
	if ( pUdp->SendReady == pSend ) {
		pUdp->SendReady = pSend->Next;
	}
	pSend->Next = NULL;
	pSend->Previous = NULL;
}



/* 丢弃已经挂入 Worker 且没有在途系统 IO 的数据报。 */
static void __xrtNetUdpDiscardAttached(xnetudp* pUdp)
{
	__xrt_net_udp_send* pSend = pUdp->SendHead;

	while ( pSend != NULL ) {
		__xrt_net_udp_send* pNext = pSend->Next;

		if ( !pSend->Submitted ) {
			__xrtNetUdpRemoveSend(pUdp, pSend);
			__xrtNetUdpDiscardSend(pSend, true);
		}
		pSend = pNext;
	}
}



/* 在所有 IO 缓冲和已受理发送终结后发布唯一 Close。 */
void __xrtNetUdpTryFinish(xnetudp* pUdp)
{
	if ( (xrtNetUdpState(pUdp) == XNET_UDP_CLOSED) ||
		 !pUdp->CloseRequested ||
		 (xrtAtomic32Load(
			&pUdp->SendSubmitters,
			XMEMORY_ACQUIRE
		 ) != 0) ||
		 (xrtAtomic32Load(
			&pUdp->SendCommands,
			XMEMORY_ACQUIRE
		 ) != 0) ||
		 (xrtAtomic32Load(
			&pUdp->ActiveReceives,
			XMEMORY_ACQUIRE
		 ) != 0) ||
		 (xrtAtomic64Load(
			&pUdp->ActiveSends,
			XMEMORY_ACQUIRE
		 ) != 0) ) {
		return;
	}
	if ( pUdp->AbortRequested ) {
		__xrtNetUdpDiscardAttached(pUdp);
	} else if ( pUdp->SendHead != NULL ) {
		__xrtNetUdpDriveWrite(pUdp);
		return;
	}
	if ( xrtAtomic64Load(
		&pUdp->QueuedPackets,
		XMEMORY_ACQUIRE
	) != 0 ) {
		return;
	}
	if ( pUdp->Socket != NULL ) {
		(void)xrtNetSocketClose(pUdp->Socket);
		pUdp->Socket = NULL;
	}
	for ( uint32 i = 0; i < pUdp->ReceiveSlots; i++ ) {
		if ( pUdp->Receives[i].Ready ) {
			xrtNetBufClear(&pUdp->Receives[i].Buffer);
			pUdp->Receives[i].Ready = false;
		}
	}
	if ( (pUdp->Errors != NULL) && pUdp->Errors->Receive.Ready ) {
		xrtNetBufClear(&pUdp->Errors->Receive.Buffer);
		pUdp->Errors->Receive.Ready = false;
	}
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		if ( pUdp->ReceiveLockReady ) {
			__xrtSpinLock(&pUdp->ReceiveLock);
			pUdp->WaitClosed = true;
			__xrtSpinUnlock(&pUdp->ReceiveLock);
		}
	#endif
	/* CLOSED 发布后必须允许外部立即销毁 Engine。 */
	if ( pUdp->EngineHeld ) {
		pUdp->EngineHeld = false;
		__xrtNetEngineObjectRelease(pUdp->Engine);
	}
	xrtAtomic32Store(
		&pUdp->State,
		XNET_UDP_CLOSED,
		XMEMORY_RELEASE
	);
	if ( pUdp->Events.Close != NULL ) {
		pUdp->Events.Close(
			pUdp,
			pUdp->CloseResult,
			pUdp->Error,
			__xrtNetUdpDataCurrent(pUdp)
		);
	}
	__xrtNetUdpNotifyFutures(pUdp);
	if ( pUdp->RuntimeHeld ) {
		pUdp->RuntimeHeld = false;
		xrtNetUdpDestroy(pUdp);
	}
}



/* 将不可恢复的驱动错误升级为异常关闭。 */
static void __xrtNetUdpFail(xnetudp* pUdp, bool bReceive)
{
	if ( xrtNetUdpState(pUdp) == XNET_UDP_CLOSED ) {
		return;
	}
	__xrtNetStatBasicAdd(
		bReceive ? &pUdp->ReceiveErrors : &pUdp->SendErrors,
		1
	);
	__xrtNetUdpRememberError(pUdp);
	pUdp->CloseResult = XNET_RESULT_ERROR;
	pUdp->CloseRequested = true;
	pUdp->AbortRequested = true;
	xrtAtomic32Store(&pUdp->SendGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pUdp->CloseGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pUdp->AbortGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(
		&pUdp->State,
		XNET_UDP_CLOSING,
		XMEMORY_RELEASE
	);
	__xrtNetUdpCancelOperations(pUdp);
	__xrtNetUdpDiscardAttached(pUdp);
	__xrtNetUdpTryFinish(pUdp);
}



/* 提交一个 completion 接收槽。 */
static bool __xrtNetUdpArmReceive(__xrt_net_udp_receive* pReceive)
{
	xnetudp* pUdp = pReceive->Udp;
	xnetwspan Span;

	if ( !xrtNetBufReserve(
		&pReceive->Buffer,
		pUdp->Config.ReceiveSize,
		&Span
	) ) {
		return false;
	}
	if ( Span.Size > pUdp->Config.ReceiveSize ) {
		Span.Size = pUdp->Config.ReceiveSize;
	}
	pReceive->Id = xrtNetWorkerOperationId(pUdp->Worker);
	if ( pReceive->Id == 0 ) {
		(void)xrtNetBufCancel(&pReceive->Buffer);
		return false;
	}
	if ( pUdp->Config.ReceiveMeta != 0 ) {
		if ( !xrtNetPortRecvMsg(
			xrtNetWorkerPort(pUdp->Worker),
			pUdp->Socket,
			Span.Data,
			Span.Size,
			pReceive->Id,
			&pReceive->Completion
		) ) {
			(void)xrtNetBufCancel(&pReceive->Buffer);
			return false;
		}
	} else if ( !xrtNetPortRecvFrom(
		xrtNetWorkerPort(pUdp->Worker),
		pUdp->Socket,
		Span.Data,
		Span.Size,
		pReceive->Id,
		&pReceive->Completion
	) ) {
		(void)xrtNetBufCancel(&pReceive->Buffer);
		return false;
	}
	pReceive->Pending = true;
	(void)xrtAtomic32FetchAdd(
		&pUdp->ActiveReceives,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 提交一个 completion 数据报错误接收槽。 */
static bool __xrtNetUdpArmError(
	__xrt_net_udp_error_receive* pReceive
)
{
	xnetudp* pUdp = pReceive->Udp;
	xnetwspan Span;

	if ( !xrtNetBufReserve(
		&pReceive->Buffer,
		pUdp->Errors->Size,
		&Span
	) ) {
		return false;
	}
	if ( Span.Size > pUdp->Errors->Size ) {
		Span.Size = pUdp->Errors->Size;
	}
	pReceive->Id = xrtNetWorkerOperationId(pUdp->Worker);
	if ( (pReceive->Id == 0) || !xrtNetPortRecvError(
		xrtNetWorkerPort(pUdp->Worker),
		pUdp->Socket,
		Span.Data,
		Span.Size,
		pReceive->Id,
		&pReceive->Completion
	) ) {
		(void)xrtNetBufCancel(&pReceive->Buffer);
		return false;
	}
	pReceive->Pending = true;
	(void)xrtAtomic32FetchAdd(
		&pUdp->ActiveReceives,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 处理一个 completion 接收终态。 */
static void __xrtNetUdpReceiveCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	__xrt_net_udp_receive* pReceive =
		(__xrt_net_udp_receive*)pData;
	xnetudp* pUdp = pReceive->Udp;

	(void)pWorker;
	pReceive->Pending = false;
	(void)xrtAtomic32FetchSub(
		&pUdp->ActiveReceives,
		1,
		XMEMORY_ACQ_REL
	);

	/*
	 * CancelIoEx 与正常接收允许竞争。关闭已经在 Worker 上建立后，无论完成包
	 * 最终报告成功还是取消，都必须丢弃借用缓冲并继续收敛唯一 Close。
	 */
	if ( pUdp->CloseRequested || xrtAtomic32Load(
		&pUdp->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetBufCancel(&pReceive->Buffer);
		__xrtNetUdpTryFinish(pUdp);
		return;
	}
	if ( (pEvent->Result == XNET_RESULT_OK) ||
		 (pEvent->Result == XNET_RESULT_TRUNCATED) ) {
		xnetspan Span;

		if ( !__xrtNetUdpReceiveCommit(
			&pReceive->Buffer,
			pEvent->Bytes,
			&Span
		) ) {
			__xrtNetUdpFail(pUdp, true);
			return;
		}
		__xrtNetUdpPublish(
			pUdp,
			&pEvent->Address,
			&pEvent->Meta,
			Span.Data,
			Span.Size,
			pEvent->Result == XNET_RESULT_TRUNCATED
		);
		xrtNetBufClear(&pReceive->Buffer);
	} else {
		(void)xrtNetBufCancel(&pReceive->Buffer);
		if ( !__xrtNetUdpQueuedSystemError(
			pUdp,
			pEvent->SystemCode
		) ) {
			__xrtNetUdpEventError(
				pEvent,
				XNET_ERROR_UDP_RECEIVE,
				"receive-udp",
				"UDP receive failed"
			);
			__xrtNetUdpReportError(pUdp, true);
		}
	}
	if ( (xrtNetUdpState(pUdp) == XNET_UDP_OPEN) &&
		 !xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) &&
		 !__xrtNetUdpArmReceive(pReceive) ) {
		__xrtNetUdpFail(pUdp, true);
	}
}



/* 处理一个 completion 数据报错误终态。 */
static void __xrtNetUdpErrorCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	__xrt_net_udp_error_receive* pReceive =
		(__xrt_net_udp_error_receive*)pData;
	xnetudp* pUdp = pReceive->Udp;

	(void)pWorker;
	pReceive->Pending = false;
	(void)xrtAtomic32FetchSub(
		&pUdp->ActiveReceives,
		1,
		XMEMORY_ACQ_REL
	);
	if ( pUdp->CloseRequested || xrtAtomic32Load(
		&pUdp->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetBufCancel(&pReceive->Buffer);
		__xrtNetUdpTryFinish(pUdp);
		return;
	}
	if ( (pEvent->Result == XNET_RESULT_OK) ||
		 (pEvent->Result == XNET_RESULT_TRUNCATED) ) {
		xnetspan Span;

		if ( !__xrtNetUdpReceiveCommit(
			&pReceive->Buffer,
			pEvent->Bytes,
			&Span
		) ) {
			__xrtNetUdpFail(pUdp, true);
			return;
		}
		__xrtNetUdpPublishError(
			pUdp,
			&pEvent->DgramError,
			Span.Data,
			Span.Size
		);
		xrtNetBufClear(&pReceive->Buffer);
	} else {
		(void)xrtNetBufCancel(&pReceive->Buffer);
		if ( pEvent->Result != XNET_RESULT_AGAIN ) {
			__xrtNetUdpEventError(
				pEvent,
				XNET_ERROR_UDP_RECEIVE,
				"receive-udp-error",
				"UDP datagram error receive failed"
			);
			__xrtNetUdpReportError(pUdp, true);
		}
	}
	if ( (xrtNetUdpState(pUdp) == XNET_UDP_OPEN) &&
		 !xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) &&
		 !__xrtNetUdpArmError(pReceive) ) {
		__xrtNetUdpFail(pUdp, true);
	}
}



/* readiness 后端按预算同步排空数据报错误队列。 */
static bool __xrtNetUdpDriveErrors(xnetudp* pUdp)
{
	__xrt_net_udp_error_receive* pReceive = pUdp->Errors != NULL ?
		&pUdp->Errors->Receive : NULL;
	bool bHandled = false;

	if ( (pReceive == NULL) ||
		 (xrtNetUdpState(pUdp) != XNET_UDP_OPEN) ||
		 xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ||
		 __xrtNetUdpCompletionPort(pUdp) ) {
		return false;
	}
	for ( uint32 i = 0; i < pUdp->Config.ReceiveBatch; i++ ) {
		xnetwspan Write;
		xnetspan Read;
		xnetdgramerror Error;
		size_t iReceived = 0;
		xnetresult Result;

		if ( !xrtNetBufReserve(
			&pReceive->Buffer,
			pUdp->Errors->Size,
			&Write
		) ) {
			__xrtNetUdpFail(pUdp, true);
			return true;
		}
		if ( Write.Size > pUdp->Errors->Size ) {
			Write.Size = pUdp->Errors->Size;
		}
		Result = xrtNetSocketDgramRecvError(
			pUdp->Socket,
			Write.Data,
			Write.Size,
			&iReceived,
			&Error
		);
		if ( Result == XNET_RESULT_AGAIN ) {
			(void)xrtNetBufCancel(&pReceive->Buffer);
			return bHandled;
		}
		bHandled = true;
		if ( (Result != XNET_RESULT_OK) &&
			 (Result != XNET_RESULT_TRUNCATED) ) {
			(void)xrtNetBufCancel(&pReceive->Buffer);
			__xrtNetUdpReportError(pUdp, true);
			return true;
		}
		if ( !__xrtNetUdpReceiveCommit(
			&pReceive->Buffer,
			iReceived,
			&Read
		) ) {
			__xrtNetUdpFail(pUdp, true);
			return true;
		}
		__xrtNetUdpPublishError(
			pUdp,
			&Error,
			Read.Data,
			Read.Size
		);
		xrtNetBufClear(&pReceive->Buffer);
		if ( (xrtNetUdpState(pUdp) != XNET_UDP_OPEN) ||
			 xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ) {
			return true;
		}
	}
	return bHandled;
}



/* readiness 后端按预算同步排空数据报。 */
static void __xrtNetUdpDriveRead(xnetudp* pUdp)
{
	__xrt_net_udp_receive* pReceive = &pUdp->Receives[0];

	if ( (xrtNetUdpState(pUdp) != XNET_UDP_OPEN) ||
		 xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ||
		 __xrtNetUdpCompletionPort(pUdp) ) {
		return;
	}
	for ( uint32 i = 0; i < pUdp->Config.ReceiveBatch; i++ ) {
		xnetwspan Write;
		xnetspan Read;
		xnetaddr Remote;
		xnetdgrammeta Meta;
		size_t iReceived = 0;
		xnetresult Result;

		if ( !xrtNetBufReserve(
			&pReceive->Buffer,
			pUdp->Config.ReceiveSize,
			&Write
		) ) {
			__xrtNetUdpFail(pUdp, true);
			return;
		}
		if ( Write.Size > pUdp->Config.ReceiveSize ) {
			Write.Size = pUdp->Config.ReceiveSize;
		}
		memset(&Meta, 0, sizeof(Meta));
		if ( pUdp->Config.ReceiveMeta != 0 ) {
			Result = xrtNetSocketRecvMsg(
				pUdp->Socket,
				Write.Data,
				Write.Size,
				&iReceived,
				&Remote,
				&Meta
			);
		} else {
			Result = xrtNetSocketRecvFrom(
				pUdp->Socket,
				Write.Data,
				Write.Size,
				&iReceived,
				&Remote
			);
		}
		if ( Result == XNET_RESULT_AGAIN ) {
			(void)xrtNetBufCancel(&pReceive->Buffer);
			if ( !__xrtNetUdpWatch(pUdp) ) {
				__xrtNetUdpFail(pUdp, true);
			}
			return;
		}
		if ( (Result != XNET_RESULT_OK) &&
			 (Result != XNET_RESULT_TRUNCATED) ) {
			(void)xrtNetBufCancel(&pReceive->Buffer);
			__xrtNetUdpReportError(pUdp, true);
			if ( !__xrtNetUdpWatch(pUdp) ) {
				__xrtNetUdpFail(pUdp, true);
			}
			return;
		}
		if ( !__xrtNetUdpReceiveCommit(
			&pReceive->Buffer,
			iReceived,
			&Read
		) ) {
			__xrtNetUdpFail(pUdp, true);
			return;
		}
		__xrtNetUdpPublish(
			pUdp,
			&Remote,
			&Meta,
			Read.Data,
			Read.Size,
			Result == XNET_RESULT_TRUNCATED
		);
		xrtNetBufClear(&pReceive->Buffer);
		if ( (xrtNetUdpState(pUdp) != XNET_UDP_OPEN) ||
			 xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ) {
			return;
		}
	}
	if ( !__xrtNetUdpWatch(pUdp) ) {
		__xrtNetUdpFail(pUdp, true);
	}
}



/* 终结一个指定发送并推进队列。 */
static void __xrtNetUdpSendDone(
	xnetudp* pUdp,
	__xrt_net_udp_send* pSend,
	bool bSent
)
{
	if ( pSend == NULL ) {
		return;
	}
	__xrtNetUdpRemoveSend(pUdp, pSend);
	if ( bSent ) {
		__xrtNetStatFullAdd(&pUdp->SentPackets, 1);
		__xrtNetStatFullAdd(&pUdp->SentBytes, (uint64)pSend->Size);
	}
	__xrtNetUdpReleaseSend(pSend);
}



/* 返回一个空闲 completion 发送槽。 */
static __xrt_net_udp_send_slot* __xrtNetUdpFreeSendSlot(xnetudp* pUdp)
{
	uint32 iCount = pUdp->SendSlotCount;

	for ( uint32 i = 0; i < iCount; i++ ) {
		uint32 iIndex = (pUdp->NextSendSlot + i) % iCount;
		__xrt_net_udp_send_slot* pSlot = &pUdp->SendSlots[iIndex];

		if ( pSlot->Send == NULL ) {
			pUdp->NextSendSlot = (iIndex + 1u) % iCount;
			return pSlot;
		}
	}
	return NULL;
}



/* 向完成式端口提交普通或带逐包控制的数据报。 */
static bool __xrtNetUdpPortSend(
	xnetudp* pUdp,
	__xrt_net_udp_send* pSend,
	uint64 Id,
	ptr pUser
)
{
	xnetport* pPort = xrtNetWorkerPort(pUdp->Worker);

	if ( pSend->Controlled ) {
		return xrtNetPortSendMsg(
			pPort,
			pUdp->Socket,
			pSend->Data,
			pSend->Size,
			pUdp->Connected ? NULL : &pSend->Remote,
			__xrtNetUdpSendControl(pSend),
			Id,
			pUser
		);
	}
	return pUdp->Connected ? xrtNetPortSend(
		pPort,
		pUdp->Socket,
		pSend->Data,
		pSend->Size,
		Id,
		pUser
	) : xrtNetPortSendTo(
		pPort,
		pUdp->Socket,
		pSend->Data,
		pSend->Size,
		&pSend->Remote,
		Id,
		pUser
	);
}



/* 在 readiness 后端同步发送普通或带逐包控制的数据报。 */
static xnetresult __xrtNetUdpSocketSend(
	xnetudp* pUdp,
	__xrt_net_udp_send* pSend,
	size_t* pSent
)
{
	if ( pSend->Controlled ) {
		return xrtNetSocketSendMsg(
			pUdp->Socket,
			pSend->Data,
			pSend->Size,
			pSent,
			pUdp->Connected ? NULL : &pSend->Remote,
			__xrtNetUdpSendControl(pSend)
		);
	}
	return pUdp->Connected ? xrtNetSocketSend(
		pUdp->Socket,
		pSend->Data,
		pSend->Size,
		pSent
	) : xrtNetSocketSendTo(
		pUdp->Socket,
		pSend->Data,
		pSend->Size,
		pSent,
		&pSend->Remote
	);
}



/* 推进发送队列的非重入主体，并严格保持每个数据报的原子边界。 */
static void __xrtNetUdpDriveWriteRun(xnetudp* pUdp)
{
	xnetudpstate State = xrtNetUdpState(pUdp);

	if ( (State != XNET_UDP_OPEN) && (State != XNET_UDP_CLOSING) ) {
		return;
	}
	if ( pUdp->AbortRequested ) {
		__xrtNetUdpDiscardAttached(pUdp);
		__xrtNetUdpTryFinish(pUdp);
		return;
	}
	if ( xrtAtomic32Load(&pUdp->AbortGate, XMEMORY_ACQUIRE) ) {
		return;
	}
	if ( __xrtNetUdpCompletionPort(pUdp) ) {
		for ( uint32 i = 0; i < XRT_NET_UDP_SEND_BUDGET; i++ ) {
			__xrt_net_udp_send* pSend = pUdp->SendReady;
			__xrt_net_udp_send_slot* pSlot =
				__xrtNetUdpFreeSendSlot(pUdp);
			uint64 Id;
			uint64 iActive;

			if ( pUdp->AbortRequested || xrtAtomic32Load(
				&pUdp->AbortGate,
				XMEMORY_ACQUIRE
			) ) {
				return;
			}
			if ( (pSend == NULL) || (pSlot == NULL) ) {
				__xrtNetUdpTryFinish(pUdp);
				return;
			}
			pUdp->SendReady = pSend->Next;
			Id = xrtNetWorkerOperationId(pUdp->Worker);
			if ( Id == 0 ) {
				__xrtNetUdpReportError(pUdp, false);
				__xrtNetUdpSendDone(pUdp, pSend, false);
				continue;
			}
			pSlot->Id = Id;
			pSlot->Send = pSend;
			pSend->Slot = pSlot;
			pSend->Submitted = true;
			iActive = xrtAtomic64FetchAdd(
				&pUdp->ActiveSends,
				1,
				XMEMORY_ACQ_REL
			) + 1;
			__xrtNetUdpPeak(&pUdp->PeakActiveSends, iActive);
			if ( __xrtNetUdpPortSend(
				pUdp,
				pSend,
				Id,
				&pSlot->Completion
			) ) {
				continue;
			}
			(void)xrtAtomic64FetchSub(
				&pUdp->ActiveSends,
				1,
				XMEMORY_ACQ_REL
			);
			pSend->Slot = NULL;
			pSend->Submitted = false;
			pSlot->Send = NULL;
			pSlot->Id = 0;
			__xrtNetUdpReportError(pUdp, false);
			__xrtNetUdpSendDone(pUdp, pSend, false);
		}
		__xrtNetUdpTryFinish(pUdp);
		return;
	}
	if ( pUdp->Config.SendConcurrency > 1 ) {
		uint32 iBudget = XRT_NET_UDP_SEND_BUDGET;

		while ( iBudget != 0 ) {
			__xrt_net_udp_send* Sends[XNET_DGRAM_BATCH_MAX];
			xnetdgramsend Items[XNET_DGRAM_BATCH_MAX];
			__xrt_net_udp_send* pSend = pUdp->SendReady;
			size_t iLimit = pUdp->Config.SendConcurrency;
			size_t iCount = 0;
			size_t iSent = 0;
			xnetresult Result;

			if ( pUdp->AbortRequested || xrtAtomic32Load(
				&pUdp->AbortGate,
				XMEMORY_ACQUIRE
			) ) {
				return;
			}
			if ( (pSend != NULL) && pSend->Controlled ) {
				xnetresult SendResult = __xrtNetUdpSocketSend(
					pUdp,
					pSend,
					&iSent
				);

				if ( SendResult == XNET_RESULT_AGAIN ) {
					if ( !__xrtNetUdpWatch(pUdp) ) {
						__xrtNetUdpFail(pUdp, false);
					}
					return;
				}
				if ( (SendResult != XNET_RESULT_OK) ||
					 (iSent != pSend->Size) ) {
					if ( SendResult == XNET_RESULT_OK ) {
						__xrtNetUdpSetError(
							XERR_IO,
							XNET_ERROR_UDP_SEND,
							"send-udp-message",
							"UDP send completed with a partial datagram"
						);
					}
					__xrtNetUdpReportError(pUdp, false);
					__xrtNetUdpSendDone(pUdp, pSend, false);
				} else {
					__xrtNetUdpSendDone(pUdp, pSend, true);
				}
				iBudget--;
				continue;
			}
			if ( iLimit > (size_t)iBudget ) {
				iLimit = (size_t)iBudget;
			}
			while ( (pSend != NULL) && !pSend->Controlled &&
				 (iCount < iLimit) ) {
				Sends[iCount] = pSend;
				Items[iCount].Remote = pUdp->Connected ?
					NULL : &pSend->Remote;
				Items[iCount].Data = pSend->Data;
				Items[iCount].Size = pSend->Size;
				iCount++;
				pSend = pSend->Next;
			}
			if ( iCount == 0 ) {
				__xrtNetUdpTryFinish(pUdp);
				return;
			}
			Result = xrtNetSocketSendBatch(
				pUdp->Socket,
				Items,
				iCount,
				&iSent
			);
			if ( iSent > iCount ) {
				__xrtNetUdpSetError(
					XERR_INTERNAL,
					XNET_ERROR_UDP_SEND,
					"send-udp",
					"socket batch returned an invalid sent count"
				);
				__xrtNetUdpReportError(pUdp, false);
				return;
			}

			/* 系统调用已接管的前缀必须全部终结，回调中止不能把它们再次丢弃。 */
			for ( size_t i = 0; i < iSent; i++ ) {
				Sends[i]->Submitted = true;
			}
			for ( size_t i = 0; i < iSent; i++ ) {
				Sends[i]->Submitted = false;
				__xrtNetUdpSendDone(pUdp, Sends[i], true);
			}
			iBudget -= (uint32)iSent;
			if ( pUdp->AbortRequested || xrtAtomic32Load(
				&pUdp->AbortGate,
				XMEMORY_ACQUIRE
			) ) {
				return;
			}

			if ( (Result == XNET_RESULT_AGAIN) ||
				 ((Result == XNET_RESULT_OK) && (iSent < iCount)) ) {
				if ( !__xrtNetUdpWatch(pUdp) ) {
					__xrtNetUdpFail(pUdp, false);
				}
				return;
			}
			if ( Result != XNET_RESULT_OK ) {
				if ( iSent == iCount ) {
					__xrtNetUdpReportError(pUdp, false);
					return;
				}
				pSend = Sends[iSent];
				pSend->Submitted = true;
				__xrtNetUdpReportError(pUdp, false);
				pSend->Submitted = false;
				__xrtNetUdpSendDone(pUdp, pSend, false);
				iBudget--;
			}
		}
		if ( (pUdp->SendReady != NULL) && !__xrtNetUdpWatch(pUdp) ) {
			__xrtNetUdpFail(pUdp, false);
		}
		return;
	}
	for ( uint32 i = 0; i < XRT_NET_UDP_SEND_BUDGET; i++ ) {
		__xrt_net_udp_send* pSend = pUdp->SendReady;

		if ( pUdp->AbortRequested || xrtAtomic32Load(
			&pUdp->AbortGate,
			XMEMORY_ACQUIRE
		) ) {
			return;
		}

		if ( pSend == NULL ) {
			__xrtNetUdpTryFinish(pUdp);
			return;
		}
		{
			size_t iSent = 0;
			xnetresult Result = __xrtNetUdpSocketSend(
				pUdp,
				pSend,
				&iSent
			);

			if ( Result == XNET_RESULT_AGAIN ) {
				if ( !__xrtNetUdpWatch(pUdp) ) {
					__xrtNetUdpFail(pUdp, false);
				}
				return;
			}
			if ( (Result != XNET_RESULT_OK) ||
				 (iSent != pSend->Size) ) {
				if ( Result == XNET_RESULT_OK ) {
					__xrtNetUdpSetError(
						XERR_IO,
						XNET_ERROR_UDP_SEND,
						"send-udp",
						"UDP send completed with a partial datagram"
					);
				}
				__xrtNetUdpReportError(pUdp, false);
				__xrtNetUdpSendDone(pUdp, pSend, false);
				continue;
			}
			__xrtNetUdpSendDone(pUdp, pSend, true);
		}
	}
	if ( (pUdp->SendReady != NULL) && !__xrtNetUdpWatch(pUdp) ) {
		__xrtNetUdpFail(pUdp, false);
	}
}



/* 串行化回调触发的递归发送推进，避免同一节点被重复提交。 */
void __xrtNetUdpDriveWrite(xnetudp* pUdp)
{
	if ( pUdp->WriteDriving ) {
		return;
	}
	if ( xrtRefRetain(&pUdp->References) < 0 ) {
		return;
	}
	pUdp->WriteDriving = true;
	__xrtNetUdpDriveWriteRun(pUdp);
	pUdp->WriteDriving = false;
	xrtNetUdpDestroy(pUdp);
}



/* 处理一个 completion 发送槽的终态。 */
void __xrtNetUdpSendCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	__xrt_net_udp_send_slot* pSlot =
		(__xrt_net_udp_send_slot*)pData;
	xnetudp* pUdp = pSlot->Udp;
	__xrt_net_udp_send* pSend = pSlot->Send;
	uint64 Id = pSlot->Id;
	bool bSent = false;

	(void)pWorker;
	if ( pSend == NULL ) {
		return;
	}
	pSlot->Send = NULL;
	pSlot->Id = 0;
	pSend->Slot = NULL;
	pSend->Submitted = false;
	(void)xrtAtomic64FetchSub(
		&pUdp->ActiveSends,
		1,
		XMEMORY_ACQ_REL
	);
	if ( (pEvent->Result == XNET_RESULT_CANCELLED) &&
		 pUdp->AbortRequested ) {
		__xrtNetUdpSendDone(pUdp, pSend, false);
		__xrtNetUdpTryFinish(pUdp);
		return;
	}
	if ( ((pEvent->Type != XNET_PORT_EVENT_SEND) &&
		  (pEvent->Type != XNET_PORT_EVENT_SEND_TO) &&
		  (pEvent->Type != XNET_PORT_EVENT_SEND_MSG)) ||
		 (pEvent->Id != Id) || (pEvent->Bytes != pSend->Size) ||
		 (pEvent->Result != XNET_RESULT_OK) ) {
		if ( pEvent->Result != XNET_RESULT_OK ) {
			__xrtNetUdpEventError(
				pEvent,
				XNET_ERROR_UDP_SEND,
				"send-udp",
				"UDP send failed"
			);
		} else {
			__xrtNetUdpSetError(
				XERR_IO,
				XNET_ERROR_UDP_SEND,
				"send-udp",
				"UDP completion did not match the submitted datagram"
			);
		}
		__xrtNetUdpReportError(pUdp, false);
	} else {
		bSent = true;
	}
	__xrtNetUdpSendDone(pUdp, pSend, bSent);
	__xrtNetUdpDriveWrite(pUdp);
}



/* 处理 readiness 事件。 */
void __xrtNetUdpCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	xnetudp* pUdp = (xnetudp*)pData;

	(void)pWorker;
	if ( xrtRefRetain(&pUdp->References) < 0 ) {
		return;
	}
	if ( pEvent->Type == XNET_PORT_EVENT_READY ) {
		pUdp->WatchPending = false;
		pUdp->WatchEvents = 0;
		if ( (pEvent->Flags & (
			XNET_PORT_EVENT_ERROR |
			XNET_PORT_EVENT_HANGUP
		)) != 0 ) {
			int64 iCode = 0;
			bool bHandled = __xrtNetUdpDriveErrors(pUdp);

			if ( bHandled ) {
				/* 协议错误已经从错误队列发布，不再作为运行时故障重复报告。 */
			} else if ( pEvent->SystemCode != 0 ) {
				__xrtNetUdpEventError(
					pEvent,
					XNET_ERROR_UDP_RECEIVE,
					"poll-udp",
					"UDP readiness backend failed"
				);
				__xrtNetUdpReportError(pUdp, true);
			} else if ( !xrtNetSocketGet(
				pUdp->Socket,
				XNET_OPTION_ERROR,
				&iCode
			) ) {
				__xrtNetUdpReportError(pUdp, true);
			} else if ( iCode != 0 ) {
				__xrtNetSocketSetSystemError(
					XNET_ERROR_UDP_RECEIVE,
					"poll-udp",
					"UDP readiness reported a socket error",
					(int)iCode
				);
				__xrtNetUdpReportError(pUdp, true);
			} else if ( pUdp->Errors == NULL ) {
				__xrtNetUdpSetError(
					XERR_IO,
					XNET_ERROR_UDP_RECEIVE,
					"poll-udp",
					"UDP readiness reported an unspecified socket error"
				);
				__xrtNetUdpReportError(pUdp, true);
			}
		}
		if ( (pEvent->Flags & XNET_PORT_EVENT_READ) != 0 ) {
			__xrtNetUdpDriveRead(pUdp);
		}
		if ( (pEvent->Flags & XNET_PORT_EVENT_WRITE) != 0 ) {
			__xrtNetUdpDriveWrite(pUdp);
		}
		if ( (xrtNetUdpState(pUdp) == XNET_UDP_OPEN) &&
			 !pUdp->WatchPending && !__xrtNetUdpWatch(pUdp) ) {
			__xrtNetUdpFail(pUdp, true);
		}
	}
	xrtNetUdpDestroy(pUdp);
}



/* 在 Worker 上开始正常或异常关闭。 */
static void __xrtNetUdpBeginClose(xnetudp* pUdp, bool bAbort)
{
	if ( xrtNetUdpState(pUdp) == XNET_UDP_CLOSED ) {
		return;
	}
	pUdp->CloseRequested = true;
	if ( bAbort ) {
		pUdp->AbortRequested = true;
		pUdp->CloseResult = XNET_RESULT_CANCELLED;
	}
	xrtAtomic32Store(
		&pUdp->State,
		XNET_UDP_CLOSING,
		XMEMORY_RELEASE
	);
	__xrtNetUdpCancelOperations(pUdp);
	if ( pUdp->AbortRequested ) {
		__xrtNetUdpDiscardAttached(pUdp);
	} else {
		__xrtNetUdpDriveWrite(pUdp);
	}
	__xrtNetUdpTryFinish(pUdp);
}



/* 原子合并启动和关闭请求，只有第一个登记请求持有命令引用。 */
static void __xrtNetUdpControlRequest(
	xnetudp* pUdp,
	uint32 iRequest
)
{
	uint32 iPrevious = xrtAtomic32FetchOr(
		&pUdp->ControlRequests,
		iRequest | XRT_NET_UDP_CONTROL_POSTED,
		XMEMORY_ACQ_REL
	);

	if ( (iPrevious & XRT_NET_UDP_CONTROL_POSTED) == 0 ) {
		xrtNetUdpRef(pUdp);
		if ( !__xrtNetEnginePostInternal(
			pUdp->Worker,
			&pUdp->ControlCommand,
			__xrtNetUdpControl,
			pUdp
		) ) {
			(void)xrtAtomic32FetchAnd(
				&pUdp->ControlRequests,
				~XRT_NET_UDP_CONTROL_POSTED,
				XMEMORY_ACQ_REL
			);
			xrtNetUdpDestroy(pUdp);
		}
	}
}



/* 请求排空已受理发送后正常关闭。 */
XRT_API bool xrtNetUdpClose(xnetudp* pUdp)
{
	uint32 iExpected = 0;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtNetUdpState(pUdp) == XNET_UDP_CLOSED ) {
		return true;
	}
	if ( xrtAtomic32Load(&pUdp->AbortGate, XMEMORY_ACQUIRE) ) {
		return true;
	}
	if ( !xrtAtomic32CompareExchange(
		&pUdp->CloseGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	xrtAtomic32Store(&pUdp->SendGate, 1, XMEMORY_RELEASE);
	__xrtNetUdpControlRequest(
		pUdp,
		XRT_NET_UDP_CONTROL_CLOSE
	);
	return true;
}



/* 请求取消 IO 并异常关闭。 */
XRT_API bool xrtNetUdpAbort(xnetudp* pUdp)
{
	uint32 iExpected = 0;

	if ( pUdp == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtNetUdpState(pUdp) == XNET_UDP_CLOSED ) {
		return true;
	}
	if ( !xrtAtomic32CompareExchange(
		&pUdp->AbortGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	xrtAtomic32Store(&pUdp->CloseGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pUdp->SendGate, 1, XMEMORY_RELEASE);
	__xrtNetUdpControlRequest(
		pUdp,
		XRT_NET_UDP_CONTROL_ABORT
	);
	return true;
}



/* 初始化 Worker 缓冲并启动收发。 */
static void __xrtNetUdpApplyStart(
	xnetworker* pWorker,
	xnetudp* pUdp
)
{
	uint32 iSlots = pUdp->ReceiveSlots;

	if ( xrtNetUdpState(pUdp) != XNET_UDP_OPENING ) {
		return;
	}
	for ( uint32 i = 0; i < iSlots; i++ ) {
		__xrt_net_udp_receive* pReceive = &pUdp->Receives[i];
		xnetwspan Warm;

		pReceive->Udp = pUdp;
		xrtNetCompletionInit(
			&pReceive->Completion,
			__xrtNetUdpReceiveCompletion,
			pReceive
		);
		if ( !xrtNetBufInit(
			&pReceive->Buffer,
			xrtNetWorkerBufPool(pWorker)
		) || !xrtNetBufReserve(
			&pReceive->Buffer,
			pUdp->Config.ReceiveSize,
			&Warm
		 ) || !xrtNetBufCancel(&pReceive->Buffer) ) {
			__xrtNetUdpFail(pUdp, true);
			return;
		}
		pReceive->Ready = true;
	}
	if ( pUdp->Errors != NULL ) {
		__xrt_net_udp_error_receive* pReceive = &pUdp->Errors->Receive;
		xnetwspan Warm;

		pReceive->Udp = pUdp;
		xrtNetCompletionInit(
			&pReceive->Completion,
			__xrtNetUdpErrorCompletion,
			pReceive
		);
		if ( !xrtNetBufInit(
			&pReceive->Buffer,
			xrtNetWorkerBufPool(pWorker)
		) || !xrtNetBufReserve(
			&pReceive->Buffer,
			pUdp->Errors->Size,
			&Warm
		 ) || !xrtNetBufCancel(&pReceive->Buffer) ) {
			__xrtNetUdpFail(pUdp, true);
			return;
		}
		pReceive->Ready = true;
	}
	if ( pUdp->CompletionPort ) {
		for ( uint32 i = 0; i < iSlots; i++ ) {
			if ( !__xrtNetUdpArmReceive(&pUdp->Receives[i]) ) {
				__xrtNetUdpFail(pUdp, true);
				return;
			}
		}
		if ( (pUdp->Errors != NULL) &&
			 !__xrtNetUdpArmError(&pUdp->Errors->Receive) ) {
			__xrtNetUdpFail(pUdp, true);
			return;
		}
	} else if ( !__xrtNetUdpWatch(pUdp) ) {
		__xrtNetUdpFail(pUdp, true);
		return;
	}
	if ( xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ) {
		return;
	}
	xrtAtomic32Store(
		&pUdp->State,
		XNET_UDP_OPEN,
		XMEMORY_RELEASE
	);
	if ( !pUdp->OpenEmitted ) {
		pUdp->OpenEmitted = true;
		if ( pUdp->Events.Open != NULL ) {
			pUdp->Events.Open(
				pUdp,
				__xrtNetUdpDataCurrent(pUdp)
			);
		}
	}
	__xrtNetUdpNotifyFutures(pUdp);
	if ( (xrtNetUdpState(pUdp) == XNET_UDP_OPEN) &&
		 !xrtAtomic32Load(&pUdp->CloseGate, XMEMORY_ACQUIRE) ) {
		__xrtNetUdpDriveWrite(pUdp);
	}
}



/* 启动、关闭和 Abort 共用一个无分配命令节点，并按终态优先级批量收敛。 */
void __xrtNetUdpControl(
	xnetworker* pWorker,
	ptr pData
)
{
	xnetudp* pUdp = (xnetudp*)pData;

	for ( ;; ) {
		uint32 iRequests = xrtAtomic32Exchange(
			&pUdp->ControlRequests,
			XRT_NET_UDP_CONTROL_POSTED,
			XMEMORY_ACQ_REL
		);
		uint32 iLifecycle = iRequests &
			XRT_NET_UDP_CONTROL_LIFECYCLE;

		if ( ((iRequests & XRT_NET_UDP_CONTROL_START) != 0) &&
			 (iLifecycle == 0) ) {
			__xrtNetUdpApplyStart(pWorker, pUdp);
		}
		if ( (iLifecycle != 0) && (xrtAtomic32Load(
			&pUdp->SendSubmitters,
			XMEMORY_ACQUIRE
		 ) || xrtAtomic32Load(
			&pUdp->SendCommands,
			XMEMORY_ACQUIRE
		 )) ) {
			uint32 iExpected;

			(void)xrtAtomic32FetchOr(
				&pUdp->ControlRequests,
				iLifecycle,
				XMEMORY_ACQ_REL
			);
			iExpected = XRT_NET_UDP_CONTROL_POSTED | iLifecycle;
			if ( xrtAtomic32CompareExchange(
				&pUdp->ControlRequests,
				&iExpected,
				iLifecycle,
				XMEMORY_ACQ_REL,
				XMEMORY_ACQUIRE
			) ) {
				if ( !xrtAtomic32Load(
					&pUdp->SendSubmitters,
					XMEMORY_ACQUIRE
				) && !xrtAtomic32Load(
					&pUdp->SendCommands,
					XMEMORY_ACQUIRE
				) ) {
					__xrtNetUdpWakeLifecycle(pUdp);
				}
				xrtNetUdpDestroy(pUdp);
				return;
			}
			continue;
		}
		if ( (iLifecycle & XRT_NET_UDP_CONTROL_ABORT) != 0 ) {
			__xrtNetUdpBeginClose(pUdp, true);
		} else if ( (iLifecycle & XRT_NET_UDP_CONTROL_CLOSE) != 0 ) {
			__xrtNetUdpBeginClose(pUdp, false);
		}
		if ( (iRequests & XRT_NET_UDP_CONTROL_FINISH) != 0 ) {
			__xrtNetUdpTryFinish(pUdp);
		}
		{
			uint32 iExpected = XRT_NET_UDP_CONTROL_POSTED;

			if ( xrtAtomic32CompareExchange(
				&pUdp->ControlRequests,
				&iExpected,
				0,
				XMEMORY_ACQ_REL,
				XMEMORY_ACQUIRE
			) ) {
				break;
			}
		}
	}
	xrtNetUdpDestroy(pUdp);
}




#endif
