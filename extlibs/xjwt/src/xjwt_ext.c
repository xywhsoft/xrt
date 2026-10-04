/* RSA 私钥 + EC 公钥/私钥 PEM 解析 + RS/ES 签发验签 + JWKS。 */
#include "xjwt_internal.h"

/* ------------------------------------------------------------------ */
/* Base64URL 解码（复用 xjwt_core.c 的实现，这里做 JWK 字段用）          */
/* ------------------------------------------------------------------ */
static unsigned char* b64url_dec(const char* s, size_t n, size_t* out)
{
	return xjwt__base64url_decode(s, n, out);
}

/* ------------------------------------------------------------------ */
/* RSA 私钥 PEM 解析（PKCS#8 或 PKCS#1 → xrsaprivatekey）              */
/* ------------------------------------------------------------------ */
typedef struct xjwt__rsa_owned {
	unsigned char* pRaw;        /* 私钥组件借用此 DER 存储 */
	size_t iRawSize;
} xjwt__rsa_owned;

static void xjwt__rsa_owned_free(xjwt__rsa_owned* p)
{
	if ( p == NULL ) return;
	if ( p->pRaw ) {
		xrtSecureZero(p->pRaw, p->iRawSize);
		xrtFree(p->pRaw);
	}
	xrtFree(p);
}

static bool rsa_private_pkcs1_parse(const void* pData, size_t iSize,
	xbytesview aParts[8])
{
	xdercursor Outer, Fields;
	xdervalue Value;
	uint64 iVersion;

	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion != 0) ) {
		return false;
	}
	for ( size_t i = 0; i < 8u; i++ ) {
		if ( xrtDerRead(&Fields, &Value) != XDER_VALUE ||
			!xrtDerUnsigned(&Value, &aParts[i]) ) return false;
	}
	return xrtDerDone(&Fields);
}

/* RFC 5208/5958 [0] IMPLICIT SET OF Attribute: validate but ignore metadata. */
static bool pkcs8_attributes_parse(const xdervalue* pAttributeField)
{
	xdercursor Attributes;
	xdervalue Value;
	xbytesview Previous = { NULL, 0 };

	if ( !xrtDerEnter(pAttributeField, &Attributes) ) {
		return false;
	}
	while ( !xrtDerDone(&Attributes) ) {
		xdercursor Attribute, Values;
		xbytesview Oid, Current;
		int iOrder;
		if ( xrtDerRead(&Attributes, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) )
			return false;
		Current = Value.Raw;
		if ( Previous.Data != NULL ) {
			size_t iCommon = Previous.Size < Current.Size ?
				Previous.Size : Current.Size;
			iOrder = memcmp(Previous.Data, Current.Data, iCommon);
			if ( (iOrder > 0) ||
				((iOrder == 0) && (Previous.Size > Current.Size)) )
				return false;
		}
		Previous = Current;
		if ( !xrtDerEnter(&Value, &Attribute) ||
			xrtDerRead(&Attribute, &Value) != XDER_VALUE ||
			!xrtDerOid(&Value, &Oid) ||
			xrtDerRead(&Attribute, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SET, true) ||
			!xrtDerDone(&Attribute) || !xrtDerEnter(&Value, &Values) ||
			xrtDerDone(&Values) ) return false;
		while ( !xrtDerDone(&Values) ) {
			if ( xrtDerRead(&Values, &Value) != XDER_VALUE ) return false;
		}
	}
	return true;
}

/* OneAsymmetricKey 的可选字段必须按标签排序，版本须与外层公钥一致。 */
static bool pkcs8_options_parse(xdercursor* pFields, uint64 iVersion,
	xbytesview* pPublic)
{
	xdervalue Value;
	bool bAttributes = false;
	bool bPublic = false;

	pPublic->Data = NULL;
	pPublic->Size = 0;
	while ( !xrtDerDone(pFields) ) {
		if ( xrtDerRead(pFields, &Value) != XDER_VALUE ||
			(Value.Tag.Class != XASN1_CONTEXT) ) return false;
		if ( (Value.Tag.Number == 0u) && Value.Tag.Constructed &&
			!bAttributes && !bPublic ) {
			if ( !pkcs8_attributes_parse(&Value) ) return false;
			bAttributes = true;
		} else if ( (Value.Tag.Number == 1u) &&
			!Value.Tag.Constructed && !bPublic &&
			(Value.Value.Size > 1u) && (Value.Value.Data[0] == 0u) ) {
			pPublic->Data = Value.Value.Data + 1u;
			pPublic->Size = Value.Value.Size - 1u;
			bPublic = true;
		} else {
			return false;
		}
	}
	return ((iVersion == 0u) && !bPublic) ||
		((iVersion == 1u) && bPublic);
}

/* 外层 RSA 公钥必须与 RSAPrivateKey 中的模数和指数完全一致。 */
static bool rsa_pkcs8_public_matches(xbytesview Encoded,
	const xbytesview aParts[8])
{
	xdercursor Outer, Fields;
	xdervalue Value;
	xbytesview Modulus, Exponent;

	return xrtDerValidate(Encoded.Data, Encoded.Size) &&
		xrtDerInit(&Outer, Encoded.Data, Encoded.Size) &&
		(xrtDerRead(&Outer, &Value) == XDER_VALUE) &&
		xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) &&
		xrtDerDone(&Outer) && xrtDerEnter(&Value, &Fields) &&
		(xrtDerRead(&Fields, &Value) == XDER_VALUE) &&
		xrtDerUnsigned(&Value, &Modulus) &&
		(xrtDerRead(&Fields, &Value) == XDER_VALUE) &&
		xrtDerUnsigned(&Value, &Exponent) && xrtDerDone(&Fields) &&
		(Modulus.Size == aParts[0].Size) &&
		(Exponent.Size == aParts[1].Size) &&
		(memcmp(Modulus.Data, aParts[0].Data, Modulus.Size) == 0) &&
		(memcmp(Exponent.Data, aParts[1].Data, Exponent.Size) == 0);
}

static bool rsa_private_der_parse(const void* pData, size_t iSize,
	bool bPkcs8, xbytesview aParts[8])
{
	static const unsigned char aRsaOid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
	};
	xdercursor Outer, Fields, Algorithm;
	xdervalue Value;
	xbytesview EncodedKey, Public;
	uint64 iVersion;

	if ( !bPkcs8 ) return rsa_private_pkcs1_parse(pData, iSize, aParts);
	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion > 1u) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerEnter(&Value, &Algorithm) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, aRsaOid, sizeof(aRsaOid)) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_NULL, false) ||
		(Value.Value.Size != 0) || !xrtDerDone(&Algorithm) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerOctets(&Value, &EncodedKey) ||
		!pkcs8_options_parse(&Fields, iVersion, &Public) ) {
		return false;
	}
	return rsa_private_pkcs1_parse(EncodedKey.Data, EncodedKey.Size, aParts) &&
		((Public.Data == NULL) ||
		 rsa_pkcs8_public_matches(Public, aParts));
}

bool xjwt__rsa_private_parse(const char* sPem, xrsaprivatekey* pKey,
                             xjwt__rsa_owned** ppOwned)
{
	xpemblock Block;
	xbytesview Parts[8];
	xrsaprivatekey Parsed;
	xjwt__rsa_owned* pOwn;
	size_t iPemSize, iDerSize = 0;
	bytes pDer;
	bool bPkcs8;

	if ( (sPem == NULL) || (pKey == NULL) || (ppOwned == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "RSA private key argument is null");
		return false;
	}
	iPemSize = strlen(sPem);
	bPkcs8 = xrtPemFind(sPem, iPemSize, "PRIVATE KEY", &Block);
	if ( !bPkcs8 &&
		!xrtPemFind(sPem, iPemSize, "RSA PRIVATE KEY", &Block) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM private key not found");
		return false;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	if ( !rsa_private_der_parse(pDer, iDerSize, bPkcs8, Parts) ) {
		xrtSecureZero(pDer, iDerSize);
		xrtFree(pDer);
		xjwt__error(XJWT_ERROR_PARSE, "RSA private key DER parse failed");
		return false;
	}
	pOwn = (xjwt__rsa_owned*)xrtMalloc(sizeof(*pOwn));
	if ( pOwn == NULL ) {
		xrtSecureZero(pDer, iDerSize);
		xrtFree(pDer);
		return false;
	}
	pOwn->pRaw = pDer;
	pOwn->iRawSize = iDerSize;
	Parsed.Public.Modulus       = Parts[0].Data; Parsed.Public.ModulusSize   = Parts[0].Size;
	Parsed.Public.Exponent      = Parts[1].Data; Parsed.Public.ExponentSize  = Parts[1].Size;
	Parsed.PrivateExponent      = Parts[2].Data; Parsed.PrivateExponentSize = Parts[2].Size;
	Parsed.Prime1               = Parts[3].Data; Parsed.Prime1Size          = Parts[3].Size;
	Parsed.Prime2               = Parts[4].Data; Parsed.Prime2Size          = Parts[4].Size;
	Parsed.Exponent1            = Parts[5].Data; Parsed.Exponent1Size       = Parts[5].Size;
	Parsed.Exponent2            = Parts[6].Data; Parsed.Exponent2Size       = Parts[6].Size;
	Parsed.Coefficient          = Parts[7].Data; Parsed.CoefficientSize     = Parts[7].Size;
	*pKey = Parsed;
	*ppOwned = pOwn;
	return true;
}

/* ------------------------------------------------------------------ */
/* EC P-256 公钥/私钥 PEM 解析                                          */
/* ------------------------------------------------------------------ */

/* EC P-256 PEM keys use id-ecPublicKey and prime256v1 namedCurve. */
static const unsigned char s_ecPublicKeyOid[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01
};
static const unsigned char s_p256Oid[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
};

bool xjwt__ecdsa_public_parse(const char* sPem, unsigned char* pPublic65)
{
	xpemblock Block;
	xdercursor Outer, Fields, Algorithm;
	xdervalue Value;
	xbytesview Point;
	uint8 iUnused;
	size_t iDerSize = 0;
	bytes pDer;

	if ( (sPem == NULL) || (pPublic65 == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "EC public key argument is null");
		return false;
	}
	if ( !xrtPemFind(sPem, strlen(sPem), "PUBLIC KEY", &Block) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM public key not found");
		return false;
	}
	if ( Block.Body.Size > 4096u ) {
		xjwt__error(XJWT_ERROR_PARSE, "EC public key PEM is too large");
		return false;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	if ( !xrtDerValidate(pDer, iDerSize) ||
		!xrtDerInit(&Outer, pDer, iDerSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerEnter(&Value, &Algorithm) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_ecPublicKeyOid, sizeof(s_ecPublicKeyOid)) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_p256Oid, sizeof(s_p256Oid)) ||
		!xrtDerDone(&Algorithm) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerBitString(&Value, &Point, &iUnused) ||
		(iUnused != 0) || (Point.Size != 65u) ||
		(Point.Data[0] != 0x04u) || !xrtDerDone(&Fields) ||
		!xrtP256Valid(Point.Data) ) {
		xrtFree(pDer);
		xjwt__error(XJWT_ERROR_PARSE, "EC public key DER parse failed");
		return false;
	}
	memcpy(pPublic65, Point.Data, 65u);
	xrtFree(pDer);
	return true;
}

/* Parse SEC1 ECPrivateKey, including its optional explicit curve/public key. */
static bool ecdsa_sec1_parse(const void* pData, size_t iSize,
	xbytesview* pPrivate, xbytesview* pPublic)
{
	xdercursor Outer, Fields;
	xdervalue Value;
	uint64 iVersion;
	int iLastOptional = -1;

	pPublic->Data = NULL;
	pPublic->Size = 0;
	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion != 1u) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerOctets(&Value, pPrivate) || (pPrivate->Size != 32u) ) {
		return false;
	}
	while ( !xrtDerDone(&Fields) ) {
		xdercursor Optional;
		xbytesview Point;
		uint8 iUnused;
		if ( xrtDerRead(&Fields, &Value) != XDER_VALUE ||
			(Value.Tag.Class != XASN1_CONTEXT) ||
			!Value.Tag.Constructed || (Value.Tag.Number > 1u) ||
			((int)Value.Tag.Number <= iLastOptional) ||
			!xrtDerEnter(&Value, &Optional) ) return false;
		iLastOptional = (int)Value.Tag.Number;
		if ( xrtDerRead(&Optional, &Value) != XDER_VALUE ) return false;
		if ( iLastOptional == 0 ) {
			if ( !xrtDerOidEqual(&Value, s_p256Oid, sizeof(s_p256Oid)) )
				return false;
		} else {
			if ( !xrtDerBitString(&Value, &Point, &iUnused) ||
				(iUnused != 0) || (Point.Size != 65u) ||
				(Point.Data[0] != 0x04u) || !xrtP256Valid(Point.Data) )
				return false;
			*pPublic = Point;
		}
		if ( !xrtDerDone(&Optional) ) return false;
	}
	return true;
}

static bool ecdsa_private_der_parse(const void* pData, size_t iSize,
	bool bPkcs8, xbytesview* pPrivate, xbytesview* pPublic,
	xbytesview* pOuterPublic)
{
	xdercursor Outer, Fields, Algorithm;
	xdervalue Value;
	xbytesview EncodedKey;
	uint64 iVersion;

	pOuterPublic->Data = NULL;
	pOuterPublic->Size = 0;
	if ( !bPkcs8 ) return ecdsa_sec1_parse(pData, iSize, pPrivate, pPublic);
	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion > 1u) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerEnter(&Value, &Algorithm) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_ecPublicKeyOid, sizeof(s_ecPublicKeyOid)) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_p256Oid, sizeof(s_p256Oid)) ||
		!xrtDerDone(&Algorithm) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerOctets(&Value, &EncodedKey) ||
		!pkcs8_options_parse(&Fields, iVersion, pOuterPublic) ) {
		return false;
	}
	return ecdsa_sec1_parse(EncodedKey.Data, EncodedKey.Size,
		pPrivate, pPublic) &&
		((pOuterPublic->Data == NULL) ||
		 ((pOuterPublic->Size == 65u) &&
		  (pOuterPublic->Data[0] == 0x04u)));
}

static bool ecdsa_private_parse(const char* sPem, unsigned char* pPrivate32)
{
	xpemblock Block;
	xbytesview Private, Public, OuterPublic;
	unsigned char ExpectedPublic[65];
	size_t iDerSize = 0;
	bytes pDer;
	bool bPkcs8, bValid;

	if ( (sPem == NULL) || (pPrivate32 == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "EC private key argument is null");
		return false;
	}
	bPkcs8 = xrtPemFind(sPem, strlen(sPem), "PRIVATE KEY", &Block);
	if ( !bPkcs8 &&
		!xrtPemFind(sPem, strlen(sPem), "EC PRIVATE KEY", &Block) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM EC private key not found");
		return false;
	}
	if ( Block.Body.Size > 4096u ) {
		xjwt__error(XJWT_ERROR_PARSE, "EC private key PEM is too large");
		return false;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	bValid = ecdsa_private_der_parse(pDer, iDerSize, bPkcs8,
		&Private, &Public, &OuterPublic);
	if ( bValid ) bValid = xrtP256Public(Private.Data, ExpectedPublic);
	if ( bValid && Public.Data != NULL )
		bValid = xrtConstTimeEqual(ExpectedPublic, Public.Data, 65u);
	if ( bValid && OuterPublic.Data != NULL )
		bValid = xrtConstTimeEqual(ExpectedPublic, OuterPublic.Data, 65u);
	if ( bValid ) memcpy(pPrivate32, Private.Data, 32u);
	xrtSecureZero(ExpectedPublic, sizeof(ExpectedPublic));
	xrtSecureZero(pDer, iDerSize);
	xrtFree(pDer);
	if ( !bValid ) {
		xjwt__error(XJWT_ERROR_PARSE, "EC private key DER parse failed");
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------ */
/* RS/ES 签发                                                           */
/* ------------------------------------------------------------------ */

static xcryptohash jwt_alg_hash(int alg)
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

bool rsa_sign_impl(int alg, const void* pData, size_t iSize,
                   const char* sPrivatePem,
                   unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	xrsaprivatekey tKey;
	xjwt__rsa_owned* pOwn = NULL;
	if ( !xjwt__rsa_private_parse(sPrivatePem, &tKey, &pOwn) ) return false;
	if ( !xjwt__rsa_jwa_key_valid(&tKey.Public) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"RSA signing key must be at least 2048 bits");
		xjwt__rsa_owned_free(pOwn);
		return false;
	}

	/* RSA 签名长度 = 模数长度；超出输出容量直接失败，不截断 */
	if ( tKey.Public.ModulusSize > iCapacity ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"RSA modulus exceeds signature output capacity");
		xjwt__rsa_owned_free(pOwn);
		return false;
	}

	xcryptohash iHash = jwt_alg_hash(alg);
	unsigned char aDigest[64];

	switch ( iHash ) {
	case XCRYPTO_HASH_SHA256:
		if ( !xrtSha256(pData, iSize, aDigest) ) goto fail;
		break;
	case XCRYPTO_HASH_SHA384:
		if ( !xrtSha384(pData, iSize, aDigest) ) goto fail;
		break;
	case XCRYPTO_HASH_SHA512:
		if ( !xrtSha512(pData, iSize, aDigest) ) goto fail;
		break;
	default:
		goto fail;
	}

	/* RSA 签名长度 = 模数长度 */
	size_t iSigSize = tKey.Public.ModulusSize;
	if ( !xrtRsaPkcs1Sign(&tKey, iHash, aDigest, pOut) ) goto fail;
	*pOutSize = iSigSize;

	xjwt__rsa_owned_free(pOwn);
	return true;

fail:
	xjwt__rsa_owned_free(pOwn);
	return false;
}

bool es256_sign_impl(const void* pData, size_t iSize,
                     const char* sPrivatePem,
                     unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	unsigned char aPrivate[32];
	unsigned char aDigest[32];
	bool bResult = false;
	if ( !ecdsa_private_parse(sPrivatePem, aPrivate) ) return false;
	if ( !xrtSha256(pData, iSize, aDigest) ) goto cleanup;

	/* RFC 7518 §3.4：JWS ES256 是定宽 32 字节 R + 32 字节 S。 */
	if ( iCapacity < 64 ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"output buffer too small for ES256 signature");
		goto cleanup;
	}
	if ( !xrtEcdsaP256Sign(XCRYPTO_HASH_SHA256, aDigest, aPrivate, pOut) )
		goto cleanup;
	*pOutSize = 64u;
	bResult = true;

cleanup:
	xrtSecureZero(aPrivate, sizeof(aPrivate));
	xrtSecureZero(aDigest, sizeof(aDigest));
	return bResult;
}

/* ------------------------------------------------------------------ */
/* JWKS                                                                 */
/* ------------------------------------------------------------------ */

struct xjwtjwks {
	int nKeys;
	struct {
		char kid[128];
		bool HasKid;      /* 显式空 kid 与未提供 kid 不同 */
		char kty[8];       /* "RSA" 或 "EC" */
		xrsapublickey rsa;
		unsigned char ec65[65];
		unsigned char* pOwnedN;   /* RSA n */
		unsigned char* pOwnedE;   /* RSA e */
	} keys[16];
};

/* JWKS 密钥数上限：超出视为异常输入，整体解析失败
 * （静默截断会让密钥轮换期的新 kid 验不过，比报错更难排查） */
#define XJWT_JWKS_MAX_KEYS 16

xjwtjwks* xjwtJwksParse(const char* sJson)
{
	if ( sJson == NULL ) return NULL;
	xerror* pPrevious = xrtTakeError();
	xvalue* p = xrtJsonParse(xrtStrView(sJson));
	if ( p == NULL ) {
		if ( !xjwt__memory_error() )
			xjwt__error(XJWT_ERROR_PARSE, "JWKS JSON parse failed");
		xrtErrorFree(pPrevious);
		return NULL;
	}
	xvalue* pKeys = xrtValueObjectGet(p, xrtStrView("keys"));
	if ( pKeys == NULL || !xrtValueIs(pKeys, XVALUE_ARRAY) ) {
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		xjwt__error(XJWT_ERROR_PARSE, "JWKS missing 'keys' array");
		return NULL;
	}
	size_t n = xrtValueCount(pKeys);
	if ( n > XJWT_JWKS_MAX_KEYS ) {
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		xjwt__error(XJWT_ERROR_PARSE, "JWKS exceeds 16 keys");
		return NULL;
	}

	xjwtjwks* pJwks = (xjwtjwks*)xrtMalloc(sizeof(xjwtjwks));
	if ( pJwks == NULL ) {
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		return NULL;
	}
	memset(pJwks, 0, sizeof(*pJwks));

	for ( size_t i = 0; i < n; i++ ) {
		xvalue* pKey = xrtValueArrayGet(pKeys, (uint32)i);
		if ( pKey == NULL ) continue;

		xstrview sv;
		int idx = pJwks->nKeys;
		memset(&pJwks->keys[idx], 0, sizeof(pJwks->keys[idx]));

		/* kid：超长（≥128）跳过该条目——静默截断会让严格匹配
		 * 永不命中且报错误导排障 */
		xvalue* pKid = xrtValueObjectGet(pKey, xrtStrView("kid"));
		if ( pKid != NULL ) {
			if ( !xrtValueGetString(pKid, &sv) ||
				sv.Size >= sizeof(pJwks->keys[idx].kid) ||
				memchr(sv.Data, 0, sv.Size) != NULL ) continue;
			memcpy(pJwks->keys[idx].kid, sv.Data, sv.Size);
			pJwks->keys[idx].kid[sv.Size] = 0;
			pJwks->keys[idx].HasKid = true;
		}
		/* kty */
		if ( !xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("kty")), &sv) ||
			sv.Size >= sizeof(pJwks->keys[idx].kty) ||
			memchr(sv.Data, 0, sv.Size) != NULL ) continue;
		memcpy(pJwks->keys[idx].kty, sv.Data, sv.Size);
		pJwks->keys[idx].kty[sv.Size] = 0;

		if ( strcmp(pJwks->keys[idx].kty, "RSA") == 0 ) {
			/* RSA: n + e；任一缺失/畸形则释放并清零槽位后跳过
			 * （残留指针既泄漏、又可能被后续条目沿用造成跨条目混淆） */
			size_t nSize = 0, eSize = 0;
			unsigned char* pn = NULL, *pe = NULL;
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("n")), &sv) ) {
				pn = b64url_dec((const char*)sv.Data, sv.Size, &nSize);
				if ( pn == NULL && xjwt__memory_error() )
					goto memory_failure;
			}
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("e")), &sv) ) {
				pe = b64url_dec((const char*)sv.Data, sv.Size, &eSize);
				if ( pe == NULL && xjwt__memory_error() ) {
					xrtFree(pn);
					goto memory_failure;
				}
			}
			if ( pn != NULL && pe != NULL ) {
				xrsapublickey Candidate;

				xjwt__int_trim(pn, &nSize);
				xjwt__int_trim(pe, &eSize);
				Candidate.Modulus = pn;
				Candidate.ModulusSize = nSize;
				Candidate.Exponent = pe;
				Candidate.ExponentSize = eSize;
				if ( xjwt__rsa_jwa_key_valid(&Candidate) ) {
					pJwks->keys[idx].pOwnedN = pn;
					pJwks->keys[idx].pOwnedE = pe;
					pJwks->keys[idx].rsa = Candidate;
					pJwks->nKeys++;
				} else {
					xrtFree(pn);
					xrtFree(pe);
				}
			} else {
				xrtFree(pn);
				xrtFree(pe);
			}
		} else if ( strcmp(pJwks->keys[idx].kty, "EC") == 0 ) {
			/* EC：仅支持 P-256，其他 crv（P-384/P-521）跳过该条目；
			 * 坐标必须恰为 32 字节，短坐标视为畸形 */
			if ( !xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("crv")), &sv) ||
				sv.Size != 5u || memcmp(sv.Data, "P-256", 5u) != 0 ) continue;

			size_t xSize = 0, ySize = 0;
			unsigned char* px = NULL, *py = NULL;
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("x")), &sv) ) {
				px = b64url_dec((const char*)sv.Data, sv.Size, &xSize);
				if ( px == NULL && xjwt__memory_error() )
					goto memory_failure;
			}
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("y")), &sv) ) {
				py = b64url_dec((const char*)sv.Data, sv.Size, &ySize);
				if ( py == NULL && xjwt__memory_error() ) {
					xrtFree(px);
					goto memory_failure;
				}
			}
			if ( px != NULL && py != NULL && xSize == 32 && ySize == 32 ) {
				pJwks->keys[idx].ec65[0] = 0x04;
				memcpy(pJwks->keys[idx].ec65 + 1, px, 32);
				memcpy(pJwks->keys[idx].ec65 + 33, py, 32);
				if ( xrtP256Valid(pJwks->keys[idx].ec65) )
					pJwks->nKeys++;
			}
			xrtFree(px); xrtFree(py);
		}
	}

	xrtValueRelease(p);
	xrtErrorFree(xrtTakeError());
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
		xrtErrorFree(pPrevious);
	}
	return pJwks;

memory_failure:
	{
		xerror* pFailure = xrtTakeError();
		xjwtJwksFree(pJwks);
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		if ( pFailure != NULL ) {
			xrtSetError(pFailure);
			xrtErrorFree(pFailure);
		}
		return NULL;
	}
}

void xjwtJwksFree(xjwtjwks* pJwks)
{
	if ( pJwks == NULL ) return;
	for ( int i = 0; i < pJwks->nKeys; i++ ) {
		if ( pJwks->keys[i].pOwnedN ) xrtFree(pJwks->keys[i].pOwnedN);
		if ( pJwks->keys[i].pOwnedE ) xrtFree(pJwks->keys[i].pOwnedE);
	}
	xrtFree(pJwks);
}

/* 算法族判定：RS 族 true / ES 族 false，HS 或未知返回 -1 */
static int jwt_alg_rsa_family(int alg)
{
	switch ( alg ) {
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512:
		return 1;
	case XJWT_ALG_ES256:
		return 0;
	}
	return -1;
}

xvalue* xjwtVerifyJwks(const char* sToken, const xjwtjwks* pJwks,
                       const xjwtcheck* pCheck)
{
	if ( sToken == NULL || pJwks == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtVerifyJwks: null argument");
		return NULL;
	}

	/* 公共前置：header(alg+kid) → claims 校验 → 签名输入/签名解码 */
	int alg = XJWT_ALG_INVALID;
	const char* sKid = NULL;
	char* sInput = NULL;
	unsigned char* pSigBin = NULL;
	size_t n = 0, iSigBinSize = 0;
	xvalue* pClaims = xjwt__verify_prepare(sToken, pCheck, &alg, &sKid,
	                                       &sInput, &n, &pSigBin, &iSigBinSize);
	if ( pClaims == NULL ) return NULL;

	/* JWKS 只承载非对称公钥：显式拒绝 HS 族，不依赖签名格式不匹配兜底 */
	int iFamily = jwt_alg_rsa_family(alg);
	if ( iFamily < 0 ) {
		xrtFree((void*)sKid); xrtFree(sInput); xrtFree(pSigBin);
		xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_ALG_MISMATCH,
			"JWKS verification only accepts RS*/ES* algorithms");
		return NULL;
	}

	/* kid 严格匹配：token 带 kid 必须精确命中；
	 * token 无 kid 才回退取第一把可用密钥 */
	int iFound = -1;
	for ( int i = 0; i < pJwks->nKeys; i++ ) {
		if ( sKid == NULL ) { iFound = i; break; }
		if ( pJwks->keys[i].HasKid &&
			strcmp(sKid, pJwks->keys[i].kid) == 0 ) { iFound = i; break; }
	}
	xrtFree((void*)sKid);
	if ( iFound < 0 ) {
		xrtFree(sInput); xrtFree(pSigBin); xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_KEY_NOT_FOUND, "no matching kid in JWKS");
		return NULL;
	}

	/* 算法族必须与密钥类型匹配：RS* ↔ RSA、ES256 ↔ EC */
	bool bKeyRsa = strcmp(pJwks->keys[iFound].kty, "RSA") == 0;
	if ( ( iFamily == 1 ) != bKeyRsa ) {
		xrtFree(sInput); xrtFree(pSigBin); xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_ALG_MISMATCH,
			"token alg family does not match JWKS key type");
		return NULL;
	}

	bool ok;
	if ( bKeyRsa ) {
		ok = xjwt__verify_rsa_raw(sInput, n, pSigBin, iSigBinSize,
		                          &pJwks->keys[iFound].rsa, alg);
	} else {
		ok = xjwt__verify_es256_raw(sInput, n, pSigBin, iSigBinSize,
		                            pJwks->keys[iFound].ec65);
	}

	xrtFree(sInput); xrtFree(pSigBin);
	if ( !ok ) {
		xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
			"JWKS signature mismatch");
		xrtValueRelease(pClaims);
		return NULL;
	}
	return pClaims;
}

/* ------------------------------------------------------------------ */
/* 公钥缓存（xjwtKeyParse / xjwtKeyFree / xjwtVerifyKey）                */
/* ------------------------------------------------------------------ */

struct xjwtkey {
	bool          bRsa;        /* true=RSA 公钥；false=EC P-256（65 字节点） */
	xrsapublickey Rsa;         /* bRsa 时有效（视图，指向 pOwnedArr 内部） */
	unsigned char* pOwnedArr;  /* xjwt__rsa_public_parse 的资源包 */
	unsigned char Ec65[65];    /* !bRsa 时有效 */
};

xjwtkey* xjwtKeyParse(const char* sPublicPem)
{
	if ( sPublicPem == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtKeyParse: null argument");
		return NULL;
	}
	xerror* pPrevious = xrtTakeError();
	xjwtkey* pKey = (xjwtkey*)xrtMalloc(sizeof(xjwtkey));
	if ( pKey == NULL ) {
		xrtErrorFree(pPrevious);
		return NULL;
	}
	memset(pKey, 0, sizeof(*pKey));

	/* 先按 RSA（SPKI / PKCS#1）解析，失败再按 EC P-256 SPKI */
	unsigned char* pOwned = NULL;
	xrsapublickey tRsa;
	if ( xjwt__rsa_public_parse(sPublicPem, &tRsa, &pOwned) ) {
		pKey->bRsa = true;
		pKey->Rsa = tRsa;
		pKey->pOwnedArr = pOwned;
		goto success;
	}
	/* 内存错误不代表算法不匹配，不能换解析器后吞掉根因。 */
	if ( xjwt__memory_error() ) goto failure;
	if ( xjwt__ecdsa_public_parse(sPublicPem, pKey->Ec65) ) {
		pKey->bRsa = false;
		goto success;
	}
failure:
	xrtFree(pKey);
	xrtErrorFree(pPrevious);
	xjwt__error_unless_memory(XJWT_ERROR_PARSE, "not an RSA/EC public key PEM");
	return NULL;

success:
	/* 丢弃内部算法探测的诊断，恢复调用前已有的错误。 */
	xrtErrorFree(xrtTakeError());
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
		xrtErrorFree(pPrevious);
	}
	return pKey;
}

void xjwtKeyFree(xjwtkey* pKey)
{
	if ( pKey == NULL ) return;
	if ( pKey->pOwnedArr != NULL )
		xjwt__rsa_public_free(&pKey->Rsa, pKey->pOwnedArr);
	xrtFree(pKey);
}

xvalue* xjwtVerifyKey(const char* sToken, const xjwtkey* pKey,
                      const xjwtcheck* pCheck)
{
	if ( sToken == NULL || pKey == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtVerifyKey: null argument");
		return NULL;
	}

	int alg = XJWT_ALG_INVALID;
	const char* sKid = NULL;
	char* sInput = NULL;
	unsigned char* pSigBin = NULL;
	size_t n = 0, iSigBinSize = 0;
	xvalue* pClaims = xjwt__verify_prepare(sToken, pCheck, &alg, &sKid,
	                                       &sInput, &n, &pSigBin, &iSigBinSize);
	if ( pClaims == NULL ) return NULL;
	xrtFree((void*)sKid);  /* 单钥路径不选钥，kid 不参与 */

	/* 缓存只承载公钥：拒绝 HS 族；算法族必须与密钥类型匹配 */
	int iFamily = jwt_alg_rsa_family(alg);
	if ( iFamily < 0 || ( iFamily == 1 ) != pKey->bRsa ) {
		xrtFree(sInput); xrtFree(pSigBin); xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_ALG_MISMATCH,
			"token alg family does not match cached key type");
		return NULL;
	}

	bool ok;
	if ( pKey->bRsa )
		ok = xjwt__verify_rsa_raw(sInput, n, pSigBin, iSigBinSize, &pKey->Rsa, alg);
	else
		ok = xjwt__verify_es256_raw(sInput, n, pSigBin, iSigBinSize, pKey->Ec65);

	xrtFree(sInput); xrtFree(pSigBin);
	if ( !ok ) {
		xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
			"cached key signature mismatch");
		xrtValueRelease(pClaims);
		return NULL;
	}
	return pClaims;
}

/* ES256 验签（PEM 路径入口） */
bool xjwt__verify_es256_full(const void* pData, size_t iSize,
                             const char* sPublicPem,
                             const void* pSig, size_t iSigSize)
{
	unsigned char aPublic[65];
	if ( !xjwt__ecdsa_public_parse(sPublicPem, aPublic) ) return false;
	return xjwt__verify_es256_raw(pData, iSize, pSig, iSigSize, aPublic);
}
