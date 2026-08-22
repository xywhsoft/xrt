#include "../internal/xrt_tcp_server.h"



#if defined(XRT_FEATURE_NET_TCP_SERVER)

#define XRT_NET_SERVER_QUEUE_DEFAULT 1024u



/* 建立带底层原因链的 Server 错误。 */
static xerror* __xrtNetServerErrorCreate(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.net";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	return xrtErrorBuild(&Desc);
}



/* 设置一个 Server 域错误，并在分配失败时保留更具体的当前错误。 */
static void __xrtNetServerSetError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtNetServerErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 从可能未对齐的附加端点数组复制一个配置。 */
static void __xrtNetServerEndpoint(
	const xnetserverconfig* pConfig,
	size_t iEndpoint,
	xnetlistenconfig* pListen
)
{
	if ( iEndpoint == 0 ) {
		memcpy(pListen, &pConfig->Listen, sizeof(*pListen));
	} else {
		memcpy(
			pListen,
			pConfig->Additional + (iEndpoint - 1u),
			sizeof(*pListen)
		);
	}
}



/* 验证端点数组、共享端口、队列和部署模式。 */
static bool __xrtNetServerConfigValid(
	const xnetserverconfig* pConfig,
	uint16* pSharedPort
)
{
	size_t iBytes;
	size_t iEndpoints;
	uint16 iSharedPort = 0;

	if ( (pConfig == NULL) || (pConfig->AcceptQueueLimit == 0) ||
		 ((pConfig->Mode != XNET_SERVER_SHARED) &&
		  (pConfig->Mode != XNET_SERVER_REUSE_PORT)) ||
		 (pConfig->AdditionalCount == SIZE_MAX) ||
		 (pConfig->AdditionalCount >
		  (SIZE_MAX / sizeof(xnetlistenconfig))) ) {
		__xrtNetServerSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SERVER_CONFIG,
			"configure-server",
			"invalid TCP server configuration",
			NULL
		);
		return false;
	}
	iBytes = pConfig->AdditionalCount * sizeof(xnetlistenconfig);
	if ( !__xrtRangeValid(pConfig->Additional, iBytes) ) {
		__xrtNetServerSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SERVER_CONFIG,
			"configure-server",
			"TCP server additional endpoint range is invalid",
			NULL
		);
		return false;
	}
	iEndpoints = pConfig->AdditionalCount + 1u;
	for ( size_t i = 0; i < iEndpoints; i++ ) {
		xnetlistenconfig Listen;

		__xrtNetServerEndpoint(pConfig, i, &Listen);
		if ( !__xrtNetListenConfigValid(&Listen) ) {
			xerror* pCause = xrtTakeError();

			__xrtNetServerSetError(
				XERR_ARGUMENT,
				XNET_ERROR_SERVER_CONFIG,
				"configure-server",
				"TCP server endpoint configuration is invalid",
				pCause
			);
			xrtErrorFree(pCause);
			return false;
		}
		if ( pConfig->SharedPort && (Listen.Address.Port != 0) ) {
			if ( iSharedPort == 0 ) {
				iSharedPort = Listen.Address.Port;
			} else if ( iSharedPort != Listen.Address.Port ) {
				__xrtNetServerSetError(
					XERR_ARGUMENT,
					XNET_ERROR_SERVER_CONFIG,
					"configure-server",
					"shared-port server endpoints must use one port",
					NULL
				);
				return false;
			}
		}
	}
	*pSharedPort = iSharedPort;
	return true;
}



/* 初始化一个可直接启动的单端点 Server 配置。 */
XRT_API void xrtNetServerConfigInit(xnetserverconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtNetListenConfigInit(&pConfig->Listen);
	pConfig->AcceptQueueLimit = XRT_NET_SERVER_QUEUE_DEFAULT;
}



/* 增加 Server 引用。 */
XRT_API xnetserver* xrtNetServerRef(xnetserver* pServer)
{
	if ( (pServer == NULL) ||
		 (xrtRefRetain(&pServer->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pServer;
}



/* 释放最后引用拥有的启动错误和聚合存储。 */
XRT_API void xrtNetServerDestroy(xnetserver* pServer)
{
	if ( (pServer != NULL) &&
		 (xrtRefRelease(&pServer->References) == 0) ) {
		xrtErrorFree(pServer->StartupError);
		xrtFree(pServer);
	}
}



/* 调用方持有 AcceptLock 时取走队首 Stream。 */
xnetstream* __xrtNetServerTakeQueued(xnetserver* pServer)
{
	xnetstream* pStream = pServer->AcceptHead;

	if ( pStream == NULL ) {
		return NULL;
	}
	pServer->AcceptHead = pStream->AcceptNext;
	if ( pServer->AcceptHead == NULL ) {
		pServer->AcceptTail = NULL;
	}
	pStream->AcceptNext = NULL;
	(void)xrtAtomic32FetchSub(
		&pServer->QueuedAccepts,
		1,
		XMEMORY_ACQ_REL
	);
	return pStream;
}



/* 把一个 Stream 放入 Server 的有界聚合队列。 */
static bool __xrtNetServerQueue(
	xnetserver* pServer,
	xnetstream* pStream
)
{
	uint32 iQueued;
	bool bQueued = false;

	__xrtSpinLock(&pServer->AcceptLock);
	iQueued = xrtAtomic32Load(
		&pServer->QueuedAccepts,
		XMEMORY_RELAXED
	);
	if ( (xrtNetServerState(pServer) == XNET_SERVER_OPEN) &&
		 (iQueued < pServer->AcceptQueueLimit) ) {
		pStream->AcceptNext = NULL;
		if ( pServer->AcceptTail != NULL ) {
			pServer->AcceptTail->AcceptNext = pStream;
		} else {
			pServer->AcceptHead = pStream;
		}
		pServer->AcceptTail = pStream;
		iQueued++;
		xrtAtomic32Store(
			&pServer->QueuedAccepts,
			iQueued,
			XMEMORY_RELEASE
		);
		if ( iQueued > xrtAtomic32Load(
			&pServer->PeakQueuedAccepts,
			XMEMORY_RELAXED
		) ) {
			xrtAtomic32Store(
				&pServer->PeakQueuedAccepts,
				iQueued,
				XMEMORY_RELAXED
			);
		}
		bQueued = true;
	}
	__xrtSpinUnlock(&pServer->AcceptLock);
	return bQueued;
}



/* 拉取模式下非阻塞取走一个聚合连接。 */
XRT_API xnetstream* xrtNetServerAccept(xnetserver* pServer)
{
	xnetstream* pStream = NULL;
	bool bWaiting = false;

	if ( pServer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pServer->Events.Accept != NULL ) {
		__xrtNetServerSetError(
			XERR_STATE,
			XNET_ERROR_SERVER_ACCEPT,
			"accept-server",
			"TCP server is configured for push accept callbacks",
			NULL
		);
		return NULL;
	}
	__xrtSpinLock(&pServer->AcceptLock);
	if ( xrtAtomic32Load(
		&pServer->AcceptWaiters,
		XMEMORY_ACQUIRE
	) != 0 ) {
		bWaiting = true;
	} else if ( xrtNetServerState(pServer) == XNET_SERVER_OPEN ) {
		pStream = __xrtNetServerTakeQueued(pServer);
	}
	__xrtSpinUnlock(&pServer->AcceptLock);
	if ( bWaiting ) {
		__xrtNetServerSetError(
			XERR_STATE,
			XNET_ERROR_SERVER_ACCEPT,
			"accept-server",
			"TCP server already has an asynchronous accept consumer",
			NULL
		);
	}
	return pStream;
}



/* 关闭并释放全部尚未被拉取消费者领取的 Stream。 */
static void __xrtNetServerDiscardQueued(xnetserver* pServer)
{
	xnetstream* pHead;

	__xrtSpinLock(&pServer->AcceptLock);
	pHead = pServer->AcceptHead;
	pServer->AcceptHead = NULL;
	pServer->AcceptTail = NULL;
	xrtAtomic32Store(
		&pServer->QueuedAccepts,
		0,
		XMEMORY_RELEASE
	);
	__xrtSpinUnlock(&pServer->AcceptLock);
	while ( pHead != NULL ) {
		xnetstream* pNext = pHead->AcceptNext;

		pHead->AcceptNext = NULL;
		(void)xrtNetStreamAbort(pHead);
		xrtNetStreamDestroy(pHead);
		pHead = pNext;
	}
}



/* 全部已创建 Listener 关闭后发布唯一 Server 终态。 */
static void __xrtNetServerTryFinish(xnetserver* pServer)
{
	bool bFinish = false;
	bool bPublished = false;

	__xrtSpinLock(&pServer->Lock);
	if ( pServer->StartDone &&
		 (xrtNetServerState(pServer) == XNET_SERVER_CLOSING) &&
		 (xrtAtomic64Load(
			&pServer->ClosedListeners,
			XMEMORY_ACQUIRE
		 ) == (uint64)pServer->StartedListeners) ) {
		xrtAtomic32Store(
			&pServer->State,
			XNET_SERVER_CLOSED,
			XMEMORY_RELEASE
		);
		bPublished = pServer->Published;
		bFinish = true;
	}
	__xrtSpinUnlock(&pServer->Lock);
	if ( !bFinish ) {
		return;
	}
	#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
		__xrtNetServerFutureNotify(pServer);
	#endif
	if ( bPublished && (pServer->Events.Close != NULL) ) {
		pServer->Events.Close(pServer, pServer->Data);
	}
	if ( pServer->RuntimeHeld ) {
		pServer->RuntimeHeld = false;
		xrtNetServerDestroy(pServer);
	}
}



/* 关闭当前已经登记的全部 Listener，不在关闭路径分配内存。 */
static void __xrtNetServerCloseListeners(xnetserver* pServer)
{
	for ( size_t i = 0; i < pServer->StartedListeners; i++ ) {
		xnetlistener* pListener = NULL;

		__xrtSpinLock(&pServer->Lock);
		if ( pServer->Children[i].Listener != NULL ) {
			pListener = xrtNetListenerRef(
				pServer->Children[i].Listener
			);
		}
		__xrtSpinUnlock(&pServer->Lock);
		if ( pListener != NULL ) {
			(void)xrtNetListenerClose(pListener);
			xrtNetListenerDestroy(pListener);
		}
	}
}



/* 原子封闭 Server Accept 入口并关闭整组 Listener。 */
XRT_API bool xrtNetServerClose(xnetserver* pServer)
{
	uint32 iExpected = XNET_SERVER_OPEN;

	if ( pServer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pServer->State,
		&iExpected,
		XNET_SERVER_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return iExpected != XNET_SERVER_STARTING;
	}
	__xrtNetServerDiscardQueued(pServer);
	#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
		__xrtNetServerFutureNotify(pServer);
	#endif
	__xrtNetServerCloseListeners(pServer);
	__xrtNetServerTryFinish(pServer);
	return true;
}



/* 记录 Listener 错误，运行期任一 Listener 失败都会关闭整组。 */
static void __xrtNetServerListenerError(
	xnetlistener* pListener,
	const xerror* pCause,
	ptr pData
)
{
	__xrt_net_server_child* pChild =
		(__xrt_net_server_child*)pData;
	xnetserver* pServer = pChild->Server;
	xerror* pError = NULL;
	xnetserverstate State;

	(void)pListener;
	(void)xrtAtomic64FetchAdd(
		&pServer->Errors,
		1,
		XMEMORY_RELAXED
	);
	__xrtSpinLock(&pServer->Lock);
	State = xrtNetServerState(pServer);
	if ( (State == XNET_SERVER_STARTING) &&
		 (pServer->StartupError == NULL) && (pCause != NULL) ) {
		pServer->StartupError = xrtErrorRef(pCause);
	}
	__xrtSpinUnlock(&pServer->Lock);
	if ( State == XNET_SERVER_OPEN ) {
		pError = __xrtNetServerErrorCreate(
			xrtErrorKind(pCause) != XERR_NONE ?
				xrtErrorKind(pCause) : XERR_IO,
			XNET_ERROR_SERVER_ACCEPT,
			"accept-server",
			"TCP server listener failed",
			pCause
		);
		if ( pServer->Events.Error != NULL ) {
			pServer->Events.Error(
				pServer,
				pChild->Endpoint,
				pError != NULL ? pError : pCause,
				pServer->Data
			);
		}
		(void)xrtNetServerClose(pServer);
	}
	xrtErrorFree(pError);
}



/* 注销一个 Child，提前关闭与正常关闭共用同一计数路径。 */
static void __xrtNetServerListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	__xrt_net_server_child* pChild =
		(__xrt_net_server_child*)pData;
	xnetserver* pServer = pChild->Server;
	bool bOwned = false;
	xnetserverstate State;

	__xrtSpinLock(&pServer->Lock);
	if ( !pChild->Closed ) {
		pChild->Closed = true;
		(void)xrtAtomic64FetchAdd(
			&pServer->ClosedListeners,
			1,
			XMEMORY_RELEASE
		);
	}
	if ( pChild->Listener == pListener ) {
		pChild->Listener = NULL;
		bOwned = true;
	}
	State = xrtNetServerState(pServer);
	__xrtSpinUnlock(&pServer->Lock);
	if ( bOwned ) {
		xrtNetListenerDestroy(pListener);
	}
	if ( State == XNET_SERVER_OPEN ) {
		(void)xrtNetServerClose(pServer);
	}
	__xrtNetServerTryFinish(pServer);
}



/* 把已接受 Stream 交给回调、Future 或聚合拉取队列。 */
static bool __xrtNetServerListenerAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	__xrt_net_server_child* pChild =
		(__xrt_net_server_child*)pData;
	xnetserver* pServer = pChild->Server;
	bool bAccepted = false;

	(void)pListener;
	if ( xrtNetServerState(pServer) == XNET_SERVER_OPEN ) {
		(void)xrtNetStreamSetData(pStream, pServer->Data);
		if ( pServer->Events.Accept != NULL ) {
			bAccepted = pServer->Events.Accept(
				pServer,
				pChild->Endpoint,
				pStream,
				pServer->Data
			);
		} else {
			#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
				bAccepted = __xrtNetServerFutureAccept(
					pServer,
					pStream
				);
			#endif
			if ( !bAccepted ) {
				bAccepted = __xrtNetServerQueue(pServer, pStream);
			}
		}
	}
	(void)xrtAtomic64FetchAdd(
		bAccepted ? &pServer->Accepted : &pServer->Rejected,
		1,
		XMEMORY_RELAXED
	);
	#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
		if ( bAccepted && (pServer->Events.Accept == NULL) ) {
			__xrtNetServerFutureNotify(pServer);
		}
	#endif
	return bAccepted;
}



/* 返回所有 Child 共用的静态 Listener 事件表。 */
static const xnetlistenerevents* __xrtNetServerListenerEvents(void)
{
	static const xnetlistenerevents Events = {
		__xrtNetServerListenerAccept,
		__xrtNetServerListenerError,
		__xrtNetServerListenerClose
	};

	return &Events;
}



/* 计算单块 Server、Child 和本地地址快照的安全布局。 */
static bool __xrtNetServerLayout(
	size_t iChildren,
	size_t iEndpoints,
	size_t* pChildrenOffset,
	size_t* pLocalsOffset,
	size_t* pTotal
)
{
	size_t iChildAlign = _Alignof(__xrt_net_server_child);
	size_t iLocalAlign = _Alignof(xnetaddr);
	size_t iChildBytes;
	size_t iLocalBytes;
	size_t iChildrenOffset;
	size_t iLocalsOffset;

	if ( (iChildren > (SIZE_MAX / sizeof(__xrt_net_server_child))) ||
		 (iEndpoints > (SIZE_MAX / sizeof(xnetaddr))) ||
		 (sizeof(xnetserver) > (SIZE_MAX - (iChildAlign - 1u))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iChildBytes = iChildren * sizeof(__xrt_net_server_child);
	iLocalBytes = iEndpoints * sizeof(xnetaddr);
	iChildrenOffset = (sizeof(xnetserver) + (iChildAlign - 1u)) &
		~(iChildAlign - 1u);
	if ( iChildrenOffset > (SIZE_MAX - iChildBytes) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iLocalsOffset = iChildrenOffset + iChildBytes;
	if ( iLocalsOffset > (SIZE_MAX - (iLocalAlign - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iLocalsOffset = (iLocalsOffset + (iLocalAlign - 1u)) &
		~(iLocalAlign - 1u);
	if ( iLocalsOffset > (SIZE_MAX - iLocalBytes) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pChildrenOffset = iChildrenOffset;
	*pLocalsOffset = iLocalsOffset;
	*pTotal = iLocalsOffset + iLocalBytes;
	return true;
}



/* 在启动失败后原子封闭入口、关闭已建 Listener 并保留失败原因。 */
static xnetserver* __xrtNetServerStartFailed(
	xnetserver* pServer,
	const xerror* pCause
)
{
	xerror* pWrapped = NULL;
	const xerror* pFailure = pCause;
	xerror* pError;

	if ( (pCause != NULL) &&
		 ((xrtErrorCode(pCause) != XNET_ERROR_SERVER_START) ||
		  (strcmp(xrtErrorDomain(pCause), "xrt.net") != 0)) ) {
		pWrapped = __xrtNetServerErrorCreate(
			xrtErrorKind(pCause) != XERR_NONE ?
				xrtErrorKind(pCause) : XERR_IO,
			XNET_ERROR_SERVER_START,
			"start-server",
			"TCP server failed to start",
			pCause
		);
		if ( pWrapped != NULL ) {
			pFailure = pWrapped;
		}
	}
	__xrtSpinLock(&pServer->Lock);
	if ( pServer->StartupError == NULL ) {
		pServer->StartupError = xrtErrorRef(pFailure);
	}
	pServer->StartDone = true;
	xrtAtomic32Store(
		&pServer->State,
		XNET_SERVER_CLOSING,
		XMEMORY_RELEASE
	);
	pError = xrtErrorRef(pServer->StartupError);
	__xrtSpinUnlock(&pServer->Lock);
	__xrtNetServerDiscardQueued(pServer);
	#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
		__xrtNetServerFutureNotify(pServer);
	#endif
	__xrtNetServerCloseListeners(pServer);
	__xrtNetServerTryFinish(pServer);
	xrtNetServerDestroy(pServer);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else {
		__xrtNetServerSetError(
			XERR_IO,
			XNET_ERROR_SERVER_START,
			"start-server",
			"TCP server failed to start",
			pCause
		);
	}
	xrtErrorFree(pWrapped);
	xrtErrorFree((xerror*)pCause);
	return NULL;
}



/* 同步建立完整 Listener 集合并在成功后一次发布 Server。 */
XRT_API xnetserver* xrtNetServerStart(
	xnetengine* pEngine,
	const xnetserverconfig* pConfig,
	const xnetserverevents* pEvents,
	const xnetstreamevents* pStreamEvents,
	ptr pData
)
{
	xnetserverconfig Config;
	xnetserver* pServer;
	xerror* pError = NULL;
	uint16 iSharedPort;
	uint32 iWorkers;
	size_t iEndpointCount;
	size_t iListenerCount;
	size_t iChildrenOffset;
	size_t iLocalsOffset;
	size_t iTotal;
	size_t iChild = 0;
	bool bStartFailed;

	if ( (pEngine == NULL) || (pConfig == NULL) ) {
		__xrtNetServerSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SERVER_CONFIG,
			"start-server",
			"TCP server requires an Engine and configuration",
			NULL
		);
		return NULL;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	if ( !__xrtNetServerConfigValid(&Config, &iSharedPort) ) {
		return NULL;
	}
	if ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
		__xrtNetServerSetError(
			XERR_CLOSED,
			XNET_ERROR_SERVER_START,
			"start-server",
			"TCP server requires a running Engine",
			NULL
		);
		return NULL;
	}
	iWorkers = xrtNetEngineWorkerCount(pEngine);
	iEndpointCount = Config.AdditionalCount + 1u;
	if ( (Config.Mode == XNET_SERVER_REUSE_PORT) &&
		 (iEndpointCount > (SIZE_MAX / iWorkers)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iListenerCount = Config.Mode == XNET_SERVER_REUSE_PORT ?
		iEndpointCount * iWorkers : iEndpointCount;
	if ( !__xrtNetServerLayout(
		iListenerCount,
		iEndpointCount,
		&iChildrenOffset,
		&iLocalsOffset,
		&iTotal
	) ) {
		return NULL;
	}
	pServer = (xnetserver*)xrtCalloc(1, iTotal);
	if ( pServer == NULL ) {
		return NULL;
	}
	pServer->References = 2;
	xrtAtomic32Init(&pServer->State, XNET_SERVER_STARTING);
	xrtAtomic64Init(&pServer->Accepted, 0);
	xrtAtomic64Init(&pServer->Rejected, 0);
	xrtAtomic64Init(&pServer->Errors, 0);
	xrtAtomic64Init(&pServer->ClosedListeners, 0);
	xrtAtomic32Init(&pServer->QueuedAccepts, 0);
	xrtAtomic32Init(&pServer->PeakQueuedAccepts, 0);
	xrtAtomic32Init(&pServer->AcceptWaiters, 0);
	__xrtSpinInit(&pServer->Lock);
	__xrtSpinInit(&pServer->AcceptLock);
	pServer->Engine = pEngine;
	pServer->Data = pData;
	pServer->Children = (__xrt_net_server_child*)(
		(bytes)pServer + iChildrenOffset
	);
	pServer->Locals = (xnetaddr*)((bytes)pServer + iLocalsOffset);
	pServer->EndpointCount = iEndpointCount;
	pServer->ListenerCount = iListenerCount;
	pServer->AcceptQueueLimit = Config.AcceptQueueLimit;
	pServer->RuntimeHeld = true;
	if ( pEvents != NULL ) {
		memcpy(&pServer->Events, pEvents, sizeof(pServer->Events));
	}
	for ( size_t iEndpoint = 0;
		iEndpoint < iEndpointCount;
		iEndpoint++ ) {
		xnetlistenconfig Listen;
		uint32 iCopies = Config.Mode == XNET_SERVER_REUSE_PORT ?
			iWorkers : 1u;
		uint16 iEndpointPort;

		__xrtNetServerEndpoint(&Config, iEndpoint, &Listen);
		if ( Config.SharedPort && (Listen.Address.Port == 0) ) {
			Listen.Address.Port = iSharedPort;
		}
		iEndpointPort = Listen.Address.Port;
		for ( uint32 iCopy = 0; iCopy < iCopies; iCopy++, iChild++ ) {
			__xrt_net_server_child* pChild =
				&pServer->Children[iChild];
			xnetlistener* pListener;
			xnetaddr Local;
			bool bClosed;

			pChild->Server = pServer;
			pChild->Endpoint = iEndpoint;
			if ( iCopy != 0 ) {
				Listen.Address.Port = iEndpointPort;
			}
			if ( Config.Mode == XNET_SERVER_REUSE_PORT ) {
				Listen.Affinity = iCopy;
				Listen.Distribution = XNET_ACCEPT_LOCAL;
				Listen.ReusePort = true;
				Listen.ExclusiveAddress = false;
			}
			pListener = xrtNetListen(
				pEngine,
				&Listen,
				__xrtNetServerListenerEvents(),
				pStreamEvents,
				pChild
			);
			if ( pListener == NULL ) {
				pError = xrtTakeError();
				return __xrtNetServerStartFailed(pServer, pError);
			}
			__xrtSpinLock(&pServer->Lock);
			pServer->StartedListeners++;
			bClosed = pChild->Closed;
			if ( !bClosed ) {
				pChild->Listener = pListener;
			}
			__xrtSpinUnlock(&pServer->Lock);
			if ( !xrtNetListenerLocal(pListener, &Local) ) {
				pError = xrtTakeError();
				if ( bClosed ) {
					xrtNetListenerDestroy(pListener);
				}
				return __xrtNetServerStartFailed(pServer, pError);
			}
			if ( bClosed ) {
				xrtNetListenerDestroy(pListener);
			}
			if ( iCopy == 0 ) {
				pServer->Locals[iEndpoint] = Local;
				iEndpointPort = Local.Port;
				if ( Config.SharedPort && (iSharedPort == 0) ) {
					iSharedPort = Local.Port;
				}
			}
		}
	}
	__xrtSpinLock(&pServer->Lock);
	pServer->StartDone = true;
	bStartFailed = (pServer->StartupError != NULL) ||
		 (xrtAtomic64Load(
			&pServer->ClosedListeners,
			XMEMORY_ACQUIRE
		 ) != 0);
	if ( bStartFailed ) {
		pError = xrtErrorRef(pServer->StartupError);
	} else {
		pServer->Published = true;
		xrtAtomic32Store(
			&pServer->State,
			XNET_SERVER_OPEN,
			XMEMORY_RELEASE
		);
	}
	__xrtSpinUnlock(&pServer->Lock);
	if ( bStartFailed ) {
		return __xrtNetServerStartFailed(pServer, pError);
	}
	return pServer;
}



/* 返回 Server 状态的原子快照。 */
XRT_API xnetserverstate xrtNetServerState(const xnetserver* pServer)
{
	return pServer != NULL ? (xnetserverstate)xrtAtomic32Load(
		&pServer->State,
		XMEMORY_ACQUIRE
	) : XNET_SERVER_CLOSED;
}



/* 返回逻辑端点数量。 */
XRT_API size_t xrtNetServerEndpointCount(const xnetserver* pServer)
{
	return pServer != NULL ? pServer->EndpointCount : 0;
}



/* 复制一个发布后保持不变的本地地址。 */
XRT_API bool xrtNetServerLocal(
	const xnetserver* pServer,
	size_t iEndpoint,
	xnetaddr* pAddress
)
{
	if ( (pServer == NULL) || (pAddress == NULL) ||
		 (iEndpoint >= pServer->EndpointCount) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pAddress = pServer->Locals[iEndpoint];
	return true;
}



/* 返回实际 Listener 数量。 */
XRT_API size_t xrtNetServerListenerCount(const xnetserver* pServer)
{
	return pServer != NULL ? pServer->ListenerCount : 0;
}



/* 在线性化锁内取得一个底层 Listener 调用方引用。 */
XRT_API xnetlistener* xrtNetServerListener(
	xnetserver* pServer,
	size_t iListener
)
{
	xnetlistener* pListener = NULL;

	if ( (pServer == NULL) || (iListener >= pServer->ListenerCount) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	__xrtSpinLock(&pServer->Lock);
	if ( pServer->Children[iListener].Listener != NULL ) {
		pListener = xrtNetListenerRef(
			pServer->Children[iListener].Listener
		);
	}
	__xrtSpinUnlock(&pServer->Lock);
	return pListener;
}



/* 返回 Server 用户数据快照。 */
XRT_API ptr xrtNetServerData(const xnetserver* pServer)
{
	return pServer != NULL ? pServer->Data : NULL;
}



/* 复制聚合并发统计。 */
XRT_API bool xrtNetServerStats(
	const xnetserver* pServer,
	xnetserverstats* pStats
)
{
	xnetserverstats Stats;

	if ( (pServer == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Stats.State = xrtNetServerState(pServer);
	Stats.Accepted = xrtAtomic64Load(
		&pServer->Accepted,
		XMEMORY_ACQUIRE
	);
	Stats.Rejected = xrtAtomic64Load(
		&pServer->Rejected,
		XMEMORY_ACQUIRE
	);
	Stats.Errors = xrtAtomic64Load(
		&pServer->Errors,
		XMEMORY_ACQUIRE
	);
	Stats.Endpoints = pServer->EndpointCount;
	Stats.Listeners = pServer->ListenerCount;
	Stats.ClosedListeners = (size_t)xrtAtomic64Load(
		&pServer->ClosedListeners,
		XMEMORY_ACQUIRE
	);
	Stats.QueuedAccepts = xrtAtomic32Load(
		&pServer->QueuedAccepts,
		XMEMORY_ACQUIRE
	);
	Stats.PeakQueuedAccepts = xrtAtomic32Load(
		&pServer->PeakQueuedAccepts,
		XMEMORY_ACQUIRE
	);
	Stats.AcceptWaiters = xrtAtomic32Load(
		&pServer->AcceptWaiters,
		XMEMORY_ACQUIRE
	);
	*pStats = Stats;
	return true;
}

#endif
