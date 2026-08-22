#include "../test.h"
#include "../fixtures/tls_identity_legacy.h"



typedef struct test_tls_identity_rsa_alloc {
	size_t Allocations;
	size_t Frees;
	size_t LastSize;
	uint8 Secret[32];
	bool Cleared;
} test_tls_identity_rsa_alloc;



/* 记录身份唯一分配，供释放时检查整个私钥尾部已经清零。 */
static ptr testTlsIdentityRsaAlloc(ptr pContext, size_t iSize)
{
	test_tls_identity_rsa_alloc* pState =
		(test_tls_identity_rsa_alloc*)pContext;

	pState->Allocations++;
	pState->LastSize = iSize;
	return malloc(iSize);
}



/* 身份当前不使用重分配，保留完整分配器契约。 */
static ptr testTlsIdentityRsaRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 释放器确认 backing block 中已经不存在私钥特有的敏感片段。 */
static void testTlsIdentityRsaFree(ptr pContext, ptr pMemory)
{
	test_tls_identity_rsa_alloc* pState =
		(test_tls_identity_rsa_alloc*)pContext;
	const uint8* pBytes = (const uint8*)pMemory;
	bool bFound = false;

	for ( size_t i = 0;
		(i + sizeof(pState->Secret)) <= pState->LastSize;
		i++ ) {
		if ( memcmp(
			pBytes + i, pState->Secret, sizeof(pState->Secret)
		) == 0 ) {
			bFound = true;
			break;
		}
	}
	pState->Cleared = !bFound;
	pState->Frees++;
	free(pMemory);
}



/* RSA 身份必须使用单块存储，并在最后释放前清除私钥区域。 */
int main(void)
{
	uint8 PrivateDer[2048];
	size_t iPrivateSize = 0;
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	test_tls_identity_rsa_alloc State = { 0 };
	xallocator Allocator;
	xtlsidentity* pIdentity;

	testRequire(testTlsIdentityLegacyKey(
		PrivateDer, sizeof(PrivateDer), &iPrivateSize
	), "RSA identity clear fixture decode failed");
	testRequire(iPrivateSize >= sizeof(State.Secret),
		"RSA identity clear fixture is unexpectedly short");
	memcpy(
		State.Secret,
		PrivateDer + iPrivateSize - sizeof(State.Secret),
		sizeof(State.Secret)
	);
	Allocator.Context = &State;
	Allocator.Alloc = testTlsIdentityRsaAlloc;
	Allocator.Realloc = testTlsIdentityRsaRealloc;
	Allocator.Free = testTlsIdentityRsaFree;
	testRequire(xrtSetAllocator(&Allocator),
		"RSA identity clear allocator install failed");
	pIdentity = xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { PrivateDer, iPrivateSize }
	);
	testRequire((pIdentity != NULL) && (State.Allocations == 1u),
		"RSA identity did not use one compact allocation");
	xrtTlsIdentityRelease(pIdentity);
	testMemoryDebugDrain(
		"RSA identity memory debug quarantine drain failed"
	);
	testRequire((State.Frees == 1u) && State.Cleared,
		"RSA identity storage was not cleared before release");
	return 0;
}
