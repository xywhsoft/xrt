#include "../test.h"
#include "../../src/internal/xrt_tls_session.h"



typedef struct test_tls_session_alloc {
	size_t Calls;
	size_t FailAt;
} test_tls_session_alloc;



/* 在指定底层请求失败，用于跨越堆缓存验证真实 OOM。 */
static ptr testTlsSessionAlloc(ptr pContext, size_t iSize)
{
	test_tls_session_alloc* pState = (test_tls_session_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享故障计数。 */
static ptr testTlsSessionRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_session_alloc* pState = (test_tls_session_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放成功的底层测试分配。 */
static void testTlsSessionFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 对象和惰性队列 OOM 都必须保持上下文、队列内容与错误原因完整。 */
int main(void)
{
	test_tls_session_alloc State = { 0, SIZE_MAX };
	xallocator Allocator;
	xtlscontext* pContext;
	xtlssession* Sessions[1024] = { 0 };
	xtlssession* pSession = NULL;
	static char aLarge[131072];
	size_t iCreated = 0;

	Allocator.Context = &State;
	Allocator.Alloc = testTlsSessionAlloc;
	Allocator.Realloc = testTlsSessionRealloc;
	Allocator.Free = testTlsSessionFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS session OOM allocator install failed");
	pContext = xrtTlsContextCreate(NULL);
	testRequire(pContext != NULL, "TLS session OOM context failed");

	for ( size_t i = 0; i < 1024u; i++ ) {
		State.FailAt = State.Calls + 1u;
		Sessions[i] = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
		if ( Sessions[i] == NULL ) {
			testRequire((xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_INTERNAL),
				"TLS session object OOM error mismatch");
			break;
		}
		iCreated++;
	}
	testRequire(iCreated < 1024u,
		"TLS session object allocation never reached injected OOM");
	for ( size_t i = 0; i < iCreated; i++ ) {
		xrtTlsSessionDestroy(Sessions[i]);
	}

	xrtClearError();
	State.FailAt = SIZE_MAX;
	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_SERVER);
	testRequire(pSession != NULL,
		"TLS session did not recover after object OOM");
	memset(aLarge, 'q', sizeof(aLarge));
	State.FailAt = State.Calls + 1u;
	testRequire(xrtTlsSessionFeed(
		pSession, aLarge, sizeof(aLarge)
	) == XTLS_ERROR, "TLS lazy feed allocation survived injected OOM");
	testRequire((xrtTlsSessionFeedSize(pSession) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(xrtErrorKind(xrtErrorCause(xrtGetError())) == XERR_MEMORY),
		"TLS lazy feed OOM changed the queue or lost its cause");

	State.FailAt = SIZE_MAX;
	xrtClearError();
	testRequire(xrtTlsSessionFeed(
		pSession, "ok", 2
	) == XTLS_OK && (xrtTlsSessionFeedSize(pSession) == 2),
		"TLS lazy feed did not recover after OOM");
	xrtTlsSessionDestroy(pSession);
	xrtTlsContextRelease(pContext);
	return 0;
}
