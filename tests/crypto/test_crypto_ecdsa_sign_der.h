#ifndef XRT_TEST_CRYPTO_ECDSA_SIGN_DER_H
#define XRT_TEST_CRYPTO_ECDSA_SIGN_DER_H

#include "test_crypto_digest.h"



typedef bool (*test_ecdsa_sign_der_fn)(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
);



/* 验证确定性 DER 签名的查询、容量、重叠和规范编码契约。 */
static inline void testCryptoEcdsaSignDer(
	size_t iScalarSize,
	size_t iDerSize,
	xcryptohash Algorithm,
	cstr sHash,
	cstr sPrivate,
	cstr sExpectedRaw,
	cstr sExpectedDer,
	test_ecdsa_sign_der_fn pSign
)
{
	uint8 Hash[48];
	uint8 Private[48];
	uint8 ExpectedRaw[96];
	uint8 ExpectedDer[104];
	uint8 Output[128];
	uint8 Before[128];
	uint8 Decoded[96];
	uint8 Buffer[256];
	size_t iSize = 0;

	testCryptoDecode(Hash, iScalarSize, sHash, "ECDSA DER sign hash mismatch");
	testCryptoDecode(
		Private, iScalarSize, sPrivate, "ECDSA DER sign private mismatch"
	);
	testCryptoDecode(
		ExpectedRaw,
		iScalarSize * 2u,
		sExpectedRaw,
		"ECDSA DER expected raw mismatch"
	);
	testCryptoDecode(
		ExpectedDer, iDerSize, sExpectedDer, "ECDSA DER expected value mismatch"
	);

	testRequire(pSign(Algorithm, Hash, Private, NULL, 0, &iSize) &&
		(iSize == iDerSize), "ECDSA DER sign query mismatch");
	testRequire(pSign(
		Algorithm, Hash, Private, Output, sizeof(Output), &iSize
	) &&
		(iSize == iDerSize) &&
		xrtConstTimeEqual(Output, ExpectedDer, iDerSize),
		"ECDSA deterministic DER signature mismatch");
	testRequire(xrtEcdsaDerDecode(Output, iSize, Decoded, iScalarSize) &&
		xrtConstTimeEqual(Decoded, ExpectedRaw, iScalarSize * 2u),
		"ECDSA DER sign did not round trip to expected raw signature");

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	xrtClearError();
	testRequire(!pSign(
		Algorithm, Hash, Private, Output, iDerSize - 1u, &iSize
	) &&
		(iSize == iDerSize) &&
		xrtConstTimeEqual(Output, Before, sizeof(Output)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"ECDSA DER insufficient capacity contract mismatch");

	memset(Buffer, 0, sizeof(Buffer));
	memcpy(Buffer, Hash, iScalarSize);
	memcpy(Buffer + 64, Private, iScalarSize);
	testRequire(pSign(
		Algorithm, Buffer, Buffer + 64,
		Buffer + 16, sizeof(Buffer) - 16u, &iSize
	) && xrtConstTimeEqual(Buffer + 16, ExpectedDer, iDerSize),
		"ECDSA DER overlapping sign mismatch");

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	xrtClearError();
	testRequire(!pSign(
		Algorithm, Hash, Private,
		Output, sizeof(Output), (size_t*)Output
	) && xrtConstTimeEqual(Output, Before, sizeof(Output)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ECDSA DER overlapping size output was accepted");
	testRequire(!pSign(
		Algorithm, Hash, Private, Output, sizeof(Output), NULL
	),
		"ECDSA DER null size output was accepted");
}

#endif
