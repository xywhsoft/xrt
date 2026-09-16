/* claims 校验（exp/nbf/iss/aud）+ 主入口（Sign/Verify/Decode）+ JWKS。 */
#include "xjwt_internal.h"

/* ------------------------------------------------------------------ */
/* claims 校验                                                         */
/* ------------------------------------------------------------------ */
void xjwtCheckInit(xjwtcheck* pCheck)
{
	if ( pCheck == NULL ) return;
	memset(pCheck, 0, sizeof(*pCheck));
}

int xjwtLastError(void)
{
	const xerror* pErr = xrtGetError();
	if ( pErr == NULL ) return 0;
	cstr sDomain = xrtErrorDomain(pErr);
	if ( sDomain == NULL || strcmp(sDomain, "xrt.jwt") != 0 ) return 0;
	return xrtErrorCode(pErr);
}

bool xjwtClaimsValid(const xvalue* claims, const xjwtcheck* check)
{
	const xvalue* p = (const xvalue*)claims;
	if ( p == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "claims is null");
		return false;
	}
	/* RFC 7519 §4：claims set 必须是 JSON 对象；
	 * 数组/标量会让 exp/nbf/iss/aud 检索全部落空（整组校验被绕过） */
	if ( !xrtValueIs(p, XVALUE_OBJECT) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "claims must be a JSON object");
		return false;
	}
	int64_t now = check && check->NowOverride > 0 ? check->NowOverride
		: (int64_t)(xrtNow() / 1000000);
	int leeway = check ? check->ClockLeeway : 0;

	/* exp：RFC 7519 NumericDate 必须是整数；存在但类型错误一律拒绝，
	 * 防止字符串时间戳绕过过期检查 */
	{
		xvalue* pExp = xrtValueObjectGet(p, xrtStrView("exp"));
		if ( pExp != NULL ) {
			int64 exp;
			if ( !xrtValueIs(pExp, XVALUE_INT) || !xrtValueGetInt(pExp, &exp) ) {
				xjwt__error(XJWT_ERROR_EXPIRED, "exp must be an integer NumericDate");
				return false;
			}
			if ( now - leeway > exp ) {
				xjwt__error(XJWT_ERROR_EXPIRED, "token expired");
				return false;
			}
		}
	}
	/* nbf：同 exp，类型错误拒绝 */
	{
		xvalue* pNbf = xrtValueObjectGet(p, xrtStrView("nbf"));
		if ( pNbf != NULL ) {
			int64 nbf;
			if ( !xrtValueIs(pNbf, XVALUE_INT) || !xrtValueGetInt(pNbf, &nbf) ) {
				xjwt__error(XJWT_ERROR_NOT_YET, "nbf must be an integer NumericDate");
				return false;
			}
			if ( now + leeway < nbf ) {
				xjwt__error(XJWT_ERROR_NOT_YET, "token not yet valid");
				return false;
			}
		}
	}
	/* iss */
	if ( check != NULL && check->Issuer != NULL ) {
		xvalue* pIss = xrtValueObjectGet(p, xrtStrView("iss"));
		xstrview sv;
		if ( pIss == NULL || !xrtValueGetString(pIss, &sv) ||
		     sv.Size != strlen(check->Issuer) || !xrtConstTimeEqual(sv.Data, check->Issuer, sv.Size) ) {
			xjwt__error(XJWT_ERROR_ISSUER, "issuer mismatch");
			return false;
		}
	}
	/* aud：RFC 7519 §4.1.3 允许字符串或字符串数组，任一命中即通过 */
	if ( check != NULL && check->Audience != NULL ) {
		xvalue* pAud = xrtValueObjectGet(p, xrtStrView("aud"));
		xstrview sv;
		bool bMatch = false;
		if ( pAud != NULL && xrtValueGetString(pAud, &sv) ) {
			bMatch = sv.Size == strlen(check->Audience) &&
				xrtConstTimeEqual(sv.Data, check->Audience, sv.Size);
		} else if ( pAud != NULL && xrtValueIs(pAud, XVALUE_ARRAY) ) {
			size_t iExpect = strlen(check->Audience);
			size_t n = xrtValueCount(pAud);
			for ( size_t i = 0; i < n && !bMatch; i++ ) {
				xvalue* pElem = xrtValueArrayGet(pAud, (uint32)i);
				if ( pElem != NULL && xrtValueGetString(pElem, &sv) &&
				     sv.Size == iExpect &&
				     xrtConstTimeEqual(sv.Data, check->Audience, iExpect) )
					bMatch = true;
			}
		}
		if ( !bMatch ) {
			xjwt__error(XJWT_ERROR_AUDIENCE, "audience mismatch");
			return false;
		}
	}
	return true;
}

/* ------------------------------------------------------------------ */
/* 字符串 claim 便捷读取                                                 */
/* ------------------------------------------------------------------ */
bool xjwtClaimString(const xvalue* claims, const char* key,
                     char* pOut, size_t iCap)
{
	if ( claims == NULL || key == NULL || pOut == NULL || iCap == 0 )
		return false;
	xvalue* pVal = xrtValueObjectGet(claims, xrtStrView(key));
	xstrview sv;
	if ( pVal == NULL || !xrtValueGetString(pVal, &sv) )
		return false;
	size_t n = sv.Size < iCap - 1 ? sv.Size : iCap - 1;
	memcpy(pOut, sv.Data, n);
	pOut[n] = 0;
	return true;
}

/* ------------------------------------------------------------------ */
/* 主入口：签发                                                          */
/* ------------------------------------------------------------------ */
void xjwtConfigInit(xjwtconfig* pConfig)
{
	if ( pConfig == NULL ) return;
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Alg = XJWT_ALG_HS256;
}

char* xjwtSign(const xjwtconfig* pConfig, xvalue* claims)
{
	if ( pConfig == NULL || claims == NULL || pConfig->KeyPem == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtSign: null argument");
		return NULL;
	}
	/* RFC 7519 §4：claims set 必须是 JSON 对象 */
	if ( !xrtValueIs(claims, XVALUE_OBJECT) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "claims must be a JSON object");
		return NULL;
	}
	/* 与 DecodeHeader 的验证侧上限保持一致：超长 kid 直接拒绝签发 */
	if ( pConfig->KeyId != NULL && strlen(pConfig->KeyId) >= 256 ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "KeyId too long (>= 256)");
		return NULL;
	}
	/* 自动注入标准 claims（就地修改传入对象） */
	{
		int64_t now = (int64_t)(xrtNow() / 1000000);
		if ( pConfig->ExpireSeconds != 0 )
			xrtValueObjectSetNew(claims, xrtStrView("exp"), xrtValueInt(now + pConfig->ExpireSeconds));
		if ( pConfig->Issuer != NULL )
			xrtValueObjectSetNew(claims, xrtStrView("iss"), xrtValueString(xrtStrView(pConfig->Issuer)));
		if ( pConfig->Audience != NULL )
			xrtValueObjectSetNew(claims, xrtStrView("aud"), xrtValueString(xrtStrView(pConfig->Audience)));
		if ( pConfig->Subject != NULL )
			xrtValueObjectSetNew(claims, xrtStrView("sub"), xrtValueString(xrtStrView(pConfig->Subject)));
		if ( pConfig->Jti != NULL )
			xrtValueObjectSetNew(claims, xrtStrView("jti"), xrtValueString(xrtStrView(pConfig->Jti)));
		/* iat 始终注入 */
		xrtValueObjectSetNew(claims, xrtStrView("iat"), xrtValueInt(now));
	}

	/* header 经 JSON 序列化构造：kid 自动转义且不受定长缓冲限制 */
	const char* sAlg = xjwtAlgName(pConfig->Alg);
	if ( sAlg == NULL ) {
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unknown algorithm");
		return NULL;
	}
	xvalue* pHead = xrtValueObject();
	if ( pHead == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "header object alloc failed");
		return NULL;
	}
	xrtValueObjectSetNew(pHead, xrtStrView("alg"), xrtValueString(xrtStrView(sAlg)));
	xrtValueObjectSetNew(pHead, xrtStrView("typ"), xrtValueString(xrtStrView("JWT")));
	if ( pConfig->KeyId != NULL )
		xrtValueObjectSetNew(pHead, xrtStrView("kid"), xrtValueString(xrtStrView(pConfig->KeyId)));
	char* sHeadJson = xrtJsonStringify(pHead, false, NULL);
	xrtValueRelease(pHead);
	if ( sHeadJson == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "header stringify failed");
		return NULL;
	}

	/* claims JSON */
	char* sClaimsJson = xrtJsonStringify(claims, false, NULL);  /* compact */
	if ( sClaimsJson == NULL ) {
		xrtFree(sHeadJson);
		xjwt__error(XJWT_ERROR_PARSE, "claims stringify failed");
		return NULL;
	}

	/* signing input = header_b64 + "." + claims_b64 */
	char* sHeadB64 = xjwt__base64url_encode(sHeadJson, strlen(sHeadJson));
	if ( sHeadB64 == NULL ) { xrtFree(sHeadJson); xrtFree(sClaimsJson); return NULL; }
	char* sClaimsB64 = xjwt__base64url_encode(sClaimsJson, strlen(sClaimsJson));
	if ( sClaimsB64 == NULL ) { xrtFree(sHeadB64); xrtFree(sHeadJson); xrtFree(sClaimsJson); return NULL; }

	size_t n = strlen(sHeadB64) + 1 + strlen(sClaimsB64);
	char* sInput = (char*)xrtMalloc(n + 1);
	if ( sInput == NULL ) {
		xrtFree(sHeadB64); xrtFree(sClaimsB64);
		xrtFree(sHeadJson); xrtFree(sClaimsJson);
		return NULL;
	}
	strcpy(sInput, sHeadB64);
	strcat(sInput, ".");
	strcat(sInput, sClaimsB64);

	/* sign：缓冲覆盖 xrt 全模数上限（RSA-8192 = 1024 字节） */
	unsigned char aSig[XRT_RSA_MAX_MODULUS_SIZE];
	size_t iSigSize = 0;
	if ( !xjwt__sign(pConfig->Alg, sInput, n, pConfig->KeyPem,
	                 aSig, sizeof(aSig), &iSigSize) ) {
		xrtFree(sInput); xrtFree(sHeadB64); xrtFree(sClaimsB64);
		xrtFree(sHeadJson); xrtFree(sClaimsJson);
		return NULL;
	}

	/* join */
	char* sOut = xjwt__join(sHeadJson, sClaimsJson, aSig, iSigSize);
	xrtFree(sInput); xrtFree(sHeadB64); xrtFree(sClaimsB64);
	xrtFree(sHeadJson); xrtFree(sClaimsJson);
	return sOut;
}

/* 一步式便捷 */
char* xjwtHs256(xvalue* claims, const char* secret, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = secret; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtHs384(xvalue* claims, const char* secret, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_HS384; cfg.KeyPem = secret; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtHs512(xvalue* claims, const char* secret, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_HS512; cfg.KeyPem = secret; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtRs256(xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_RS256; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtRs384(xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_RS384; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtRs512(xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_RS512; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtEs256(xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_ES256; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}

/* ------------------------------------------------------------------ */
/* 主入口：验证                                                          */
/* ------------------------------------------------------------------ */

/* 三条验证路径共用前置：header(alg+kid) → claims 解码校验 → 重建签名输入 → 解码签名。
 * 成功返回 claims 并交出中间量（调用方 xrtFree）；失败返回 NULL。 */
xvalue* xjwt__verify_prepare(const char* sToken, const xjwtcheck* pCheck,
                             int* pAlg, const char** pKid,
                             char** psInput, size_t* pn,
                             unsigned char** ppSig, size_t* pSigSize)
{
	*pKid = NULL;

	/* header：alg + kid */
	xvalue* pHdr = xjwtDecodeHeader(sToken, pAlg, pKid);
	if ( pHdr == NULL ) return NULL;
	xrtValueRelease(pHdr);
	if ( *pAlg == XJWT_ALG_INVALID ) {
		xrtFree((void*)*pKid); *pKid = NULL;
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unknown alg in header");
		return NULL;
	}

	/* claims 解码（不验签）+ 校验（exp/nbf/iss/aud） */
	xvalue* claims = xjwtDecode(sToken, NULL);
	if ( claims == NULL ) goto fail;
	if ( !xjwtClaimsValid(claims, pCheck) ) {
		xrtValueRelease((xvalue*)claims);
		goto fail;
	}

	/* 拆段并重建签名输入 */
	const char *pHead, *pClaimsB64, *pSigB64;
	size_t iHeadSize, iClaimsSize, iSigSize;
	if ( !xjwt__split(sToken, &pHead, &iHeadSize, &pClaimsB64, &iClaimsSize, &pSigB64, &iSigSize) ) {
		xrtValueRelease((xvalue*)claims);
		xjwt__error(XJWT_ERROR_MALFORMED, "token split failed");
		goto fail;
	}
	size_t n = iHeadSize + 1 + iClaimsSize;
	char* sInput = (char*)xrtMalloc(n + 1);
	if ( sInput == NULL ) {
		xrtValueRelease((xvalue*)claims);
		goto fail;
	}
	memcpy(sInput, pHead, iHeadSize);
	sInput[iHeadSize] = '.';
	memcpy(sInput + iHeadSize + 1, pClaimsB64, iClaimsSize);
	sInput[n] = 0;

	/* 解码签名 */
	size_t iSigBinSize = 0;
	unsigned char* pSigBin = xjwt__base64url_decode(pSigB64, iSigSize, &iSigBinSize);
	if ( pSigBin == NULL ) {
		xrtFree(sInput);
		xrtValueRelease((xvalue*)claims);
		goto fail;
	}

	*psInput = sInput; *pn = n; *ppSig = pSigBin; *pSigSize = iSigBinSize;
	return claims;

fail:
	xrtFree((void*)*pKid); *pKid = NULL;
	return NULL;
}

xvalue* xjwtVerify(const char* token, const char* keyPem, const xjwtcheck* check)
{
	if ( token == NULL || keyPem == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtVerify: null argument");
		return NULL;
	}

	int alg = XJWT_ALG_INVALID;
	const char* sKid = NULL;
	char* sInput = NULL;
	unsigned char* pSigBin = NULL;
	size_t n = 0, iSigBinSize = 0;
	xvalue* claims = xjwt__verify_prepare(token, check, &alg, &sKid,
	                                      &sInput, &n, &pSigBin, &iSigBinSize);
	if ( claims == NULL ) return NULL;
	xrtFree((void*)sKid);  /* 单钥 PEM 路径不选钥，kid 不参与 */

	bool ok = xjwt__verify(alg, sInput, n, keyPem, pSigBin, iSigBinSize);
	xrtFree(sInput); xrtFree(pSigBin);
	if ( !ok ) {
		xrtValueRelease((xvalue*)claims);
		return NULL;
	}
	return claims;
}

/* ------------------------------------------------------------------ */
/* 解码（不验签）                                                        */
/* ------------------------------------------------------------------ */
xvalue* xjwtDecode(const char* token, int* pAlg)
{
	if ( token == NULL ) return NULL;
	const char *pHead, *pClaims, *pSig;
	size_t iHeadSize, iClaimsSize, iSigSize;
	if ( !xjwt__split(token, &pHead, &iHeadSize, &pClaims, &iClaimsSize, &pSig, &iSigSize) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "invalid token format");
		return NULL;
	}
	size_t n = 0;
	unsigned char* pJson = xjwt__base64url_decode(pClaims, iClaimsSize, &n);
	if ( pJson == NULL ) return NULL;
	xvalue* p = xrtJsonParse(xrtStrViewN((cstr)pJson, n));
	xrtFree(pJson);
	if ( p == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "claims JSON parse failed");
		return NULL;
	}
	if ( pAlg != NULL ) {
		/* 解析 header alg */
		unsigned char* pHeadJson = xjwt__base64url_decode(pHead, iHeadSize, &n);
		if ( pHeadJson != NULL ) {
			xvalue* pHeadVal = xrtJsonParse(xrtStrViewN((cstr)pHeadJson, n));
			xrtFree(pHeadJson);
			if ( pHeadVal != NULL ) {
				xstrview sv;
				if ( xrtValueGetString(xrtValueObjectGet(pHeadVal, xrtStrView("alg")), &sv) ) {
					char a[16];
					if ( sv.Size < sizeof(a) ) {
						memcpy(a, sv.Data, sv.Size); a[sv.Size] = 0;
						*pAlg = xjwtAlgParse(a);
					}
				}
				xrtValueRelease(pHeadVal);
			}
		}
	}
	return p;
}

/* ------------------------------------------------------------------ */
/* 解码 header（不验签）：kid 为堆拷贝，调用方 xrtFree                    */
/* ------------------------------------------------------------------ */
xvalue* xjwtDecodeHeader(const char* token, int* pAlg, const char** pKid)
{
	if ( token == NULL ) return NULL;
	const char *pHead, *pClaims, *pSig;
	size_t iHeadSize, iClaimsSize, iSigSize;
	if ( !xjwt__split(token, &pHead, &iHeadSize, &pClaims, &iClaimsSize, &pSig, &iSigSize) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "invalid token format");
		return NULL;
	}
	size_t n = 0;
	unsigned char* pJson = xjwt__base64url_decode(pHead, iHeadSize, &n);
	if ( pJson == NULL ) return NULL;
	xvalue* p = xrtJsonParse(xrtStrViewN((cstr)pJson, n));
	xrtFree(pJson);
	if ( p == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "header JSON parse failed");
		return NULL;
	}
	if ( pAlg != NULL ) {
		*pAlg = XJWT_ALG_INVALID;
		xstrview sv;
		if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("alg")), &sv) ) {
			char a[16];
			if ( sv.Size < sizeof(a) ) {
				memcpy(a, sv.Data, sv.Size); a[sv.Size] = 0;
				*pAlg = xjwtAlgParse(a);
			}
		}
	}
	if ( pKid != NULL ) {
		*pKid = NULL;
		xstrview sv;
		xvalue* pKidVal = xrtValueObjectGet(p, xrtStrView("kid"));
		if ( pKidVal != NULL && xrtValueGetString(pKidVal, &sv) && sv.Size > 0 ) {
			/* 超长 kid 视为"存在但不可用"：整条验证拒绝，
			 * 不允许退化成"无 kid 取第一把钥"绕过严格匹配 */
			if ( sv.Size >= 256 ) {
				xrtValueRelease(p);
				xjwt__error(XJWT_ERROR_PARSE, "kid too long (>= 256)");
				return NULL;
			}
			char* s = (char*)xrtMalloc(sv.Size + 1);
			if ( s != NULL ) {
				memcpy(s, sv.Data, sv.Size); s[sv.Size] = 0;
				*pKid = s;
			}
		}
	}
	return p;
}

/* ------------------------------------------------------------------ */
/* 算法名                                                              */
/* ------------------------------------------------------------------ */
const char* xjwtAlgName(int alg)
{
	switch ( alg ) {
	case XJWT_ALG_HS256: return "HS256";
	case XJWT_ALG_HS384: return "HS384";
	case XJWT_ALG_HS512: return "HS512";
	case XJWT_ALG_RS256: return "RS256";
	case XJWT_ALG_RS384: return "RS384";
	case XJWT_ALG_RS512: return "RS512";
	case XJWT_ALG_ES256: return "ES256";
	}
	return NULL;
}

int xjwtAlgParse(const char* name)
{
	if ( name == NULL ) return XJWT_ALG_INVALID;
	if ( strcmp(name, "HS256") == 0 ) return XJWT_ALG_HS256;
	if ( strcmp(name, "HS384") == 0 ) return XJWT_ALG_HS384;
	if ( strcmp(name, "HS512") == 0 ) return XJWT_ALG_HS512;
	if ( strcmp(name, "RS256") == 0 ) return XJWT_ALG_RS256;
	if ( strcmp(name, "RS384") == 0 ) return XJWT_ALG_RS384;
	if ( strcmp(name, "RS512") == 0 ) return XJWT_ALG_RS512;
	if ( strcmp(name, "ES256") == 0 ) return XJWT_ALG_ES256;
	return XJWT_ALG_INVALID;
}
