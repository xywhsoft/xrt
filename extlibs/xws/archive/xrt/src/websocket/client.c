#include "../internal/xrt_websocket_http.h"



#if defined(XRT_FEATURE_WEBSOCKET_CLIENT)

typedef struct __xrt_ws_connect {
	xwsconnconfig Connection;
	xwsconnevents Events;
	xwsconnectproc Proc;
	ptr EventData;
	ptr Data;
	char Key[XWS_KEY_CAPACITY];
	xstrview Protocols;
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		xwsdeflate Deflate;
		bool EnableDeflate;
		bool RequireDeflate;
	#endif
	char Storage[];
} __xrt_ws_connect;



#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)

typedef struct __xrt_ws_client_deflate {
	const xwsdeflate* Offer;
	xwsdeflate Response;
	bool Found;
} __xrt_ws_client_deflate;



/* 客户端只接受唯一且确实由本次 offer 允许的 permessage-deflate 响应。 */
static bool __xrtWsClientDeflateVisit(
	const xwsextension* pExtension,
	ptr pData
)
{
	__xrt_ws_client_deflate* pDeflate =
		(__xrt_ws_client_deflate*)pData;

	if ( !xrtWsDeflateIs(pExtension) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket response selected an unsupported extension"
		);
		return false;
	}
	if ( pDeflate->Found ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket response repeats permessage-deflate"
		);
		return false;
	}
	if ( !xrtWsDeflateResponseParse(
		pExtension,
		&pDeflate->Response
	) || !xrtWsDeflateResponseCheck(
		pDeflate->Offer,
		&pDeflate->Response
	) ) {
		__xrtWsHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket permessage-deflate response violates the offer"
		);
		return false;
	}
	pDeflate->Found = true;
	return true;
}



/* 验证扩展响应并把协商参数复制到独立握手快照。 */
static bool __xrtWsClientDeflateCheck(
	const __xrt_ws_fields* pFields,
	const xwsclientconfig* pConfig,
	xwsclienthandshake* pHandshake
)
{
	__xrt_ws_client_deflate Deflate;

	if ( !pConfig->EnableDeflate ) {
		if ( pConfig->RequireDeflate ) {
			__xrtWsHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"Required WebSocket compression is not enabled"
			);
			return false;
		}
		if ( __xrtWsHttpFieldCount(
			pFields,
			XRT_STR_LITERAL(
				"Sec-WebSocket-Extensions"
			)
		) != 0 ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"WebSocket response selected an unoffered extension"
			);
			return false;
		}
		return true;
	}
	memset(&Deflate, 0, sizeof(Deflate));
	Deflate.Offer = &pConfig->Deflate;
	if ( !__xrtWsHttpExtensionsVisit(
		pFields,
		__xrtWsClientDeflateVisit,
		&Deflate
	) ) {
		return false;
	}
	if ( !Deflate.Found ) {
		if ( pConfig->RequireDeflate ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"WebSocket response omitted required compression"
			);
			return false;
		}
		return true;
	}
	pHandshake->Deflate = Deflate.Response;
	pHandshake->DeflateEnabled = true;
	return true;
}

#endif



/* 创建仅把 ws/wss scheme 映射到 HTTP 执行层的基础 GET 请求。 */
XRT_API xhttprequest* xrtWsRequestCreate(
	xstrview Url
)
{
	xurl Parsed;
	xhttprequest* pRequest;
	str sHttpUrl;
	xstrview HttpUrl;
	xstrview Scheme;
	size_t iSize;

	if ( (Url.Size == 0) ||
		!__xrtRangeValid(Url.Data, Url.Size) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL view is empty or invalid"
		);
		return NULL;
	}
	if ( !xrtUrlParse(Url, &Parsed) ) {
		__xrtWsHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL is invalid"
		);
		return NULL;
	}
	if ( xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("http")
	) || xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("https")
	) ) {
		pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("GET"),
			Url
		);
		if ( pRequest == NULL ) {
			__xrtWsHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_ARGUMENT,
				"create-websocket-request",
				"WebSocket URL cannot be used by the HTTP client"
			);
		}
		return pRequest;
	}
	if ( xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("ws")
	) ) {
		Scheme = XRT_STR_LITERAL("http");
	} else if ( xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("wss")
	) ) {
		Scheme = XRT_STR_LITERAL("https");
	} else {
		__xrtWsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL scheme must be ws, wss, http or https"
		);
		return NULL;
	}
	if ( Url.Size >
		(SIZE_MAX - (Scheme.Size - Parsed.Scheme.Size) - 1u) ) {
		__xrtWsHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"create-websocket-request",
			"WebSocket URL is too large"
		);
		return NULL;
	}
	iSize = Url.Size + Scheme.Size - Parsed.Scheme.Size;
	sHttpUrl = (str)xrtMalloc(iSize + 1u);
	if ( sHttpUrl == NULL ) {
		__xrtWsHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"create-websocket-request",
			"WebSocket HTTP URL allocation failed"
		);
		return NULL;
	}
	memcpy(sHttpUrl, Scheme.Data, Scheme.Size);
	memcpy(
		sHttpUrl + Scheme.Size,
		Url.Data + Parsed.Scheme.Size,
		Url.Size - Parsed.Scheme.Size
	);
	sHttpUrl[iSize] = '\0';
	HttpUrl.Data = sHttpUrl;
	HttpUrl.Size = iSize;
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		HttpUrl
	);
	xrtFree(sHttpUrl);
	if ( pRequest == NULL ) {
		__xrtWsHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL cannot be used by the HTTP client"
		);
	}
	return pRequest;
}



/* 在私有请求副本上原子构造 RFC 6455 客户端管理字段。 */
static bool __xrtWsClientRequestConfigure(
	xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
)
{
	const xurl* pUrl;
	char sGenerated[XWS_KEY_CAPACITY];
	xstrview Protocols;
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		char sExtensions[XWS_DEFLATE_MAX_SIZE];
		size_t iExtensions = 0;
	#endif

	if ( (pRequest == NULL) || (pConfig == NULL) ||
		(sKey == NULL) ||
		((pConfig->Protocols.Data == NULL) &&
		 (pConfig->Protocols.Size != 0)) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"configure-websocket-request",
			"WebSocket request, protocols or key output is invalid"
		);
		return false;
	}

	/* WebSocket URI 不允许 fragment，包括显式空 fragment。 */
	pUrl = xrtHttpRequestUrl(pRequest);
	if ( (pUrl == NULL) ||
		((pUrl->Flags & XURL_HAS_FRAGMENT) != 0) ) {
		__xrtWsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"configure-websocket-request",
			"WebSocket URL must not contain a fragment"
		);
		return false;
	}
	Protocols = pConfig->Protocols;
	if ( !xrtWsProtocolsValid(Protocols) ) {
		__xrtWsHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"configure-websocket-request",
			"WebSocket client protocol list is invalid"
		);
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		if ( pConfig->RequireDeflate &&
			!pConfig->EnableDeflate ) {
			__xrtWsHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"configure-websocket-request",
				"Required WebSocket compression is not enabled"
			);
			return false;
		}
		if ( pConfig->EnableDeflate &&
			!xrtWsDeflateOfferWrite(
				&pConfig->Deflate,
				sExtensions,
				sizeof(sExtensions),
				&iExtensions
			) ) {
			__xrtWsHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"configure-websocket-request",
				"WebSocket compression offer is invalid"
			);
			return false;
		}
	#endif
	if ( !__xrtWsHttpTextEqual(
		xrtHttpRequestMethod(pRequest),
		XRT_STR_LITERAL("GET")
	) ) {
		__xrtWsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_METHOD,
			"configure-websocket-request",
			"WebSocket client request method must be GET"
		);
		return false;
	}
	if ( (xrtHttpRequestBody(pRequest) != NULL) ||
		(xrtHttpRequestHeader(
			pRequest,
			XRT_STR_LITERAL("Content-Length")
		) != NULL) || (xrtHttpRequestHeader(
			pRequest,
			XRT_STR_LITERAL("Transfer-Encoding")
		) != NULL) ) {
		__xrtWsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_BODY,
			"configure-websocket-request",
			"WebSocket client request must not carry a body"
		);
		return false;
	}
	if ( xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Extensions")
	) != NULL ) {
		__xrtWsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"configure-websocket-request",
			"WebSocket extensions require an explicit extension adapter"
		);
		return false;
	}
	if ( !xrtWsKeyGenerate(
		sGenerated,
		sizeof(sGenerated)
	) ) {
		return false;
	}
	if ( !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Upgrade"),
		XRT_STR_LITERAL("websocket")
	) || !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	) || !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Version"),
		XRT_STR_LITERAL("13")
	) || !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Key"),
		(xstrview) { sGenerated, XWS_KEY_SIZE }
	) || ((Protocols.Size != 0) &&
		!xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol"),
			Protocols
		))
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		|| (pConfig->EnableDeflate &&
			!xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Extensions"
				),
				(xstrview) {
					sExtensions,
					iExtensions
				}
			))
	#endif
		) {
		__xrtWsHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"configure-websocket-request",
			"WebSocket request fields could not be stored"
		);
		return false;
	}
	if ( Protocols.Size == 0 ) {
		(void)xrtHttpRequestRemoveHeader(
			pRequest,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol")
		);
	}
	memcpy(sKey, sGenerated, sizeof(sGenerated));
	return true;
}



/* 初始化客户端握手与已建立连接的默认配置。 */
XRT_API void xrtWsClientConfigInit(
	xwsclientconfig* pConfig
)
{
	xwsclientconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-init-websocket-client",
			"WebSocket client configuration range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtWsConnConfigInit(&Config.Connection);
	Config.Connection.Role = XWS_ROLE_CLIENT;
	xrtHttpCallOptionsInit(&Config.Http);
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		Config.Http.Redirect =
			XHTTP_REDIRECT_MANUAL;
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 从 ws/wss 或 HTTP URL 创建完整客户端握手请求。 */
XRT_API xhttprequest* xrtWsClientRequestCreate(
	xstrview Url,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
)
{
	xwsclientconfig Config;
	xhttprequest* pRequest;

	if ( !__xrtWsClientConfigSnapshot(
		&Config,
		pConfig,
		"create-websocket-request"
	) || !__xrtWsHttpRangeCheck(
		sKey,
		XWS_KEY_CAPACITY,
		"create-websocket-request",
		"WebSocket key output range is invalid"
	) ) {
		return NULL;
	}
	pRequest = xrtWsRequestCreate(Url);
	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( !__xrtWsClientRequestConfigure(
		pRequest,
		&Config,
		sKey
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* 克隆自定义 GET 请求并替换 WebSocket 管理字段。 */
XRT_API xhttprequest* xrtWsClientRequestClone(
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
)
{
	xwsclientconfig Config;
	xhttprequest* pClone;

	if ( !__xrtWsClientConfigSnapshot(
		&Config,
		pConfig,
		"clone-websocket-request"
	) || !__xrtWsHttpRangeCheck(
		pRequest,
		sizeof(*pRequest),
		"clone-websocket-request",
		"WebSocket source request range is invalid"
	) || !__xrtWsHttpRangeCheck(
		sKey,
		XWS_KEY_CAPACITY,
		"clone-websocket-request",
		"WebSocket key output range is invalid"
	) ) {
		return NULL;
	}
	pClone = xrtHttpRequestClone(pRequest);
	if ( pClone == NULL ) {
		__xrtWsHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"clone-websocket-request",
			"WebSocket source request could not be cloned"
		);
		return NULL;
	}
	if ( !__xrtWsClientRequestConfigure(
		pClone,
		&Config,
		sKey
	) ) {
		xrtHttpRequestDestroy(pClone);
		return NULL;
	}
	return pClone;
}



/* 严格验证服务器 101 响应和客户端 offer 的绑定关系。 */
XRT_API bool xrtWsClientCheck(
	const xhttpresponse* pResponse,
	xstrview Key,
	const xwsclientconfig* pConfig,
	xwsclienthandshake* pHandshake
)
{
	__xrt_ws_fields Fields;
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	const xhttpfield* pAccept;
	const xhttpfield* pProtocol;
	xstrview Selected = { 0 };
	bool bConnection;
	size_t iProtocols;

	if ( !__xrtWsClientConfigSnapshot(
		&Config,
		pConfig,
		"check-websocket-response"
	) || !__xrtWsHttpRangeCheck(
		pResponse,
		sizeof(*pResponse),
		"check-websocket-response",
		"WebSocket response range is invalid"
	) || !__xrtWsHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"check-websocket-response",
		"WebSocket client handshake output range is invalid"
	) ) {
		return false;
	}
	if ( !__xrtRangeValid(Key.Data, Key.Size) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket key range is invalid"
		);
		return false;
	}
	if ( (Config.Protocols.Data == NULL) &&
		(Config.Protocols.Size != 0) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket client protocol range is invalid"
		);
		return false;
	}
	if ( __xrtRangesOverlap(
		pHandshake,
		sizeof(Handshake),
		pResponse,
		sizeof(*pResponse)
	) || ((pConfig != NULL) && __xrtRangesOverlap(
		pHandshake,
		sizeof(Handshake),
		pConfig,
		sizeof(*pConfig)
	)) || __xrtRangesOverlap(
		pHandshake,
		sizeof(Handshake),
		Key.Data,
		Key.Size
	) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket client handshake output overlaps an input"
		);
		return false;
	}
	if ( !xrtWsKeyValid(Key) ||
		!xrtWsProtocolsValid(Config.Protocols) ) {
		__xrtWsHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket response check input is invalid"
		);
		return false;
	}
	memset(&Handshake, 0, sizeof(Handshake));
	Fields.Data = xrtHttpHeadersData(
		xrtHttpResponseHeaders(pResponse)
	);
	Fields.Count = xrtHttpResponseHeaderCount(
		pResponse
	);
	if ( xrtHttpResponseVersion(pResponse) !=
		XHTTP_VERSION_1_1 ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-response",
			"WebSocket response requires HTTP/1.1"
		);
		return false;
	}
	if ( xrtHttpResponseStatus(pResponse) !=
		XHTTP_STATUS_SWITCHING_PROTOCOLS ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_STATUS,
			"check-websocket-response",
			"WebSocket response status is not 101"
		);
		return false;
	}
	if ( !__xrtWsHttpUpgradeExact(&Fields) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_UPGRADE,
			"check-websocket-response",
			"WebSocket Upgrade response field is missing or ambiguous"
		);
		return false;
	}
	if ( !__xrtWsHttpTokenHas(
		&Fields,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade"),
		&bConnection
	) || !bConnection ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_CONNECTION,
			"check-websocket-response",
			"WebSocket response Connection field does not select Upgrade"
		);
		return false;
	}
	pAccept = __xrtWsHttpFieldUnique(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Accept")
	);
	if ( (pAccept == NULL) ||
		!xrtWsAcceptValid(Key, pAccept->Value) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_ACCEPT,
			"check-websocket-response",
			"WebSocket response Accept is missing, repeated or invalid"
		);
		return false;
	}
	if ( (__xrtWsHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Content-Length")
	) != 0) || (__xrtWsHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != 0) || (xrtHttpResponseBodyBytes(
		pResponse
	) != 0) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_BODY,
			"check-websocket-response",
			"WebSocket 101 response contains an HTTP body"
		);
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		if ( !__xrtWsClientDeflateCheck(
			&Fields,
			&Config,
			&Handshake
		) ) {
			return false;
		}
	#else
		if ( __xrtWsHttpFieldCount(
			&Fields,
			XRT_STR_LITERAL(
				"Sec-WebSocket-Extensions"
			)
		) != 0 ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"Unsolicited WebSocket extensions are not accepted"
			);
			return false;
		}
	#endif
	iProtocols = __xrtWsHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Protocol")
	);
	if ( iProtocols > 1 ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"check-websocket-response",
			"WebSocket response repeats the selected protocol"
		);
		return false;
	}
	if ( iProtocols == 1 ) {
		pProtocol = __xrtWsHttpFieldUnique(
			&Fields,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol")
		);
		if ( (pProtocol == NULL) ||
			!xrtHttpTokenValid(pProtocol->Value) ||
			(Config.Protocols.Size == 0) ||
			!xrtWsProtocolsHas(
				Config.Protocols,
				pProtocol->Value
			) ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_PROTOCOL,
				"check-websocket-response",
				"WebSocket response selected an unoffered protocol"
			);
			return false;
		}
		Selected = pProtocol->Value;
	}
	Handshake.Protocol = Selected;
	memcpy(pHandshake, &Handshake, sizeof(Handshake));
	return true;
}



/* 异常关闭并释放 HTTP Call 转移出的升级传输。 */
static void __xrtWsClientTransportAbort(
	xnetstream* pTcp
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		, xtlsstream* pTls
	#endif
)
{
	if ( pTcp != NULL ) {
		(void)xrtNetStreamAbort(pTcp);
		xrtNetStreamDestroy(pTcp);
	}
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTls != NULL ) {
			(void)xrtTlsStreamAbort(pTls);
			xrtTlsStreamDestroy(pTls);
		}
	#endif
}



/* 把 HTTP Client 的升级终态接管为共享 WebSocket Connection。 */
static void __xrtWsClientDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	__xrt_ws_connect* pConnect =
		(__xrt_ws_connect*)pData;
	xwsconn* pConnection = NULL;
	xerror* pOwnedError = NULL;
	const xerror* pError;
	xnetresult Result;
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	bool bTransport;

	Result = pResult->Result;
	pError = pResult->Error;
	if ( Result == XNET_RESULT_OK ) {
		xrtWsClientConfigInit(&Config);
		Config.Protocols = pConnect->Protocols;
		#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
			Config.Deflate = pConnect->Deflate;
			Config.EnableDeflate =
				pConnect->EnableDeflate;
			Config.RequireDeflate =
				pConnect->RequireDeflate;
		#endif
		bTransport = pResult->Upgraded &&
			((pResult->Tcp != NULL)
			#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
				!= (pResult->Tls != NULL)
			#endif
			);
		if ( !xrtWsClientCheck(
			pResult->Response,
			(xstrview) {
				pConnect->Key,
				XWS_KEY_SIZE
			},
			&Config,
			&Handshake
		) ) {
			/* xrtWsClientCheck 已经发布准确原因。 */
		} else if ( !bTransport ) {
			__xrtWsHandshakeError(
				XERR_STATE,
				XWS_HANDSHAKE_ERROR_UPGRADE,
				"attach-websocket-client",
				"HTTP client did not return one upgraded transport"
			);
		} else {
			pConnect->Connection.Protocol =
				Handshake.Protocol;
			#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
				pConnect->Connection.DeflateEnabled =
					Handshake.DeflateEnabled;
				if ( Handshake.DeflateEnabled ) {
					pConnect->Connection.Deflate =
						Handshake.Deflate;
				}
			#endif
			if ( pResult->Tcp != NULL ) {
				pConnection = xrtWsConnAttach(
					pResult->Tcp,
					&pConnect->Connection,
					&pConnect->Events,
					pConnect->EventData
				);
			}
			#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_HTTPS)
				else if ( pResult->Tls != NULL ) {
					pConnection = xrtWsConnAttachTls(
						pResult->Tls,
						&pConnect->Connection,
						&pConnect->Events,
						pConnect->EventData
					);
				}
			#endif
			#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS) && \
				!defined(XRT_FEATURE_WEBSOCKET_CLIENT_HTTPS)
				else {
					__xrtWsHandshakeError(
						XERR_UNSUPPORTED,
						XWS_HANDSHAKE_ERROR_UPGRADE,
						"attach-websocket-client",
						"TLS WebSocket adapter is not present in this build"
					);
				}
			#endif
		}
		if ( pConnection == NULL ) {
			pOwnedError = xrtTakeError();
			__xrtWsClientTransportAbort(
				pResult->Tcp
				#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
					, pResult->Tls
				#endif
			);
			Result = XNET_RESULT_ERROR;
			pError = pOwnedError;
		}
	} else {
		__xrtWsClientTransportAbort(
			pResult->Tcp
			#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
				, pResult->Tls
			#endif
		);
	}
	pConnect->Proc(
		pCall,
		Result,
		pConnection,
		pResult->Response,
		pError,
		pConnect->Data
	);
	xrtErrorFree(pOwnedError);
	xrtFree(pConnect);
}



/* 克隆请求、冻结异步上下文并提交 HTTP Client。 */
XRT_API xhttpcall* xrtWsConnectRequest(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
)
{
	xwsclientconfig Config;
	xwsconnevents Events;
	xhttpcalloptions Options;
	xhttprequest* pPrepared;
	__xrt_ws_connect* pConnect;
	xhttpcall* pCall;
	char sKey[XWS_KEY_CAPACITY];
	size_t iAllocation;

	if ( !__xrtWsClientConfigSnapshot(
		&Config,
		pConfig,
		"connect-websocket-client"
	) || !__xrtWsConnEventsSnapshot(
		&Events,
		pEvents,
		"connect-websocket-client"
	) || !__xrtWsHttpRangeCheck(
		pClient,
		sizeof(*pClient),
		"connect-websocket-client",
		"HTTP client range is invalid"
	) || !__xrtWsHttpRangeCheck(
		pRequest,
		sizeof(*pRequest),
		"connect-websocket-client",
		"HTTP request range is invalid"
	) ) {
		return NULL;
	}
	Config.Connection.Role = XWS_ROLE_CLIENT;
	Config.Connection.Protocol =
		(xstrview) { 0 };
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		Config.Connection.DeflateEnabled =
			Config.EnableDeflate;
		if ( Config.EnableDeflate ) {
			/*
				HTTP 提交前还没有服务端响应。
				用无参数响应验证本地编解码配置，实际方向在握手完成后覆盖。
			*/
			xrtWsDeflateInit(
				&Config.Connection.Deflate
			);
		}
	#elif defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		Config.Connection.DeflateEnabled = false;
	#endif
	if ( (pProc == NULL) ||
		((Config.Protocols.Data == NULL) &&
		 (Config.Protocols.Size != 0)) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"connect-websocket-client",
			"WebSocket client, request, protocols or callback is invalid"
		);
		return NULL;
	}
	if ( !xrtWsConnConfigValid(
		&Config.Connection
	) ) {
		__xrtWsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"connect-websocket-client",
			"WebSocket client connection configuration is invalid"
		);
		return NULL;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		Config.Connection.DeflateEnabled = false;
	#endif
	pPrepared = xrtWsClientRequestClone(
		pRequest,
		&Config,
		sKey
	);
	if ( pPrepared == NULL ) {
		return NULL;
	}
	if ( Config.Protocols.Size >
		(SIZE_MAX - sizeof(*pConnect) - 1u) ) {
		xrtHttpRequestDestroy(pPrepared);
		__xrtWsHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"connect-websocket-client",
			"WebSocket client protocol list is too large"
		);
		return NULL;
	}
	iAllocation = sizeof(*pConnect) +
		Config.Protocols.Size + 1u;
	pConnect = (__xrt_ws_connect*)xrtCalloc(
		1,
		iAllocation
	);
	if ( pConnect == NULL ) {
		xrtHttpRequestDestroy(pPrepared);
		__xrtWsHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"connect-websocket-client",
			"WebSocket client context allocation failed"
		);
		return NULL;
	}
	pConnect->Connection = Config.Connection;
	pConnect->Events = Events;
	pConnect->Proc = pProc;
	pConnect->EventData = pEventData;
	pConnect->Data = pData;
	memcpy(pConnect->Key, sKey, sizeof(sKey));
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		pConnect->Deflate = Config.Deflate;
		pConnect->EnableDeflate =
			Config.EnableDeflate;
		pConnect->RequireDeflate =
			Config.RequireDeflate;
	#endif
	if ( Config.Protocols.Size != 0 ) {
		memcpy(
			pConnect->Storage,
			Config.Protocols.Data,
			Config.Protocols.Size
		);
		pConnect->Protocols.Data =
			pConnect->Storage;
		pConnect->Protocols.Size =
			Config.Protocols.Size;
	}
	Options = Config.Http;
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		Options.Redirect = XHTTP_REDIRECT_MANUAL;
	#endif
	pCall = xrtHttpClientDo(
		pClient,
		pPrepared,
		&Options,
		__xrtWsClientDone,
		pConnect
	);
	xrtHttpRequestDestroy(pPrepared);
	if ( pCall == NULL ) {
		xrtFree(pConnect);
	}
	return pCall;
}



/* 从 URL 创建基础请求并提交异步 WebSocket 握手。 */
XRT_API xhttpcall* xrtWsConnect(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
)
{
	xwsclientconfig Config;
	xwsconnevents Events;
	xhttprequest* pRequest;
	xhttpcall* pCall;

	if ( !__xrtWsClientConfigSnapshot(
		&Config,
		pConfig,
		"connect-websocket-client"
	) || !__xrtWsConnEventsSnapshot(
		&Events,
		pEvents,
		"connect-websocket-client"
	) || !__xrtWsHttpRangeCheck(
		pClient,
		sizeof(*pClient),
		"connect-websocket-client",
		"HTTP client range is invalid"
	) ) {
		return NULL;
	}
	if ( pProc == NULL ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"connect-websocket-client",
			"WebSocket completion callback is null"
		);
		return NULL;
	}
	pRequest = xrtWsRequestCreate(Url);
	if ( pRequest == NULL ) {
		return NULL;
	}
	pCall = xrtWsConnectRequest(
		pClient,
		pRequest,
		&Config,
		&Events,
		pEventData,
		pProc,
		pData
	);
	xrtHttpRequestDestroy(pRequest);
	return pCall;
}

#endif
