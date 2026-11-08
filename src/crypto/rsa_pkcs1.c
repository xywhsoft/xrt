#include "../internal/xrt_crypto_rsa_pkcs1.h"



#if defined(XRT_FEATURE_CRYPTO_RSA_PKCS1)

static const uint8 __xrtRsaSha1DigestInfo[] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x0E,
	0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14
};

static const uint8 __xrtRsaSha224DigestInfo[] = {
	0x30, 0x2D, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05,
	0x00, 0x04, 0x1C
};

static const uint8 __xrtRsaSha256DigestInfo[] = {
	0x30, 0x31, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};

static const uint8 __xrtRsaSha384DigestInfo[] = {
	0x30, 0x41, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05,
	0x00, 0x04, 0x30
};

static const uint8 __xrtRsaSha512DigestInfo[] = {
	0x30, 0x51, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
	0x00, 0x04, 0x40
};



/* 选择摘要对应的规范 DigestInfo 前缀和长度。 */
bool __xrtRsaDigestInfo(
	xcryptohash iHash,
	const uint8** pPrefix,
	size_t* pPrefixSize,
	size_t* pHashSize
)
{
	switch ( iHash ) {
		case XCRYPTO_HASH_SHA1:
			*pPrefix = __xrtRsaSha1DigestInfo;
			*pPrefixSize = sizeof(__xrtRsaSha1DigestInfo);
			*pHashSize = 20u;
			return true;
		case XCRYPTO_HASH_SHA224:
			*pPrefix = __xrtRsaSha224DigestInfo;
			*pPrefixSize = sizeof(__xrtRsaSha224DigestInfo);
			*pHashSize = 28u;
			return true;
		case XCRYPTO_HASH_SHA256:
			*pPrefix = __xrtRsaSha256DigestInfo;
			*pPrefixSize = sizeof(__xrtRsaSha256DigestInfo);
			*pHashSize = 32u;
			return true;
		case XCRYPTO_HASH_SHA384:
			*pPrefix = __xrtRsaSha384DigestInfo;
			*pPrefixSize = sizeof(__xrtRsaSha384DigestInfo);
			*pHashSize = 48u;
			return true;
		case XCRYPTO_HASH_SHA512:
			*pPrefix = __xrtRsaSha512DigestInfo;
			*pPrefixSize = sizeof(__xrtRsaSha512DigestInfo);
			*pHashSize = 64u;
			return true;
		default:
			return false;
	}
}



/* 设置统一的 RSA PKCS#1 验证失败错误。 */
static bool __xrtRsaPkcs1Fail(cstr sMessage, int iCode)
{
	__xrtRsaError("crypto.rsa.pkcs1.verify", sMessage, iCode);
	return false;
}



/* 严格验证带规范 DigestInfo 的 EMSA-PKCS1-v1_5 签名。 */
xrt_crypto_check __xrtRsaPkcs1Check(
	const xrsapublickey* pKey,
	xcryptohash iHash,
	const void* pHash,
	const void* pSignature,
	size_t iSignatureSize
)
{
	uint8 Encoded[XRT_RSA_MAX_MODULUS_SIZE];
	const uint8* pPrefix;
	size_t iPrefixSize;
	size_t iHashSize;
	size_t iPaddingSize;
	size_t iPosition;
	__xrt_rsa_result iPower;
	bool bValid = false;

	if ( (pKey == NULL) || (pHash == NULL) || (pSignature == NULL) ||
		 !__xrtRsaDigestInfo(
			iHash,
			&pPrefix,
			&iPrefixSize,
			&iHashSize
		) ) {
		return XRT_CRYPTO_CHECK_ARGUMENT;
	}
	if ( (iSignatureSize != pKey->ModulusSize) ||
		 (iSignatureSize > XRT_RSA_MAX_MODULUS_SIZE) ) {
		return XRT_CRYPTO_CHECK_SIGNATURE;
	}

	iPower = __xrtRsaPower(
		pKey,
		pSignature,
		iSignatureSize,
		Encoded
	);
	if ( iPower == XRT_RSA_RESULT_KEY ) {
		xrtSecureZero(Encoded, sizeof(Encoded));
		return XRT_CRYPTO_CHECK_KEY;
	}
	if ( iPower != XRT_RSA_RESULT_OK ) {
		xrtSecureZero(Encoded, sizeof(Encoded));
		return XRT_CRYPTO_CHECK_SIGNATURE;
	}

	if ( iSignatureSize < (3u + 8u + iPrefixSize + iHashSize) ) {
		goto cleanup;
	}
	iPaddingSize = iSignatureSize - 3u - iPrefixSize - iHashSize;
	if ( (Encoded[0] != 0) || (Encoded[1] != 1u) ||
		 (iPaddingSize < 8u) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iPaddingSize; i++ ) {
		if ( Encoded[2u + i] != 0xFFu ) {
			goto cleanup;
		}
	}
	iPosition = 2u + iPaddingSize;
	if ( Encoded[iPosition++] != 0 ) {
		goto cleanup;
	}
	if ( !xrtConstTimeEqual(
		Encoded + iPosition,
		pPrefix,
		iPrefixSize
	) ) {
		goto cleanup;
	}
	iPosition += iPrefixSize;
	bValid = xrtConstTimeEqual(
		Encoded + iPosition,
		pHash,
		iHashSize
	);

cleanup:
	xrtSecureZero(Encoded, sizeof(Encoded));
	return bValid ? XRT_CRYPTO_CHECK_VALID : XRT_CRYPTO_CHECK_SIGNATURE;
}

bool xrtRsaPkcs1Verify(
	const xrsapublickey* pKey, xcryptohash iHash, const void* pHash,
	const void* pSignature, size_t iSignatureSize
)
{
	xrt_crypto_check Result = __xrtRsaPkcs1Check(
		pKey, iHash, pHash, pSignature, iSignatureSize
	);

	if ( Result == XRT_CRYPTO_CHECK_VALID ) {
		return true;
	}
	if ( Result == XRT_CRYPTO_CHECK_ERROR ) {
		return false;
	}
	return __xrtRsaPkcs1Fail(
		Result == XRT_CRYPTO_CHECK_KEY ? "the RSA public key is invalid" :
		"the RSA PKCS#1 signature or arguments are invalid",
		Result == XRT_CRYPTO_CHECK_KEY ? XCRYPTO_ERROR_KEY : XCRYPTO_ERROR_SIGNATURE
	);
}

#endif
