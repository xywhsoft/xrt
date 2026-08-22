#include "../test.h"
#include "../fixtures/x509_legacy_cert.h"



typedef struct test_tls_verify_alloc {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_tls_verify_alloc;



/* 在指定分配点失败并统计存活块。 */
static ptr testTlsVerifyAlloc(ptr pContext, size_t iSize)
{
	test_tls_verify_alloc* pState = (test_tls_verify_alloc*)pContext;
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



/* 重分配保持分配点和存活计数一致。 */
static ptr testTlsVerifyRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_verify_alloc* pState = (test_tls_verify_alloc*)pContext;
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



/* 释放成功分配并统计存活块。 */
static void testTlsVerifyFree(ptr pContext, ptr pMemory)
{
	test_tls_verify_alloc* pState = (test_tls_verify_alloc*)pContext;

	if ( pMemory != NULL ) {
		testRequire(pState->Live != 0, "TLS verifier allocator underflow");
		pState->Live--;
		free(pMemory);
	}
}



/* 读取逻辑活动块；未启用调试时回退到底层存活计数。 */
static void testTlsVerifyLive(
	const test_tls_verify_alloc* pState,
	size_t* pCount,
	size_t* pBytes
)
{
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xmemdebugsnapshot Snapshot;

		(void)pState;
		xrtMemDebugSnapshot(&Snapshot);
		*pCount = Snapshot.LiveCount;
		*pBytes = Snapshot.LiveBytes;
	#else
		*pCount = pState->Live;
		*pBytes = 0;
	#endif
}



/* 验证信任快照在每个分配失败点都完整回滚。 */
int main(void)
{
	test_tls_verify_alloc State = { 0, SIZE_MAX, 0 };
	xallocator Allocator;
	xx509store* pStore;
	xtlsverifierconfig Config;
	xtlsverifier* pVerifier;
	size_t iBaselineCount;
	size_t iBaselineBytes;
	size_t iLiveCount;
	size_t iLiveBytes;
	bool bSucceeded = false;

	Allocator.Context = &State;
	Allocator.Alloc = testTlsVerifyAlloc;
	Allocator.Realloc = testTlsVerifyRealloc;
	Allocator.Free = testTlsVerifyFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS verifier OOM allocator install failed");
	pStore = xrtX509StoreCreate();
	testRequire((pStore != NULL) &&
		(xrtX509StoreAdd(
			pStore, X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
		) == X509_VALUE), "TLS verifier OOM store setup failed");
	xrtTlsVerifierConfigInit(&Config);
	Config.Store = pStore;
	pVerifier = xrtTlsVerifierCreate(&Config);
	testRequire(pVerifier != NULL,
		"TLS verifier OOM warm-up creation failed");
	xrtTlsVerifierRelease(pVerifier);
	State.FailAt = State.Calls + 1u;
	testRequire(xrtTlsVerifierCreate(&Config) == NULL,
		"TLS verifier OOM warm-up failure did not fail");
	xrtClearError();
	State.FailAt = SIZE_MAX;
	testTlsVerifyLive(
		&State,
		&iBaselineCount,
		&iBaselineBytes
	);

	for ( size_t i = 1; i <= 16u; i++ ) {
		State.FailAt = State.Calls + i;
		pVerifier = xrtTlsVerifierCreate(&Config);
		if ( pVerifier != NULL ) {
			xrtTlsVerifierRelease(pVerifier);
			bSucceeded = true;
			break;
		}
		xrtClearError();
		testTlsVerifyLive(&State, &iLiveCount, &iLiveBytes);
		testRequire(
			(iLiveCount == iBaselineCount) &&
			(iLiveBytes == iBaselineBytes),
			"TLS verifier OOM leaked a partial trust snapshot");
	}
	testRequire(bSucceeded,
		"TLS verifier did not recover after injected allocation failures");
	State.FailAt = SIZE_MAX;
	xrtX509StoreFree(pStore);
	testTlsVerifyLive(&State, &iLiveCount, &iLiveBytes);
	testRequire(
		(iLiveCount <= iBaselineCount) &&
		(iLiveBytes <= iBaselineBytes),
		"TLS verifier OOM test grew the warmed heap baseline");
	return 0;
}
