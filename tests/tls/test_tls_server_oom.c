#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_tls_server.h"



#define TEST_TLS_SERVER_OOM_TICKET_SIZE 2048u



/* 大对象故障注入器记录底层调用和仍存活的原始分配。 */
typedef struct test_tls_server_oom_alloc {
	size_t Calls;
	size_t FailAt;
	size_t Denied;
	size_t Live;
} test_tls_server_oom_alloc;



/* 在指定底层分配调用注入一次失败。 */
static ptr testTlsServerOomAlloc(ptr pContext, size_t iSize)
{
	test_tls_server_oom_alloc* pState =
		(test_tls_server_oom_alloc*)pContext;
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



/* 重分配和普通分配共享故障序号与存活块统计。 */
static ptr testTlsServerOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_server_oom_alloc* pState =
		(test_tls_server_oom_alloc*)pContext;
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



/* 释放成功取得的底层内存并维护存活块计数。 */
static void testTlsServerOomFree(ptr pContext, ptr pMemory)
{
	test_tls_server_oom_alloc* pState =
		(test_tls_server_oom_alloc*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"TLS server OOM allocator live-count underflow");
	pState->Live--;
	free(pMemory);
}



/* 堆的空闲 slab 可以保留；此测试模块启用 memory_debug 来比较逻辑活动块。 */
static void testTlsServerOomLive(
	const test_tls_server_oom_alloc* pState,
	size_t* pCount,
	size_t* pBytes
)
{
	xmemdebugsnapshot Snapshot;

	(void)pState;
	xrtMemDebugSnapshot(&Snapshot);
	*pCount = Snapshot.LiveCount;
	*pBytes = Snapshot.LiveBytes;
}



/* 在恢复对象、消息和发送块三个提交前分配点逐一注入 OOM。 */
static void testTlsServerTicketOom(
	test_tls_server_oom_alloc* pAllocator,
	xtlssession* pServer,
	xtlsserverstate* pState
)
{
	uint8 Ticket[TEST_TLS_SERVER_OOM_TICKET_SIZE];
	uint8 Traffic[XTLS_SERVER_SECRET_MAX_SIZE];

	memset(Ticket, 0x5Cu, sizeof(Ticket));
	for ( size_t i = 1u; i <= 3u; i++ ) {
		xtlsresume* pResume = (xtlsresume*)(uintptr_t)1u;
		size_t iBaselineCount;
		size_t iBaselineBytes;
		size_t iLiveCount;
		size_t iLiveBytes;
		uint64 iSequence = pServer->WriteKey.Sequence;
		size_t iDenied = pAllocator->Denied;
		xtlsresult Result;

		testTlsServerOomLive(
			pAllocator,
			&iBaselineCount,
			&iBaselineBytes
		);
		memcpy(
			Traffic, pState->ServerApplicationTraffic,
			pState->HashSize
		);
		pAllocator->FailAt = pAllocator->Calls + i;
		xrtClearError();
		Result = xrtTlsServerTicket(
			pServer, (xbytesview) { Ticket, sizeof(Ticket) },
			60u, &pResume
		);
		testRequire((Result == XTLS_ERROR) && (pResume == NULL) &&
			(pAllocator->Denied == iDenied + 1u) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_READY) &&
			(xrtTlsSessionSendSize(pServer) == 0) &&
			(pServer->WriteKey.Sequence == iSequence) &&
			(memcmp(
				pState->ServerApplicationTraffic,
				Traffic, pState->HashSize
			) == 0) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
			(xrtErrorCause(xrtGetError()) != NULL),
			"TLS server ticket OOM changed committed session state");

		pAllocator->FailAt = SIZE_MAX;
		xrtClearError();
		testTlsServerOomLive(
			pAllocator,
			&iLiveCount,
			&iLiveBytes
		);
		testRequire(
			(iLiveCount == iBaselineCount) &&
			(iLiveBytes == iBaselineBytes),
			"TLS server ticket OOM leaked a logical allocation"
		);
		testRequire((xrtTlsServerTicket(
			pServer, (xbytesview) { Ticket, sizeof(Ticket) },
			60u, &pResume
		) == XTLS_OK) && (pResume != NULL) &&
			(xrtTlsSessionSendSize(pServer) != 0) &&
			(pServer->WriteKey.Sequence == (iSequence + 1u)),
			"TLS server ticket did not recover after injected OOM");
		testRequire(xrtTlsSessionSendConsume(
			pServer, xrtTlsSessionSendSize(pServer)
		), "TLS server OOM ticket output consumption failed");
		xrtTlsResumeRelease(pResume);
		testTlsServerOomLive(
			pAllocator,
			&iLiveCount,
			&iLiveBytes
		);
		testRequire(
			(iLiveCount == iBaselineCount) &&
			(iLiveBytes == iBaselineBytes),
			"TLS server ticket recovery leaked a backing allocation");
	}
	xrtSecureZero(Traffic, sizeof(Traffic));
	xrtSecureZero(Ticket, sizeof(Ticket));
}



/* 验证服务端票据在每个提交前 OOM 后都保持 READY 且可重试。 */
int main(void)
{
	/* 底层分配器还会被 main 返回后的线程缓存析构使用。 */
	static test_tls_server_oom_alloc State = { 0, SIZE_MAX, 0, 0 };
	xallocator Allocator;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier;
	test_tls_server_rng Rng = { UINT32_C(0x00C0FFEE) };
	xtlssession* pClient;
	xtlssession* pServer;
	xtlsserverstate* pServerState;

	Allocator.Context = &State;
	Allocator.Alloc = testTlsServerOomAlloc;
	Allocator.Realloc = testTlsServerOomRealloc;
	Allocator.Free = testTlsServerOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS server OOM allocator install failed");
	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS server OOM fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"TLS server OOM verifier creation failed");
	testRequire(testTlsServerReady(
		pContext, pIdentity, pVerifier, &Rng, &pClient, &pServer
	), "TLS server OOM handshake failed");
	pServerState = (xtlsserverstate*)__xrtTlsSessionRoleData(pServer);
	testRequire((pServerState != NULL) &&
		pServerState->ResumptionReady &&
		(pServerState->Step == XTLS_SERVER_READY),
		"TLS server OOM role state is not ready");

	testTlsServerTicketOom(&State, pServer, pServerState);

	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	xrtClearError();
	testMemoryDebugDrain("TLS server OOM left live allocations");
	return 0;
}
