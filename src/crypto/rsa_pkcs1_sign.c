#include "../internal/xrt_crypto_rsa_pkcs1.h"



#if defined(XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN)

/* 设置 RSA PKCS#1 签名失败错误并保持调用点简短。 */
static bool __xrtRsaPkcs1SignFail(cstr sMessage, int iCode)
{
	__xrtRsaError("crypto.rsa.pkcs1.sign", sMessage, iCode);
	return false;
}



/* 构造规范 EMSA-PKCS1-v1_5 编码并执行带复核的私钥运算。 */
bool xrtRsaPkcs1Sign(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	const void* pHash,
	void* pSignature
)
{
	uint8 Encoded[XRT_RSA_MAX_MODULUS_SIZE];
	const uint8* pPrefix;
	size_t iPrefixSize;
	size_t iHashSize;
	size_t iPaddingSize;
	size_t iModulusSize;
	__xrt_rsa_result iPower;
	bool bResult = false;

	if ( (pKey == NULL) || (pHash == NULL) || (pSignature == NULL) ||
		 !__xrtRsaDigestInfo(
			iHash,
			&pPrefix,
			&iPrefixSize,
			&iHashSize
		 ) ||
		 !__xrtRsaKeyValid(&pKey->Public) ) {
		return __xrtRsaPkcs1SignFail(
			"the RSA PKCS#1 signing arguments are invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
	}
	iModulusSize = pKey->Public.ModulusSize;
	if ( iModulusSize < (iPrefixSize + iHashSize + 11u) ) {
		return __xrtRsaPkcs1SignFail(
			"the RSA modulus is too short for the DigestInfo",
			XCRYPTO_ERROR_SIGNATURE
		);
	}

	iPaddingSize = iModulusSize - iPrefixSize - iHashSize - 3u;
	Encoded[0] = 0;
	Encoded[1] = 1u;
	memset(Encoded + 2u, 0xFF, iPaddingSize);
	Encoded[2u + iPaddingSize] = 0;
	memcpy(Encoded + 3u + iPaddingSize, pPrefix, iPrefixSize);
	memcpy(
		Encoded + 3u + iPaddingSize + iPrefixSize,
		pHash,
		iHashSize
	);

	iPower = __xrtRsaPrivatePower(
		pKey,
		Encoded,
		iModulusSize,
		pSignature
	);
	if ( iPower == XRT_RSA_RESULT_OK ) {
		bResult = true;
	}
	xrtSecureZero(Encoded, sizeof(Encoded));
	if ( !bResult ) {
		return __xrtRsaPkcs1SignFail(
			"the RSA PKCS#1 private key is invalid",
			XCRYPTO_ERROR_KEY
		);
	}
	return true;
}

#endif
