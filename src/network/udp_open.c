#include "../internal/xrt_udp.h"



#if defined(XRT_FEATURE_NET_UDP)

#define XRT_NET_UDP_RECEIVE_DEFAULT 2048u
#define XRT_NET_UDP_RECEIVE_CONCURRENCY_DEFAULT 1u
#define XRT_NET_UDP_RECEIVE_CONCURRENCY_MAX 64u
#define XRT_NET_UDP_RECEIVE_BATCH_DEFAULT 16u
#define XRT_NET_UDP_RECEIVE_BATCH_MAX 1024u
#define XRT_NET_UDP_RECEIVE_QUEUE_DEFAULT 256u
#define XRT_NET_UDP_RECEIVE_QUEUE_BYTES_DEFAULT (1024u * 1024u)
#define XRT_NET_UDP_SEND_HIGH_DEFAULT (256u * 1024u)
#define XRT_NET_UDP_SEND_LOW_DEFAULT (64u * 1024u)
#define XRT_NET_UDP_SEND_LIMIT_DEFAULT (1024u * 1024u)
#define XRT_NET_UDP_SEND_PACKETS_DEFAULT 1024u
#define XRT_NET_UDP_SEND_CONCURRENCY_DEFAULT 1u
#define XRT_NET_UDP_SEND_CONCURRENCY_MAX 64u



/* 检查 UDP 配置的内存、队列和平台选项边界。 */
static bool __xrtNetUdpConfigValid(const xnetudpconfig* pConfig)
{
	const uint32 iMeta = XNET_DGRAM_META_DESTINATION |
		XNET_DGRAM_META_INTERFACE |
		XNET_DGRAM_META_HOP_LIMIT |
		XNET_DGRAM_META_TRAFFIC_CLASS |
		XNET_DGRAM_META_SEGMENT_SIZE;

	if ( (pConfig->ReceiveSize == 0) ||
		 (pConfig->ReceiveSize > 65535u) ||
		 (pConfig->ReceiveConcurrency == 0) ||
		 (pConfig->ReceiveConcurrency >
		  XRT_NET_UDP_RECEIVE_CONCURRENCY_MAX) ||
		 (pConfig->ReceiveBatch == 0) ||
		 (pConfig->ReceiveBatch > XRT_NET_UDP_RECEIVE_BATCH_MAX) ||
		 ((pConfig->ReceiveMeta & ~iMeta) != 0) ||
		 (pConfig->Overflow < XNET_UDP_DROP_NEWEST) ||
		 (pConfig->Overflow > XNET_UDP_DROP_ERROR) ||
		 (pConfig->Truncation < XNET_UDP_TRUNCATE_DELIVER) ||
		 (pConfig->Truncation > XNET_UDP_TRUNCATE_ERROR) ||
		 (pConfig->ErrorSize == 0) ||
		 (pConfig->ErrorSize > 65535u) ||
		 (pConfig->ErrorOverflow < XNET_UDP_DROP_NEWEST) ||
		 (pConfig->ErrorOverflow > XNET_UDP_DROP_ERROR) ||
		 (pConfig->SendLimit == 0) ||
		 (pConfig->SendPacketLimit == 0) ||
		 (pConfig->SendHighWater == 0) ||
		 (pConfig->SendHighWater > pConfig->SendLimit) ||
		 (pConfig->SendLowWater > pConfig->SendHighWater) ||
		 (pConfig->SendConcurrency == 0) ||
		 (pConfig->SendConcurrency > XRT_NET_UDP_SEND_CONCURRENCY_MAX) ||
		 (pConfig->ReceiveBuffer < -1) ||
		 (pConfig->SendBuffer < -1) ||
		 (pConfig->HopLimit < -1) ||
		 (pConfig->HopLimit > 255) ||
		 (pConfig->TrafficClass < -1) ||
		 (pConfig->TrafficClass > 255) ||
		 (pConfig->PathMtu < XNET_PMTU_SYSTEM) ||
		 (pConfig->PathMtu > XNET_PMTU_PROBE) ||
		 (pConfig->ExclusiveAddress &&
		  (pConfig->ReuseAddress || pConfig->ReusePort)) ) {
		__xrtNetUdpSetError(
			XERR_ARGUMENT,
			XNET_ERROR_UDP_CONFIG,
			"configure-udp",
			"invalid UDP configuration"
		);
		return false;
	}
	return true;
}



/* 初始化 UDP 默认配置。 */
XRT_API void xrtNetUdpConfigInit(xnetudpconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->ReceiveSize = XRT_NET_UDP_RECEIVE_DEFAULT;
	pConfig->ReceiveConcurrency =
		XRT_NET_UDP_RECEIVE_CONCURRENCY_DEFAULT;
	pConfig->ReceiveBatch = XRT_NET_UDP_RECEIVE_BATCH_DEFAULT;
	pConfig->ReceiveQueueLimit = XRT_NET_UDP_RECEIVE_QUEUE_DEFAULT;
	pConfig->ReceiveQueueByteLimit =
		XRT_NET_UDP_RECEIVE_QUEUE_BYTES_DEFAULT;
	pConfig->Overflow = XNET_UDP_DROP_NEWEST;
	pConfig->Truncation = XNET_UDP_TRUNCATE_DELIVER;
	pConfig->ErrorSize = 256u;
	pConfig->ErrorQueueLimit = 64u;
	pConfig->ErrorQueueByteLimit = 64u * 1024u;
	pConfig->ErrorOverflow = XNET_UDP_DROP_NEWEST;
	pConfig->SendHighWater = XRT_NET_UDP_SEND_HIGH_DEFAULT;
	pConfig->SendLowWater = XRT_NET_UDP_SEND_LOW_DEFAULT;
	pConfig->SendLimit = XRT_NET_UDP_SEND_LIMIT_DEFAULT;
	pConfig->SendPacketLimit = XRT_NET_UDP_SEND_PACKETS_DEFAULT;
	pConfig->SendConcurrency = XRT_NET_UDP_SEND_CONCURRENCY_DEFAULT;
	pConfig->ReceiveBuffer = -1;
	pConfig->SendBuffer = -1;
	pConfig->HopLimit = -1;
	pConfig->TrafficClass = -1;
	#if defined(_WIN32) || defined(_WIN64)
		pConfig->ExclusiveAddress = true;
	#endif
}



/* 应用 UDP 常用 Socket 选项。 */
static bool __xrtNetUdpSocketOptions(
	xnetsocket Socket,
	const xnetudpconfig* pConfig,
	xnetfamily Family,
	bool bConnected
)
{
	/* 共享端点不能让任一关闭远端的 ICMP 决定本地 socket 生命周期。 */
	if ( !bConnected && !__xrtNetSocketUdpConnReset(Socket, false) ) {
		return false;
	}
	if ( pConfig->ReuseAddress && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_REUSE_ADDRESS,
		1
	) ) {
		return false;
	}
	if ( pConfig->ReusePort && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_REUSE_PORT,
		1
	) ) {
		return false;
	}
	if ( pConfig->ExclusiveAddress && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_EXCLUSIVE_ADDRESS,
		1
	) ) {
		return false;
	}
	if ( pConfig->Broadcast && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_BROADCAST,
		1
	) ) {
		return false;
	}
	if ( (Family == XNET_FAMILY_IPV6) && pConfig->IPv6Only &&
		 !xrtNetSocketSet(Socket, XNET_OPTION_IPV6_ONLY, 1) ) {
		return false;
	}
	if ( (pConfig->ReceiveBuffer >= 0) && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_RECEIVE_BUFFER,
		pConfig->ReceiveBuffer
	) ) {
		return false;
	}
	if ( (pConfig->SendBuffer >= 0) && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_SEND_BUFFER,
		pConfig->SendBuffer
	) ) {
		return false;
	}
	if ( (pConfig->HopLimit >= 0) && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_HOP_LIMIT,
		pConfig->HopLimit
	) ) {
		return false;
	}
	if ( (pConfig->TrafficClass >= 0) && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_TRAFFIC_CLASS,
		pConfig->TrafficClass
	) ) {
		return false;
	}
	if ( !xrtNetSocketDgramMetaSet(Socket, pConfig->ReceiveMeta) ) {
		return false;
	}
	if ( (pConfig->PathMtu != XNET_PMTU_SYSTEM) && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_PATH_MTU_MODE,
		(int64)pConfig->PathMtu
	) ) {
		return false;
	}
	if ( pConfig->ReceiveErrors && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_DGRAM_ERRORS,
		1
	) ) {
		return false;
	}
	return true;
}



/* 分配一个持有 Engine 生命周期的 UDP 对象。 */
static xnetudp* __xrtNetUdpCreate(
	xnetengine* pEngine,
	xnetworker* pWorker,
	xnetsocket Socket,
	const xnetaddr* pLocal,
	const xnetaddr* pPeer,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents,
	ptr pData
)
{
	xnetudp* pUdp;
	xnetport* pPort;
	size_t iReceiveAlignment = XRT_INTERNAL_ALIGNOF(__xrt_net_udp_receive);
	size_t iSendAlignment = XRT_INTERNAL_ALIGNOF(__xrt_net_udp_send_slot);
	size_t iErrorAlignment = XRT_INTERNAL_ALIGNOF(
		__xrt_net_udp_error_state
	);
	size_t iReceiveOffset;
	size_t iReceiveBytes;
	size_t iSendOffset;
	size_t iSendBytes;
	size_t iErrorOffset;
	size_t iErrorBytes;
	size_t iReceiveSlots;
	size_t iSendSlots;
	size_t iAllocation;
	bool bCompletionPort;
	bool bNeedReceiveLock = (pEvents == NULL) ||
		(pEvents->Receive == NULL) ||
		(pConfig->ReceiveErrors &&
		 ((pEvents == NULL) || (pEvents->DatagramError == NULL)));

	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		bNeedReceiveLock = true;
	#endif

	if ( !__xrtNetEngineObjectHold(pEngine) ) {
		return NULL;
	}
	pPort = xrtNetWorkerPort(pWorker);
	if ( pPort == NULL ) {
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	bCompletionPort = (xrtNetPortCapabilities(pPort) &
		XNET_PORT_CAP_COMPLETION) != 0;
	if ( pConfig->ReceiveErrors && bCompletionPort &&
		 ((xrtNetPortCapabilities(pPort) &
		   XNET_PORT_CAP_DGRAM_ERROR) == 0) ) {
		__xrtNetEngineObjectRelease(pEngine);
		__xrtNetUdpSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_UDP_CONFIG,
			"configure-udp",
			"completion backend cannot receive datagram errors"
		);
		return NULL;
	}
	iReceiveSlots = bCompletionPort ?
		(size_t)pConfig->ReceiveConcurrency : 1u;
	iSendSlots = bCompletionPort ?
		(size_t)pConfig->SendConcurrency : 0u;
	if ( (sizeof(*pUdp) > (SIZE_MAX - (iReceiveAlignment - 1u))) ||
		 (iReceiveSlots > (SIZE_MAX /
		  sizeof(__xrt_net_udp_receive))) ||
		 (iSendSlots > (SIZE_MAX / sizeof(__xrt_net_udp_send_slot))) ) {
		__xrtNetEngineObjectRelease(pEngine);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iReceiveOffset = (sizeof(*pUdp) + (iReceiveAlignment - 1u)) &
		~(iReceiveAlignment - 1u);
	iReceiveBytes = iReceiveSlots * sizeof(__xrt_net_udp_receive);
	if ( iReceiveBytes > (SIZE_MAX - iReceiveOffset) ) {
		__xrtNetEngineObjectRelease(pEngine);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSendOffset = iReceiveOffset + iReceiveBytes;
	if ( iSendSlots != 0 ) {
		if ( iSendOffset > (SIZE_MAX - (iSendAlignment - 1u)) ) {
			__xrtNetEngineObjectRelease(pEngine);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iSendOffset = (iSendOffset + (iSendAlignment - 1u)) &
			~(iSendAlignment - 1u);
		iSendBytes = iSendSlots * sizeof(__xrt_net_udp_send_slot);
		if ( iSendBytes > (SIZE_MAX - iSendOffset) ) {
			__xrtNetEngineObjectRelease(pEngine);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
	} else {
		iSendBytes = 0;
	}
	iErrorOffset = iSendOffset + iSendBytes;
	if ( pConfig->ReceiveErrors ) {
		if ( iErrorOffset > (SIZE_MAX - (iErrorAlignment - 1u)) ) {
			__xrtNetEngineObjectRelease(pEngine);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iErrorOffset = (iErrorOffset + (iErrorAlignment - 1u)) &
			~(iErrorAlignment - 1u);
		iErrorBytes = sizeof(__xrt_net_udp_error_state);
		if ( iErrorBytes > (SIZE_MAX - iErrorOffset) ) {
			__xrtNetEngineObjectRelease(pEngine);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
	} else {
		iErrorBytes = 0;
	}
	iAllocation = iErrorOffset + iErrorBytes;
	pUdp = (xnetudp*)xrtCalloc(1, iAllocation);
	if ( pUdp == NULL ) {
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	pUdp->References = 2;
	xrtAtomic32Init(&pUdp->State, XNET_UDP_OPENING);
	xrtAtomic32Init(
		&pUdp->ControlRequests,
		XRT_NET_UDP_CONTROL_START | XRT_NET_UDP_CONTROL_POSTED
	);
	xrtAtomic32Init(&pUdp->SendGate, 0);
	xrtAtomic32Init(&pUdp->CloseGate, 0);
	xrtAtomic32Init(&pUdp->AbortGate, 0);
	xrtAtomic32Init(&pUdp->SendSubmitters, 0);
	xrtAtomic32Init(&pUdp->SendCommands, 0);
	xrtAtomic32Init(&pUdp->ActiveReceives, 0);
	xrtAtomic64Init(&pUdp->ActiveSends, 0);
	xrtAtomic64Init(&pUdp->PeakActiveSends, 0);
	xrtAtomic64Init(&pUdp->QueuedBytes, 0);
	xrtAtomic64Init(&pUdp->PeakQueuedBytes, 0);
	xrtAtomic64Init(&pUdp->QueuedPackets, 0);
	xrtAtomic64Init(&pUdp->PeakQueuedPackets, 0);
	xrtAtomic64Init(&pUdp->ReceiveQueued, 0);
	xrtAtomic64Init(&pUdp->PeakReceiveQueued, 0);
	xrtAtomic64Init(&pUdp->ReceiveQueuedBytes, 0);
	xrtAtomic64Init(&pUdp->PeakReceiveQueuedBytes, 0);
	xrtAtomic64Init(&pUdp->ReceivedPackets, 0);
	xrtAtomic64Init(&pUdp->ReceivedBytes, 0);
	xrtAtomic64Init(&pUdp->SentPackets, 0);
	xrtAtomic64Init(&pUdp->SentBytes, 0);
	xrtAtomic64Init(&pUdp->Truncated, 0);
	xrtAtomic64Init(&pUdp->TruncatedDropped, 0);
	xrtAtomic64Init(&pUdp->DroppedNewest, 0);
	xrtAtomic64Init(&pUdp->DroppedOldest, 0);
	xrtAtomic64Init(&pUdp->ReceiveErrors, 0);
	xrtAtomic64Init(&pUdp->SendErrors, 0);
	xrtAtomic64Init(&pUdp->SendRejected, 0);
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		xrtAtomic64Init(&pUdp->ReceiveWaiters, 0);
	#endif
	xrtAtomicPtrInit(&pUdp->Data, pData);
	pUdp->Engine = pEngine;
	pUdp->Worker = pWorker;
	pUdp->Socket = Socket;
	pUdp->SendControl = xrtNetSocketDgramControlAvailable(Socket);
	pUdp->Config.ReceiveSize = pConfig->ReceiveSize;
	pUdp->Config.ReceiveConcurrency = pConfig->ReceiveConcurrency;
	pUdp->Config.ReceiveBatch = pConfig->ReceiveBatch;
	pUdp->Config.ReceiveMeta = pConfig->ReceiveMeta;
	pUdp->Config.ReceiveQueueLimit = pConfig->ReceiveQueueLimit;
	pUdp->Config.ReceiveQueueByteLimit = pConfig->ReceiveQueueByteLimit;
	pUdp->Config.Overflow = pConfig->Overflow;
	pUdp->Config.Truncation = pConfig->Truncation;
	pUdp->Config.SendHighWater = pConfig->SendHighWater;
	pUdp->Config.SendLowWater = pConfig->SendLowWater;
	pUdp->Config.SendLimit = pConfig->SendLimit;
	pUdp->Config.SendPacketLimit = pConfig->SendPacketLimit;
	pUdp->Config.SendConcurrency = pConfig->SendConcurrency;
	if ( pEvents != NULL ) {
		pUdp->Events.Open = pEvents->Open;
		pUdp->Events.Receive = pEvents->Receive;
		pUdp->Events.Error = pEvents->Error;
		pUdp->Events.HighWater = pEvents->HighWater;
		pUdp->Events.LowWater = pEvents->LowWater;
		pUdp->Events.Drain = pEvents->Drain;
		pUdp->Events.Close = pEvents->Close;
	}
	pUdp->Local = *pLocal;
	if ( pPeer != NULL ) {
		pUdp->Peer = *pPeer;
		pUdp->Connected = true;
	}
	pUdp->CloseResult = XNET_RESULT_OK;
	pUdp->Receives = (__xrt_net_udp_receive*)((bytes)pUdp +
		iReceiveOffset);
	pUdp->SendSlots = iSendSlots != 0 ?
		(__xrt_net_udp_send_slot*)((bytes)pUdp + iSendOffset) : NULL;
	pUdp->Errors = pConfig->ReceiveErrors ?
		(__xrt_net_udp_error_state*)((bytes)pUdp + iErrorOffset) : NULL;
	if ( pUdp->Errors != NULL ) {
		pUdp->Errors->Size = pConfig->ErrorSize;
		pUdp->Errors->QueueLimit = pConfig->ErrorQueueLimit;
		pUdp->Errors->QueueByteLimit = pConfig->ErrorQueueByteLimit;
		pUdp->Errors->Overflow = pConfig->ErrorOverflow;
		pUdp->Errors->Callback = pEvents != NULL ?
			pEvents->DatagramError : NULL;
		xrtAtomic64Init(&pUdp->Errors->Received, 0);
		xrtAtomic64Init(&pUdp->Errors->Dropped, 0);
		xrtAtomic64Init(&pUdp->Errors->PathMtuUpdates, 0);
		xrtAtomic64Init(&pUdp->Errors->PathMtu, 0);
		xrtAtomic64Init(&pUdp->Errors->Queued, 0);
		xrtAtomic64Init(&pUdp->Errors->PeakQueued, 0);
		xrtAtomic64Init(&pUdp->Errors->QueuedBytes, 0);
		xrtAtomic64Init(&pUdp->Errors->PeakQueuedBytes, 0);
		#if defined(XRT_FEATURE_NET_UDP_FUTURE)
			xrtAtomic64Init(&pUdp->Errors->ErrorWaiters, 0);
		#endif
	}
	pUdp->ReceiveSlots = (uint32)iReceiveSlots;
	pUdp->SendSlotCount = (uint32)iSendSlots;
	pUdp->CompletionPort = bCompletionPort;
	pUdp->EngineHeld = true;
	pUdp->RuntimeHeld = true;
	xrtNetCompletionInit(
		&pUdp->Completion,
		__xrtNetUdpCompletion,
		pUdp
	);
	for ( uint32 i = 0; i < pUdp->SendSlotCount; i++ ) {
		pUdp->SendSlots[i].Udp = pUdp;
		xrtNetCompletionInit(
			&pUdp->SendSlots[i].Completion,
			__xrtNetUdpSendCompletion,
			&pUdp->SendSlots[i]
		);
	}
	if ( bNeedReceiveLock ) {
		__xrtSpinInit(&pUdp->ReceiveLock);
		pUdp->ReceiveLockReady = true;
	}
	return pUdp;
}



/* 关闭构造失败路径的 Socket，同时保留原始错误。 */
static void __xrtNetUdpClosePreserveError(xnetsocket Socket)
{
	xerror* pError = xrtTakeError();

	(void)xrtNetSocketClose(Socket);
	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 打开、绑定并可选连接 UDP。 */
XRT_API xnetudp* xrtNetUdpOpen(
	xnetengine* pEngine,
	const xnetaddr* pLocal,
	const xnetaddr* pPeer,
	uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents,
	ptr pData
)
{
	xnetudpconfig Config;
	xnetaddr Local;
	xnetfamily Family;
	xnetworker* pWorker;
	xnetsocket Socket;
	xnetudp* pUdp;
	xnetresult Result;

	if ( (pEngine == NULL) ||
		 ((pLocal == NULL) && (pPeer == NULL)) ) {
		__xrtNetUdpSetError(
			XERR_ARGUMENT,
			XNET_ERROR_UDP_CREATE,
			"open-udp",
			"UDP requires an engine and an address family"
		);
		return NULL;
	}
	if ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
		__xrtNetUdpSetError(
			XERR_CLOSED,
			XNET_ERROR_UDP_CREATE,
			"open-udp",
			"UDP requires a running engine"
		);
		return NULL;
	}
	Family = (xnetfamily)(pLocal != NULL ?
		pLocal->Family : pPeer->Family);
	if ( ((Family != XNET_FAMILY_IPV4) &&
		  (Family != XNET_FAMILY_IPV6)) ||
		 ((pLocal != NULL) && (pLocal->Family != Family)) ||
		 ((pPeer != NULL) &&
		  ((pPeer->Family != Family) || (pPeer->Port == 0))) ) {
		__xrtNetUdpSetError(
			XERR_ARGUMENT,
			XNET_ERROR_UDP_CREATE,
			"open-udp",
			"invalid UDP local or peer address"
		);
		return NULL;
	}
	xrtNetUdpConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( !__xrtNetUdpConfigValid(&Config) ) {
		return NULL;
	}
	if ( pLocal != NULL ) {
		Local = *pLocal;
	} else if ( !xrtNetAddrAny(&Local, Family, 0) ) {
		return NULL;
	}
	pWorker = xrtNetEngineWorker(
		pEngine,
		(uint32)(iAffinity % xrtNetEngineWorkerCount(pEngine))
	);
	if ( pWorker == NULL ) {
		return NULL;
	}
	Socket = xrtNetSocketOpen(Family, XNET_SOCKET_DGRAM, 0);
	if ( Socket == NULL ) {
		return NULL;
	}
	if ( !__xrtNetUdpSocketOptions(
		Socket,
		&Config,
		Family,
		pPeer != NULL
	) ||
		 !xrtNetSocketBind(Socket, &Local) ) {
		__xrtNetUdpClosePreserveError(Socket);
		return NULL;
	}
	if ( pPeer != NULL ) {
		Result = xrtNetSocketConnect(Socket, pPeer);
		if ( Result != XNET_RESULT_OK ) {
			if ( Result == XNET_RESULT_AGAIN ) {
				__xrtNetUdpSetError(
					XERR_IO,
					XNET_ERROR_UDP_CREATE,
					"connect-udp",
					"blocking UDP connect did not complete immediately"
				);
			}
			__xrtNetUdpClosePreserveError(Socket);
			return NULL;
		}
	}
	if ( !xrtNetSocketSet(Socket, XNET_OPTION_NONBLOCK, 1) ||
		 !xrtNetSocketLocal(Socket, &Local) ) {
		__xrtNetUdpClosePreserveError(Socket);
		return NULL;
	}
	pUdp = __xrtNetUdpCreate(
		pEngine,
		pWorker,
		Socket,
		&Local,
		pPeer,
		&Config,
		pEvents,
		pData
	);
	if ( pUdp == NULL ) {
		__xrtNetUdpClosePreserveError(Socket);
		return NULL;
	}
	xrtNetUdpRef(pUdp);
	if ( !__xrtNetEnginePostInternal(
		pWorker,
		&pUdp->ControlCommand,
		__xrtNetUdpControl,
		pUdp
	) ) {
		__xrtNetUdpSetError(
			XERR_CLOSED,
			XNET_ERROR_UDP_CREATE,
			"open-udp",
			"network worker shutdown is sealed"
		);
		__xrtNetUdpClosePreserveError(Socket);
		pUdp->Socket = NULL;
		xrtNetUdpDestroy(pUdp);
		xrtNetUdpDestroy(pUdp);
		xrtNetUdpDestroy(pUdp);
		return NULL;
	}
	return pUdp;
}



/* 打开未连接 UDP。 */
XRT_API xnetudp* xrtNetUdpBind(
	xnetengine* pEngine,
	const xnetaddr* pLocal,
	uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents,
	ptr pData
)
{
	return xrtNetUdpOpen(
		pEngine,
		pLocal,
		NULL,
		iAffinity,
		pConfig,
		pEvents,
		pData
	);
}



/* 打开连接式 UDP。 */
XRT_API xnetudp* xrtNetUdpConnect(
	xnetengine* pEngine,
	const xnetaddr* pPeer,
	uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents,
	ptr pData
)
{
	return xrtNetUdpOpen(
		pEngine,
		NULL,
		pPeer,
		iAffinity,
		pConfig,
		pEvents,
		pData
	);
}



/* 检查只能从 UDP Worker 调用的 Socket 扩展操作。 */
static bool __xrtNetUdpWorkerSocket(
	xnetudp* pUdp,
	cstr sOperation
)
{
	if ( (pUdp == NULL) || !xrtNetWorkerIsCurrent(pUdp->Worker) ||
		 (pUdp->Socket == NULL) ||
		 (xrtNetUdpState(pUdp) == XNET_UDP_CLOSED) ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_CONFIG,
			sOperation,
			"UDP socket operation is only available on its worker"
		);
		return false;
	}
	return true;
}



/* 加入一个多播组。 */
XRT_API bool xrtNetUdpJoin(
	xnetudp* pUdp,
	const xnetaddr* pGroup,
	const xnetaddr* pInterface
)
{
	return __xrtNetUdpWorkerSocket(pUdp, "join-udp-multicast") &&
		xrtNetSocketMulticastJoin(
			pUdp->Socket,
			pGroup,
			pInterface
		);
}



/* 离开一个多播组。 */
XRT_API bool xrtNetUdpLeave(
	xnetudp* pUdp,
	const xnetaddr* pGroup,
	const xnetaddr* pInterface
)
{
	return __xrtNetUdpWorkerSocket(pUdp, "leave-udp-multicast") &&
		xrtNetSocketMulticastLeave(
			pUdp->Socket,
			pGroup,
			pInterface
		);
}



/* 设置多播回环。 */
XRT_API bool xrtNetUdpMulticastLoop(xnetudp* pUdp, bool bEnabled)
{
	return __xrtNetUdpWorkerSocket(pUdp, "set-udp-multicast-loop") &&
		xrtNetSocketMulticastLoop(pUdp->Socket, bEnabled);
}



/* 设置多播跳数。 */
XRT_API bool xrtNetUdpMulticastHopLimit(
	xnetudp* pUdp,
	int iHopLimit
)
{
	return __xrtNetUdpWorkerSocket(
		pUdp,
		"set-udp-multicast-hop-limit"
	) && xrtNetSocketMulticastHopLimit(pUdp->Socket, iHopLimit);
}



/* 选择多播发送接口。 */
XRT_API bool xrtNetUdpMulticastInterface(
	xnetudp* pUdp,
	const xnetaddr* pInterface
)
{
	return __xrtNetUdpWorkerSocket(
		pUdp,
		"set-udp-multicast-interface"
	) && xrtNetSocketMulticastInterface(pUdp->Socket, pInterface);
}



/* 返回 UDP 当前状态。 */
XRT_API xnetudpstate xrtNetUdpState(const xnetudp* pUdp)
{
	return pUdp != NULL ? (xnetudpstate)xrtAtomic32Load(
		&pUdp->State,
		XMEMORY_ACQUIRE
	) : XNET_UDP_CLOSED;
}



/* 复制 UDP 本地地址。 */
XRT_API bool xrtNetUdpLocal(
	const xnetudp* pUdp,
	xnetaddr* pAddress
)
{
	if ( (pUdp == NULL) || (pAddress == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pAddress = pUdp->Local;
	return true;
}



/* 复制连接式 UDP 的固定 Peer。 */
XRT_API bool xrtNetUdpPeer(
	const xnetudp* pUdp,
	xnetaddr* pAddress
)
{
	if ( (pUdp == NULL) || (pAddress == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !pUdp->Connected ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_CONFIG,
			"get-udp-peer",
			"UDP is not connected to a fixed peer"
		);
		return false;
	}
	*pAddress = pUdp->Peer;
	return true;
}



/* 返回 UDP 是否连接固定 Peer。 */
XRT_API bool xrtNetUdpConnected(const xnetudp* pUdp)
{
	return (pUdp != NULL) && pUdp->Connected;
}



/* 返回创建时冻结的逐数据报发送控制能力。 */
XRT_API uint32 xrtNetUdpSendControlAvailable(const xnetudp* pUdp)
{
	return pUdp != NULL ? pUdp->SendControl : 0;
}



/* 返回 UDP 所属 Worker。 */
XRT_API xnetworker* xrtNetUdpWorker(const xnetudp* pUdp)
{
	return pUdp != NULL ? pUdp->Worker : NULL;
}



/* 只在 UDP Worker 上返回借用 Socket。 */
XRT_API xnetsocket xrtNetUdpSocket(xnetudp* pUdp)
{
	if ( !__xrtNetUdpWorkerSocket(pUdp, "get-udp-socket") ) {
		return NULL;
	}
	return pUdp->Socket;
}



/* 只允许在 UDP Worker 上替换用户数据。 */
XRT_API bool xrtNetUdpSetData(xnetudp* pUdp, ptr pData)
{
	if ( (pUdp == NULL) || !xrtNetWorkerIsCurrent(pUdp->Worker) ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_CONFIG,
			"set-udp-data",
			"UDP data can only be changed on its worker"
		);
		return false;
	}
	xrtAtomicPtrStore(&pUdp->Data, pData, XMEMORY_RELEASE);
	return true;
}



/* 返回 UDP 用户数据。 */
XRT_API ptr xrtNetUdpData(const xnetudp* pUdp)
{
	return pUdp != NULL ? xrtAtomicPtrLoad(
		&pUdp->Data,
		XMEMORY_ACQUIRE
	) : NULL;
}



/* 返回 UDP 终止错误。 */
XRT_API const xerror* xrtNetUdpError(const xnetudp* pUdp)
{
	if ( (pUdp == NULL) ||
		 (xrtNetUdpState(pUdp) != XNET_UDP_CLOSED) ) {
		return NULL;
	}
	return pUdp->Error;
}



/* 返回发送队列字节数。 */
XRT_API size_t xrtNetUdpPending(const xnetudp* pUdp)
{
	return pUdp != NULL ? (size_t)xrtAtomic64Load(
		&pUdp->QueuedBytes,
		XMEMORY_ACQUIRE
	) : 0;
}



/* 返回拉取接收队列包数。 */
XRT_API size_t xrtNetUdpQueued(const xnetudp* pUdp)
{
	return pUdp != NULL ? (size_t)xrtAtomic64Load(
		&pUdp->ReceiveQueued,
		XMEMORY_ACQUIRE
	) : 0;
}



/* 返回拉取接收队列载荷字节数。 */
XRT_API size_t xrtNetUdpQueuedBytes(const xnetudp* pUdp)
{
	return pUdp != NULL ? (size_t)xrtAtomic64Load(
		&pUdp->ReceiveQueuedBytes,
		XMEMORY_ACQUIRE
	) : 0;
}



/* 返回拉取错误队列条目数。 */
XRT_API size_t xrtNetUdpQueuedErrors(const xnetudp* pUdp)
{
	return ((pUdp != NULL) && (pUdp->Errors != NULL)) ?
		(size_t)xrtAtomic64Load(
		&pUdp->Errors->Queued,
		XMEMORY_ACQUIRE
	) : 0;
}



/* 返回拉取错误队列负载前缀字节数。 */
XRT_API size_t xrtNetUdpQueuedErrorBytes(const xnetudp* pUdp)
{
	return ((pUdp != NULL) && (pUdp->Errors != NULL)) ?
		(size_t)xrtAtomic64Load(
		&pUdp->Errors->QueuedBytes,
		XMEMORY_ACQUIRE
	) : 0;
}



/* 返回最近一次错误队列确认的路径 MTU。 */
XRT_API size_t xrtNetUdpPathMtu(const xnetudp* pUdp)
{
	return ((pUdp != NULL) && (pUdp->Errors != NULL)) ?
		(size_t)xrtAtomic64Load(
		&pUdp->Errors->PathMtu,
		XMEMORY_ACQUIRE
	) : 0;
}



/* 复制 UDP 并发统计。 */
XRT_API bool xrtNetUdpStats(
	const xnetudp* pUdp,
	xnetudpstats* pStats
)
{
	if ( (pUdp == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pStats, 0, sizeof(*pStats));
	pStats->State = xrtNetUdpState(pUdp);
	pStats->ReceivedPackets = xrtAtomic64Load(
		&pUdp->ReceivedPackets,
		XMEMORY_RELAXED
	);
	pStats->ReceivedBytes = xrtAtomic64Load(
		&pUdp->ReceivedBytes,
		XMEMORY_RELAXED
	);
	pStats->SentPackets = xrtAtomic64Load(
		&pUdp->SentPackets,
		XMEMORY_RELAXED
	);
	pStats->SentBytes = xrtAtomic64Load(
		&pUdp->SentBytes,
		XMEMORY_RELAXED
	);
	pStats->Truncated = xrtAtomic64Load(
		&pUdp->Truncated,
		XMEMORY_RELAXED
	);
	pStats->TruncatedDropped = xrtAtomic64Load(
		&pUdp->TruncatedDropped,
		XMEMORY_RELAXED
	);
	pStats->DroppedNewest = xrtAtomic64Load(
		&pUdp->DroppedNewest,
		XMEMORY_RELAXED
	);
	pStats->DroppedOldest = xrtAtomic64Load(
		&pUdp->DroppedOldest,
		XMEMORY_RELAXED
	);
	pStats->ReceiveErrors = xrtAtomic64Load(
		&pUdp->ReceiveErrors,
		XMEMORY_RELAXED
	);
	pStats->SendErrors = xrtAtomic64Load(
		&pUdp->SendErrors,
		XMEMORY_RELAXED
	);
	pStats->SendRejected = xrtAtomic64Load(
		&pUdp->SendRejected,
		XMEMORY_RELAXED
	);
	if ( pUdp->Errors != NULL ) {
		pStats->DatagramErrors = xrtAtomic64Load(
			&pUdp->Errors->Received,
			XMEMORY_RELAXED
		);
		pStats->DatagramErrorsDropped = xrtAtomic64Load(
			&pUdp->Errors->Dropped,
			XMEMORY_RELAXED
		);
		pStats->PathMtuUpdates = xrtAtomic64Load(
			&pUdp->Errors->PathMtuUpdates,
			XMEMORY_RELAXED
		);
		pStats->PathMtu = xrtNetUdpPathMtu(pUdp);
	}
	pStats->QueuedBytes = xrtNetUdpPending(pUdp);
	pStats->PeakQueuedBytes = (size_t)xrtAtomic64Load(
		&pUdp->PeakQueuedBytes,
		XMEMORY_RELAXED
	);
	pStats->QueuedPackets = (size_t)xrtAtomic64Load(
		&pUdp->QueuedPackets,
		XMEMORY_ACQUIRE
	);
	pStats->PeakQueuedPackets = (size_t)xrtAtomic64Load(
		&pUdp->PeakQueuedPackets,
		XMEMORY_RELAXED
	);
	pStats->ReceiveQueued = xrtNetUdpQueued(pUdp);
	pStats->PeakReceiveQueued = (size_t)xrtAtomic64Load(
		&pUdp->PeakReceiveQueued,
		XMEMORY_RELAXED
	);
	pStats->ReceiveQueuedBytes = xrtNetUdpQueuedBytes(pUdp);
	pStats->PeakReceiveQueuedBytes = (size_t)xrtAtomic64Load(
		&pUdp->PeakReceiveQueuedBytes,
		XMEMORY_RELAXED
	);
	pStats->ErrorQueued = xrtNetUdpQueuedErrors(pUdp);
	pStats->ErrorQueuedBytes = xrtNetUdpQueuedErrorBytes(pUdp);
	if ( pUdp->Errors != NULL ) {
		pStats->PeakErrorQueued = (size_t)xrtAtomic64Load(
			&pUdp->Errors->PeakQueued,
			XMEMORY_RELAXED
		);
		pStats->PeakErrorQueuedBytes = (size_t)xrtAtomic64Load(
			&pUdp->Errors->PeakQueuedBytes,
			XMEMORY_RELAXED
		);
	}
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		pStats->ReceiveWaiters = (size_t)xrtAtomic64Load(
			&pUdp->ReceiveWaiters,
			XMEMORY_ACQUIRE
		);
		if ( pUdp->Errors != NULL ) {
			pStats->ErrorWaiters = (size_t)xrtAtomic64Load(
				&pUdp->Errors->ErrorWaiters,
				XMEMORY_ACQUIRE
			);
		}
	#endif
	pStats->ActiveReceives = xrtAtomic32Load(
		&pUdp->ActiveReceives,
		XMEMORY_ACQUIRE
	);
	pStats->ActiveSends = (uint32)xrtAtomic64Load(
		&pUdp->ActiveSends,
		XMEMORY_ACQUIRE
	);
	pStats->PeakActiveSends = (uint32)xrtAtomic64Load(
		&pUdp->PeakActiveSends,
		XMEMORY_RELAXED
	);
	pStats->Connected = pUdp->Connected;
	return true;
}


#endif
