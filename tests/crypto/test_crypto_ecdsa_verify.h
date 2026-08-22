#ifndef XRT_TEST_CRYPTO_ECDSA_VERIFY_H
#define XRT_TEST_CRYPTO_ECDSA_VERIFY_H

#include "test_crypto_digest.h"



typedef bool (*test_ecdsa_verify_fn)(
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic
);



/* 验证曲线专用 raw ECDSA API 的向量、边界与错误契约。 */
static inline void testCryptoEcdsaVerify(
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOrder,
	cstr sHash,
	cstr sPublic,
	cstr sSignature,
	cstr sZeroSignature,
	cstr sOperation,
	test_ecdsa_verify_fn pVerify
)
{
	uint8 Hash[64];
	uint8 ZeroHash[64] = { 0 };
	uint8 Public[97];
	uint8 Signature[96];
	uint8 ZeroSignature[96];
	uint8 Order[48];

	testCryptoDecode(Hash, iScalarSize, sHash, "ECDSA hash size mismatch");
	testCryptoDecode(
		Public, iPublicSize, sPublic, "ECDSA public key size mismatch"
	);
	testCryptoDecode(
		Signature,
		iScalarSize * 2u,
		sSignature,
		"ECDSA raw signature size mismatch"
	);
	testCryptoDecode(
		ZeroSignature,
		iScalarSize * 2u,
		sZeroSignature,
		"ECDSA zero-hash signature size mismatch"
	);
	testCryptoDecode(Order, iScalarSize, sOrder, "ECDSA order size mismatch");

	testRequire(pVerify(Hash, iScalarSize, Signature, Public),
		"ECDSA independent raw signature was rejected");
	testRequire(pVerify(ZeroHash, iScalarSize, ZeroSignature, Public),
		"ECDSA zero hash signature was rejected");

	Hash[iScalarSize - 1u] ^= 1u;
	xrtClearError();
	testRequire(!pVerify(Hash, iScalarSize, Signature, Public) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_SIGNATURE) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sOperation) == 0),
		"ECDSA hash mismatch error contract failed");
	Hash[iScalarSize - 1u] ^= 1u;

	Signature[(iScalarSize * 2u) - 1u] ^= 1u;
	testRequire(!pVerify(Hash, iScalarSize, Signature, Public),
		"ECDSA modified signature was accepted");
	Signature[(iScalarSize * 2u) - 1u] ^= 1u;

	memset(Signature, 0, iScalarSize);
	xrtClearError();
	testRequire(!pVerify(Hash, iScalarSize, Signature, Public) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_SIGNATURE) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sOperation) == 0),
		"ECDSA zero r was accepted");
	testCryptoDecode(
		Signature,
		iScalarSize * 2u,
		sSignature,
		"ECDSA raw signature restore failed"
	);
	memcpy(Signature, Order, iScalarSize);
	testRequire(!pVerify(Hash, iScalarSize, Signature, Public),
		"ECDSA order r was accepted");
	testCryptoDecode(
		Signature,
		iScalarSize * 2u,
		sSignature,
		"ECDSA raw signature second restore failed"
	);

	Public[0] = 0x02;
	testRequire(!pVerify(Hash, iScalarSize, Signature, Public),
		"ECDSA compressed or malformed public key was accepted");
	Public[0] = 0x04;

	xrtClearError();
	testRequire(!pVerify(NULL, iScalarSize, Signature, Public) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ECDSA null hash contract mismatch");
	testRequire(!pVerify(Hash, 0, Signature, Public),
		"ECDSA empty hash was accepted");
	testRequire(!pVerify(Hash, iScalarSize, NULL, Public),
		"ECDSA null signature was accepted");
	testRequire(!pVerify(Hash, iScalarSize, Signature, NULL),
		"ECDSA null public key was accepted");
}

#endif
