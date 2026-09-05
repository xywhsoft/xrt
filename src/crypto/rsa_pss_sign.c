#include "../internal/xrt_crypto_rsa_pss.h"



#if defined(XRT_FEATURE_CRYPTO_RSA_PSS_SIGN)

/* 设置 RSA-PSS 签名失败错误并保持调用点简短。 */
static bool __xrtRsaPssSignFail(cstr sMessage, int iCode)
{
	__xrtRsaError("crypto.rsa.pss.sign", sMessage, iCode);
	return false;
}



/* 使用显式盐构造 EMSA-PSS 编码并执行带复核的私钥运算。 */
bool xrtRsaPssSignSalt(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	const void* pSalt,
	size_t iSaltSize,
	const void* pHash,
	void* pSignature
)
{
	uint8 Encoded[XRT_RSA_MAX_MODULUS_SIZE] = { 0 };
	uint8 Digest[XRT_SHA512_SIZE];
	uint8* pMessage;
	uint8* pDatabase;
	size_t iHashSize = __xrtRsaHashSize(iHash);
	size_t iMaskHashSize = __xrtRsaHashSize(iMaskHash);
	size_t iModulusBits;
	size_t iEncodedBits;
	size_t iEncodedSize;
	size_t iDatabaseSize;
	size_t iPaddingSize;
	size_t iOffset;
	uint8 iUnusedBits;
	__xrt_rsa_result iPower = XRT_RSA_RESULT_KEY;
	bool bResult = false;

	if ( (pKey == NULL) || (pHash == NULL) || (pSignature == NULL) ||
		 ((pSalt == NULL) && (iSaltSize != 0)) ||
		 (iHashSize == 0) || (iMaskHashSize == 0) ||
		 !__xrtRsaKeyValid(&pKey->Public) ) {
		return __xrtRsaPssSignFail(
			"the RSA-PSS signing arguments are invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
	}

	iModulusBits = __xrtRsaModulusBits(&pKey->Public);
	iEncodedBits = iModulusBits - 1u;
	iEncodedSize = (iEncodedBits + 7u) >> 3u;
	if ( (iEncodedSize < (iHashSize + 2u)) ||
		 (iSaltSize > (iEncodedSize - iHashSize - 2u)) ) {
		return __xrtRsaPssSignFail(
			"the RSA modulus is too short for the requested PSS salt",
			XCRYPTO_ERROR_SIGNATURE
		);
	}
	iOffset = pKey->Public.ModulusSize - iEncodedSize;
	pMessage = Encoded + iOffset;
	pDatabase = pMessage;
	iDatabaseSize = iEncodedSize - iHashSize - 1u;
	iPaddingSize = iDatabaseSize - iSaltSize - 1u;

	/* H = Hash(0x00 * 8 || messageHash || salt)。 */
	if ( !__xrtRsaPssHash(
		iHash,
		pHash,
		pSalt,
		iSaltSize,
		Digest
	) ) {
		goto cleanup;
	}

	/* DB = PS || 0x01 || salt，再与 MGF1(H) 异或。 */
	memset(pDatabase, 0, iPaddingSize);
	pDatabase[iPaddingSize] = 1u;
	if ( iSaltSize != 0 ) {
		memcpy(pDatabase + iPaddingSize + 1u, pSalt, iSaltSize);
	}
	if ( !__xrtRsaMgf1Xor(
		iMaskHash,
		Digest,
		iHashSize,
		pDatabase,
		iDatabaseSize
	) ) {
		goto cleanup;
	}
	iUnusedBits = (uint8)((iEncodedSize * 8u) - iEncodedBits);
	if ( iUnusedBits != 0 ) {
		pDatabase[0] &= (uint8)(0xFFu >> iUnusedBits);
	}
	memcpy(pMessage + iDatabaseSize, Digest, iHashSize);
	pMessage[iEncodedSize - 1u] = 0xBCu;

	iPower = __xrtRsaPrivatePower(
		pKey,
		Encoded,
		pKey->Public.ModulusSize,
		pSignature
	);
	if ( iPower != XRT_RSA_RESULT_OK ) {
		goto cleanup;
	}
	bResult = true;

cleanup:
	xrtSecureZero(Encoded, sizeof(Encoded));
	xrtSecureZero(Digest, sizeof(Digest));
	if ( iPower == XRT_RSA_RESULT_RANDOM ) {
		return false;
	}
	if ( !bResult ) {
		return __xrtRsaPssSignFail(
			"the RSA-PSS private key or encoding is invalid",
			XCRYPTO_ERROR_KEY
		);
	}
	return true;
}



/* 使用与消息摘要等长的系统安全随机盐生成 PSS 签名。 */
bool xrtRsaPssSign(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	const void* pHash,
	void* pSignature
)
{
	uint8 Salt[XRT_SHA512_SIZE];
	size_t iSaltSize = __xrtRsaHashSize(iHash);
	bool bResult;

	if ( (pKey == NULL) || (pHash == NULL) || (pSignature == NULL) ||
		 (iSaltSize == 0) || !__xrtRsaKeyValid(&pKey->Public) ) {
		return __xrtRsaPssSignFail(
			"the RSA-PSS signing arguments are invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
	}
	if ( !xrtSecureRandom(Salt, iSaltSize) ) {
		xrtSecureZero(Salt, sizeof(Salt));
		return false;
	}
	bResult = xrtRsaPssSignSalt(
		pKey,
		iHash,
		iMaskHash,
		Salt,
		iSaltSize,
		pHash,
		pSignature
	);
	xrtSecureZero(Salt, sizeof(Salt));
	return bResult;
}

#endif
