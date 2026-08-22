#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_http_server_runtime.h"
#include "../../src/internal/xrt_tls_session.h"



#define TEST_HTTP_SERVER_TLS_OOM_HELD_MAX ((size_t)4096)



/* TLS 组合层故障注入器记录底层分配序号和仍存活的原始块。 */
typedef struct test_http_server_tls_oom_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Denied;
	size_t Live;
} test_http_server_tls_oom_allocator;



/* 在指定底层分配序号精确失败一次。 */
static ptr testHttpServerTlsOomAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_server_tls_oom_allocator* pState =
		(test_http_server_tls_oom_allocator*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Denied++;
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配与普通分配共享故障序号和存活块统计。 */
static ptr testHttpServerTlsOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_tls_oom_allocator* pState =
		(test_http_server_tls_oom_allocator*)pContext;
	bool bNew = pMemory == NULL;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Denied++;
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && bNew ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放成功取得的底层块并维护存活计数。 */
static void testHttpServerTlsOomFree(
	ptr pContext,
	ptr pMemory
)
{
	test_http_server_tls_oom_allocator* pState =
		(test_http_server_tls_oom_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(
		pState->Live != 0,
		"HTTPS OOM allocator live-count underflow"
	);
	pState->Live--;
	free(pMemory);
}



/* 验证失败配置已经恢复成可再次配置的空状态。 */
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



/* 验证组合层 OOM 使用统一错误域并保留底层原因。 */
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



/*
	耗尽目标大小类的全部空闲块。
	返回的块必须在目标调用结束后统一释放。
*/
static size_t testHttpServerTlsOomExhaust(
	test_http_server_tls_oom_allocator* pAllocator,
	size_t iSize,
	ptr* pHeld,
	size_t iCapacity
)
{
	size_t iCount = 0;
	size_t iDenied = pAllocator->Denied;

	pAllocator->FailAt = pAllocator->Calls + 1u;
	while ( iCount < iCapacity ) {
		pHeld[iCount] = xrtMalloc(iSize);
		if ( pHeld[iCount] == NULL ) {
			break;
		}
		iCount++;
	}
	testRequire(
		(iCount < iCapacity) &&
		(pAllocator->Denied == (iDenied + 1u)) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTPS OOM could not exhaust the target size class"
	);
	pAllocator->FailAt = SIZE_MAX;
	xrtClearError();
	return iCount;
}



/* 释放大小类耗尽阶段暂时持有的块。 */
static void testHttpServerTlsOomRelease(
	ptr* pHeld,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		xrtFree(pHeld[i]);
	}
}



/* 记录当前逻辑活动块，用于检查失败路径没有增加存活对象。 */
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



/* 验证指定故障点失败后完整清理并保持相同逻辑活动块。 */
static void testHttpServerTlsOomFailure(
	test_http_server_tls_oom_allocator* pAllocator,
	xhttpserver* pServer,
	const xhttpservertlsconfig* pTls,
	cstr sMessage
)
{
	size_t iBaselineCount;
	size_t iBaselineBytes;
	size_t iLiveCount;
	size_t iLiveBytes;
	size_t iDenied = pAllocator->Denied;

	testHttpServerTlsOomSnapshot(
		&iBaselineCount,
		&iBaselineBytes
	);
	pAllocator->FailAt = pAllocator->Calls + 1u;
	xrtClearError();
	testRequire(
		!__xrtHttpServerTlsSetup(pServer, pTls) &&
		(pAllocator->Denied == (iDenied + 1u)),
		sMessage
	);
	pAllocator->FailAt = SIZE_MAX;
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



/* 验证 HTTPS 配置快照、预验证和监听失败均可完整回滚。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_server_tls_oom_allocator AllocatorState = {
		0, SIZE_MAX, 0, 0
	};
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig TlsConfig;
	xhttpservertlsconfig SessionConfig;
	xnetengine* pEngine;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlssession* pSession;
	xhttpserver* pServer;
	ptr Held[TEST_HTTP_SERVER_TLS_OOM_HELD_MAX];
	size_t iHeld;
	size_t iSessionSize;
	size_t iBaselineCount;
	size_t iBaselineBytes;
	size_t iLiveCount;
	size_t iLiveBytes;
	const xerror* pError;

	Allocator.Context = &AllocatorState;
	Allocator.Alloc = testHttpServerTlsOomAlloc;
	Allocator.Realloc = testHttpServerTlsOomRealloc;
	Allocator.Free = testHttpServerTlsOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTPS OOM allocator install failed"
	);

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
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pContext;
	TlsConfig.Handshake.Identity = pIdentity;
	TlsConfig.Handshake.Protocols = Protocols;
	TlsConfig.Handshake.ProtocolCount =
		sizeof(Protocols) / sizeof(Protocols[0]);
	TlsConfig.Handshake.RequireProtocol = true;
	SessionConfig = TlsConfig;
	SessionConfig.Handshake.Protocols = NULL;
	SessionConfig.Handshake.ProtocolCount = 0;
	SessionConfig.Handshake.RequireProtocol = false;
	pServer = __xrtHttpServerCreate(
		pEngine, &ServerConfig, NULL
	);
	testRequire(
		pServer != NULL,
		"HTTPS OOM fixture Server creation failed"
	);

	/* 从真实 Session 读取精确大小，避免测试绑定当前私有结构布局。 */
	testRequire(
		__xrtHttpServerTlsSetup(pServer, &TlsConfig),
		"HTTPS OOM fixture TLS warm-up failed"
	);
	__xrtHttpServerTlsCleanup(pServer);
	pSession = xrtTlsServerCreate(
		&SessionConfig.Handshake,
		NULL
	);
	testRequire(
		pSession != NULL,
		"HTTPS OOM Session size probe failed"
	);
	iSessionSize = pSession->AllocationSize;
	xrtTlsSessionDestroy(pSession);
	iHeld = testHttpServerTlsOomExhaust(
		&AllocatorState,
		iSessionSize,
		Held,
		sizeof(Held) / sizeof(Held[0])
	);
	testHttpServerTlsOomFailure(
		&AllocatorState,
		pServer,
		&SessionConfig,
		"HTTPS TLS Session OOM contract mismatch"
	);
	testHttpServerTlsOomRelease(Held, iHeld);
	testRequire(
		__xrtHttpServerTlsSetup(pServer, &SessionConfig),
		"HTTPS did not recover after TLS Session OOM"
	);
	__xrtHttpServerTlsCleanup(pServer);

	/* 耗尽 ALPN 大小类后，下一次配置必须在深复制阶段失败。 */
	iHeld = testHttpServerTlsOomExhaust(
		&AllocatorState,
		sizeof(xstrview) +
			(sizeof("http/1.1") - 1u),
		Held,
		sizeof(Held) / sizeof(Held[0])
	);
	testHttpServerTlsOomFailure(
		&AllocatorState,
		pServer,
		&TlsConfig,
		"HTTPS ALPN snapshot OOM contract mismatch"
	);
	testHttpServerTlsOomRelease(Held, iHeld);
	testRequire(
		__xrtHttpServerTlsSetup(pServer, &TlsConfig),
		"HTTPS did not recover after ALPN OOM"
	);
	__xrtHttpServerTlsCleanup(pServer);
	xrtHttpServerDestroy(pServer);

	/* 停止状态 Engine 强制监听失败，验证完整 TLS 快照随 Server 释放。 */
	testHttpServerTlsOomSnapshot(
		&iBaselineCount,
		&iBaselineBytes
	);
	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		&TlsConfig,
		NULL
	);
	testRequire(
		pServer == NULL,
		"HTTPS started on a stopped Engine"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_SERVER_ERROR_LISTEN) &&
		(xrtErrorCause(pError) != NULL) &&
		(xrtErrorKind(xrtErrorCause(pError)) == XERR_CLOSED),
		"HTTPS listen failure error mismatch"
	);
	xrtClearError();
	testHttpServerTlsOomSnapshot(
		&iLiveCount,
		&iLiveBytes
	);
	testRequire(
		(iLiveCount == iBaselineCount) &&
		(iLiveBytes == iBaselineBytes),
		"HTTPS listen failure leaked its TLS snapshot"
	);

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
