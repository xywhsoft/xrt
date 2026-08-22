#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_http_server_runtime.h"



/* 验证失败配置已经恢复为可再次配置的空状态。 */
static void testHttpServerTlsOomClean(
	const xhttpserver* pServer
)
{
	testRequire(
		(pServer != NULL) &&
		!pServer->Secure &&
		(pServer->TlsProtocols == NULL) &&
		(pServer->TlsIdentity == NULL) &&
		(pServer->TlsContext == NULL) &&
		(pServer->Tls.Context == NULL) &&
		(pServer->Tls.Identity == NULL) &&
		(pServer->Tls.Protocols == NULL) &&
		(pServer->Tls.ProtocolCount == 0),
		"HTTPS OOM left a partial TLS configuration"
	);
}



/* 验证组合层保留底层内存错误并建立统一 HTTP Server 错误。 */
static void testHttpServerTlsOomError(cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_MEMORY) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_SERVER_ERROR_CONFIG) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0) &&
		(strcmp(
			xrtErrorOperation(pError),
			"configure-https-server"
		 ) == 0) &&
		(xrtErrorCause(pError) != NULL),
		sMessage
	);
}



/* 记录当前逻辑活动块，检查失败路径没有增加所有权。 */
static void testHttpServerTlsOomSnapshot(
	size_t* pCount,
	size_t* pBytes
)
{
	xmemdebugsnapshot Snapshot;

	xrtMemDebugSnapshot(&Snapshot);
	*pCount = Snapshot.LiveCount;
	*pBytes = Snapshot.LiveBytes;
}



/* 让下一次逻辑分配失败，并验证 TLS 快照完整回滚。 */
static void testHttpServerTlsOomFailure(
	xhttpserver* pServer,
	const xhttpservertlsconfig* pTls,
	cstr sMessage
)
{
	size_t iBaselineCount;
	size_t iBaselineBytes;
	size_t iLiveCount;
	size_t iLiveBytes;

	testHttpServerTlsOomSnapshot(
		&iBaselineCount,
		&iBaselineBytes
	);
	xrtClearError();
	testRequire(
		xrtMemDebugFailAfter(0),
		"HTTPS OOM fault injection failed"
	);
	testRequire(
		!__xrtHttpServerTlsSetup(pServer, pTls) &&
		xrtMemDebugFailTriggered(),
		sMessage
	);
	xrtMemDebugFailClear();
	testHttpServerTlsOomError(sMessage);
	testHttpServerTlsOomClean(pServer);
	xrtClearError();
	testHttpServerTlsOomSnapshot(
		&iLiveCount,
		&iLiveBytes
	);
	testRequire(
		(iLiveCount == iBaselineCount) &&
		(iLiveBytes == iBaselineBytes),
		"HTTPS OOM leaked a logical allocation"
	);
}



/* 覆盖 Context、ALPN 与 Session 三个独立分配阶段的失败恢复。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig ContextConfig;
	xhttpservertlsconfig AlpnConfig;
	xhttpservertlsconfig SessionConfig;
	xnetengine* pEngine;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xhttpserver* pServer;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTPS OOM fixture TLS objects failed"
	);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		pEngine != NULL,
		"HTTPS OOM fixture Engine creation failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	pServer = __xrtHttpServerCreate(
		pEngine,
		&ServerConfig,
		NULL
	);
	testRequire(
		pServer != NULL,
		"HTTPS OOM fixture Server creation failed"
	);

	xrtHttpServerTlsConfigInit(&ContextConfig);
	ContextConfig.Handshake.Identity = pIdentity;
	testHttpServerTlsOomFailure(
		pServer,
		&ContextConfig,
		"HTTPS TLS Context OOM contract mismatch"
	);

	AlpnConfig = ContextConfig;
	AlpnConfig.Handshake.Context = pContext;
	AlpnConfig.Handshake.Protocols = Protocols;
	AlpnConfig.Handshake.ProtocolCount =
		sizeof(Protocols) / sizeof(Protocols[0]);
	AlpnConfig.Handshake.RequireProtocol = true;
	testHttpServerTlsOomFailure(
		pServer,
		&AlpnConfig,
		"HTTPS ALPN snapshot OOM contract mismatch"
	);

	SessionConfig = AlpnConfig;
	SessionConfig.Handshake.Protocols = NULL;
	SessionConfig.Handshake.ProtocolCount = 0;
	SessionConfig.Handshake.RequireProtocol = false;
	testHttpServerTlsOomFailure(
		pServer,
		&SessionConfig,
		"HTTPS TLS Session OOM contract mismatch"
	);
	testRequire(
		__xrtHttpServerTlsSetup(pServer, &AlpnConfig),
		"HTTPS did not recover after TLS configuration OOM"
	);
	__xrtHttpServerTlsCleanup(pServer);
	xrtHttpServerDestroy(pServer);

	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTPS OOM fixture Engine destroy failed"
	);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTPS OOM left a logical allocation"
	);
	printf("[PASS] HTTPS server configuration OOM\n");
	return 0;
}
