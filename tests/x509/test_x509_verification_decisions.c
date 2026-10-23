/* Routine ABI/diagnostic checks and five normal fixed signatures only.
 * This does not run the native crypto/security test programs. */
#include "../test.h"
#include "../../src/internal/xrt_crypto_rsa_pkcs1.h"
#include "../../src/internal/xrt_crypto_rsa_pss.h"
#include "../../src/internal/xrt_crypto_ecdsa.h"
#include "../../src/internal/xrt_crypto_ed25519.h"
#include "../../src/internal/xrt_crypto_nist.h"
#include "../crypto/rsa_fixture.h"
#include "../crypto/test_crypto_digest.h"
#include "../fixtures/x509_verify_vectors.h"

static void normalVectors(void)
{
	uint8 Modulus[128], Digest[64], Signature[128], Public[97], Raw[96];
	xrsapublickey Key;
	const size_t Size = sizeof(X509_VERIFY_MESSAGE) - 1u;

	testRequire(__xrtTestRsaFixture(&Key, Modulus, Digest), "RSA fixture");
	testRequire(xrtSha224(X509_VERIFY_MESSAGE, Size, Digest), "SHA224");
	testCryptoDecode(Signature, 128, X509_VERIFY_RSA_PKCS1_SHA224, "PKCS1 fixture");
	testRequire(__xrtRsaPkcs1Check(&Key, XCRYPTO_HASH_SHA224, Digest,
		Signature, 128) == XRT_CRYPTO_CHECK_VALID, "PKCS1 decision");
	testRequire(xrtRsaPkcs1Verify(&Key, XCRYPTO_HASH_SHA224, Digest,
		Signature, 128), "PKCS1 legacy");

	testRequire(xrtSha256(X509_VERIFY_MESSAGE, Size, Digest), "SHA256");
	testCryptoDecode(Signature, 128, X509_VERIFY_RSA_PSS_SHA256, "PSS fixture");
	testRequire(__xrtRsaPssCheck(&Key, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA384,
		32, Digest, Signature, 128) == XRT_CRYPTO_CHECK_VALID, "PSS decision");
	testRequire(xrtRsaPssVerify(&Key, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA384,
		32, Digest, Signature, 128), "PSS legacy");

	testRequire(xrtSha384(X509_VERIFY_MESSAGE, Size, Digest), "SHA384");
	testCryptoDecode(Public, 65, X509_VERIFY_P256_PUBLIC, "P256 key");
	testCryptoDecode(Signature, 71, X509_VERIFY_P256_SHA384, "P256 fixture");
	testRequire(__xrtEcdsaCheckDer(XRT_NIST_P256, Digest, 48, Signature, 71,
		Public, 32, 65) == XRT_CRYPTO_CHECK_VALID, "P256 DER decision");
	testRequire(xrtEcdsaP256VerifyDer(Digest, 48, Signature, 71, Public), "P256 legacy");
	testRequire(__xrtEcdsaDerDecodeCheck(Signature, 71, Raw, 32) ==
		XRT_CRYPTO_CHECK_VALID, "P256 DER decode decision");
	testRequire(__xrtEcdsaCheck(XRT_NIST_P256, Digest, 48, Raw, Public, 32, 65) ==
		XRT_CRYPTO_CHECK_VALID, "P256 raw decision");

	testRequire(xrtSha256(X509_VERIFY_MESSAGE, Size, Digest), "SHA256 P384");
	testCryptoDecode(Public, 97, X509_VERIFY_P384_PUBLIC, "P384 key");
	testCryptoDecode(Signature, 102, X509_VERIFY_P384_SHA256, "P384 fixture");
	testRequire(__xrtEcdsaCheckDer(XRT_NIST_P384, Digest, 32, Signature, 102,
		Public, 48, 97) == XRT_CRYPTO_CHECK_VALID, "P384 DER decision");
	testRequire(xrtEcdsaP384VerifyDer(Digest, 32, Signature, 102, Public), "P384 legacy");
	testRequire(__xrtEcdsaDerDecodeCheck(Signature, 102, Raw, 48) ==
		XRT_CRYPTO_CHECK_VALID, "P384 DER decode decision");
	testRequire(__xrtEcdsaCheck(XRT_NIST_P384, Digest, 32, Raw, Public, 48, 97) ==
		XRT_CRYPTO_CHECK_VALID, "P384 raw decision");

	testCryptoDecode(Public, 32, X509_VERIFY_ED25519_PUBLIC, "Ed25519 key");
	testCryptoDecode(Signature, 64, X509_VERIFY_ED25519_SIGNATURE, "Ed25519 fixture");
	testRequire(__xrtEd25519Check(Public, XED25519_PURE, NULL, 0,
		X509_VERIFY_MESSAGE, Size, Signature) == XRT_CRYPTO_CHECK_VALID, "Ed25519 decision");
	testRequire(xrtEd25519Verify(Public, X509_VERIFY_MESSAGE, Size, Signature), "Ed25519 legacy");
}

static void argumentDecisions(void)
{
	const uint8 Dummy[128] = {0};
	/* Invalid pointer arguments are ordinary API contract checks, not
	 * malformed cryptographic input. No decision may allocate a diagnostic. */
	testRequire(__xrtRsaPkcs1Check(NULL, XCRYPTO_HASH_SHA256, NULL, NULL, 0) ==
		XRT_CRYPTO_CHECK_ARGUMENT, "PKCS1 argument decision");
	testRequire(__xrtRsaPssCheck(NULL, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
		32, NULL, NULL, 0) == XRT_CRYPTO_CHECK_ARGUMENT, "PSS argument decision");
	testRequire(__xrtEcdsaCheck(XRT_NIST_P256, NULL, 0, NULL, NULL, 32, 65) ==
		XRT_CRYPTO_CHECK_ARGUMENT, "ECDSA raw argument decision");
	testRequire(__xrtEcdsaCheckDer(XRT_NIST_P256, NULL, 0, NULL, 0, NULL, 32, 65) ==
		XRT_CRYPTO_CHECK_ARGUMENT, "ECDSA DER argument decision");
	testRequire(__xrtEcdsaDerDecodeCheck(NULL, 0, NULL, 32) ==
		XRT_CRYPTO_CHECK_ARGUMENT, "ECDSA decode argument decision");
	testRequire(__xrtEd25519Check(NULL, XED25519_PURE, NULL, 0, NULL, 0, NULL) ==
		XRT_CRYPTO_CHECK_ARGUMENT, "Ed25519 argument decision");
	testRequire(__xrtEd25519Check(Dummy, (xed25519mode)99, NULL, 0,
		NULL, 0, Dummy) == XRT_CRYPTO_CHECK_ARGUMENT, "Ed25519 mode decision");
	testRequire(__xrtEd25519Check(Dummy, XED25519_CONTEXT, Dummy,
		XRT_ED25519_CONTEXT_MAX_SIZE + 1u, NULL, 0, Dummy) ==
		XRT_CRYPTO_CHECK_ARGUMENT, "Ed25519 context capacity decision");
	testRequire(__xrtEd25519Check(Dummy, XED25519_CONTEXT, NULL, 1,
		NULL, 0, Dummy) == XRT_CRYPTO_CHECK_ARGUMENT, "Ed25519 null context decision");
	testRequire(__xrtEd25519Check(Dummy, XED25519_PURE, Dummy, 1,
		NULL, 0, Dummy) == XRT_CRYPTO_CHECK_ARGUMENT, "Ed25519 pure context decision");
	testRequire(__xrtEd25519Check(Dummy, XED25519_PURE, NULL, 0,
		NULL, 1, Dummy) == XRT_CRYPTO_CHECK_ARGUMENT, "Ed25519 message pointer decision");
	testRequire(__xrtEd25519Check(Dummy, XED25519_PREHASH, NULL, 0,
		Dummy, 1, Dummy) == XRT_CRYPTO_CHECK_ARGUMENT, "Ed25519 prehash size decision");
	testRequire(__xrtEcdsaCheckDer(XRT_NIST_P384, Dummy, 32, Dummy, 1,
		Dummy, 66, 97) == XRT_CRYPTO_CHECK_ARGUMENT, "ECDSA scratch capacity decision");
}

int main(void)
{
	xmemdebugsnapshot Before, After;
	xrtMemDebugSnapshot(&Before);
	testRequire(xrtMemDebugFailAfter(0), "fail allocator setup");
	normalVectors();
	argumentDecisions();
	testRequire(!xrtMemDebugFailTriggered() && !xrtGetError(), "decision unexpectedly allocated");
	xrtMemDebugFailClear();

	xrtSetErrorKind(XERR_STATE);
	const xerror* Pending = xrtGetError();
	testRequire(Pending != NULL, "pending error setup");
	testRequire(xrtMemDebugFailAfter(0), "pending error fail allocator setup");
	normalVectors();
	argumentDecisions();
	testRequire(xrtGetError() == Pending && !xrtMemDebugFailTriggered(), "decision overwrote pending error");
	xrtMemDebugFailClear();
	xrtClearError();

	testRequire(!xrtRsaPkcs1Verify(NULL, XCRYPTO_HASH_SHA256, NULL, NULL, 0) &&
		xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_SIGNATURE, "legacy PKCS1 diagnostic");
	xrtClearError();
	testRequire(!xrtRsaPssVerify(NULL, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
		32, NULL, NULL, 0) && xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_SIGNATURE,
		"legacy PSS diagnostic");
	xrtClearError();
	testRequire(!xrtEcdsaP256VerifyDer(NULL, 0, NULL, 0, NULL) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "legacy ECDSA diagnostic");
	xrtClearError();
	testRequire(!xrtEcdsaDerDecode(NULL, 0, NULL, 32) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "legacy DER diagnostic");
	xrtClearError();
	testRequire(!xrtEd25519Verify(NULL, NULL, 0, NULL) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "legacy Ed25519 diagnostic");
	xrtClearError();
	testRequire(xrtMemDebugReset(), "verification cleanup");
	xrtMemDebugSnapshot(&After);
	testRequire(After.LiveCount == Before.LiveCount, "verification resource balance");
	printf("[PASS] verification decisions: five normal vectors, 13 argument decisions, pending-error preservation\n");
	return 0;
}
