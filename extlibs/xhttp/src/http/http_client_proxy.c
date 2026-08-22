#include "../internal/xrt_http_client_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 设置代理策略入口的客户端域错误，并保留底层代理或内存根因。 */
static void __xrtHttpProxySetError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 从完整原因链中提取对调用方最有价值的代理失败类别。 */
static xerrkind __xrtHttpProxyCauseKind(
	const xerror* pCause,
	xnetresult Result
)
{
	if ( Result == XNET_RESULT_CANCELLED ) {
		return XERR_CANCELLED;
	}
	if ( xrtErrorIs(pCause, XERR_MEMORY) != NULL ) {
		return XERR_MEMORY;
	}
	if ( xrtErrorIs(pCause, XERR_TIMEOUT) != NULL ) {
		return XERR_TIMEOUT;
	}
	if ( xrtErrorIs(pCause, XERR_PERMISSION) != NULL ) {
		return XERR_PERMISSION;
	}
	if ( xrtErrorIs(pCause, XERR_PROTOCOL) != NULL ) {
		return XERR_PROTOCOL;
	}
	if ( xrtErrorIs(pCause, XERR_UNSUPPORTED) != NULL ) {
		return XERR_UNSUPPORTED;
	}
	return XERR_IO;
}



/* 初始化为继承 Client 默认代理。 */
XRT_API void xrtHttpProxyOptionsInit(
	xhttpproxyoptions* pOptions
)
{
	const xhttpproxyoptions Options = {
		XHTTP_PROXY_DEFAULT,
		NULL
	};

	if ( !__xrtRangeValid(pOptions, sizeof(Options)) ) {
		__xrtHttpProxySetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-proxy-options",
			"HTTP proxy options storage is invalid",
			NULL
		);
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 冻结本次调用的代理选择并取得独立引用。 */
bool __xrtHttpProxyInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	const xnetproxy* pProxy = NULL;

	if ( (pCall == NULL) || (pOptions == NULL) ) {
		__xrtHttpProxySetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"prepare-http-proxy",
			"HTTP call or proxy options are null",
			NULL
		);
		return false;
	}
	switch ( pOptions->Proxy.Mode ) {
		case XHTTP_PROXY_DEFAULT:
			if ( pOptions->Proxy.Proxy != NULL ) {
				__xrtHttpProxySetError(
					XERR_ARGUMENT,
					XHTTP_CLIENT_ERROR_CONFIG,
					"prepare-http-proxy",
					"DEFAULT proxy mode does not accept an explicit proxy",
					NULL
				);
				return false;
			}
			pProxy = pCall->Client->Proxy;
			break;
		case XHTTP_PROXY_DIRECT:
			if ( pOptions->Proxy.Proxy != NULL ) {
				__xrtHttpProxySetError(
					XERR_ARGUMENT,
					XHTTP_CLIENT_ERROR_CONFIG,
					"prepare-http-proxy",
					"DIRECT proxy mode requires a null proxy",
					NULL
				);
				return false;
			}
			break;
		case XHTTP_PROXY_EXPLICIT:
			if ( pOptions->Proxy.Proxy == NULL ) {
				__xrtHttpProxySetError(
					XERR_ARGUMENT,
					XHTTP_CLIENT_ERROR_CONFIG,
					"prepare-http-proxy",
					"EXPLICIT proxy mode requires a proxy",
					NULL
				);
				return false;
			}
			pProxy = pOptions->Proxy.Proxy;
			break;
		default:
			__xrtHttpProxySetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"prepare-http-proxy",
				"HTTP proxy mode is invalid",
				NULL
			);
			return false;
	}
	if ( pProxy != NULL ) {
		pCall->Proxy = xrtNetProxyRetain(pProxy);
		if ( pCall->Proxy == NULL ) {
			xerror* pCause = xrtTakeError();

			__xrtHttpProxySetError(
				xrtErrorIs(pCause, XERR_MEMORY) != NULL ?
					XERR_MEMORY : XERR_STATE,
				XHTTP_CLIENT_ERROR_PROXY,
				"prepare-http-proxy",
				"HTTP call could not retain its proxy",
				pCause
			);
			xrtErrorFree(pCause);
			return false;
		}
	}
	return true;
}



/* 从 Call 摘除已经完成的 Proxy Dial 调用方引用。 */
static void __xrtHttpProxyDialDetach(
	xhttpcall* pCall,
	xnetproxydial* pDial
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->ProxyDial == pDial ) {
		pCall->ProxyDial = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 从 Call 摘除代理后的 TLS 握手引用，并记录同步回调窗口。 */
static void __xrtHttpProxyTlsDetach(
	xhttpcall* pCall,
	xtlsstream* pStream
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->ProxyTls == pStream ) {
		pCall->ProxyTls = NULL;
	}
	if ( pCall->ProxyTlsStarting ) {
		pCall->ProxyTlsCallback = true;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* TLS 握手完成后沿用直连 HTTPS 的 ALPN、取消和池接管路径。 */
static void __xrtHttpProxyTlsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	__xrtHttpProxyTlsDetach(pCall, pStream);
	__xrtHttpCallTlsConnected(
		pCall,
		pStream,
		"dial-https-proxy"
	);
}



/* 代理隧道中的 TLS 在 Open 前关闭时发布唯一失败终态。 */
static void __xrtHttpProxyTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	bool bCancelled = (Result == XNET_RESULT_CANCELLED) ||
		(xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) != 0);
	xnetresult FailureResult = bCancelled ?
		XNET_RESULT_CANCELLED :
		(Result == XNET_RESULT_OK ?
			XNET_RESULT_ERROR : Result);

	__xrtHttpProxyTlsDetach(pCall, pStream);
	__xrtHttpCallFail(
		pCall,
		FailureResult,
		bCancelled ?
			XHTTP_CLIENT_ERROR_CANCELLED :
			XHTTP_CLIENT_ERROR_TLS,
		__xrtHttpProxyCauseKind(
			pError,
			FailureResult
		),
		"dial-https-proxy",
		bCancelled ?
			"HTTPS proxy call was cancelled during TLS handshake" :
			"TLS handshake inside the HTTP proxy tunnel failed",
		pError
	);
	xrtTlsStreamDestroy(pStream);
}



/* 代理后的 TLS Stream 只在握手阶段使用这组事件。 */
static const xtlsstreamevents __xrtHttpProxyTlsEvents = {
	.Open = __xrtHttpProxyTlsOpen,
	.Read = NULL,
	.End = NULL,
	.Writable = NULL,
	.Drain = NULL,
	.Close = __xrtHttpProxyTlsClose,
	.Ticket = NULL
};



/* 在已经建立的代理隧道内启动目标 TLS 客户端。 */
static bool __xrtHttpProxyStartTls(
	xhttpcall* pCall,
	xnetstream* pTransport
)
{
	xtlsclientconfig Tls;
	xtlsstream* pTls = NULL;
	bool bCallback;
	bool bStarted;

	/* TLS Attach 可以同步发布终态，短暂引用保护返回后的状态收口。 */
	if ( xrtHttpCallRef(pCall) == NULL ) {
		return false;
	}
	__xrtHttpCallTlsConfig(pCall, &Tls);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_TLS
	);
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_HANDSHAKING,
		XMEMORY_RELEASE
	);
	(void)xrtSpinLock(&pCall->Lock);
	pCall->ProxyTlsStarting = true;
	pCall->ProxyTlsCallback = false;
	(void)xrtSpinUnlock(&pCall->Lock);
	bStarted = xrtTlsStreamClient(
		pTransport,
		&Tls,
		&pCall->Client->Config.TlsStream,
		&__xrtHttpProxyTlsEvents,
		pCall,
		&pTls
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xrtTlsResumeRelease((xtlsresume*)Tls.Resume);
	#endif
	if ( !bStarted ) {
		(void)xrtSpinLock(&pCall->Lock);
		pCall->ProxyTlsStarting = false;
		pCall->ProxyTlsCallback = false;
		(void)xrtSpinUnlock(&pCall->Lock);
		xrtHttpCallDestroy(pCall);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->ProxyTlsStarting = false;
	bCallback = pCall->ProxyTlsCallback;
	pCall->ProxyTlsCallback = false;
	if ( !bCallback ) {
		pCall->ProxyTls = pTls;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( !bCallback && xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtTlsStreamAbort(pTls);
	}
	xrtHttpCallDestroy(pCall);
	return true;
}

#endif



/* Proxy Dial 完成后进入普通 HTTP 或代理隧道内的 TLS。 */
static void __xrtHttpProxyDialDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xerror* pCause;
	#endif

	__xrtHttpProxyDialDetach(pCall, pDial);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			Result,
			Result == XNET_RESULT_CANCELLED ?
				XHTTP_CLIENT_ERROR_CANCELLED :
				XHTTP_CLIENT_ERROR_PROXY,
			__xrtHttpProxyCauseKind(pError, Result),
			"dial-http-proxy",
			Result == XNET_RESULT_CANCELLED ?
				"HTTP proxy dial was cancelled" :
				"HTTP proxy tunnel could not be established",
			pError
		);
		xrtNetProxyDialDestroy(pDial);
		return;
	}
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetStreamAbort(pStream);
		xrtNetStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"dial-http-proxy",
			"HTTP call was cancelled after the proxy tunnel opened",
			NULL
		);
		xrtNetProxyDialDestroy(pDial);
		return;
	}
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			if ( !__xrtHttpProxyStartTls(
				pCall,
				pStream
			) ) {
				pCause = xrtTakeError();
				(void)xrtNetStreamAbort(pStream);
				xrtNetStreamDestroy(pStream);
				__xrtHttpCallFail(
					pCall,
					XNET_RESULT_ERROR,
					XHTTP_CLIENT_ERROR_TLS,
					xrtErrorIs(
						pCause,
						XERR_MEMORY
					) != NULL ?
						XERR_MEMORY : XERR_IO,
					"dial-https-proxy",
					"TLS could not start inside the HTTP proxy tunnel",
					pCause
				);
				xrtErrorFree(pCause);
			}
		#else
			(void)xrtNetStreamAbort(pStream);
			xrtNetStreamDestroy(pStream);
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_TLS,
				XERR_UNSUPPORTED,
				"dial-https-proxy",
				"HTTPS support is not present in this build",
				NULL
			);
		#endif
	} else {
		__xrtHttpCallTcpConnected(
			pCall,
			pStream,
			"dial-http-proxy"
		);
	}
	xrtNetProxyDialDestroy(pDial);
}



/* 连接代理端点并为当前目标建立 CONNECT 或 SOCKS5 隧道。 */
bool __xrtHttpCallStartProxy(xhttpcall* pCall)
{
	xnetproxydialconfig Config;
	xnetstreamevents Events;
	xnetproxydial* pDial;

	xrtNetProxyDialConfigInit(&Config);
	Config.Transport = pCall->Client->Config.Dial;
	Config.Transport.Affinity = pCall->Affinity;
	Config.Timeout = 0;
	if ( Config.ReceiveLimit >
		Config.Transport.Stream.ReadLimit ) {
		Config.ReceiveLimit =
			Config.Transport.Stream.ReadLimit;
	}
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_DIALING,
		XMEMORY_RELEASE
	);
	pDial = xrtNetProxyDial(
		pCall->Client->Engine,
		pCall->Client->Resolver,
		pCall->Proxy,
		pCall->Host,
		pCall->Port,
		&Config,
		&Events,
		pCall,
		__xrtHttpProxyDialDone,
		pCall
	);
	if ( pDial == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->ProxyDial = pDial;
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetProxyDialCancel(pDial);
	}
	return true;
}



#endif
