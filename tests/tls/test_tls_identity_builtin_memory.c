#include "../test.h"
#include "../crypto/test_crypto_digest.h"

#define TEST_TLS_IDENTITY_FIXTURE_EC
#define TEST_TLS_IDENTITY_FIXTURE_ED25519
#include "../fixtures/tls_identity_ec_ed.h"

#define TEST_TLS_IDENTITY_CERT_COUNT 16u



typedef struct test_tls_identity_builtin_alloc {
	size_t Calls;
	size_t Allocations;
	size_t Frees;
	size_t Cleared;
	size_t Size;
	size_t SecretSize;
	uint8 Secret[XRT_P384_PRIVATE_SIZE];
	bool Fail;
} test_tls_identity_builtin_alloc;



/* 记录每个内置身份唯一的紧凑分配，并支持运行期 OOM 注入。 */
static ptr testTlsIdentityBuiltinAlloc(ptr pContext, size_t iSize)
{
	test_tls_identity_builtin_alloc* pState =
		(test_tls_identity_builtin_alloc*)pContext;

	pState->Calls++;
	if ( pState->Fail ) {
		return NULL;
	}
	pState->Allocations++;
	pState->Size = iSize;
	return malloc(iSize);
}



/* 身份不应重分配，仍提供完整的分配器契约。 */
static ptr testTlsIdentityBuiltinRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 释放时 backing block 中不得再存在当前私钥字节。 */
static void testTlsIdentityBuiltinFree(ptr pContext, ptr pMemory)
{
	test_tls_identity_builtin_alloc* pState =
		(test_tls_identity_builtin_alloc*)pContext;
	const uint8* pBytes = (const uint8*)pMemory;
	bool bFound = false;

	for ( size_t i = 0;
		(i + pState->SecretSize) <= pState->Size;
		i++ ) {
		if ( memcmp(
			pBytes + i, pState->Secret, pState->SecretSize
		) == 0 ) {
			bFound = true;
			break;
		}
	}
	if ( !bFound ) {
		pState->Cleared++;
	}
	pState->Frees++;
	free(pMemory);
}



/* 创建并释放一个身份，验证恰好一次分配和释放前清零。 */
static void testTlsIdentityBuiltinRelease(
	xtlsidentity* pIdentity,
	test_tls_identity_builtin_alloc* pState,
	cstr sMessage
)
{
	size_t iFrees = pState->Frees;
	size_t iCleared = pState->Cleared;

	testRequire(pIdentity != NULL, sMessage);
	testRequire(pState->Allocations == (iFrees + 1u),
		"TLS built-in identity did not use one compact allocation");
	xrtTlsIdentityRelease(pIdentity);
	testMemoryDebugDrain(
		"TLS built-in identity memory debug quarantine drain failed"
	);
	testRequire((pState->Frees == (iFrees + 1u)) &&
		(pState->Cleared == (iCleared + 1u)),
		"TLS built-in identity storage was not cleared before release");
}



/* P-256、P-384 与 Ed25519 必须共享单分配、全清零和 OOM 契约。 */
int main(void)
{
	uint8 P256Private[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 P256Public[XRT_P256_PUBLIC_SIZE];
	uint8 P256Certificate[512];
	uint8 P256Sec1[192];
	uint8 P256Pkcs8[256];
	uint8 P384Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 P384Public[XRT_P384_PUBLIC_SIZE];
	uint8 P384Certificate[512];
	uint8 P384Sec1[224];
	uint8 P384Pkcs8[320];
	uint8 EdSeed[XRT_ED25519_SEED_SIZE];
	uint8 EdPublic[XRT_ED25519_PUBLIC_SIZE];
	uint8 EdCertificate[512];
	uint8 EdPkcs8[64];
	size_t iP256CertificateSize = 0;
	size_t iP256Sec1Size = 0;
	size_t iP256Pkcs8Size = 0;
	size_t iP384CertificateSize = 0;
	size_t iP384Sec1Size = 0;
	size_t iP384Pkcs8Size = 0;
	size_t iEdCertificateSize = 0;
	size_t iEdPkcs8Size = 0;
	xbytesview P256Chain[TEST_TLS_IDENTITY_CERT_COUNT];
	xbytesview P384Chain[TEST_TLS_IDENTITY_CERT_COUNT];
	xbytesview EdChain[TEST_TLS_IDENTITY_CERT_COUNT];
	test_tls_identity_builtin_alloc State = { 0 };
	xallocator Allocator;
	size_t iCalls;

	memset(P256Private, 0x31, sizeof(P256Private));
	memset(P384Private, 0x42, sizeof(P384Private));
	testCryptoDecode(
		EdSeed, sizeof(EdSeed),
		"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
		"Ed25519 memory fixture seed mismatch"
	);
	testRequire(xrtP256Public(P256Private, P256Public) &&
		testTlsIdentityEcCertificate(
			P256Public, sizeof(P256Public), P256Certificate,
			sizeof(P256Certificate), &iP256CertificateSize
		) && testTlsIdentityEcSec1(
			P256Private, sizeof(P256Private), P256Public,
			P256Sec1, sizeof(P256Sec1), &iP256Sec1Size
		) && testTlsIdentityEcPkcs8(
			P256Sec1, iP256Sec1Size, sizeof(P256Private),
			P256Pkcs8, sizeof(P256Pkcs8), &iP256Pkcs8Size
		) && xrtP384Public(P384Private, P384Public) &&
		testTlsIdentityEcCertificate(
			P384Public, sizeof(P384Public), P384Certificate,
			sizeof(P384Certificate), &iP384CertificateSize
		) && testTlsIdentityEcSec1(
			P384Private, sizeof(P384Private), P384Public,
			P384Sec1, sizeof(P384Sec1), &iP384Sec1Size
		) && testTlsIdentityEcPkcs8(
			P384Sec1, iP384Sec1Size, sizeof(P384Private),
			P384Pkcs8, sizeof(P384Pkcs8), &iP384Pkcs8Size
		) && xrtEd25519Public(EdSeed, EdPublic) &&
		testTlsIdentityEdCertificate(
			EdPublic, EdCertificate,
			sizeof(EdCertificate), &iEdCertificateSize
		) && testTlsIdentityEdPkcs8(
			EdSeed, EdPkcs8, sizeof(EdPkcs8), &iEdPkcs8Size
		), "TLS built-in identity memory fixtures failed");
	/* 重复视图仅用于把紧凑对象推过小块池阈值，使 backing 释放可观察。 */
	for ( size_t i = 0; i < TEST_TLS_IDENTITY_CERT_COUNT; i++ ) {
		P256Chain[i] = (xbytesview) {
			P256Certificate, iP256CertificateSize
		};
		P384Chain[i] = (xbytesview) {
			P384Certificate, iP384CertificateSize
		};
		EdChain[i] = (xbytesview) {
			EdCertificate, iEdCertificateSize
		};
	}
	Allocator.Context = &State;
	Allocator.Alloc = testTlsIdentityBuiltinAlloc;
	Allocator.Realloc = testTlsIdentityBuiltinRealloc;
	Allocator.Free = testTlsIdentityBuiltinFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS built-in identity allocator install failed");

	memcpy(State.Secret, P256Private, sizeof(P256Private));
	State.SecretSize = sizeof(P256Private);
	testTlsIdentityBuiltinRelease(xrtTlsIdentityP256(
		P256Chain, TEST_TLS_IDENTITY_CERT_COUNT,
		(xbytesview) { P256Pkcs8, iP256Pkcs8Size }
	), &State, "P-256 identity creation failed");
	memcpy(State.Secret, P384Private, sizeof(P384Private));
	State.SecretSize = sizeof(P384Private);
	testTlsIdentityBuiltinRelease(xrtTlsIdentityP384(
		P384Chain, TEST_TLS_IDENTITY_CERT_COUNT,
		(xbytesview) { P384Pkcs8, iP384Pkcs8Size }
	), &State, "P-384 identity creation failed");
	memcpy(State.Secret, EdSeed, sizeof(EdSeed));
	State.SecretSize = sizeof(EdSeed);
	testTlsIdentityBuiltinRelease(xrtTlsIdentityEd25519(
		EdChain, TEST_TLS_IDENTITY_CERT_COUNT,
		(xbytesview) { EdPkcs8, iEdPkcs8Size }
	), &State, "Ed25519 identity creation failed");

	State.Fail = true;
	iCalls = State.Calls;
	xrtClearError();
	testRequire((xrtTlsIdentityP256(
		P256Chain, TEST_TLS_IDENTITY_CERT_COUNT,
		(xbytesview) { P256Pkcs8, iP256Pkcs8Size }
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"P-256 identity did not preserve OOM cause");
	xrtClearError();
	testRequire((xrtTlsIdentityP384(
		P384Chain, TEST_TLS_IDENTITY_CERT_COUNT,
		(xbytesview) { P384Pkcs8, iP384Pkcs8Size }
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"P-384 identity did not preserve OOM cause");
	xrtClearError();
	testRequire((xrtTlsIdentityEd25519(
		EdChain, TEST_TLS_IDENTITY_CERT_COUNT,
		(xbytesview) { EdPkcs8, iEdPkcs8Size }
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(State.Calls >= (iCalls + 3u)) &&
		(State.Allocations == 3u) && (State.Frees == 3u),
		"Ed25519 identity OOM or allocation accounting mismatch");
	return 0;
}
