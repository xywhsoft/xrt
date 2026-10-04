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
static bool rsa_public_der_parse(const void* pData, size_t iSize,
	bool bSpki, xbytesview* pModulus, xbytesview* pExponent)
{
	static const unsigned char aRsaOid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
	};
	xdercursor Outer, Body, Key;
	xdervalue Value;

	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Body) ) {
		return false;
	}
	if ( bSpki ) {
		xdercursor Algorithm;
		xbytesview EncodedKey;
		uint8 iUnused;

		if ( xrtDerRead(&Body, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
			!xrtDerEnter(&Value, &Algorithm) ||
			xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
			!xrtDerOidEqual(&Value, aRsaOid, sizeof(aRsaOid)) ||
			xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_NULL, false) ||
			(Value.Value.Size != 0) || !xrtDerDone(&Algorithm) ||
			xrtDerRead(&Body, &Value) != XDER_VALUE ||
			!xrtDerBitString(&Value, &EncodedKey, &iUnused) ||
			(iUnused != 0) || !xrtDerDone(&Body) ||
			!xrtDerInit(&Key, EncodedKey.Data, EncodedKey.Size) ||
			xrtDerRead(&Key, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
			!xrtDerDone(&Key) || !xrtDerEnter(&Value, &Key) ) {
			return false;
		}
	} else {
		Key = Body;
	}
	return xrtDerRead(&Key, &Value) == XDER_VALUE &&
		xrtDerUnsigned(&Value, pModulus) &&
		xrtDerRead(&Key, &Value) == XDER_VALUE &&
		xrtDerUnsigned(&Value, pExponent) && xrtDerDone(&Key);
}

bool xjwt__rsa_public_parse(const char* sPem, xrsapublickey* pKey,
                            unsigned char** ppOwned)
{
	xpemblock tBlock;
	xbytesview Modulus, Exponent;
	xrsapublickey Parsed;
	unsigned char *pModulus = NULL, *pExponent = NULL, **ppArr = NULL;
	bool bSpki;
	size_t iPemSize;

	if ( (sPem == NULL) || (pKey == NULL) || (ppOwned == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "RSA public key argument is null");
		return false;
	}
	iPemSize = strlen(sPem);
	bSpki = xrtPemFind(sPem, iPemSize, "PUBLIC KEY", &tBlock);
	if ( !bSpki &&
		!xrtPemFind(sPem, iPemSize, "RSA PUBLIC KEY", &tBlock) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM public key not found");
		return false;
	}
	size_t iDerSize = 0;
	bytes pDer = xrtPemDecodeNew(&tBlock, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	if ( !rsa_public_der_parse(pDer, iDerSize, bSpki,
		&Modulus, &Exponent) ) {
		goto invalid;
	}
	Parsed.Modulus = Modulus.Data;
	Parsed.ModulusSize = Modulus.Size;
	Parsed.Exponent = Exponent.Data;
	Parsed.ExponentSize = Exponent.Size;
	if ( !xjwt__rsa_jwa_key_valid(&Parsed) ) goto invalid;
	pModulus = (unsigned char*)xrtMalloc(Modulus.Size);
	pExponent = (unsigned char*)xrtMalloc(Exponent.Size);
	ppArr = (unsigned char**)xrtMalloc(2u * sizeof(*ppArr));
	if ( (pModulus == NULL) || (pExponent == NULL) || (ppArr == NULL) ) {
		goto cleanup;
	}
	memcpy(pModulus, Modulus.Data, Modulus.Size);
	memcpy(pExponent, Exponent.Data, Exponent.Size);
	ppArr[0] = pModulus;
	ppArr[1] = pExponent;
	pKey->Modulus = pModulus;
	pKey->ModulusSize = Modulus.Size;
	pKey->Exponent = pExponent;
	pKey->ExponentSize = Exponent.Size;
	*ppOwned = (unsigned char*)ppArr;
	xrtFree(pDer);
	return true;

invalid:
	xjwt__error(XJWT_ERROR_PARSE, "RSA public key DER parse failed");
cleanup:
	xrtFree(pModulus);
	xrtFree(pExponent);
	xrtFree(ppArr);
	xrtFree(pDer);
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
			xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
				"HS256 HMAC computation failed");
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
			xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
				"HS384 HMAC computation failed");
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
			xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
				"HS512 HMAC computation failed");
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
		if ( !ok ) xjwt__error_unless_memory(
			XJWT_ERROR_SIGNATURE, "RSA signature mismatch");
		return ok;
	}
	case XJWT_ALG_ES256: {
		bool ok = xjwt__verify_es256_full(pData, iSize, sKeyPem, pSig, iSigSize);
		if ( !ok ) xjwt__error_unless_memory(
			XJWT_ERROR_SIGNATURE, "ES256 signature mismatch");
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
	if ( !xjwt__rsa_jwa_key_valid(pKey) ) return false;
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
	if ( iSigSize != 64 ) return false;
	unsigned char aDigest[32];
	if ( !xrtSha256(pData, iSize, aDigest) ) return false;
	return xrtEcdsaP256Verify(aDigest, 32, pSig, pPublic65);
}
