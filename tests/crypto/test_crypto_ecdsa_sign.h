#ifndef XRT_TEST_CRYPTO_ECDSA_SIGN_H
#define XRT_TEST_CRYPTO_ECDSA_SIGN_H

#include "test_crypto_digest.h"



typedef bool (*test_ecdsa_sign_fn)(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature
);



/* 验证确定性 ECDSA raw 签名、low-S、重叠与私钥错误契约。 */
static inline void testCryptoEcdsaSign(
	size_t iScalarSize,
	xcryptohash Algorithm,
	xcryptohash Unavailable,
	cstr sOrder,
	cstr sHash,
	cstr sPrivate,
	cstr sExpected,
	cstr sOperation,
	test_ecdsa_sign_fn pSign
)
{
	uint8 Order[48];
	uint8 Hash[48];
	uint8 Private[48];
	uint8 Expected[96];
	uint8 Output[96];
	uint8 Before[96];
	uint8 Buffer[192];
	bool bUnavailable = true;

	testCryptoDecode(Order, iScalarSize, sOrder, "ECDSA sign order mismatch");
	testCryptoDecode(Hash, iScalarSize, sHash, "ECDSA sign hash mismatch");
	testCryptoDecode(
		Private, iScalarSize, sPrivate, "ECDSA sign private key mismatch"
	);
	testCryptoDecode(
		Expected,
		iScalarSize * 2u,
		sExpected,
		"ECDSA expected signature mismatch"
	);

	testRequire(pSign(Algorithm, Hash, Private, Output) &&
		xrtConstTimeEqual(Output, Expected, iScalarSize * 2u),
		"ECDSA RFC 6979 low-S signature mismatch");
	memset(Output, 0, sizeof(Output));
	testRequire(pSign(Algorithm, Hash, Private, Output) &&
		xrtConstTimeEqual(Output, Expected, iScalarSize * 2u),
		"ECDSA signature was not deterministic");

	memset(Buffer, 0, sizeof(Buffer));
	memcpy(Buffer, Hash, iScalarSize);
	memcpy(Buffer + 64, Private, iScalarSize);
	testRequire(pSign(Algorithm, Buffer, Buffer + 64, Buffer + 16) &&
		xrtConstTimeEqual(
			Buffer + 16, Expected, iScalarSize * 2u
		), "ECDSA overlapping sign mismatch");

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	memset(Private, 0, iScalarSize);
	xrtClearError();
	testRequire(!pSign(Algorithm, Hash, Private, Output) &&
		xrtConstTimeEqual(Output, Before, sizeof(Output)) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_KEY) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sOperation) == 0),
		"ECDSA zero private key contract mismatch");
	memcpy(Private, Order, iScalarSize);
	testRequire(!pSign(Algorithm, Hash, Private, Output) &&
		xrtConstTimeEqual(Output, Before, sizeof(Output)),
		"ECDSA order private key was accepted");
	testCryptoDecode(
		Private, iScalarSize, sPrivate, "ECDSA private key restore failed"
	);

	/* 负向断言只适用于当前裁剪闭包中确实缺失的 HMAC 后端。 */
	#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)
		if ( Unavailable == XCRYPTO_HASH_SHA256 ) {
			bUnavailable = false;
		}
	#endif
	#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)
		if ( (Unavailable == XCRYPTO_HASH_SHA384) ||
			(Unavailable == XCRYPTO_HASH_SHA512) ) {
			bUnavailable = false;
		}
	#endif
	if ( bUnavailable ) {
		memset(Output, 0xA5, sizeof(Output));
		memcpy(Before, Output, sizeof(Output));
		xrtClearError();
		testRequire(!pSign(Unavailable, Hash, Private, Output) &&
			xrtConstTimeEqual(Output, Before, sizeof(Output)) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
			"ECDSA accepted a digest backend omitted by trimming");
	}
	xrtClearError();
	testRequire(!pSign((xcryptohash)0, Hash, Private, Output) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ECDSA accepted an unknown digest algorithm");

	xrtClearError();
	testRequire(!pSign(Algorithm, NULL, Private, Output) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ECDSA null hash contract mismatch");
	testRequire(!pSign(Algorithm, Hash, NULL, Output),
		"ECDSA null private key was accepted");
	testRequire(!pSign(Algorithm, Hash, Private, NULL),
		"ECDSA null signature output was accepted");
}

#endif
