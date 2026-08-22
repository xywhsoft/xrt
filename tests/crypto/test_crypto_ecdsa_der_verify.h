#ifndef XRT_TEST_CRYPTO_ECDSA_DER_VERIFY_H
#define XRT_TEST_CRYPTO_ECDSA_DER_VERIFY_H

#include "test_crypto_digest.h"



typedef bool (*test_ecdsa_verify_der_fn)(
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic
);



/* 验证曲线专用 DER 便利入口严格复用表示层与 raw 验签层。 */
static inline void testCryptoEcdsaVerifyDer(
	size_t iScalarSize,
	size_t iPublicSize,
	size_t iDerSize,
	cstr sHash,
	cstr sPublic,
	cstr sDer,
	cstr sOperation,
	test_ecdsa_verify_der_fn pVerify
)
{
	uint8 Hash[64];
	uint8 Public[97];
	uint8 Der[104];

	testCryptoDecode(Hash, iScalarSize, sHash, "ECDSA DER hash size mismatch");
	testCryptoDecode(
		Public, iPublicSize, sPublic, "ECDSA DER public key size mismatch"
	);
	testCryptoDecode(Der, iDerSize, sDer, "ECDSA DER vector size mismatch");
	testRequire(pVerify(Hash, iScalarSize, Der, iDerSize, Public),
		"ECDSA independent DER signature was rejected");

	Hash[iScalarSize - 1u] ^= 1u;
	xrtClearError();
	testRequire(!pVerify(Hash, iScalarSize, Der, iDerSize, Public) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_SIGNATURE) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sOperation) == 0),
		"ECDSA DER verification mismatch contract failed");
	Hash[iScalarSize - 1u] ^= 1u;

	Der[iDerSize] = 0;
	xrtClearError();
	testRequire(!pVerify(Hash, iScalarSize, Der, iDerSize + 1u, Public) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(strcmp(xrtErrorOperation(xrtGetError()),
			"ecdsa-der-decode") == 0),
		"ECDSA DER trailing data was accepted");
	xrtClearError();
	testRequire(!pVerify(NULL, iScalarSize, Der, iDerSize, Public) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ECDSA DER null hash contract mismatch");
	testRequire(!pVerify(Hash, 0, Der, iDerSize, Public),
		"ECDSA DER empty hash was accepted");
}

#endif
