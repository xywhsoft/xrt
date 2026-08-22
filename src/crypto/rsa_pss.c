#include "../internal/xrt_crypto_rsa_pss.h"



#if defined(XRT_FEATURE_CRYPTO_RSA_PSS)

/* 返回受支持摘要的固定输出长度。 */
size_t __xrtRsaHashSize(xcryptohash iHash)
{
	return xrtCryptoHashSize(iHash);
}



/* 使用指定摘要算法计算一段连续数据。 */
bool __xrtRsaHash(
	xcryptohash iHash,
	const void* pData,
	size_t iSize,
	void* pDigest
)
{
	switch ( iHash ) {
		case XCRYPTO_HASH_SHA1:
			return xrtSha1(pData, iSize, pDigest);
		case XCRYPTO_HASH_SHA224:
			return xrtSha224(pData, iSize, pDigest);
		case XCRYPTO_HASH_SHA256:
			return xrtSha256(pData, iSize, pDigest);
		case XCRYPTO_HASH_SHA384:
			return xrtSha384(pData, iSize, pDigest);
		case XCRYPTO_HASH_SHA512:
			return xrtSha512(pData, iSize, pDigest);
		default:
			return false;
	}
}



/* 以流式摘要计算 PSS 的 M'，避免盐长度扩大固定栈缓冲。 */
bool __xrtRsaPssHash(
	xcryptohash iHash,
	const void* pHash,
	const void* pSalt,
	size_t iSaltSize,
	void* pDigest
)
{
	static const uint8 Zero[8] = { 0 };
	union {
		xsha1 Sha1;
		xsha224 Sha224;
		xsha256 Sha256;
		xsha512 Sha512;
	} State;
	bool bResult = false;

	switch ( iHash ) {
		case XCRYPTO_HASH_SHA1:
			xrtSha1Init(&State.Sha1);
			bResult = xrtSha1Update(&State.Sha1, Zero, sizeof(Zero)) &&
				xrtSha1Update(&State.Sha1, pHash, XRT_SHA1_SIZE) &&
				xrtSha1Update(&State.Sha1, pSalt, iSaltSize) &&
				xrtSha1Final(&State.Sha1, pDigest);
			break;
		case XCRYPTO_HASH_SHA224:
			xrtSha224Init(&State.Sha224);
			bResult = xrtSha224Update(&State.Sha224, Zero, sizeof(Zero)) &&
				xrtSha224Update(&State.Sha224, pHash, XRT_SHA224_SIZE) &&
				xrtSha224Update(&State.Sha224, pSalt, iSaltSize) &&
				xrtSha224Final(&State.Sha224, pDigest);
			break;
		case XCRYPTO_HASH_SHA256:
			xrtSha256Init(&State.Sha256);
			bResult = xrtSha256Update(&State.Sha256, Zero, sizeof(Zero)) &&
				xrtSha256Update(&State.Sha256, pHash, XRT_SHA256_SIZE) &&
				xrtSha256Update(&State.Sha256, pSalt, iSaltSize) &&
				xrtSha256Final(&State.Sha256, pDigest);
			break;
		case XCRYPTO_HASH_SHA384:
			xrtSha384Init(&State.Sha512);
			bResult = xrtSha384Update(&State.Sha512, Zero, sizeof(Zero)) &&
				xrtSha384Update(&State.Sha512, pHash, XRT_SHA384_SIZE) &&
				xrtSha384Update(&State.Sha512, pSalt, iSaltSize) &&
				xrtSha384Final(&State.Sha512, pDigest);
			break;
		case XCRYPTO_HASH_SHA512:
			xrtSha512Init(&State.Sha512);
			bResult = xrtSha512Update(&State.Sha512, Zero, sizeof(Zero)) &&
				xrtSha512Update(&State.Sha512, pHash, XRT_SHA512_SIZE) &&
				xrtSha512Update(&State.Sha512, pSalt, iSaltSize) &&
				xrtSha512Final(&State.Sha512, pDigest);
			break;
		default:
			break;
	}
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 分块生成 MGF1 并立即异或目标，避免为模数宽度另建掩码。 */
bool __xrtRsaMgf1Xor(
	xcryptohash iHash,
	const uint8* pSeed,
	size_t iSeedSize,
	uint8* pData,
	size_t iDataSize
)
{
	uint8 Input[XRT_SHA512_SIZE + 4u];
	uint8 Digest[XRT_SHA512_SIZE];
	size_t iHashSize = __xrtRsaHashSize(iHash);
	size_t iOffset = 0;
	uint32 iCounter = 0;
	bool bResult = false;

	if ( (iHashSize == 0) || (iSeedSize > XRT_SHA512_SIZE) ) {
		goto cleanup;
	}
	memcpy(Input, pSeed, iSeedSize);
	while ( iOffset < iDataSize ) {
		size_t iCopy;

		__xrtCryptoStoreBe32(Input + iSeedSize, iCounter);
		if ( !__xrtRsaHash(iHash, Input, iSeedSize + 4u, Digest) ) {
			goto cleanup;
		}
		iCopy = iDataSize - iOffset;
		if ( iCopy > iHashSize ) {
			iCopy = iHashSize;
		}
		for ( size_t i = 0; i < iCopy; i++ ) {
			pData[iOffset + i] ^= Digest[i];
		}
		iOffset += iCopy;
		iCounter++;
	}
	bResult = true;

cleanup:
	xrtSecureZero(Input, sizeof(Input));
	xrtSecureZero(Digest, sizeof(Digest));
	return bResult;
}



/* 返回大端模数去除前导零后的真实位数。 */
size_t __xrtRsaModulusBits(const xrsapublickey* pKey)
{
	const uint8* pModulus = (const uint8*)pKey->Modulus;
	uint8 iTop = pModulus[0];
	size_t iBits = (pKey->ModulusSize - 1u) * 8u;

	while ( iTop != 0 ) {
		iBits++;
		iTop >>= 1u;
	}
	return iBits;
}



/* 设置统一的 RSA-PSS 验证失败错误。 */
static bool __xrtRsaPssFail(cstr sMessage, int iCode)
{
	__xrtRsaError("crypto.rsa.pss.verify", sMessage, iCode);
	return false;
}



/* 严格验证 EMSA-PSS 签名，可分别指定消息摘要与 MGF1 摘要。 */
bool xrtRsaPssVerify(
	const xrsapublickey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	size_t iSaltSize,
	const void* pHash,
	const void* pSignature,
	size_t iSignatureSize
)
{
	uint8 Encoded[XRT_RSA_MAX_MODULUS_SIZE];
	uint8 Expected[XRT_SHA512_SIZE];
	const uint8* pHashBytes = (const uint8*)pHash;
	uint8* pMessage;
	uint8* pDatabase;
	uint8* pEncodedHash;
	size_t iHashSize = __xrtRsaHashSize(iHash);
	size_t iMaskHashSize = __xrtRsaHashSize(iMaskHash);
	size_t iModulusBits;
	size_t iEncodedBits;
	size_t iEncodedSize;
	size_t iOffset;
	size_t iDatabaseSize;
	size_t iDelimiter;
	size_t iActualSalt;
	uint8 iUnusedBits;
	__xrt_rsa_result iPower;
	bool bValid = false;

	if ( (pKey == NULL) || (pHash == NULL) || (pSignature == NULL) ||
		 (iHashSize == 0) || (iMaskHashSize == 0) ) {
		return __xrtRsaPssFail(
			"the RSA-PSS arguments are invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
	}
	if ( (iSignatureSize != pKey->ModulusSize) ||
		 (iSignatureSize > XRT_RSA_MAX_MODULUS_SIZE) ) {
		return __xrtRsaPssFail(
			"the RSA-PSS signature length is invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
	}

	iPower = __xrtRsaPower(
		pKey,
		pSignature,
		iSignatureSize,
		Encoded
	);
	if ( iPower == XRT_RSA_RESULT_KEY ) {
		return __xrtRsaPssFail(
			"the RSA public key is invalid",
			XCRYPTO_ERROR_KEY
		);
	}
	if ( iPower != XRT_RSA_RESULT_OK ) {
		return __xrtRsaPssFail(
			"the RSA-PSS signature representative is invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
	}

	iModulusBits = __xrtRsaModulusBits(pKey);
	iEncodedBits = iModulusBits - 1u;
	iEncodedSize = (iEncodedBits + 7u) >> 3u;
	if ( iEncodedSize < (iHashSize + 2u) ) {
		goto cleanup;
	}
	iOffset = iSignatureSize - iEncodedSize;
	for ( size_t i = 0; i < iOffset; i++ ) {
		bValid |= Encoded[i] != 0;
	}
	if ( bValid ) {
		bValid = false;
		goto cleanup;
	}
	pMessage = Encoded + iOffset;
	iDatabaseSize = iEncodedSize - iHashSize - 1u;
	pDatabase = pMessage;
	pEncodedHash = pMessage + iDatabaseSize;
	if ( pMessage[iEncodedSize - 1u] != 0xBCu ) {
		goto cleanup;
	}
	iUnusedBits = (uint8)((iEncodedSize * 8u) - iEncodedBits);
	if ( (iUnusedBits != 0) &&
		 ((pDatabase[0] >> (8u - iUnusedBits)) != 0) ) {
		goto cleanup;
	}
	if ( !__xrtRsaMgf1Xor(
		iMaskHash,
		pEncodedHash,
		iHashSize,
		pDatabase,
		iDatabaseSize
	) ) {
		goto cleanup;
	}
	if ( iUnusedBits != 0 ) {
		pDatabase[0] &= (uint8)(0xFFu >> iUnusedBits);
	}

	if ( iSaltSize == XRT_RSA_PSS_SALT_ANY ) {
		iDelimiter = 0;
		while ( (iDelimiter < iDatabaseSize) &&
			 (pDatabase[iDelimiter] == 0) ) {
			iDelimiter++;
		}
		if ( (iDelimiter == iDatabaseSize) ||
			 (pDatabase[iDelimiter] != 1u) ) {
			goto cleanup;
		}
		iActualSalt = iDatabaseSize - iDelimiter - 1u;
	} else {
		if ( iSaltSize > (iDatabaseSize - 1u) ) {
			goto cleanup;
		}
		iDelimiter = iDatabaseSize - iSaltSize - 1u;
		for ( size_t i = 0; i < iDelimiter; i++ ) {
			if ( pDatabase[i] != 0 ) {
				goto cleanup;
			}
		}
		if ( pDatabase[iDelimiter] != 1u ) {
			goto cleanup;
		}
		iActualSalt = iSaltSize;
	}

	if ( !__xrtRsaPssHash(
		iHash,
		pHashBytes,
		pDatabase + iDelimiter + 1u,
		iActualSalt,
		Expected
	) ) {
		goto cleanup;
	}
	bValid = xrtConstTimeEqual(Expected, pEncodedHash, iHashSize);

cleanup:
	xrtSecureZero(Encoded, sizeof(Encoded));
	xrtSecureZero(Expected, sizeof(Expected));
	if ( !bValid ) {
		return __xrtRsaPssFail(
			"the RSA-PSS signature is invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
	}
	return true;
}

#endif
