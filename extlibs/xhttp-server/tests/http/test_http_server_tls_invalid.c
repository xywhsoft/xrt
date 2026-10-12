#include "../fixtures/tls_server.h"



/* 验证当前线程保存的是指定 HTTP Server 错误。 */
static void testHttpServerTlsErrorCode(
	xhttpservererror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
	xrtClearError();
}



/* 验证 HTTPS 在监听前拒绝缺失身份、错误 ALPN 和不兼容写上限。 */
int main(void)
{
	static const xstrview InvalidProtocols[] = {
		XRT_STR_INIT("h2")
	};
	uint8 TlsStorage[sizeof(xhttpservertlsconfig) + 2u];
	uint8 ProtocolStorage[sizeof(xstrview) + 2u];
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig TlsConfig;
	xnetengine* pEngine;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	const xtlslimits* pLimits;
	xhttpserver* pServer;

	xrtHttpServerTlsConfigInit(NULL);
	testHttpServerTlsErrorCode(
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTPS null config error mismatch"
	);
	testRequire(
		xrtHttpConnTls(NULL) == NULL,
		"HTTPS null Connection TLS query mismatch"
	);
	memset(TlsStorage, 0xA5, sizeof(TlsStorage));
	xrtHttpServerTlsConfigInit(
		(xhttpservertlsconfig*)(void*)(TlsStorage + 1u)
	);
	memcpy(&TlsConfig, TlsStorage + 1u, sizeof(TlsConfig));
	testRequire(
		(TlsStorage[0] == UINT8_C(0xA5)) &&
		(TlsStorage[sizeof(TlsStorage) - 1u] == UINT8_C(0xA5)) &&
		(TlsConfig.Handshake.ProtocolCount == 1u) &&
		(TlsConfig.Handshake.Protocols != NULL),
		"HTTPS TLS config init did not support unaligned storage"
	);
	xrtHttpServerTlsConfigInit(
		(xhttpservertlsconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testHttpServerTlsErrorCode(
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTPS wrapping config init error mismatch"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		pEngine != NULL,
		"HTTPS invalid fixture Engine creation failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	xrtHttpServerTlsConfigInit(&TlsConfig);
	memcpy(TlsStorage + 1u, &TlsConfig, sizeof(TlsConfig));
	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		(const xhttpservertlsconfig*)(const void*)(
			TlsStorage + 1u
		),
		NULL
	);
	testRequire(
		pServer == NULL,
		"HTTPS accepted a missing identity"
	);
	testHttpServerTlsErrorCode(
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTPS missing identity error mismatch"
	);

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTPS invalid fixture TLS objects failed"
	);
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pContext;
	TlsConfig.Handshake.Identity = pIdentity;
	memset(ProtocolStorage, 0xA5, sizeof(ProtocolStorage));
	memcpy(
		ProtocolStorage + 1u,
		InvalidProtocols,
		sizeof(InvalidProtocols[0])
	);
	TlsConfig.Handshake.Protocols =
		(const xstrview*)(const void*)(ProtocolStorage + 1u);
	TlsConfig.Handshake.ProtocolCount =
		sizeof(InvalidProtocols) /
		sizeof(InvalidProtocols[0]);
	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		&TlsConfig,
		NULL
	);
	testRequire(
		pServer == NULL,
		"HTTPS accepted a non-HTTP/1.1 ALPN"
	);
	testHttpServerTlsErrorCode(
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTPS invalid ALPN error mismatch"
	);
	testRequire(
		(ProtocolStorage[0] == UINT8_C(0xA5)) &&
		(ProtocolStorage[sizeof(ProtocolStorage) - 1u] ==
			UINT8_C(0xA5)),
		"HTTPS ALPN snapshot changed descriptor guards"
	);

	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		(const xhttpservertlsconfig*)(uintptr_t)(
			UINTPTR_MAX - 1u
		),
		NULL
	);
	testRequire(
		pServer == NULL,
		"HTTPS accepted a wrapping TLS config range"
	);
	testHttpServerTlsErrorCode(
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTPS wrapping TLS config error mismatch"
	);

	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pContext;
	TlsConfig.Handshake.Identity = pIdentity;
	TlsConfig.Handshake.Protocols =
		(const xstrview*)(uintptr_t)(UINTPTR_MAX - 1u);
	TlsConfig.Handshake.ProtocolCount = 1u;
	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		&TlsConfig,
		NULL
	);
	testRequire(
		pServer == NULL,
		"HTTPS accepted a wrapping ALPN descriptor range"
	);
	testHttpServerTlsErrorCode(
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTPS wrapping ALPN descriptor error mismatch"
	);

	{
		xstrview Protocol = {
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
			8u
		};

		xrtHttpServerTlsConfigInit(&TlsConfig);
		TlsConfig.Handshake.Context = pContext;
		TlsConfig.Handshake.Identity = pIdentity;
		TlsConfig.Handshake.Protocols = &Protocol;
		TlsConfig.Handshake.ProtocolCount = 1u;
		pServer = xrtHttpServerStartTls(
			pEngine,
			&ServerConfig,
			&TlsConfig,
			NULL
		);
		testRequire(
			pServer == NULL,
			"HTTPS accepted a wrapping ALPN byte range"
		);
		testHttpServerTlsErrorCode(
			XHTTP_SERVER_ERROR_CONFIG,
			"HTTPS wrapping ALPN byte error mismatch"
		);
	}

	pLimits = xrtTlsContextLimits(pContext);
	testRequire(
		(pLimits != NULL) && (pLimits->SendLimit > 2u),
		"HTTPS TLS send limit query failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	ServerConfig.Network.Listen.Stream.WriteLimit =
		pLimits->SendLimit - 1u;
	ServerConfig.Network.Listen.Stream.WriteHighWater =
		pLimits->SendLimit - 2u;
	if ( ServerConfig.Network.Listen.Stream.WriteLowWater >
		ServerConfig.Network.Listen.Stream.WriteHighWater ) {
		ServerConfig.Network.Listen.Stream.WriteLowWater =
			ServerConfig.Network.Listen.Stream.WriteHighWater;
	}
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pContext;
	TlsConfig.Handshake.Identity = pIdentity;
	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		&TlsConfig,
		NULL
	);
	testRequire(
		pServer == NULL,
		"HTTPS accepted a TCP limit below the TLS send limit"
	);
	testHttpServerTlsErrorCode(
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTPS incompatible write limit error mismatch"
	);

	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTPS invalid fixture Engine destroy failed"
	);
	printf("[PASS] HTTPS server invalid configuration\n");
	return 0;
}
