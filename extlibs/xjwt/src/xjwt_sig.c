/* 签名与验签：按算法分派到 xrt HMAC/RSA PKCS1/ECDSA P-256。 */
#include "xjwt_internal.h"

/* ------------------------------------------------------------------ */
/* 算法 → 哈希枚举                                                      */
/* ------------------------------------------------------------------ */
static xcryptohash alg_hash(int alg)
{
	switch ( alg ) {
	case XJWT_ALG_HS256: case XJWT_ALG_RS256: case XJWT_ALG_ES256:
		return XCRYPTO_HASH_SHA256;
	case XJWT_ALG_HS384: case XJWT_ALG_RS384:
		return XCRYPTO_HASH_SHA384;
	case XJWT_ALG_HS512: case XJWT_ALG_RS512:
		return XCRYPTO_HASH_SHA512;
	}
	return (xcryptohash)0;
}

/* ------------------------------------------------------------------ */
/* HMAC 签名（HS256/384/512）                                           */
/* ------------------------------------------------------------------ */
static bool hmac_sign(int alg, const void* pData, size_t iSize,
                      const char* sSecret,
                      unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	size_t iKeySize = strlen(sSecret);
	size_t iMacSize = xrtCryptoHashSize(alg_hash(alg));
	if ( iCapacity < iMacSize ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "output buffer too small for HMAC");
		return false;
	}
	switch ( alg ) {
	case XJWT_ALG_HS256:
		if ( !xrtHmacSha256(sSecret, iKeySize, pData, iSize, pOut) ) return false;
		break;
	case XJWT_ALG_HS384:
		if ( !xrtHmacSha384(sSecret, iKeySize, pData, iSize, pOut) ) return false;
		break;
	case XJWT_ALG_HS512:
		if ( !xrtHmacSha512(sSecret, iKeySize, pData, iSize, pOut) ) return false;
		break;
	default: return false;
	}
	*pOutSize = iMacSize;
	return true;
}

/* ------------------------------------------------------------------ */
/* RSA PEM 解析（SPKI 或 PKCS#1 → xrt 公钥视图）                        */
/* ------------------------------------------------------------------ */
bool xjwt__rsa_public_parse(const char* sPem, xrsapublickey* pKey,
                            unsigned char** ppOwned)
{
	xpemblock tBlock;
	if ( !xrtPemFind(sPem, strlen(sPem), "PUBLIC KEY", &tBlock) ) {
		/* 也试 PKCS#1 标签 */
		if ( !xrtPemFind(sPem, strlen(sPem), "RSA PUBLIC KEY", &tBlock) ) {
			xjwt__error(XJWT_ERROR_PARSE, "PEM public key not found");
			return false;
		}
	}
	size_t iDerSize = 0;
	bytes pDer = xrtPemDecodeNew(&tBlock, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM decode failed");
		return false;
	}

	/* SPKI DER：SEQ { SEQ { OID, NULL }, BIT STRING { SEQ { INTEGER n, INTEGER e } } }
	 * PKCS#1 DER：SEQ { INTEGER n, INTEGER e }
	 * 我们简化：手动走 DER 找两个 INTEGER（n 和 e）。 */
	xdercursor tCur;
	xdervalue tVal;
	unsigned char* pModulus = NULL;
	unsigned char* pExponent = NULL;
	size_t iModSize = 0, iExpSize = 0;

	xrtDerInit(&tCur, pDer, iDerSize);
	/* 外层 SEQ */
	if ( xrtDerRead(&tCur, &tVal) != XDER_VALUE ) goto fail;
	{
		/* 尝试 SPKI：内层第一个是 SEQ（AlgorithmIdentifier），跳过后 BIT STRING */
		xdercursor tInner;
		xrtDerInit(&tInner, tVal.Value.Data, tVal.Value.Size);
		if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ) goto fail;
		if ( tVal.Tag.Number == XASN1_SEQUENCE ) {
			/* SPKI：跳过 AlgorithmIdentifier，读 BIT STRING */
			if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ) goto fail;
			if ( tVal.Tag.Number != XASN1_BIT_STRING ) goto fail;
			/* BIT STRING 第一字节是 unused-bits 计数 */
			{
				const unsigned char* p = (const unsigned char*)tVal.Value.Data;
				size_t n = tVal.Value.Size;
				if ( n < 1 ) goto fail;
				p++; n--;
				/* 内层 SEQ { INTEGER n, INTEGER e } */
				xdercursor tKey;
				xrtDerInit(&tKey, p, n);
				if ( xrtDerRead(&tKey, &tVal) != XDER_VALUE || tVal.Tag.Number != XASN1_SEQUENCE ) goto fail;
				/* 在 SEQ 内容上重开游标读两个 INTEGER */
				xrtDerInit(&tKey, tVal.Value.Data, tVal.Value.Size);
				if ( xrtDerRead(&tKey, &tVal) != XDER_VALUE || tVal.Tag.Number != XASN1_INTEGER ) goto fail;
				iModSize = tVal.Value.Size;
				pModulus = (unsigned char*)xrtMalloc(iModSize);
				if ( pModulus ) memcpy(pModulus, tVal.Value.Data, iModSize);
				if ( xrtDerRead(&tKey, &tVal) != XDER_VALUE || tVal.Tag.Number != XASN1_INTEGER ) goto fail;
				iExpSize = tVal.Value.Size;
				pExponent = (unsigned char*)xrtMalloc(iExpSize);
				if ( pExponent ) memcpy(pExponent, tVal.Value.Data, iExpSize);
			}
		} else if ( tVal.Tag.Number == XASN1_INTEGER ) {
			/* PKCS#1：SEQ { INTEGER n, INTEGER e } */
			iModSize = tVal.Value.Size;
			pModulus = (unsigned char*)xrtMalloc(iModSize);
			if ( pModulus ) memcpy(pModulus, tVal.Value.Data, iModSize);
			if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE || tVal.Tag.Number != XASN1_INTEGER ) goto fail;
			iExpSize = tVal.Value.Size;
			pExponent = (unsigned char*)xrtMalloc(iExpSize);
			if ( pExponent ) memcpy(pExponent, tVal.Value.Data, iExpSize);
		} else {
			goto fail;
		}
	}

	if ( pModulus == NULL || pExponent == NULL ) goto fail;
	xjwt__int_trim(pModulus, &iModSize);
	xjwt__int_trim(pExponent, &iExpSize);
	pKey->Modulus = pModulus;
	pKey->ModulusSize = iModSize;
	pKey->Exponent = pExponent;
	pKey->ExponentSize = iExpSize;
	{
		unsigned char** ppArr = (unsigned char**)xrtMalloc(2 * sizeof(void*));
		if ( ppArr == NULL ) goto fail;
		ppArr[0] = pModulus;
		ppArr[1] = pExponent;
		*ppOwned = (unsigned char*)ppArr;
	}
	xrtFree(pDer);
	return true;

fail:
	if ( pModulus ) xrtFree(pModulus);
	if ( pExponent ) xrtFree(pExponent);
	xrtFree(pDer);
	xjwt__error(XJWT_ERROR_PARSE, "RSA public key DER parse failed");
	return false;
}

void xjwt__rsa_public_free(xrsapublickey* pKey, unsigned char* pOwned)
{
	if ( pOwned != NULL ) {
		unsigned char** ppArr = (unsigned char**)pOwned;
		xrtFree(ppArr[0]);
		xrtFree(ppArr[1]);
		xrtFree(pOwned);
	}
	(void)pKey;
}

/* ------------------------------------------------------------------ */
/* RSA 签名                                                            */
/* ------------------------------------------------------------------ */
static bool rsa_sign(int alg, const void* pData, size_t iSize,
                     const char* sPrivatePem,
                     unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	return rsa_sign_impl(alg, pData, iSize, sPrivatePem, pOut, iCapacity, pOutSize);
}

/* ------------------------------------------------------------------ */
/* ECDSA P-256                                                         */
/* ------------------------------------------------------------------ */
/* ECDSA 公钥解析完整版在 xjwt_ext.c */

/* ------------------------------------------------------------------ */
/* 分派                                                                */
/* ------------------------------------------------------------------ */
bool xjwt__sign(int alg, const void* pData, size_t iSize,
                const char* sKeyPem, unsigned char* pOut,
                size_t iCapacity, size_t* pOutSize)
{
	switch ( alg ) {
	case XJWT_ALG_HS256: case XJWT_ALG_HS384: case XJWT_ALG_HS512:
		return hmac_sign(alg, pData, iSize, sKeyPem, pOut, iCapacity, pOutSize);
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512:
		return rsa_sign(alg, pData, iSize, sKeyPem, pOut, iCapacity, pOutSize);
	case XJWT_ALG_ES256:
		return es256_sign_impl(pData, iSize, sKeyPem, pOut, iCapacity, pOutSize);
	}
	xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unsupported algorithm");
	return false;
}

bool xjwt__verify(int alg, const void* pData, size_t iSize,
                  const char* sKeyPem, const void* pSig, size_t iSigSize)
{
	/* 安全检查：拒绝 alg=none / alg 未知 */
	if ( alg <= XJWT_ALG_NONE || alg == XJWT_ALG_INVALID ) {
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "algorithm 'none' or invalid rejected");
		return false;
	}
	/* 安全检查：key 类型必须与算法族匹配（防算法混淆攻击）
	 * HS 族只能收 HMAC 密钥；RS 族与 ES 族只能收 PEM 公钥 */
	bool bIsPem = sKeyPem != NULL && strlen(sKeyPem) > 20 &&
	              strstr(sKeyPem, "-----BEGIN") != NULL;
	switch ( alg ) {
	case XJWT_ALG_HS256: case XJWT_ALG_HS384: case XJWT_ALG_HS512:
		if ( bIsPem ) {
			xjwt__error(XJWT_ERROR_ALG_MISMATCH,
				"HMAC algorithm with PEM key rejected (alg confusion)");
			return false;
		}
		break;
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512: case XJWT_ALG_ES256:
		if ( !bIsPem ) {
			xjwt__error(XJWT_ERROR_ALG_MISMATCH,
				"RSA/EC algorithm requires PEM key (alg confusion)");
			return false;
		}
		break;
	}
	xcryptohash iHash = alg_hash(alg);
	if ( iHash == 0 ) {
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unsupported algorithm");
		return false;
	}

	switch ( alg ) {
	case XJWT_ALG_HS256: {
		unsigned char aMac[32];
		if ( !xrtHmacSha256(sKeyPem, strlen(sKeyPem), pData, iSize, aMac) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS256 HMAC computation failed");
			return false;
		}
		if ( iSigSize != 32 ) { xjwt__error(XJWT_ERROR_SIGNATURE, "HS256 signature size mismatch"); return false; }
		if ( !xrtConstTimeEqual(aMac, pSig, 32) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS256 signature mismatch");
			return false;
		}
		return true;
	}
	case XJWT_ALG_HS384: {
		unsigned char aMac[48];
		if ( !xrtHmacSha384(sKeyPem, strlen(sKeyPem), pData, iSize, aMac) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS384 HMAC computation failed");
			return false;
		}
		if ( iSigSize != 48 ) { xjwt__error(XJWT_ERROR_SIGNATURE, "HS384 signature size mismatch"); return false; }
		if ( !xrtConstTimeEqual(aMac, pSig, 48) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS384 signature mismatch");
			return false;
		}
		return true;
	}
	case XJWT_ALG_HS512: {
		unsigned char aMac[64];
		if ( !xrtHmacSha512(sKeyPem, strlen(sKeyPem), pData, iSize, aMac) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS512 HMAC computation failed");
			return false;
		}
		if ( iSigSize != 64 ) { xjwt__error(XJWT_ERROR_SIGNATURE, "HS512 signature size mismatch"); return false; }
		if ( !xrtConstTimeEqual(aMac, pSig, 64) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS512 signature mismatch");
			return false;
		}
		return true;
	}
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512: {
		xrsapublickey tKey;
		unsigned char* pOwned = NULL;
		if ( !xjwt__rsa_public_parse(sKeyPem, &tKey, &pOwned) )
			return false;
		bool ok = xjwt__verify_rsa_raw(pData, iSize, pSig, iSigSize, &tKey, alg);
		xjwt__rsa_public_free(&tKey, pOwned);
		if ( !ok ) xjwt__error(XJWT_ERROR_SIGNATURE, "RSA signature mismatch");
		return ok;
	}
	case XJWT_ALG_ES256: {
		bool ok = xjwt__verify_es256_full(pData, iSize, sKeyPem, pSig, iSigSize);
		if ( !ok ) xjwt__error(XJWT_ERROR_SIGNATURE, "ES256 signature mismatch");
		return ok;
	}
	}
	xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unsupported algorithm");
	return false;
}

bool xjwt__verify_rsa_raw(const void* pData, size_t iSize,
                          const void* pSig, size_t iSigSize,
                          const xrsapublickey* pKey, int alg)
{
	xcryptohash iHash = alg_hash(alg);
	unsigned char aDigest[64];
	switch ( iHash ) {
	case XCRYPTO_HASH_SHA256:
		if ( !xrtSha256(pData, iSize, aDigest) ) return false;
		break;
	case XCRYPTO_HASH_SHA384:
		if ( !xrtSha384(pData, iSize, aDigest) ) return false;
		break;
	case XCRYPTO_HASH_SHA512:
		if ( !xrtSha512(pData, iSize, aDigest) ) return false;
		break;
	default: return false;
	}
	return xrtRsaPkcs1Verify(pKey, iHash, aDigest, pSig, iSigSize);
}

bool xjwt__verify_es256_raw(const void* pData, size_t iSize,
                            const void* pSig, size_t iSigSize,
                            const unsigned char* pPublic65)
{
	if ( iSigSize < 8 || iSigSize > 72 ) return false;
	unsigned char aDigest[32];
	if ( !xrtSha256(pData, iSize, aDigest) ) return false;
	/* RFC 7518 §3.4：JWS 的 ES256 签名是 DER 编码（非 raw r||s） */
	return xrtEcdsaP256VerifyDer(aDigest, 32, pSig, iSigSize, pPublic65);
}
