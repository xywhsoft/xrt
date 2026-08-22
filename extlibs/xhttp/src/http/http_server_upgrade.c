#include "../internal/xrt_http_server_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)

/* 释放与 HTTP Connection 传输类型匹配的 Upgrade 引用。 */
static void __xrtHttpUpgradeTransportAbort(
	uint32 iTransportKind,
	ptr pTransport
)
{
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( iTransportKind == XRT_HTTP_CONN_TRANSPORT_TLS ) {
			(void)xrtTlsStreamAbort((xtlsstream*)pTransport);
			xrtTlsStreamDestroy((xtlsstream*)pTransport);
			return;
		}
	#else
		(void)iTransportKind;
	#endif
	(void)xrtNetStreamAbort((xnetstream*)pTransport);
	xrtNetStreamDestroy((xnetstream*)pTransport);
}



/* 异常关闭并清空一个尚未绑定到新协议对象的 Upgrade 传输。 */
XRT_API void xrtHttpUpgradeAbort(xhttpupgrade* pUpgrade)
{
	xhttpupgrade Upgrade;
	xerror* pPrevious;
	xerror* pDiscard;

	if ( pUpgrade == NULL ) {
		return;
	}
	if ( !__xrtRangeValid(pUpgrade, sizeof(Upgrade)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(&Upgrade, pUpgrade, sizeof(Upgrade));
	memset(pUpgrade, 0, sizeof(Upgrade));
	pPrevious = __xrtErrorSwapOwned(NULL);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( Upgrade.Tls != NULL ) {
			(void)xrtTlsStreamAbort(Upgrade.Tls);
			xrtTlsStreamDestroy(Upgrade.Tls);
		}
	#endif
	if ( Upgrade.Tcp != NULL ) {
		(void)xrtNetStreamAbort(Upgrade.Tcp);
		xrtNetStreamDestroy(Upgrade.Tcp);
	}
	pDiscard = __xrtErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
}



/* 检查延迟交付前传输是否仍然处于可接管状态。 */
static xnetresult __xrtHttpUpgradeTransportResult(
	uint32 iTransportKind,
	ptr pTransport,
	xerror** ppError
)
{
	*ppError = NULL;
	if ( pTransport == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	#if !defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		(void)iTransportKind;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( iTransportKind == XRT_HTTP_CONN_TRANSPORT_TLS ) {
			xtlsstream* pTls = (xtlsstream*)pTransport;
			xtlsstreamstate State = xrtTlsStreamState(pTls);

			if ( State == XTLS_STREAM_OPEN ) {
				return XNET_RESULT_OK;
			}
			*ppError = xrtErrorRef(xrtTlsStreamError(pTls));
			return State == XTLS_STREAM_FAILED ?
				XNET_RESULT_ERROR : XNET_RESULT_CLOSED;
		}
	#endif
	if ( xrtNetStreamState((xnetstream*)pTransport) ==
		XNET_STREAM_OPEN ) {
		return XNET_RESULT_OK;
	}
	*ppError = xrtErrorRef(
		xrtNetStreamError((xnetstream*)pTransport)
	);
	return *ppError != NULL ?
		XNET_RESULT_ERROR : XNET_RESULT_CLOSED;
}



/* 在下一次 Worker 调度中发布唯一 Upgrade 终态。 */
static void __xrtHttpConnUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpupgradeproc pProc = pConnection->Upgrade;
	ptr pUserData = pConnection->UpgradeData;
	ptr pTransport = pConnection->UpgradeTransport;
	xhttpserver* pServer = pConnection->Server;
	xerror* pError = pConnection->UpgradeError;
	xnetresult Result = pConnection->UpgradeResult;
	size_t iBuffered = pConnection->UpgradeBuffered;
	xhttpupgrade Upgrade;

	(void)pWorker;
	memset(&Upgrade, 0, sizeof(Upgrade));
	pConnection->Upgrade = NULL;
	pConnection->UpgradeData = NULL;
	pConnection->UpgradeTransport = NULL;
	pConnection->UpgradeError = NULL;
	pConnection->UpgradeBuffered = 0;
	pConnection->UpgradePosted = false;
	if ( Result == XNET_RESULT_OK ) {
		xerror* pTransportError = NULL;

		Result = __xrtHttpUpgradeTransportResult(
			pConnection->TransportKind,
			pTransport,
			&pTransportError
		);
		if ( Result != XNET_RESULT_OK ) {
			xrtErrorFree(pError);
			pError = pTransportError;
			__xrtHttpUpgradeTransportAbort(
				pConnection->TransportKind,
				pTransport
			);
			pTransport = NULL;
		}
	}
	if ( Result == XNET_RESULT_OK ) {
		#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				Upgrade.Tls = (xtlsstream*)pTransport;
			} else
		#endif
		{
			Upgrade.Tcp = (xnetstream*)pTransport;
		}
		Upgrade.Buffered = iBuffered;
	}
	if ( pProc != NULL ) {
		pProc(
			pConnection,
			Result,
			Upgrade,
			pError,
			pUserData
		);
	}
	xrtErrorFree(pError);
	(void)xrtAtomic64FetchSub(
		&pServer->UpgradeCallbacks,
		1,
		XMEMORY_ACQ_REL
	);
	if ( pConnection->Counted ) {
		pConnection->Counted = false;
		__xrtHttpServerConnectionClosed(pServer);
	}
	if ( pConnection->RuntimeHeld ) {
		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
	}
	__xrtHttpServerConnectionFinished(pServer);
}



/* 保存 Upgrade 终态并使用 Connection 内嵌命令延迟回调。 */
static bool __xrtHttpConnUpgradePost(
	xhttpconn* pConnection,
	xnetresult Result,
	ptr pTransport,
	size_t iBuffered,
	const xerror* pError
)
{
	if ( (pConnection == NULL) ||
		(pConnection->Upgrade == NULL) ||
		pConnection->UpgradePosted ) {
		return false;
	}
	pConnection->UpgradeResult = Result;
	pConnection->UpgradeTransport = pTransport;
	pConnection->UpgradeBuffered = iBuffered;
	pConnection->UpgradeError = xrtErrorRef(pError);
	pConnection->UpgradePosted = true;
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->UpgradeCallbacks,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !xrtNetPost(
		pConnection->Worker,
		&pConnection->UpgradePost,
		__xrtHttpConnUpgradeTask,
		pConnection
	) ) {
		/* Connection 回调始终位于所属 Worker；失败时同步完成唯一终态。 */
		xrtClearError();
		__xrtHttpConnUpgradeTask(
			pConnection->Worker,
			pConnection
		);
	}
	return true;
}



/* 为连接关闭前已经受理的 Upgrade 安排唯一失败回调。 */
bool __xrtHttpConnUpgradeFail(
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError
)
{
	return __xrtHttpConnUpgradePost(
		pConnection,
		Result,
		NULL,
		0,
		pError
	);
}



/* 摘除 HTTP 事件并取得 Connection 唯一传输引用。 */
static ptr __xrtHttpConnUpgradeTransport(
	xhttpconn* pConnection,
	size_t* pBuffered
)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	*pBuffered = 0;
	if ( pTransport == NULL ) {
		return NULL;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			xtlsstream* pTls = (xtlsstream*)pTransport;

			*pBuffered = xrtTlsStreamAvailable(pTls);
			if ( !xrtTlsStreamSetEvents(
				pTls, NULL, NULL
			) ) {
				return NULL;
			}
		} else
	#endif
	{
		xnetstream* pTcp = (xnetstream*)pTransport;

		*pBuffered = xrtNetStreamAvailable(pTcp);
		if ( !xrtNetStreamSetEvents(
			pTcp, NULL, NULL
		) ) {
			return NULL;
		}
	}
	__xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xrtSpinUnlock(&pConnection->TransportLock);
	return pTransport;
}



/* 释放已经离开 HTTP 的请求、响应与排队信息响应。 */
static void __xrtHttpConnUpgradeReleaseProtocol(
	xhttpconn* pConnection
)
{
	__xrt_http_response_queue* pQueued;

	__xrtHttpConnCancelTimer(pConnection);
	pConnection->WriteDeadline = 0;
	xrtHttp1ServerResponseDestroy(pConnection->Response);
	pConnection->Response = NULL;
	while ( pConnection->ResponseHead != NULL ) {
		pQueued = pConnection->ResponseHead;
		pConnection->ResponseHead = pQueued->Next;
		xrtHttp1ServerResponseDestroy(pQueued->Response);
		xrtFree(pQueued);
	}
	pConnection->ResponseTail = NULL;
	xrtHttp1ServerExchangeDestroy(pConnection->Exchange);
	pConnection->Exchange = NULL;
	xrtAtomic32Store(
		&pConnection->RequestActive,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_UPGRADED,
		XMEMORY_RELEASE
	);
	__xrtHttpServerRemoveConnection(
		pConnection->Server,
		pConnection
	);
}



/* 完成 Tunnel 输出，并在下一次 Worker 调度中交付传输。 */
void __xrtHttpConnUpgradeFinish(xhttpconn* pConnection)
{
	uint32 iExpected = XRT_HTTP_CONN_GATE_OPEN;
	size_t iBuffered = 0;
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	if ( pConnection->Upgrade == NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_UPGRADE,
			"finish-http-upgrade",
			"HTTP tunnel has no registered Upgrade owner",
			NULL
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	/*
	 * 关闭和 Upgrade 只能有一方取得传输所有权；成功后任意线程的
	 * Close 都不能越过交付窗口误报成功。
	 */
	if ( !xrtAtomic32CompareExchange(
		&pConnection->CloseGate,
		&iExpected,
		XRT_HTTP_CONN_GATE_UPGRADED,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	pTransport = __xrtHttpConnUpgradeTransport(
		pConnection,
		&iBuffered
	);
	if ( pTransport == NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_UPGRADE,
			"finish-http-upgrade",
			"HTTP transport could not leave its event owner",
			xrtGetError()
		);
		__xrtHttpConnEmitError(pConnection);
		xrtAtomic32Store(
			&pConnection->CloseGate,
			XRT_HTTP_CONN_GATE_CLOSING,
			XMEMORY_RELEASE
		);
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->Responses,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->Upgraded,
		1,
		XMEMORY_RELAXED
	);
	(void)__xrtHttpConnUpgradePost(
		pConnection,
		XNET_RESULT_OK,
		pTransport,
		iBuffered,
		NULL
	);
	__xrtHttpConnUpgradeReleaseProtocol(pConnection);
}



/* 检查并登记唯一 Upgrade 完成过程。 */
static bool __xrtHttpConnUpgradeRegister(
	xhttpconn* pConnection,
	xhttpupgradeproc pProc,
	ptr pData,
	cstr sOperation
)
{
	if ( (pConnection == NULL) || (pProc == NULL) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			sOperation,
			"HTTP Upgrade connection or completion callback is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnCanRespond(
		pConnection,
		sOperation,
		true
	) ) {
		return false;
	}
	if ( !xrtHttp1ServerExchangeComplete(
		pConnection->Exchange
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP Upgrade requires a complete request",
			NULL
		);
		return false;
	}
	if ( (pConnection->Upgrade != NULL) ||
		xrtAtomic32Load(
			&pConnection->FinalGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection already committed its final owner",
			NULL
		);
		return false;
	}
	pConnection->Upgrade = pProc;
	pConnection->UpgradeData = pData;
	return true;
}



/* 清除尚未由响应门受理的 Upgrade 回调。 */
static void __xrtHttpConnUpgradeUnregister(
	xhttpconn* pConnection
)
{
	if ( (pConnection == NULL) ||
		pConnection->UpgradePosted ) {
		return;
	}
	pConnection->Upgrade = NULL;
	pConnection->UpgradeData = NULL;
}



/* 提交拥有型 Tunnel 响应计划并登记传输接管过程。 */
XRT_API xnetresult xrtHttpConnUpgradeResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	xhttpupgradeproc pProc,
	ptr pData
)
{
	xnetresult Result;

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"upgrade-http-connection",
			"HTTP Upgrade response plan is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !xrtHttp1ServerResponseTunnel(pResponse) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		__xrtHttpServerSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-http-connection",
			"HTTP Upgrade requires a Tunnel response plan",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtHttpConnUpgradeRegister(
		pConnection,
		pProc,
		pData,
		"upgrade-http-connection"
	) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		return XNET_RESULT_ERROR;
	}
	Result = __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		"upgrade-http-connection",
		true
	);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpConnUpgradeUnregister(pConnection);
	}
	return Result;
}



/* 冻结 Reply 并提交常用 HTTP Upgrade。 */
XRT_API xnetresult xrtHttpConnUpgrade(
	xhttpconn* pConnection,
	const xhttpreply* pReply,
	xhttpupgradeproc pProc,
	ptr pData
)
{
	const xhttpserverrequest* pRequest;
	xhttp1serverresponse* pResponse;

	if ( (pConnection == NULL) || (pReply == NULL) ||
		(pProc == NULL) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"upgrade-http-connection",
			"HTTP Upgrade connection, Reply or callback is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pRequest = xrtHttpConnRequest(pConnection);
	if ( pRequest == NULL ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"upgrade-http-connection",
			"HTTP Upgrade requires an active request",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest,
		pReply
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-http-connection",
			"HTTP Upgrade Reply could not form a Tunnel response",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return xrtHttpConnUpgradeResponse(
		pConnection,
		pResponse,
		pProc,
		pData
	);
}



/* 复制完整切换响应，并用公共线缆计划工厂提交 Upgrade。 */
XRT_API xnetresult xrtHttpConnUpgradeRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	xhttpupgradeproc pProc,
	ptr pData
)
{
	const xhttpserverrequest* pRequest;
	xhttp1serverresponse* pResponse;
	xhttpbody* pBody;

	if ( (pConnection == NULL) || (pProc == NULL) ||
		(Response.Size == 0) ||
		!__xrtRangeValid(Response.Data, Response.Size) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade requires a connection, complete byte range and callback",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pRequest = xrtHttpConnRequest(pConnection);
	if ( (pRequest == NULL) ||
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_UPGRADE) == 0) ) {
		__xrtHttpServerSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade requires an accepted Upgrade request",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pBody = xrtHttpBodyCopy(Response);
	if ( pBody == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade bytes could not be copied",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnWireResponse(
		pBody,
		(uint64)Response.Size,
		false,
		true
	);
	xrtHttpBodyDestroy(pBody);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade response plan could not be created",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return xrtHttpConnUpgradeResponse(
		pConnection,
		pResponse,
		pProc,
		pData
	);
}

#endif
