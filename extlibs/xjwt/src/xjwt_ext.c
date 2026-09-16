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
	unsigned char* pRaw;        /* DER 解码后的原始字节 */
	unsigned char** ppParts;    /* 各组件指针数组（8 个：n,e,d,p,q,dp,dq,qinv） */
	int nParts;
} xjwt__rsa_owned;

static void xjwt__rsa_owned_free(xjwt__rsa_owned* p)
{
	if ( p == NULL ) return;
	if ( p->ppParts ) {
		for ( int i = 0; i < p->nParts; i++ ) {
			if ( p->ppParts[i] ) xrtFree(p->ppParts[i]);
		}
		xrtFree(p->ppParts);
	}
	if ( p->pRaw ) xrtFree(p->pRaw);
	xrtFree(p);
}

/* 从 DER 序列中顺序读取 count 个 INTEGER */
static bool der_read_ints(xdercursor* pCur, int count,
                          unsigned char** ppOut, size_t* pSizes)
{
	for ( int i = 0; i < count; i++ ) {
		xdervalue tVal;
		if ( xrtDerRead(pCur, &tVal) != XDER_VALUE ||
		     tVal.Tag.Number != XASN1_INTEGER || tVal.Value.Size == 0 ) {
			return false;
		}
		ppOut[i] = (unsigned char*)xrtMalloc(tVal.Value.Size);
		if ( ppOut[i] == NULL ) return false;
		memcpy(ppOut[i], tVal.Value.Data, tVal.Value.Size);
		pSizes[i] = tVal.Value.Size;
		xjwt__int_trim(ppOut[i], &pSizes[i]);
	}
	return true;
}

bool xjwt__rsa_private_parse(const char* sPem, xrsaprivatekey* pKey,
                             xjwt__rsa_owned** ppOwned)
{
	xpemblock tBlock;
	if ( !xrtPemFind(sPem, strlen(sPem), "PRIVATE KEY", &tBlock) &&
	     !xrtPemFind(sPem, strlen(sPem), "RSA PRIVATE KEY", &tBlock) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM private key not found");
		return false;
	}
	size_t iDerSize = 0;
	bytes pDer = xrtPemDecodeNew(&tBlock, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM decode failed");
		return false;
	}

	xjwt__rsa_owned* pOwn = (xjwt__rsa_owned*)xrtMalloc(sizeof(xjwt__rsa_owned));
	if ( pOwn == NULL ) { xrtFree(pDer); return false; }
	memset(pOwn, 0, sizeof(*pOwn));
	pOwn->pRaw = pDer;

	xdervalue tVal;
	xdercursor tCur;
	xrtDerInit(&tCur, pDer, iDerSize);

	/* 外层 SEQ */
	if ( xrtDerRead(&tCur, &tVal) != XDER_VALUE ||
	     tVal.Tag.Number != XASN1_SEQUENCE ) goto fail;

	/* 判断 PKCS#8 vs PKCS#1：两者都以 version INTEGER 开头，
	 * 区别在第二个元素 —— PKCS#1 是 INTEGER(n)，PKCS#8 是 SEQ(AlgorithmIdentifier)。
	 * PKCS#1  RSAPrivateKey  = SEQ { INT ver, INT n, INT e, INT d, INT p, INT q, INT dp, INT dq, INT qinv }
	 * PKCS#8  PrivateKeyInfo = SEQ { INT ver, SEQ algid, OCTET STRING { PKCS#1 } } */
	{
		xdercursor tInner;
		xrtDerInit(&tInner, tVal.Value.Data, tVal.Value.Size);
		if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
		     tVal.Tag.Number != XASN1_INTEGER ) goto fail;  /* version */

		xdervalue tSecond;
		if ( xrtDerRead(&tInner, &tSecond) != XDER_VALUE ) goto fail;

		unsigned char** pp = (unsigned char**)xrtMalloc(8 * sizeof(void*));
		size_t aSizes[8];
		if ( pp == NULL ) goto fail;
		memset(pp, 0, 8 * sizeof(void*));

		bool ok8 = false;
		if ( tSecond.Tag.Number == XASN1_SEQUENCE ) {
			/* PKCS#8：跳过 algid，取 OCTET STRING 内嵌 PKCS#1 */
			xdercursor tInts;
			if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
			     tVal.Tag.Number != XASN1_OCTET_STRING ) goto fail_ints;
			xrtDerInit(&tInts, tVal.Value.Data, tVal.Value.Size);
			if ( xrtDerRead(&tInts, &tVal) != XDER_VALUE ||
			     tVal.Tag.Number != XASN1_SEQUENCE ) goto fail_ints;
			/* 在 SEQ 内容上重开游标，跳过 version */
			xrtDerInit(&tInts, tVal.Value.Data, tVal.Value.Size);
			if ( xrtDerRead(&tInts, &tVal) != XDER_VALUE ||
			     tVal.Tag.Number != XASN1_INTEGER ) goto fail_ints;  /* version */
			ok8 = der_read_ints(&tInts, 8, pp, aSizes);
		} else if ( tSecond.Tag.Number == XASN1_INTEGER ) {
			/* PKCS#1：tSecond 已是 n（不可回退），先入槽 0 再读余下 7 个 */
			pp[0] = (unsigned char*)xrtMalloc(tSecond.Value.Size);
			if ( pp[0] != NULL ) {
				memcpy(pp[0], tSecond.Value.Data, tSecond.Value.Size);
				aSizes[0] = tSecond.Value.Size;
				xjwt__int_trim(pp[0], &aSizes[0]);
				ok8 = true;
				for ( int i = 1; i < 8 && ok8; i++ ) {
					xdervalue tInt;
					if ( xrtDerRead(&tInner, &tInt) != XDER_VALUE ||
					     tInt.Tag.Number != XASN1_INTEGER ||
					     tInt.Value.Size == 0 ) { ok8 = false; break; }
					pp[i] = (unsigned char*)xrtMalloc(tInt.Value.Size);
					if ( pp[i] == NULL ) { ok8 = false; break; }
					memcpy(pp[i], tInt.Value.Data, tInt.Value.Size);
					aSizes[i] = tInt.Value.Size;
					xjwt__int_trim(pp[i], &aSizes[i]);
				}
			}
		}
		if ( !ok8 ) {
fail_ints:
			for ( int i = 0; i < 8; i++ ) if ( pp[i] ) xrtFree(pp[i]);
			xrtFree(pp);
			goto fail;
		}
		pOwn->ppParts = pp;
		pOwn->nParts = 8;
		pKey->Public.Modulus       = pp[0]; pKey->Public.ModulusSize   = aSizes[0];
		pKey->Public.Exponent      = pp[1]; pKey->Public.ExponentSize  = aSizes[1];
		pKey->PrivateExponent      = pp[2]; pKey->PrivateExponentSize = aSizes[2];
		pKey->Prime1               = pp[3]; pKey->Prime1Size          = aSizes[3];
		pKey->Prime2               = pp[4]; pKey->Prime2Size          = aSizes[4];
		pKey->Exponent1            = pp[5]; pKey->Exponent1Size       = aSizes[5];
		pKey->Exponent2            = pp[6]; pKey->Exponent2Size       = aSizes[6];
		pKey->Coefficient          = pp[7]; pKey->CoefficientSize     = aSizes[7];
	}

	*ppOwned = pOwn;
	return true;

fail:
	xjwt__rsa_owned_free(pOwn);
	xjwt__error(XJWT_ERROR_PARSE, "RSA private key DER parse failed");
	return false;
}

/* ------------------------------------------------------------------ */
/* EC P-256 公钥/私钥 PEM 解析                                          */
/* ------------------------------------------------------------------ */

bool xjwt__ecdsa_public_parse(const char* sPem, unsigned char* pPublic65)
{
	xpemblock tBlock;
	if ( !xrtPemFind(sPem, strlen(sPem), "PUBLIC KEY", &tBlock) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM public key not found");
		return false;
	}
	size_t iDerSize = 0;
	bytes pDer = xrtPemDecodeNew(&tBlock, &iDerSize);
	if ( pDer == NULL ) return false;

	xdervalue tVal;
	xdercursor tCur;
	xrtDerInit(&tCur, pDer, iDerSize);

	/* SPKI: SEQ { SEQ { OID ecPublicKey, OID P-256 }, BIT STRING { 65B point } } */
	if ( xrtDerRead(&tCur, &tVal) != XDER_VALUE ||
	     tVal.Tag.Number != XASN1_SEQUENCE ) goto fail;
	{
		xdercursor tInner;
		xrtDerInit(&tInner, tVal.Value.Data, tVal.Value.Size);
		/* 跳过 AlgorithmIdentifier */
		if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
		     tVal.Tag.Number != XASN1_SEQUENCE ) goto fail;
		/* 读 BIT STRING */
		if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
		     tVal.Tag.Number != XASN1_BIT_STRING ) goto fail;
		/* BIT STRING 第一字节 unused-bits = 0，剩余 65 字节 */
		if ( tVal.Value.Size != 66 || tVal.Value.Data[0] != 0 ) goto fail;
		memcpy(pPublic65, (const unsigned char*)tVal.Value.Data + 1, 65);
	}
	xrtFree(pDer);
	return true;

fail:
	xrtFree(pDer);
	xjwt__error(XJWT_ERROR_PARSE, "EC public key DER parse failed");
	return false;
}

static bool ecdsa_private_parse(const char* sPem, unsigned char* pPrivate32)
{
	xpemblock tBlock;
	if ( !xrtPemFind(sPem, strlen(sPem), "EC PRIVATE KEY", &tBlock) &&
	     !xrtPemFind(sPem, strlen(sPem), "PRIVATE KEY", &tBlock) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM EC private key not found");
		return false;
	}
	size_t iDerSize = 0;
	bytes pDer = xrtPemDecodeNew(&tBlock, &iDerSize);
	if ( pDer == NULL ) return false;

	xdervalue tVal;
	xdercursor tCur;
	xrtDerInit(&tCur, pDer, iDerSize);

	/* SEC1:  SEQ { INTEGER version, OCTET STRING { 32B private }, ... }
	 * PKCS#8: SEQ { INTEGER version, SEQ algid, OCTET STRING { SEC1 } } */
	if ( xrtDerRead(&tCur, &tVal) != XDER_VALUE ||
	     tVal.Tag.Number != XASN1_SEQUENCE ) goto fail;
	{
		xdercursor tInner;
		xrtDerInit(&tInner, tVal.Value.Data, tVal.Value.Size);
		/* 跳过 version */
		if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
		     tVal.Tag.Number != XASN1_INTEGER ) goto fail;
		xdervalue tNext;
		if ( xrtDerRead(&tInner, &tNext) != XDER_VALUE ) goto fail;
		if ( tNext.Tag.Number == XASN1_SEQUENCE ) {
			/* PKCS#8：跳过 algid 取 OCTET STRING，进入内嵌 SEC1 */
			if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
			     tVal.Tag.Number != XASN1_OCTET_STRING ) goto fail;
			xrtDerInit(&tInner, tVal.Value.Data, tVal.Value.Size);
			if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
			     tVal.Tag.Number != XASN1_SEQUENCE ) goto fail;
			/* 在 SEC1 SEQ 内容上重开游标 */
			xrtDerInit(&tInner, tVal.Value.Data, tVal.Value.Size);
			if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
			     tVal.Tag.Number != XASN1_INTEGER ) goto fail;  /* version */
			if ( xrtDerRead(&tInner, &tVal) != XDER_VALUE ||
			     tVal.Tag.Number != XASN1_OCTET_STRING || tVal.Value.Size != 32 ) goto fail;
			memcpy(pPrivate32, tVal.Value.Data, 32);
		} else if ( tNext.Tag.Number == XASN1_OCTET_STRING && tNext.Value.Size == 32 ) {
			/* SEC1：直接就是 32 字节私钥 */
			memcpy(pPrivate32, tNext.Value.Data, 32);
		} else {
			goto fail;
		}
	}
	xrtFree(pDer);
	return true;

fail:
	xrtFree(pDer);
	xjwt__error(XJWT_ERROR_PARSE, "EC private key DER parse failed");
	return false;
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
	if ( !ecdsa_private_parse(sPrivatePem, aPrivate) ) return false;

	unsigned char aDigest[32];
	if ( !xrtSha256(pData, iSize, aDigest) ) return false;

	/* RFC 7518 §3.4：JWS 的 ES256 签名是 DER 编码（非 raw r||s，≤72 字节） */
	if ( iCapacity < 72 ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"output buffer too small for ES256 signature");
		return false;
	}
	return xrtEcdsaP256SignDer(XCRYPTO_HASH_SHA256, aDigest, aPrivate,
	                           pOut, iCapacity, pOutSize);
}

/* ------------------------------------------------------------------ */
/* JWKS                                                                 */
/* ------------------------------------------------------------------ */

struct xjwtjwks {
	int nKeys;
	struct {
		char kid[128];
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
	xvalue* p = xrtJsonParse(xrtStrView(sJson));
	if ( p == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "JWKS JSON parse failed");
		return NULL;
	}
	xvalue* pKeys = xrtValueObjectGet(p, xrtStrView("keys"));
	if ( pKeys == NULL || !xrtValueIs(pKeys, XVALUE_ARRAY) ) {
		xrtValueRelease(p);
		xjwt__error(XJWT_ERROR_PARSE, "JWKS missing 'keys' array");
		return NULL;
	}
	size_t n = xrtValueCount(pKeys);
	if ( n > XJWT_JWKS_MAX_KEYS ) {
		xrtValueRelease(p);
		xjwt__error(XJWT_ERROR_PARSE, "JWKS exceeds 16 keys");
		return NULL;
	}

	xjwtjwks* pJwks = (xjwtjwks*)xrtMalloc(sizeof(xjwtjwks));
	if ( pJwks == NULL ) { xrtValueRelease(p); return NULL; }
	memset(pJwks, 0, sizeof(*pJwks));

	for ( size_t i = 0; i < n; i++ ) {
		xvalue* pKey = xrtValueArrayGet(pKeys, (uint32)i);
		if ( pKey == NULL ) continue;

		xstrview sv;
		int idx = pJwks->nKeys;

		/* kid：超长（≥128）跳过该条目——静默截断会让严格匹配
		 * 永不命中且报错误导排障 */
		if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("kid")), &sv) ) {
			if ( sv.Size >= sizeof(pJwks->keys[idx].kid) ) continue;
			memcpy(pJwks->keys[idx].kid, sv.Data, sv.Size);
			pJwks->keys[idx].kid[sv.Size] = 0;
		}
		/* kty */
		if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("kty")), &sv) ) {
			size_t c = sv.Size < 7 ? sv.Size : 7;
			memcpy(pJwks->keys[idx].kty, sv.Data, c);
			pJwks->keys[idx].kty[c] = 0;
		}

		if ( strcmp(pJwks->keys[idx].kty, "RSA") == 0 ) {
			/* RSA: n + e；任一缺失/畸形则释放并清零槽位后跳过
			 * （残留指针既泄漏、又可能被后续条目沿用造成跨条目混淆） */
			size_t nSize = 0, eSize = 0;
			unsigned char* pn = NULL, *pe = NULL;
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("n")), &sv) )
				pn = b64url_dec((const char*)sv.Data, sv.Size, &nSize);
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("e")), &sv) )
				pe = b64url_dec((const char*)sv.Data, sv.Size, &eSize);
			if ( pn != NULL && pe != NULL ) {
				xjwt__int_trim(pn, &nSize);
				xjwt__int_trim(pe, &eSize);
				pJwks->keys[idx].pOwnedN = pn;
				pJwks->keys[idx].pOwnedE = pe;
				pJwks->keys[idx].rsa.Modulus = pn;
				pJwks->keys[idx].rsa.ModulusSize = nSize;
				pJwks->keys[idx].rsa.Exponent = pe;
				pJwks->keys[idx].rsa.ExponentSize = eSize;
				pJwks->nKeys++;
			} else {
				xrtFree(pn);
				xrtFree(pe);
			}
		} else if ( strcmp(pJwks->keys[idx].kty, "EC") == 0 ) {
			/* EC：仅支持 P-256，其他 crv（P-384/P-521）跳过该条目；
			 * 坐标必须恰为 32 字节，短坐标视为畸形 */
			char aCrv[16] = {0};
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("crv")), &sv) ) {
				size_t c = sv.Size < sizeof(aCrv) - 1 ? sv.Size : sizeof(aCrv) - 1;
				memcpy(aCrv, sv.Data, c);
			}
			if ( strcmp(aCrv, "P-256") != 0 ) continue;

			size_t xSize = 0, ySize = 0;
			unsigned char* px = NULL, *py = NULL;
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("x")), &sv) )
				px = b64url_dec((const char*)sv.Data, sv.Size, &xSize);
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("y")), &sv) )
				py = b64url_dec((const char*)sv.Data, sv.Size, &ySize);
			if ( px != NULL && py != NULL && xSize == 32 && ySize == 32 ) {
				pJwks->keys[idx].ec65[0] = 0x04;
				memcpy(pJwks->keys[idx].ec65 + 1, px, 32);
				memcpy(pJwks->keys[idx].ec65 + 33, py, 32);
				pJwks->nKeys++;
			}
			xrtFree(px); xrtFree(py);
		}
	}

	xrtValueRelease(p);
	return pJwks;
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
		if ( strcmp(sKid, pJwks->keys[i].kid) == 0 ) { iFound = i; break; }
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
		xjwt__error(XJWT_ERROR_SIGNATURE, "JWKS signature mismatch");
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
	xjwtkey* pKey = (xjwtkey*)xrtMalloc(sizeof(xjwtkey));
	if ( pKey == NULL ) return NULL;
	memset(pKey, 0, sizeof(*pKey));

	/* 先按 RSA（SPKI / PKCS#1）解析，失败再按 EC P-256 SPKI */
	unsigned char* pOwned = NULL;
	xrsapublickey tRsa;
	if ( xjwt__rsa_public_parse(sPublicPem, &tRsa, &pOwned) ) {
		pKey->bRsa = true;
		pKey->Rsa = tRsa;
		pKey->pOwnedArr = pOwned;
		return pKey;
	}
	if ( xjwt__ecdsa_public_parse(sPublicPem, pKey->Ec65) ) {
		pKey->bRsa = false;
		return pKey;
	}
	xrtFree(pKey);
	xjwt__error(XJWT_ERROR_PARSE, "not an RSA/EC public key PEM");
	return NULL;
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
		xjwt__error(XJWT_ERROR_SIGNATURE, "cached key signature mismatch");
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
