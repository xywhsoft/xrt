#include "../test.h"



typedef struct test_tls_context_alloc {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_tls_context_alloc;



/* 在指定调用失败并统计仍然存活的分配。 */
static ptr testTlsContextAlloc(ptr pContext, size_t iSize)
{
	test_tls_context_alloc* pState = (test_tls_context_alloc*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配保持故障点和存活块统计一致。 */
static ptr testTlsContextRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_context_alloc* pState = (test_tls_context_alloc*)pContext;
	bool bNew = pMemory == NULL;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && bNew ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放成功分配并核对引用生命周期。 */
static void testTlsContextFree(ptr pContext, ptr pMemory)
{
	test_tls_context_alloc* pState = (test_tls_context_alloc*)pContext;

	if ( pMemory != NULL ) {
		testRequire(pState->Live != 0, "TLS context allocator underflow");
		pState->Live--;
		free(pMemory);
	}
}



/* 上下文使用单次分配，失败和释放后都不能遗留快照块。 */
int main(void)
{
	static test_tls_context_alloc State = { 0, SIZE_MAX, 0 };
	xallocator Allocator;
	xtlscontext* pContext;
	ptr pWarm;
	size_t iHeapBaseline;

	Allocator.Context = &State;
	Allocator.Alloc = testTlsContextAlloc;
	Allocator.Realloc = testTlsContextRealloc;
	Allocator.Free = testTlsContextFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS context OOM allocator install failed");

	/* 先在无关尺寸类建立线程缓存，使故障点落在上下文 span。 */
	pWarm = xrtMalloc(1u);
	testRequire(pWarm != NULL,
		"TLS context OOM heap warm-up failed");
	xrtFree(pWarm);

	State.FailAt = State.Calls + 1u;
	pContext = xrtTlsContextCreate(NULL);
	testRequire((pContext == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"TLS context unexpectedly survived OOM");
	xrtClearError();

	State.FailAt = SIZE_MAX;
	pContext = xrtTlsContextCreate(NULL);
	testRequire(pContext != NULL, "TLS context recovery after OOM failed");
	testRequire(xrtTlsContextRetain(pContext) == pContext,
		"TLS context retain after OOM failed");
	xrtTlsContextRelease(pContext);
	xrtTlsContextRelease(pContext);
	xrtClearError();
	testMemoryDebugDrain(
		"TLS context baseline memory debug drain failed");
	iHeapBaseline = State.Live;

	for ( size_t i = 0; i < 64u; i++ ) {
		pContext = xrtTlsContextCreate(NULL);
		testRequire(pContext != NULL,
			"repeated TLS context creation failed");
		xrtTlsContextRelease(pContext);
	}
	testMemoryDebugDrain(
		"TLS context final memory debug drain failed");
	testRequire(State.Live == iHeapBaseline,
		"repeated TLS context release grew backing memory");
	return 0;
}
