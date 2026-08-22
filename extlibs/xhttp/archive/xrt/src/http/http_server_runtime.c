#include "../internal/xrt_http_server_runtime.h"



#if defined(XRT_FEATURE_HTTP_SERVER)

#define XRT_HTTP_SERVER_WRITE_DEFAULT ((size_t)16384)
#define XRT_HTTP_SERVER_HEADER_TIMEOUT_DEFAULT UINT64_C(10000000)
#define XRT_HTTP_SERVER_BODY_TIMEOUT_DEFAULT UINT64_C(30000000)
#define XRT_HTTP_SERVER_REQUEST_TIMEOUT_DEFAULT UINT64_C(30000000)
#define XRT_HTTP_SERVER_IDLE_TIMEOUT_DEFAULT UINT64_C(60000000)
#define XRT_HTTP_SERVER_WRITE_TIMEOUT_DEFAULT UINT64_C(30000000)
#define XRT_HTTP_SERVER_INFORMATION_DEFAULT ((size_t)16)



/* 建立 Server 域错误并保留底层原因。 */
xerror* __xrtHttpServerErrorCreate(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.server";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 设置当前线程的 Server 同步错误。 */
void __xrtHttpServerSetError(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpServerErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 沿原因链选取最内层有效错误类别。 */
xerrkind __xrtHttpServerCauseKind(
	const xerror* pError,
	xerrkind Fallback
)
{
	xerrkind Kind = Fallback;

	while ( pError != NULL ) {
		if ( xrtErrorKind(pError) != XERR_NONE ) {
			Kind = xrtErrorKind(pError);
		}
		pError = xrtErrorCause(pError);
	}
	return Kind;
}



/* 保存连接第一个稳定错误。 */
void __xrtHttpConnRememberError(
	xhttpconn* pConnection,
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError;
	ptr pExpected = NULL;

	if ( pConnection == NULL ) {
		return;
	}
	pError = __xrtHttpServerErrorCreate(
		__xrtHttpServerCauseKind(pCause, Kind),
		Code,
		sOperation,
		sMessage,
		pCause
	);
	if ( pError == NULL ) {
		return;
	}
	if ( !xrtAtomicPtrCompareExchange(
		&pConnection->Error,
		&pExpected,
		pError,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		xrtErrorFree(pError);
		pError = (xerror*)pExpected;
	}
	xrtSetError(pError);
}



/* 原子提高连接峰值。 */
static void __xrtHttpServerPeak(
	xhttpserver* pServer,
	uint64 iValue
)
{
	uint64 iPeak = xrtAtomic64Load(
		&pServer->PeakConnections,
		XMEMORY_RELAXED
	);

	while ( (iValue > iPeak) &&
		!xrtAtomic64CompareExchange(
			&pServer->PeakConnections,
			&iPeak,
			iValue,
			XMEMORY_RELAXED,
			XMEMORY_RELAXED
		) ) {
	}
}



/* 把已保留计数的 Connection 加入 Server 列表。 */
void __xrtHttpServerAddConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
)
{
	uint64 iConnections = xrtAtomic64Load(
		&pServer->Connections,
		XMEMORY_RELAXED
	);

	__xrtSpinLock(&pServer->Lock);
	pConnection->Previous = pServer->Tail;
	pConnection->Next = NULL;
	if ( pServer->Tail != NULL ) {
		pServer->Tail->Next = pConnection;
	} else {
		pServer->Head = pConnection;
	}
	pServer->Tail = pConnection;
	pConnection->Listed = true;
	pConnection->Counted = true;
	__xrtSpinUnlock(&pServer->Lock);
	__xrtHttpServerPeak(pServer, iConnections);
}



/* 从 Server 的可遍历列表摘除 Connection，但保留尚未退出的运行时计数。 */
void __xrtHttpServerRemoveConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
)
{
	__xrtSpinLock(&pServer->Lock);
	if ( pConnection->Listed ) {
		if ( pConnection->Previous != NULL ) {
			pConnection->Previous->Next = pConnection->Next;
		} else {
			pServer->Head = pConnection->Next;
		}
		if ( pConnection->Next != NULL ) {
			pConnection->Next->Previous =
				pConnection->Previous;
		} else {
			pServer->Tail = pConnection->Previous;
		}
		pConnection->Previous = NULL;
		pConnection->Next = NULL;
		pConnection->Listed = false;
	}
	__xrtSpinUnlock(&pServer->Lock);

}



/* 在发布 Connection Close 前归还公开活跃连接计数。 */
void __xrtHttpServerConnectionClosed(xhttpserver* pServer)
{
	(void)xrtAtomic64FetchSub(
		&pServer->Connections,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 最后归还 Connection 生命周期计数，再尝试发布 Server 关闭终态。 */
void __xrtHttpServerConnectionFinished(xhttpserver* pServer)
{
	(void)xrtAtomic64FetchSub(
		&pServer->RuntimeConnections,
		1,
		XMEMORY_ACQ_REL
	);
	__xrtHttpServerTryFinish(pServer);
}



/* 增加 Server 引用。 */
XRT_API xhttpserver* xrtHttpServerRef(xhttpserver* pServer)
{
	if ( (pServer == NULL) ||
		(xrtRefRetain(&pServer->References) < 0) ) {
		__xrtHttpServerSetError(
			pServer == NULL ? XERR_ARGUMENT : XERR_STATE,
			pServer == NULL ?
				XHTTP_SERVER_ERROR_ARGUMENT :
				XHTTP_SERVER_ERROR_STATE,
			"retain-http-server",
			"HTTP server is null or no longer retainable",
			NULL
		);
		return NULL;
	}
	return pServer;
}



/* 释放 Server 最终存储。 */
XRT_API void xrtHttpServerDestroy(xhttpserver* pServer)
{
	if ( (pServer == NULL) ||
		(xrtRefRelease(&pServer->References) != 0) ) {
		return;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		__xrtHttpServerTlsCleanup(pServer);
	#endif
	xrtErrorFree((xerror*)xrtAtomicPtrLoad(
		&pServer->NetworkError,
		XMEMORY_ACQUIRE
	));
	xrtNetServerDestroy(pServer->Network);
	__xrtSpinUnit(&pServer->Lock);
	memset(pServer, 0, sizeof(*pServer));
	xrtFree(pServer);
}



/* 连接和聚合 TCP Server 全部退出后发布唯一关闭事件。 */
void __xrtHttpServerTryFinish(xhttpserver* pServer)
{
	uint32 iState;
	uint32 iExpected;
	bool bPublished;

	if ( (pServer == NULL) ||
		!xrtAtomic32Load(
			&pServer->NetworkClosed,
			XMEMORY_ACQUIRE
		) ||
		(xrtAtomic64Load(
			&pServer->RuntimeConnections,
			XMEMORY_ACQUIRE
		) != 0) ) {
		return;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
		if ( xrtAtomic64Load(
			&pServer->UpgradeCallbacks,
			XMEMORY_ACQUIRE
		) != 0 ) {
			return;
		}
	#endif
	iState = xrtAtomic32Load(
		&pServer->State,
		XMEMORY_ACQUIRE
	);
	if ( (iState != XHTTP_SERVER_DRAINING) &&
		(iState != XHTTP_SERVER_ABORTING) ) {
		return;
	}
	iExpected = iState;
	if ( !xrtAtomic32CompareExchange(
		&pServer->State,
		&iExpected,
		XHTTP_SERVER_CLOSED,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	bPublished = xrtAtomic32Load(
		&pServer->NetworkPublished,
		XMEMORY_ACQUIRE
	) != 0;
	if ( bPublished && (pServer->Events.Shutdown != NULL) ) {
		pServer->Events.Shutdown(
			pServer, pServer->Events.Data
		);
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_FUTURE)
		if ( bPublished ) {
			xrtAtomic32Store(
				&pServer->ShutdownPublished,
				1,
				XMEMORY_RELEASE
			);
			__xrtHttpServerFutureFinish(pServer);
		}
	#endif
	if ( pServer->RuntimeHeld ) {
		pServer->RuntimeHeld = false;
		xrtHttpServerDestroy(pServer);
	}
}



/* 保存并提升聚合 TCP Server 错误。 */
static void __xrtHttpServerNetworkError(
	xnetserver* pNetwork,
	size_t iEndpoint,
	const xerror* pCause,
	ptr pData
)
{
	xhttpserver* pServer = (xhttpserver*)pData;
	xerror* pError;
	ptr pExpected = NULL;
	bool bStored = false;

	(void)pNetwork;
	(void)iEndpoint;
	pError = __xrtHttpServerErrorCreate(
		__xrtHttpServerCauseKind(pCause, XERR_IO),
		XHTTP_SERVER_ERROR_LISTEN,
		"listen-http-server",
		"HTTP server network failed",
		pCause
	);
	if ( pError != NULL ) {
		bStored = xrtAtomicPtrCompareExchange(
			&pServer->NetworkError,
			&pExpected,
			pError,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		);
	}
	if ( (pError != NULL) &&
		 xrtAtomic32Load(
			&pServer->NetworkPublished,
			XMEMORY_ACQUIRE
		 ) && (pServer->Events.Error != NULL) ) {
		pServer->Events.Error(
			pServer,
			NULL,
			pError,
			pServer->Events.Data
		);
	}
	if ( !bStored ) {
		xrtErrorFree(pError);
	}
}



/* 聚合 TCP Server 关闭后封闭 HTTP 接入，并终止意外中断的连接。 */
static void __xrtHttpServerNetworkClose(
	xnetserver* pNetwork,
	ptr pData
)
{
	xhttpserver* pServer = (xhttpserver*)pData;
	xhttpserver* pOwned = xrtHttpServerRef(pServer);
	bool bPublished;

	(void)pNetwork;
	if ( pOwned == NULL ) {
		return;
	}
	__xrtSpinLock(&pServer->Lock);
	xrtAtomic32Store(
		&pServer->NetworkClosed,
		1,
		XMEMORY_RELEASE
	);
	bPublished = xrtAtomic32Load(
		&pServer->NetworkPublished,
		XMEMORY_ACQUIRE
	) != 0;
	__xrtSpinUnlock(&pServer->Lock);
	if ( bPublished &&
		 (xrtHttpServerState(pServer) == XHTTP_SERVER_RUNNING) ) {
		(void)xrtHttpServerAbort(pServer);
	}
	__xrtHttpServerTryFinish(pServer);
	xrtHttpServerDestroy(pOwned);
}



/* 为一个新连接原子保留 MaxConnections 预算。 */
static bool __xrtHttpServerReserveConnection(
	xhttpserver* pServer
)
{
	uint64 iPrevious;

	if ( xrtHttpServerState(pServer) != XHTTP_SERVER_RUNNING ) {
		return false;
	}
	iPrevious = xrtAtomic64FetchAdd(
		&pServer->Connections,
		1,
		XMEMORY_ACQ_REL
	);
	(void)xrtAtomic64FetchAdd(
		&pServer->RuntimeConnections,
		1,
		XMEMORY_ACQ_REL
	);
	if ( (pServer->Config.MaxConnections != 0) &&
		(iPrevious >=
		 (uint64)pServer->Config.MaxConnections) ) {
		(void)xrtAtomic64FetchSub(
			&pServer->Connections,
			1,
			XMEMORY_ACQ_REL
		);
		(void)xrtAtomic64FetchSub(
			&pServer->RuntimeConnections,
			1,
			XMEMORY_ACQ_REL
		);
		return false;
	}
	return true;
}



/* 接管聚合 TCP Server 转移的 Stream 引用并建立 HTTP Connection。 */
static bool __xrtHttpServerNetworkAccept(
	xnetserver* pNetwork,
	size_t iEndpoint,
	xnetstream* pStream,
	ptr pData
)
{
	xhttpserver* pServer = (xhttpserver*)pData;
	xhttpconn* pConnection;
	xerror* pError;

	(void)pNetwork;
	if ( !xrtAtomic32Load(
		&pServer->NetworkPublished,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtAtomic64FetchAdd(
			&pServer->Rejected,
			1,
			XMEMORY_RELAXED
		);
		return false;
	}
	if ( !__xrtHttpServerReserveConnection(pServer) ) {
		(void)xrtAtomic64FetchAdd(
			&pServer->Rejected,
			1,
			XMEMORY_RELAXED
		);
		return false;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pServer->Secure ) {
			pConnection = __xrtHttpConnCreateTls(
				pServer, iEndpoint, pStream
			);
		} else {
			pConnection = __xrtHttpConnCreateTcp(
				pServer, iEndpoint, pStream
			);
		}
	#else
		pConnection = __xrtHttpConnCreateTcp(
			pServer, iEndpoint, pStream
		);
	#endif
	if ( pConnection == NULL ) {
		pError = xrtTakeError();
		(void)xrtAtomic64FetchAdd(
			&pServer->Rejected,
			1,
			XMEMORY_RELAXED
		);
		if ( (pError != NULL) &&
			(pServer->Events.Error != NULL) ) {
			pServer->Events.Error(
				pServer,
				NULL,
				pError,
				pServer->Events.Data
			);
		}
		xrtSetError(pError);
		xrtErrorFree(pError);
		__xrtHttpServerConnectionClosed(pServer);
		__xrtHttpServerConnectionFinished(pServer);
		return false;
	}
	__xrtHttpServerAddConnection(pServer, pConnection);
	(void)xrtAtomic64FetchAdd(
		&pServer->Accepted,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtHttpServerState(pServer) != XHTTP_SERVER_RUNNING ) {
		(void)xrtHttpConnClose(pConnection);
	}
	return true;
}



/* 返回静态聚合 TCP Server 事件表。 */
static const xnetserverevents* __xrtHttpServerNetworkEvents(void)
{
	static const xnetserverevents Events = {
		__xrtHttpServerNetworkAccept,
		__xrtHttpServerNetworkError,
		__xrtHttpServerNetworkClose
	};

	return &Events;
}



/* 验证公开输出不会覆盖 Server 状态或紧随其后的端点地址。 */
static bool __xrtHttpServerOutputValid(
	const xhttpserver* pServer,
	const void* pOutput,
	size_t iSize
)
{
	if ( !__xrtRangeValid(pOutput, iSize) ) {
		return false;
	}
	if ( pServer == NULL ) {
		return true;
	}
	return !__xrtRangesOverlap(
		pOutput, iSize, pServer, sizeof(*pServer)
	) && !__xrtRangesOverlap(
		pOutput,
		iSize,
		pServer->Locals,
		pServer->EndpointCount * sizeof(xnetaddr)
	);
}



/* 初始化 Server 默认策略。 */
XRT_API void xrtHttpServerConfigInit(xhttpserverconfig* pConfig)
{
	xhttpserverconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"init-http-server-config",
			"HTTP server config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtNetServerConfigInit(&Config.Network);
	xrtHttp1ServerConfigInit(&Config.Http1);
	Config.WriteSize = XRT_HTTP_SERVER_WRITE_DEFAULT;
	Config.HeaderTimeout =
		XRT_HTTP_SERVER_HEADER_TIMEOUT_DEFAULT;
	Config.BodyTimeout =
		XRT_HTTP_SERVER_BODY_TIMEOUT_DEFAULT;
	Config.RequestTimeout =
		XRT_HTTP_SERVER_REQUEST_TIMEOUT_DEFAULT;
	Config.IdleTimeout =
		XRT_HTTP_SERVER_IDLE_TIMEOUT_DEFAULT;
	Config.WriteTimeout =
		XRT_HTTP_SERVER_WRITE_TIMEOUT_DEFAULT;
	Config.MaxInformations =
		XRT_HTTP_SERVER_INFORMATION_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化空 Server 事件表。 */
XRT_API void xrtHttpServerEventsInit(xhttpserverevents* pEvents)
{
	const xhttpserverevents Events = { 0 };

	if ( !__xrtRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"init-http-server-events",
			"HTTP server events range is invalid",
			NULL
		);
		return;
	}
	memcpy(pEvents, &Events, sizeof(Events));
}



/* 检查全部逻辑端点的 TCP 写入硬上限。 */
bool __xrtHttpServerNetworkWriteLimit(
	const xhttpserverconfig* pConfig,
	size_t iMinimum
)
{
	size_t iEndpoints = pConfig->Network.AdditionalCount + 1u;

	for ( size_t i = 0; i < iEndpoints; i++ ) {
		xnetlistenconfig Listen;

		if ( i == 0 ) {
			memcpy(
				&Listen,
				&pConfig->Network.Listen,
				sizeof(Listen)
			);
		} else {
			memcpy(
				&Listen,
				pConfig->Network.Additional + (i - 1u),
				sizeof(Listen)
			);
		}
		if ( Listen.Stream.WriteLimit < iMinimum ) {
			return false;
		}
	}
	return true;
}



/* 检查必须在监听前确定的 Server 组合边界。 */
static bool __xrtHttpServerConfigValid(
	const xhttpserverconfig* pConfig
)
{
	size_t iAdditionalBytes;

	if ( (pConfig == NULL) ||
		(pConfig->WriteSize == 0) ||
		(pConfig->MaxInformations == 0) ||
		(pConfig->Network.AdditionalCount == SIZE_MAX) ||
		(pConfig->Network.AdditionalCount >
		 (SIZE_MAX / sizeof(xnetlistenconfig))) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-http-server",
			"HTTP server configuration is invalid",
			NULL
		);
		return false;
	}
	iAdditionalBytes = pConfig->Network.AdditionalCount *
		sizeof(xnetlistenconfig);
	if ( !__xrtRangeValid(
		pConfig->Network.Additional,
		iAdditionalBytes
	) || !__xrtHttpServerNetworkWriteLimit(
		pConfig,
		pConfig->WriteSize
	) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-http-server",
			"HTTP write size must fit every TCP endpoint write limit",
			NULL
		);
		return false;
	}
	if ( !__xrtHttp1ServerConfigValid(&pConfig->Http1) ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(), XERR_ARGUMENT
			),
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-http-server",
			"HTTP server Exchange configuration is invalid",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 验证配置并创建尚未开始监听的 Server。 */
xhttpserver* __xrtHttpServerCreate(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
)
{
	xhttpserverconfig Config;
	xhttpserverevents Events;
	xhttpserver* pServer;
	size_t iEndpointCount;
	size_t iTotal;

	xrtHttpServerConfigInit(&Config);
	xrtHttpServerEventsInit(&Events);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"start-http-server",
				"HTTP server config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( pEvents != NULL ) {
		if ( !__xrtRangeValid(pEvents, sizeof(Events)) ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"start-http-server",
				"HTTP server events range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Events, pEvents, sizeof(Events));
	}
	if ( (pEngine == NULL) ||
		!__xrtHttpServerConfigValid(&Config) ) {
		if ( pEngine == NULL ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"start-http-server",
				"HTTP server requires a network engine",
				NULL
			);
		}
		return NULL;
	}
	iEndpointCount = Config.Network.AdditionalCount + 1u;
	if ( iEndpointCount >
		 ((SIZE_MAX - sizeof(*pServer)) / sizeof(xnetaddr)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = sizeof(*pServer) +
		(iEndpointCount * sizeof(xnetaddr));
	pServer = (xhttpserver*)xrtCalloc(1, iTotal);
	if ( pServer == NULL ) {
		return NULL;
	}
	pServer->References = 1;
	xrtAtomic32Init(&pServer->State, XHTTP_SERVER_RUNNING);
	xrtAtomic64Init(&pServer->Accepted, 0);
	xrtAtomic64Init(&pServer->Rejected, 0);
	xrtAtomic64Init(&pServer->Requests, 0);
	xrtAtomic64Init(&pServer->Responses, 0);
	xrtAtomic64Init(&pServer->Informations, 0);
	xrtAtomic64Init(&pServer->Upgraded, 0);
	xrtAtomic64Init(&pServer->ProtocolErrors, 0);
	xrtAtomic64Init(&pServer->Timeouts, 0);
	xrtAtomic64Init(&pServer->Connections, 0);
	xrtAtomic64Init(&pServer->RuntimeConnections, 0);
	xrtAtomic64Init(&pServer->PeakConnections, 0);
	#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
		xrtAtomic64Init(&pServer->UpgradeCallbacks, 0);
	#endif
	xrtAtomic32Init(&pServer->NetworkClosed, 0);
	xrtAtomic32Init(&pServer->NetworkPublished, 0);
	xrtAtomicPtrInit(&pServer->NetworkError, NULL);
	#if defined(XRT_FEATURE_HTTP_SERVER_FUTURE)
		xrtAtomic32Init(&pServer->ShutdownPublished, 0);
	#endif
	__xrtSpinInit(&pServer->Lock);
	pServer->Engine = pEngine;
	pServer->Locals = (xnetaddr*)(pServer + 1u);
	pServer->Config = Config;
	pServer->Events = Events;
	pServer->EndpointCount = iEndpointCount;
	return pServer;
}



/* 启动聚合 TCP Server，并建立独立 HTTP 运行时引用。 */
xhttpserver* __xrtHttpServerListen(xhttpserver* pServer)
{
	const xnetstreamevents* pStreamEvents;
	xnetserver* pNetwork;
	xerror* pCause;
	bool bPublished = false;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"listen-http-server",
			"HTTP server is null",
			NULL
		);
		return NULL;
	}
	if ( xrtHttpServerRef(pServer) == NULL ) {
		xrtHttpServerDestroy(pServer);
		return NULL;
	}
	pServer->RuntimeHeld = true;
	pStreamEvents = pServer->Secure ?
		NULL : __xrtHttpConnStreamEvents();
	pNetwork = xrtNetServerStart(
		pServer->Engine,
		&pServer->Config.Network,
		__xrtHttpServerNetworkEvents(),
		pStreamEvents,
		pServer
	);
	if ( pNetwork == NULL ) {
		pCause = xrtErrorRef(xrtGetError());

		pServer->RuntimeHeld = false;
		xrtHttpServerDestroy(pServer);
		xrtHttpServerDestroy(pServer);
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(pCause, XERR_IO),
			XHTTP_SERVER_ERROR_LISTEN,
			"start-http-server",
			"HTTP server could not start its TCP listener",
			pCause
		);
		xrtErrorFree(pCause);
		return NULL;
	}
	pServer->Network = pNetwork;
	pServer->ListenerCount = xrtNetServerListenerCount(pNetwork);
	pServer->Config.Network.Additional = NULL;
	pServer->Config.Network.AdditionalCount = 0;
	for ( size_t i = 0; i < pServer->EndpointCount; i++ ) {
		if ( !xrtNetServerLocal(pNetwork, i, &pServer->Locals[i]) ) {
			pCause = xrtErrorRef(xrtGetError());
			goto Failed;
		}
	}
	__xrtSpinLock(&pServer->Lock);
	if ( !xrtAtomic32Load(
		&pServer->NetworkClosed,
		XMEMORY_ACQUIRE
	) ) {
		xrtAtomic32Store(
			&pServer->NetworkPublished,
			1,
			XMEMORY_RELEASE
		);
		bPublished = true;
	}
	__xrtSpinUnlock(&pServer->Lock);
	if ( !bPublished ) {
		pCause = xrtErrorRef((const xerror*)xrtAtomicPtrLoad(
			&pServer->NetworkError,
			XMEMORY_ACQUIRE
		));
		goto Failed;
	}
	return pServer;

Failed:
	xrtAtomic32Store(
		&pServer->State,
		XHTTP_SERVER_ABORTING,
		XMEMORY_RELEASE
	);
	(void)xrtNetServerClose(pNetwork);
	__xrtHttpServerTryFinish(pServer);
	__xrtHttpServerSetError(
		__xrtHttpServerCauseKind(pCause, XERR_CLOSED),
		XHTTP_SERVER_ERROR_LISTEN,
		"start-http-server",
		"HTTP server network closed during startup",
		pCause
	);
	xrtErrorFree(pCause);
	xrtHttpServerDestroy(pServer);
	return NULL;
}



/* 创建并启动明文 HTTP/1 Server。 */
XRT_API xhttpserver* xrtHttpServerStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
)
{
	xhttpserver* pServer = __xrtHttpServerCreate(
		pEngine, pConfig, pEvents
	);

	return pServer != NULL ?
		__xrtHttpServerListen(pServer) : NULL;
}



/* 返回 Server 状态。 */
XRT_API xhttpserverstate xrtHttpServerState(
	const xhttpserver* pServer
)
{
	return pServer != NULL ?
		(xhttpserverstate)xrtAtomic32Load(
			&pServer->State,
			XMEMORY_ACQUIRE
		) : XHTTP_SERVER_CLOSED;
}



/* 返回 Server 是否使用 TLS 传输。 */
XRT_API bool xrtHttpServerSecure(const xhttpserver* pServer)
{
	return (pServer != NULL) && pServer->Secure;
}



/* 返回 Server 逻辑端点数量。 */
XRT_API size_t xrtHttpServerEndpointCount(
	const xhttpserver* pServer
)
{
	return pServer != NULL ? pServer->EndpointCount : 0;
}



/* 复制 Server 指定逻辑端点的实际绑定地址。 */
XRT_API bool xrtHttpServerLocal(
	const xhttpserver* pServer,
	size_t iEndpoint,
	xnetaddr* pAddress
)
{
	if ( (pServer == NULL) ||
		!__xrtHttpServerOutputValid(
			pServer, pAddress, sizeof(*pAddress)
		) ||
		 (iEndpoint >= pServer->EndpointCount) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-server-local",
			"HTTP server, endpoint or address output is invalid",
			NULL
		);
		return false;
	}
	memcpy(
		pAddress,
		&pServer->Locals[iEndpoint],
		sizeof(*pAddress)
	);
	return true;
}



/* 返回 Server 实际底层 Listener 数量。 */
XRT_API size_t xrtHttpServerListenerCount(
	const xhttpserver* pServer
)
{
	return pServer != NULL ? pServer->ListenerCount : 0;
}



/* 增加并返回底层聚合 TCP Server 引用。 */
XRT_API xnetserver* xrtHttpServerNetwork(xhttpserver* pServer)
{
	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-server-network",
			"HTTP server is null",
			NULL
		);
		return NULL;
	}
	return xrtNetServerRef(pServer->Network);
}



/* 返回聚合 TCP Server 的第一个稳定终止错误。 */
XRT_API const xerror* xrtHttpServerError(
	const xhttpserver* pServer
)
{
	return pServer != NULL ?
		(const xerror*)xrtAtomicPtrLoad(
			&pServer->NetworkError,
			XMEMORY_ACQUIRE
		) : NULL;
}



/* 复制 Server 统计快照。 */
XRT_API bool xrtHttpServerStats(
	const xhttpserver* pServer,
	xhttpserverstats* pStats
)
{
	xhttpserverstats Stats;
	uint64 iConnections;
	uint64 iPeak;

	if ( (pServer == NULL) ||
		!__xrtHttpServerOutputValid(
			pServer, pStats, sizeof(Stats)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-server-stats",
			"HTTP server or stats output is null",
			NULL
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	Stats.State = xrtHttpServerState(pServer);
	Stats.Accepted = xrtAtomic64Load(
		&pServer->Accepted, XMEMORY_RELAXED
	);
	Stats.Rejected = xrtAtomic64Load(
		&pServer->Rejected, XMEMORY_RELAXED
	);
	Stats.Requests = xrtAtomic64Load(
		&pServer->Requests, XMEMORY_RELAXED
	);
	Stats.Responses = xrtAtomic64Load(
		&pServer->Responses, XMEMORY_RELAXED
	);
	Stats.Informations = xrtAtomic64Load(
		&pServer->Informations, XMEMORY_RELAXED
	);
	Stats.Upgraded = xrtAtomic64Load(
		&pServer->Upgraded, XMEMORY_RELAXED
	);
	Stats.ProtocolErrors = xrtAtomic64Load(
		&pServer->ProtocolErrors, XMEMORY_RELAXED
	);
	Stats.Timeouts = xrtAtomic64Load(
		&pServer->Timeouts, XMEMORY_RELAXED
	);
	iConnections = xrtAtomic64Load(
		&pServer->Connections, XMEMORY_RELAXED
	);
	iPeak = xrtAtomic64Load(
		&pServer->PeakConnections, XMEMORY_RELAXED
	);
	Stats.Connections = iConnections > SIZE_MAX ?
		SIZE_MAX : (size_t)iConnections;
	Stats.PeakConnections = iPeak > SIZE_MAX ?
		SIZE_MAX : (size_t)iPeak;
	Stats.Endpoints = pServer->EndpointCount;
	Stats.Listeners = pServer->ListenerCount;
	Stats.Secure = pServer->Secure;
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}



/* 请求关闭聚合 TCP Server。 */
static void __xrtHttpServerCloseNetwork(xhttpserver* pServer)
{
	if ( pServer->Network != NULL ) {
		(void)xrtNetServerClose(pServer->Network);
	}
}



/* 每批保留少量待关闭连接，避免持有 Server 锁进入传输层。 */
static size_t __xrtHttpServerSelectConnections(
	xhttpserver* pServer,
	xhttpconn** pConnections,
	size_t iCapacity,
	bool bAbort
)
{
	xhttpconn* pConnection;
	size_t iCount = 0;

	__xrtSpinLock(&pServer->Lock);
	pConnection = pServer->Head;
	while ( (pConnection != NULL) &&
		(iCount < iCapacity) ) {
		bool bSelect;

		if ( bAbort ) {
			bSelect = !pConnection->ServerAbortQueued;
		} else {
			bSelect =
				!pConnection->ServerCloseQueued &&
				!xrtAtomic32Load(
					&pConnection->RequestActive,
					XMEMORY_ACQUIRE
				);
		}
		if ( bSelect ) {
			if ( bAbort ) {
				pConnection->ServerAbortQueued = true;
			} else {
				pConnection->ServerCloseQueued = true;
			}
			pConnections[iCount] =
				xrtHttpConnRef(pConnection);
			if ( pConnections[iCount] != NULL ) {
				iCount++;
			}
		}
		pConnection = pConnection->Next;
	}
	__xrtSpinUnlock(&pServer->Lock);
	return iCount;
}



/* 不持有 Server 锁批量关闭 Connection。 */
static void __xrtHttpServerCloseConnections(
	xhttpserver* pServer,
	bool bAbort
)
{
	xhttpconn* Connections[64];
	size_t iCount;
	size_t i;

	do {
		iCount = __xrtHttpServerSelectConnections(
			pServer,
			Connections,
			sizeof(Connections) / sizeof(Connections[0]),
			bAbort
		);
		for ( i = 0; i < iCount; i++ ) {
			if ( bAbort ) {
				(void)xrtHttpConnAbort(Connections[i]);
			} else {
				(void)xrtHttpConnClose(Connections[i]);
			}
			xrtHttpConnDestroy(Connections[i]);
		}
	} while ( iCount ==
		(sizeof(Connections) / sizeof(Connections[0])) );
}



/* 原子从 RUNNING 进入指定关闭状态。 */
static bool __xrtHttpServerBeginClose(
	xhttpserver* pServer,
	xhttpserverstate State
)
{
	uint32 iExpected = XHTTP_SERVER_RUNNING;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"close-http-server",
			"HTTP server is null",
			NULL
		);
		return false;
	}
	if ( xrtAtomic32CompareExchange(
		&pServer->State,
		&iExpected,
		State,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	return (iExpected == (uint32)State) ||
		(iExpected == XHTTP_SERVER_CLOSED);
}



/* 开始优雅排空。 */
XRT_API bool xrtHttpServerDrain(xhttpserver* pServer)
{
	if ( !__xrtHttpServerBeginClose(
		pServer, XHTTP_SERVER_DRAINING
	) ) {
		return false;
	}
	__xrtHttpServerCloseNetwork(pServer);
	__xrtHttpServerCloseConnections(pServer, false);
	__xrtHttpServerTryFinish(pServer);
	return true;
}



/* 开始异常终止。 */
XRT_API bool xrtHttpServerAbort(xhttpserver* pServer)
{
	uint32 iState;

	if ( pServer == NULL ) {
		return __xrtHttpServerBeginClose(
			pServer, XHTTP_SERVER_ABORTING
		);
	}
	for ( ;; ) {
		uint32 iExpected;

		iState = xrtAtomic32Load(
			&pServer->State,
			XMEMORY_ACQUIRE
		);
		if ( (iState == XHTTP_SERVER_ABORTING) ||
			(iState == XHTTP_SERVER_CLOSED) ) {
			break;
		}
		if ( (iState != XHTTP_SERVER_RUNNING) &&
			(iState != XHTTP_SERVER_DRAINING) ) {
			return false;
		}
		iExpected = iState;
		if ( xrtAtomic32CompareExchange(
			&pServer->State,
			&iExpected,
			XHTTP_SERVER_ABORTING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	__xrtHttpServerCloseNetwork(pServer);
	__xrtHttpServerCloseConnections(pServer, true);
	__xrtHttpServerTryFinish(pServer);
	return true;
}

#endif
