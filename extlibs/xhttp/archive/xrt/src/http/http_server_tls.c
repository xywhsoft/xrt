#include "../internal/xrt_http_server_runtime.h"



#if defined(XRT_FEATURE_HTTP_SERVER_TLS)

static const xstrview __xrtHttpServerTlsProtocols[] = {
	XRT_STR_INIT("http/1.1")
};



/* 判断当前错误是否已经由 HTTP Server 层建立。 */
static bool __xrtHttpServerTlsErrorOwned(const xerror* pError)
{
	cstr sDomain;

	if ( pError == NULL ) {
		return false;
	}
	sDomain = xrtErrorDomain(pError);
	return (sDomain != NULL) &&
		(strcmp(sDomain, "xrt.http.server") == 0);
}



/* 判断一个 ALPN 标识是否是当前 Server 支持的 HTTP/1.1。 */
static bool __xrtHttpServerTlsProtocolValid(xstrview Protocol)
{
	static const char sHttp11[] = "http/1.1";

	return (Protocol.Size == (sizeof(sHttp11) - 1u)) &&
		__xrtRangeValid(Protocol.Data, Protocol.Size) &&
		(memcmp(
			Protocol.Data,
			sHttp11,
			sizeof(sHttp11) - 1u
		) == 0);
}



/* 深复制 ALPN 数组和全部协议字节。 */
static bool __xrtHttpServerTlsProtocolsCopy(
	xhttpserver* pServer,
	const xtlsserverconfig* pConfig
)
{
	xstrview* pProtocols;
	xstrview Protocol;
	char* pBytes;

	if ( pConfig->ProtocolCount == 0 ) {
		pServer->Tls.Protocols = NULL;
		pServer->Tls.ProtocolCount = 0;
		return true;
	}
	if ( (pConfig->Protocols == NULL) ||
		(pConfig->ProtocolCount != 1u) ||
		!__xrtRangeValid(
			pConfig->Protocols,
			sizeof(Protocol)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS HTTP/1 server ALPN must be http/1.1",
			NULL
		);
		return false;
	}
	memcpy(&Protocol, pConfig->Protocols, sizeof(Protocol));
	if ( !__xrtHttpServerTlsProtocolValid(Protocol) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS HTTP/1 server ALPN must be http/1.1",
			NULL
		);
		return false;
	}
	pProtocols = (xstrview*)xrtMalloc(
		sizeof(*pProtocols) + Protocol.Size
	);
	if ( pProtocols == NULL ) {
		return false;
	}
	pBytes = (char*)(pProtocols + 1u);
	pProtocols[0].Data = pBytes;
	pProtocols[0].Size = Protocol.Size;
	memcpy(pBytes, Protocol.Data, Protocol.Size);
	pServer->TlsProtocols = pProtocols;
	pServer->Tls.Protocols = pProtocols;
	pServer->Tls.ProtocolCount = 1u;
	return true;
}



/* 初始化 HTTP/1.1 TLS Server 默认配置。 */
XRT_API void xrtHttpServerTlsConfigInit(
	xhttpservertlsconfig* pConfig
)
{
	xhttpservertlsconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"init-https-server-config",
			"HTTPS server TLS config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtTlsServerConfigInit(&Config.Handshake);
	xrtTlsStreamConfigInit(&Config.Stream);
	Config.Handshake.Protocols =
		__xrtHttpServerTlsProtocols;
	Config.Handshake.ProtocolCount =
		sizeof(__xrtHttpServerTlsProtocols) /
		sizeof(__xrtHttpServerTlsProtocols[0]);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 读取可选 TLS 配置，并建立一次对齐且不可变的启动快照。 */
static bool __xrtHttpServerTlsConfigRead(
	const xhttpservertlsconfig* pInput,
	xhttpservertlsconfig* pConfig
)
{
	xrtHttpServerTlsConfigInit(pConfig);
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xrtRangeValid(pInput, sizeof(*pConfig)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"configure-https-server",
			"HTTPS server TLS config range is invalid",
			NULL
		);
		return false;
	}
	memcpy(pConfig, pInput, sizeof(*pConfig));
	return true;
}



/* 释放 Server 保存的 TLS 快照。 */
void __xrtHttpServerTlsCleanup(xhttpserver* pServer)
{
	if ( pServer == NULL ) {
		return;
	}
	xrtFree(pServer->TlsProtocols);
	xrtTlsIdentityRelease(pServer->TlsIdentity);
	xrtTlsContextRelease(pServer->TlsContext);
	memset(&pServer->Tls, 0, sizeof(pServer->Tls));
	memset(&pServer->TlsStream, 0, sizeof(pServer->TlsStream));
	pServer->TlsProtocols = NULL;
	pServer->TlsIdentity = NULL;
	pServer->TlsContext = NULL;
	pServer->Secure = false;
}



/* 保存 TLS 对象、ALPN 和回调配置，并提前验证组合边界。 */
static bool __xrtHttpServerTlsSetupSnapshot(
	xhttpserver* pServer,
	const xhttpservertlsconfig* pConfig
)
{
	const xtlslimits* pLimits;
	xtlssession* pSession;
	xerror* pCause;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"configure-https-server",
			"HTTPS server is null",
			NULL
		);
		return false;
	}
	if ( pServer->Secure ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS server is already configured",
			NULL
		);
		return false;
	}
	pServer->Tls = pConfig->Handshake;
	pServer->TlsStream = pConfig->Stream;
	if ( pConfig->Handshake.Context != NULL ) {
		pServer->TlsContext = xrtTlsContextRetain(
			pConfig->Handshake.Context
		);
	} else {
		pServer->TlsContext = xrtTlsContextCreate(NULL);
	}
	if ( pServer->TlsContext == NULL ) {
		goto Failed;
	}
	pServer->Tls.Context = pServer->TlsContext;
	if ( pConfig->Handshake.Identity != NULL ) {
		pServer->TlsIdentity = xrtTlsIdentityRetain(
			pConfig->Handshake.Identity
		);
		if ( pServer->TlsIdentity == NULL ) {
			goto Failed;
		}
	}
	pServer->Tls.Identity = pServer->TlsIdentity;
	if ( !__xrtHttpServerTlsProtocolsCopy(
		pServer, &pConfig->Handshake
	) ) {
		goto Failed;
	}
	pLimits = xrtTlsContextLimits(pServer->TlsContext);
	if ( (pLimits == NULL) ||
		!__xrtHttpServerNetworkWriteLimit(
			&pServer->Config,
			pLimits->SendLimit
		) ) {
		__xrtHttpServerSetError(
			XERR_RANGE,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS TCP write limit is smaller than the TLS send limit",
			NULL
		);
		goto Failed;
	}
	pSession = xrtTlsServerCreate(&pServer->Tls, NULL);
	if ( pSession == NULL ) {
		pCause = xrtErrorRef(xrtGetError());
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				pCause, XERR_ARGUMENT
			),
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS TLS server configuration is invalid",
			pCause
		);
		xrtErrorFree(pCause);
		goto Failed;
	}
	xrtTlsSessionDestroy(pSession);
	pServer->Secure = true;
	return true;

Failed:
	pCause = xrtErrorRef(xrtGetError());
	__xrtHttpServerTlsCleanup(pServer);
	if ( __xrtHttpServerTlsErrorOwned(pCause) ) {
		xrtSetError(pCause);
	} else {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				pCause, XERR_ARGUMENT
			),
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS server could not save its TLS configuration",
			pCause
		);
	}
	xrtErrorFree(pCause);
	return false;
}



/* 校验并快照配置后，为尚未监听的 Server 建立 TLS 运行时。 */
bool __xrtHttpServerTlsSetup(
	xhttpserver* pServer,
	const xhttpservertlsconfig* pConfig
)
{
	xhttpservertlsconfig Config;

	if ( !__xrtHttpServerTlsConfigRead(pConfig, &Config) ) {
		return false;
	}
	return __xrtHttpServerTlsSetupSnapshot(pServer, &Config);
}



/* 创建并启动 TLS HTTP/1 Server。 */
XRT_API xhttpserver* xrtHttpServerStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	const xhttpserverevents* pEvents
)
{
	xhttpservertlsconfig Tls;
	xhttpserver* pServer;
	xerror* pError;

	if ( !__xrtHttpServerTlsConfigRead(pTls, &Tls) ) {
		return NULL;
	}
	pServer = __xrtHttpServerCreate(
		pEngine, pConfig, pEvents
	);
	if ( pServer == NULL ) {
		return NULL;
	}
	if ( !__xrtHttpServerTlsSetupSnapshot(pServer, &Tls) ) {
		pError = xrtTakeError();
		xrtHttpServerDestroy(pServer);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	return __xrtHttpServerListen(pServer);
}

#endif
