#include "../test.h"



typedef struct test_tls_client_alloc {
	size_t Calls;
	size_t FailAt;
} test_tls_client_alloc;



/* 在指定底层分配请求注入失败。 */
static ptr testTlsClientAlloc(ptr pContext, size_t iSize)
{
	test_tls_client_alloc* pState = (test_tls_client_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配和普通分配共享同一故障序号。 */
static ptr testTlsClientRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_client_alloc* pState = (test_tls_client_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器成功交付的内存。 */
static void testTlsClientFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 跨记录握手重组 OOM 必须保留输入并把会话推进到失败终态。 */
static void testTlsClientReaderOom(
	test_tls_client_alloc* pState,
	xtlssession* pSession
)
{
	uint8 Partial[2048] = { 0 };
	uint8 Record[2053];
	size_t iRecordSize = xrtTlsRecordSize(sizeof(Partial));
	size_t iFeed;
	xtlsresult Result;

	Partial[0] = XTLS_HANDSHAKE_SERVER_HELLO;
	Partial[1] = 0x00;
	Partial[2] = 0x10;
	Partial[3] = 0x00;
	Partial[4] = 0x03;
	Partial[5] = 0x03;
	testRequire(xrtTlsSessionSendConsume(
		pSession, xrtTlsSessionSendSize(pSession)
	), "TLS client OOM initial send consumption failed");
	testRequire((iRecordSize <= sizeof(Record)) && xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
		(xbytesview) { Partial, sizeof(Partial) }, Record, sizeof(Record)
	) && (xrtTlsSessionFeed(
		pSession, Record, iRecordSize
	) == XTLS_OK), "TLS client OOM partial handshake feed failed");
	iFeed = xrtTlsSessionFeedSize(pSession);
	xrtClearError();
	pState->FailAt = pState->Calls + 1u;
	Result = xrtTlsClientDrive(pSession);
	testRequire(Result == XTLS_ERROR,
		"TLS client handshake-reader OOM did not fail the drive");
	testRequire(xrtTlsSessionState(pSession) == XTLS_STATE_FAILED,
		"TLS client handshake-reader OOM did not enter failed state");
	testRequire(xrtTlsSessionFeedSize(pSession) == iFeed,
		"TLS client handshake-reader OOM consumed pending input");
	testRequire(xrtGetError() != NULL,
		"TLS client handshake-reader OOM did not publish an error");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"TLS client handshake-reader OOM error kind mismatch");
	pState->FailAt = SIZE_MAX;
}



/* 客户端任意创建阶段 OOM 都必须完整回滚并允许后续恢复。 */
int main(void)
{
	test_tls_client_alloc State = { 0, SIZE_MAX };
	xallocator Allocator;
	xtlscontext* pContext;
	xtlsclientconfig Config;
	xtlssession* Sessions[1024] = { 0 };
	xtlssession* pRecovered;
	size_t iCreated = 0;

	Allocator.Context = &State;
	Allocator.Alloc = testTlsClientAlloc;
	Allocator.Realloc = testTlsClientRealloc;
	Allocator.Free = testTlsClientFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS client OOM allocator install failed");
	pContext = xrtTlsContextCreate(NULL);
	testRequire(pContext != NULL, "TLS client OOM context failed");
	xrtTlsClientConfigInit(&Config);
	Config.Context = pContext;
	Config.ServerName = XRT_STR_LITERAL("example.com");

	for ( size_t i = 0; i < 1024u; i++ ) {
		State.FailAt = State.Calls + 1u;
		Sessions[i] = xrtTlsClientCreate(&Config, NULL);
		if ( Sessions[i] == NULL ) {
			const xerror* pError = xrtGetError();

			testRequire((pError != NULL) &&
				(xrtErrorKind(pError) == XERR_MEMORY),
				"TLS client OOM error kind mismatch");
			break;
		}
		iCreated++;
	}
	testRequire(iCreated < 1024u,
		"TLS client allocation never reached injected OOM");
	for ( size_t i = 0; i < iCreated; i++ ) {
		xrtTlsSessionDestroy(Sessions[i]);
	}

	State.FailAt = SIZE_MAX;
	xrtClearError();
	pRecovered = xrtTlsClientCreate(&Config, NULL);
	testRequire((pRecovered != NULL) &&
		(xrtTlsSessionState(pRecovered) == XTLS_STATE_HANDSHAKE) &&
		(xrtTlsSessionSendSize(pRecovered) != 0),
		"TLS client did not recover after injected OOM");
	testTlsClientReaderOom(&State, pRecovered);
	xrtTlsSessionDestroy(pRecovered);
	xrtTlsContextRelease(pContext);
	return 0;
}
