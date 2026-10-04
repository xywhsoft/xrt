/* xjwt 测试：单 TU（XRT_IMPLEMENTATION 只定义一次）。 */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include "../xjwt.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_keys.h"
#include "time_bounds_cases.h"

/* ---- 计数分配器：泄漏回归实测（main 首行安装，须在首个 xrt 分配前） ---- */
#include <stdlib.h>
long g_probeLiveBytes = 0;
static long s_probeLive = 0;
static ptr probe_alloc(ptr ctx, size_t size)
{
	(void)ctx;
	unsigned char* p = (unsigned char*)malloc(size + 16);
	if ( p == NULL ) return NULL;
	*(size_t*)p = size;
	s_probeLive++; g_probeLiveBytes += (long)size;
	return (ptr)(p + 16);
}
static void probe_free(ptr ctx, ptr mem)
{
	(void)ctx;
	if ( mem ) {
		unsigned char* p = (unsigned char*)mem - 16;
		s_probeLive--; g_probeLiveBytes -= (long)(*(size_t*)p);
		free(p);
	}
}
static ptr probe_realloc(ptr ctx, ptr mem, size_t size)
{
	(void)ctx;
	if ( mem == NULL ) return probe_alloc(ctx, size);
	unsigned char* p = (unsigned char*)mem - 16;
	unsigned char* q = (unsigned char*)realloc(p, size + 16);
	if ( q == NULL ) return NULL;
	g_probeLiveBytes -= (long)(*(size_t*)q);
	*(size_t*)q = size;
	g_probeLiveBytes += (long)size;
	return (ptr)(q + 16);
}

/* fuzz 用的已解析公钥 */
static xjwtkey* g_fuzzKey;

static int s_pass = 0, s_fail = 0;

#define CHECK(expr, msg) do { \
	if ( !(expr) ) { printf("FAIL: %s\n", msg); s_fail++; } \
	else { s_pass++; } \
} while (0)

/* 手工签名不可信 JSON，确保拒绝原因是字段语义而非无效签名。 */
static char* jwt_test_sign_json(const char* header, const char* claims,
	int alg, const char* key)
{
	char* h64 = xjwt__base64url_encode(header, strlen(header));
	char* c64 = xjwt__base64url_encode(claims, strlen(claims));
	char* input = NULL;
	char* token = NULL;
	unsigned char signature[1024];
	size_t sigSize = 0;
	if ( h64 == NULL || c64 == NULL ) goto cleanup;
	size_t inputSize = strlen(h64) + 1u + strlen(c64);
	input = (char*)xrtMalloc(inputSize + 1u);
	if ( input == NULL ) goto cleanup;
	snprintf(input, inputSize + 1u, "%s.%s", h64, c64);
	if ( xjwt__sign(alg, input, inputSize, key, signature,
		sizeof(signature), &sigSize) )
		token = xjwt__join(header, claims, signature, sigSize);
cleanup:
	xrtFree(h64);
	xrtFree(c64);
	xrtFree(input);
	return token;
}

static bool ec_public_der_rejected(const void* pDer, size_t iSize)
{
	str pem = xrtPemEncodeNew("PUBLIC KEY", pDer, iSize);
	xjwtkey* key = pem != NULL ? xjwtKeyParse(pem) : NULL;
	bool rejected = pem != NULL && key == NULL;
	xjwtKeyFree(key);
	xrtFree(pem);
	return rejected;
}

static bool ec_private_der_rejected(const char* sLabel,
	const void* pDer, size_t iSize)
{
	str pem = xrtPemEncodeNew(sLabel, pDer, iSize);
	unsigned char scalar[32];
	bool encoded = pem != NULL;
	bool accepted = pem != NULL && ecdsa_private_parse(pem, scalar);
	xrtSecureZero(scalar, sizeof(scalar));
	xrtFree(pem);
	return encoded && !accepted;
}

static bytes pkcs8_with_options(const char* sPem,
	xbytesview Attributes, xbytesview Public, uint64 iVersion,
	size_t* pOutSize)
{
	xpemblock Block;
	xdercursor Outer, Fields;
	xdervalue Value;
	xbytesview Parts[2];
	xbuffer Content, Encoded, PublicBits;
	bytes pDer = NULL, pResult = NULL;
	size_t iDerSize = 0;
	bool bOk = false;

	*pOutSize = 0;
	xrtBufferInit(&Content);
	xrtBufferInit(&Encoded);
	xrtBufferInit(&PublicBits);
	if ( !xrtPemFind(sPem, strlen(sPem), "PRIVATE KEY", &Block) )
		goto cleanup;
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL || !xrtDerValidate(pDer, iDerSize) ||
		!xrtDerInit(&Outer, pDer, iDerSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerEnter(&Value, &Fields) ) goto cleanup;
	if ( xrtDerRead(&Fields, &Value) != XDER_VALUE ) goto cleanup;
	for ( int i = 0; i < 2; i++ ) {
		if ( xrtDerRead(&Fields, &Value) != XDER_VALUE ) goto cleanup;
		Parts[i] = Value.Raw;
	}
	if ( !xrtDerDone(&Fields) ) goto cleanup;
	bOk = xrtDerAppendUInt64(&Content, iVersion) &&
		xrtBufferAppend(&Content, Parts[0]) &&
		xrtBufferAppend(&Content, Parts[1]);
	if ( bOk && Attributes.Data != NULL )
		bOk = xrtDerAppend(&Content, XASN1_CONTEXT, 0u,
			true, Attributes);
	if ( bOk && Public.Data != NULL ) {
		static const uint8 unused = 0;
		bOk = xrtBufferAppend(&PublicBits,
			(xbytesview){ &unused, 1u }) &&
			xrtBufferAppend(&PublicBits, Public) &&
			xrtDerAppend(&Content, XASN1_CONTEXT, 1u, false,
				xrtBufferView(&PublicBits));
	}
	if ( bOk ) bOk = xrtDerAppend(&Encoded, XASN1_UNIVERSAL,
		XASN1_SEQUENCE, true, xrtBufferView(&Content));
	if ( bOk ) {
		pResult = Encoded.Data;
		*pOutSize = Encoded.Size;
		Encoded.Data = NULL;
		Encoded.Size = Encoded.Capacity = 0;
	}

cleanup:
	if ( pDer != NULL ) {
		xrtSecureZero(pDer, iDerSize);
		xrtFree(pDer);
	}
	if ( Content.Data != NULL ) xrtSecureZero(Content.Data, Content.Size);
	if ( Encoded.Data != NULL ) xrtSecureZero(Encoded.Data, Encoded.Size);
	if ( PublicBits.Data != NULL )
		xrtSecureZero(PublicBits.Data, PublicBits.Size);
	xrtBufferUnit(&Content);
	xrtBufferUnit(&Encoded);
	xrtBufferUnit(&PublicBits);
	return pResult;
}

static bytes pkcs8_with_attributes(const char* sPem,
	xbytesview Attributes, size_t* pOutSize)
{
	return pkcs8_with_options(sPem, Attributes,
		(xbytesview){ NULL, 0 }, 0u, pOutSize);
}

/* 从测试 SPKI 中取出公钥 BIT STRING 内容，供 OneAsymmetricKey 外层字段复用。 */
static bytes spki_public_copy(const char* sPem, size_t* pOutSize)
{
	xpemblock Block;
	xdercursor Outer, Fields;
	xdervalue Value;
	xbytesview Public;
	uint8 iUnused;
	size_t iDerSize = 0;
	bytes pDer = NULL, pCopy = NULL;

	*pOutSize = 0;
	if ( !xrtPemFind(sPem, strlen(sPem), "PUBLIC KEY", &Block) )
		return NULL;
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL || !xrtDerValidate(pDer, iDerSize) ||
		!xrtDerInit(&Outer, pDer, iDerSize) ||
		(xrtDerRead(&Outer, &Value) != XDER_VALUE) ||
		!xrtDerEnter(&Value, &Fields) ||
		(xrtDerRead(&Fields, &Value) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &Value) != XDER_VALUE) ||
		!xrtDerBitString(&Value, &Public, &iUnused) ||
		(iUnused != 0u) || !xrtDerDone(&Fields) ) goto cleanup;
	pCopy = (bytes)xrtMalloc(Public.Size);
	if ( pCopy != NULL ) {
		memcpy(pCopy, Public.Data, Public.Size);
		*pOutSize = Public.Size;
	}
cleanup:
	xrtFree(pDer);
	return pCopy;
}

static bool one_asym_public_offsets(const void* pDer, size_t iSize,
	size_t* pTag, size_t* pUnused)
{
	xdercursor Outer, Fields;
	xdervalue Value;

	if ( !xrtDerInit(&Outer, pDer, iSize) ||
		(xrtDerRead(&Outer, &Value) != XDER_VALUE) ||
		!xrtDerEnter(&Value, &Fields) ) return false;
	for ( int i = 0; i < 4; i++ ) {
		if ( xrtDerRead(&Fields, &Value) != XDER_VALUE ) return false;
	}
	if ( !xrtDerDone(&Fields) ||
		(Value.Tag.Class != XASN1_CONTEXT) ||
		(Value.Tag.Number != 1u) || Value.Tag.Constructed ||
		(Value.Value.Size < 2u) ) return false;
	*pTag = (size_t)(Value.Raw.Data - (const uint8*)pDer);
	*pUnused = (size_t)(Value.Value.Data - (const uint8*)pDer);
	return true;
}

static bool rsa_private_der_rejected(const void* pDer, size_t iSize)
{
	str pem = xrtPemEncodeNew("PRIVATE KEY", pDer, iSize);
	xrsaprivatekey key;
	xjwt__rsa_owned* owned = NULL;
	bool encoded = pem != NULL;
	bool accepted = pem != NULL && xjwt__rsa_private_parse(pem, &key, &owned);
	xjwt__rsa_owned_free(owned);
	xrtFree(pem);
	return encoded && !accepted;
}

int main(void)
{
	xallocator tProbeAlloc = { NULL, probe_alloc, probe_realloc, probe_free };
	xrtSetAllocator(&tProbeAlloc);
	g_fuzzKey = xjwtKeyParse(K_RSA_PUB);
	/* ---- HS256 往返 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("user123")));
		xrtValueObjectSetNew(claims, xrtStrView("role"),
			xrtValueString(xrtStrView("admin")));

		char* token = xjwtHs256(claims, "test-secret", 3600);
		CHECK(token != NULL, "xjwtHs256 returns token");
		if ( token != NULL ) {
			CHECK(strlen(token) > 30, "token is non-trivial length");

			xjwtcheck check;
			xjwtCheckInit(&check);
			xvalue* out = xjwtVerify(token, "test-secret", &check);
			CHECK(out != NULL, "xjwtVerify succeeds on valid token");
			if ( out != NULL ) {
				xstrview sub;
				CHECK(xrtValueGetString(
					xrtValueObjectGet(out, xrtStrView("sub")), &sub)
					&& sub.Size == 7 && memcmp(sub.Data, "user123", 7) == 0,
					"sub claim matches");
				CHECK(xrtValueIs(
					xrtValueObjectGet(out, xrtStrView("exp")), XVALUE_INT),
					"exp auto-injected");
				xrtValueRelease(out);
			}
			xvalue* bad = xjwtVerify(token, "wrong-secret", NULL);
			CHECK(bad == NULL, "wrong secret rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- Base64url 末组必须规范化，签名串不可有等价别名 ---- */
	{
		static const char sAlphabet[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		unsigned char Zero = 0;
		size_t iDecoded = 0;
		unsigned char* pDecoded = xjwt__base64url_decode("AA", 2u, &iDecoded);
		CHECK(pDecoded != NULL && iDecoded == 1u && pDecoded[0] == 0u,
			"canonical two-character base64url accepted");
		xrtFree(pDecoded);
		CHECK(xjwt__base64url_decode("AB", 2u, &iDecoded) == NULL &&
			xjwtLastError() == XJWT_ERROR_MALFORMED,
			"nonzero four pad bits rejected");
		pDecoded = xjwt__base64url_decode("AAA", 3u, &iDecoded);
		CHECK(pDecoded != NULL && iDecoded == 2u &&
			pDecoded[0] == 0u && pDecoded[1] == 0u,
			"canonical three-character base64url accepted");
		xrtFree(pDecoded);
		CHECK(xjwt__base64url_decode("AAB", 3u, &iDecoded) == NULL &&
			xjwtLastError() == XJWT_ERROR_MALFORMED,
			"nonzero two pad bits rejected");
		CHECK(xjwt__base64url_encode(&Zero, SIZE_MAX) == NULL &&
			xjwtLastError() == XJWT_ERROR_ARGUMENT,
			"base64url encode size overflow rejected");

		xvalue* claims = xrtValueObject();
		char* token = xjwtHs256(claims, "canonical-secret", 60);
		CHECK(token != NULL, "canonical signature test token signed");
		if ( token != NULL ) {
			size_t iSize = strlen(token);
			const char* pDigit = strchr(sAlphabet, token[iSize - 1u]);
			CHECK(pDigit != NULL &&
				((size_t)(pDigit - sAlphabet) % 4u) == 0u,
				"HS256 signature has canonical final digit");
			if ( pDigit != NULL &&
				((size_t)(pDigit - sAlphabet) % 4u) == 0u ) {
				char* sAlias = (char*)xrtMalloc(iSize + 1u);
				if ( sAlias != NULL ) {
					memcpy(sAlias, token, iSize + 1u);
					sAlias[iSize - 1u] = pDigit[1];
					CHECK(xjwtVerify(sAlias, "canonical-secret", NULL) == NULL &&
						xjwtLastError() == XJWT_ERROR_MALFORMED,
						"equivalent noncanonical signature rejected");
					xrtFree(sAlias);
				}
			}
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- HS384 / HS512 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("k"),
			xrtValueString(xrtStrView("v")));
		char* t384 = xjwtHs384(claims, "s3", 60);
		CHECK(t384 != NULL && xjwtVerify(t384, "s3", NULL) != NULL,
			"HS384 round-trip");
		if ( t384 ) xrtFree(t384);
		char* t512 = xjwtHs512(claims, "s5", 60);
		CHECK(t512 != NULL && xjwtVerify(t512, "s5", NULL) != NULL,
			"HS512 round-trip");
		if ( t512 ) xrtFree(t512);
		xrtValueRelease(claims);
	}

	/* ---- 过期拒绝 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("x")));
		char* token = xjwtHs256(claims, "k", -10);
		if ( token != NULL ) {
			xvalue* out = xjwtVerify(token, "k", NULL);
			CHECK(out == NULL, "expired token rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- iss / aud 校验 ---- */
	{
		xjwtconfig cfg;
		xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256;
		cfg.KeyPem = "k";
		cfg.Issuer = "myapp";
		cfg.Audience = "myusers";
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("d"),
			xrtValueString(xrtStrView("v")));
		char* token = xjwtSign(&cfg, claims);
		if ( token != NULL ) {
			xjwtcheck check;
			xjwtCheckInit(&check);
			check.Issuer = "myapp";
			check.Audience = "myusers";
			CHECK(xjwtVerify(token, "k", &check) != NULL, "iss+aud match");

			check.Issuer = "wrong";
			CHECK(xjwtVerify(token, "k", &check) == NULL, "iss mismatch rejected");

			check.Issuer = "myapp";
			check.Audience = "wrong";
			CHECK(xjwtVerify(token, "k", &check) == NULL, "aud mismatch rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- 解码（不验签） ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("n"), xrtValueInt(42));
		char* token = xjwtHs256(claims, "k", 60);
		if ( token != NULL ) {
			int alg = 0;
			xvalue* out = xjwtDecode(token, &alg);
			CHECK(out != NULL, "decode succeeds");
			CHECK(alg == XJWT_ALG_HS256, "decode returns alg HS256");
			if ( out ) xrtValueRelease(out);
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- 算法名 ---- */
	CHECK(strcmp(xjwtAlgName(XJWT_ALG_HS256), "HS256") == 0, "algName HS256");
	CHECK(xjwtAlgParse("RS256") == XJWT_ALG_RS256, "algParse RS256");
	CHECK(xjwtAlgParse("bogus") == XJWT_ALG_INVALID, "algParse bogus");

	/* ---- 安全：alg=none 拒绝 ---- */
	CHECK(xjwtAlgParse("none") == XJWT_ALG_INVALID, "alg 'none' rejected");

	/* ---- 安全：四段 token 拒绝 ---- */
	{
		xvalue* out = xjwtDecode("a.b.c.d", NULL);
		CHECK(out == NULL, "4-segment token rejected");
	}

	/* ---- 安全：算法混淆（HMAC token + PEM key） ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("x"),
			xrtValueString(xrtStrView("v")));
		char* token = xjwtHs256(claims, "hmac_secret", 60);
		if ( token != NULL ) {
			/* 试图用 PEM 公钥验 HMAC token → 应被拒绝 */
			xvalue* out = xjwtVerify(token,
				"-----BEGIN PUBLIC KEY-----\nMAA=\n-----END PUBLIC KEY-----",
				NULL);
			CHECK(out == NULL, "alg confusion: HS256 token with PEM key rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- 安全：空段 token 拒绝 ---- */
	{
		CHECK(xjwtDecode(".b.c", NULL) == NULL, "empty header rejected");
		CHECK(xjwtDecode("a..c", NULL) == NULL, "empty claims rejected");
		CHECK(xjwtDecode("a.b.", NULL) == NULL, "empty signature rejected");
	}

	/* ============ RS 族（openssl 参考密钥） ============ */

	/* ---- RS256/384/512 往返 ---- */
	{
		const char* names[] = { "RS256", "RS384", "RS512" };
		for ( int i = 0; i < 3; i++ ) {
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("rs-user")));
			char* token;
			if ( i == 0 ) token = xjwtRs256(claims, K_RSA_PRIV, 3600);
			else if ( i == 1 ) token = xjwtRs384(claims, K_RSA_PRIV, 3600);
			else token = xjwtRs512(claims, K_RSA_PRIV, 3600);
			CHECK(token != NULL, "RS sign returns token");
			if ( token != NULL ) {
				xvalue* out = xjwtVerify(token, K_RSA_PUB, NULL);
				CHECK(out != NULL, "RS verify round-trip");
				if ( out != NULL ) {
					xstrview sv;
					CHECK(xrtValueGetString(xrtValueObjectGet(out, xrtStrView("sub")), &sv)
						&& sv.Size == 7 && memcmp(sv.Data, "rs-user", 7) == 0,
						"RS round-trip sub preserved");
					xrtValueRelease(out);
				}
				xrtFree(token);
			}
			(void)names;
			xrtValueRelease(claims);
		}
	}

	/* ---- RS256 篡改签名拒绝 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("tamper")));
		char* token = xjwtRs256(claims, K_RSA_PRIV, 3600);
		if ( token != NULL ) {
			size_t n = strlen(token);
			token[n - 2] = (token[n - 2] == 'A') ? 'B' : 'A';
			CHECK(xjwtVerify(token, K_RSA_PUB, NULL) == NULL,
				"RS256 tampered signature rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- RS256 错误公钥拒绝 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("wrongkey")));
		char* token = xjwtRs256(claims, K_RSA_PRIV, 3600);
		if ( token != NULL ) {
			CHECK(xjwtVerify(token, K_EC_PUB, NULL) == NULL,
				"RS256 with wrong public key rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ============ ES256 ============ */

	/* ---- ES256 往返 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("es-user")));
		char* token = xjwtEs256(claims, K_EC_PRIV, 3600);
		CHECK(token != NULL, "ES256 sign returns token");
		if ( token != NULL ) {
			const char *head, *body, *sig;
			size_t head_size, body_size, sig_size;
			CHECK(xjwt__split(token, &head, &head_size, &body, &body_size,
				&sig, &sig_size) &&
				xjwt__base64url_decode_size(sig, sig_size) == 64,
				"ES256 JWS signature is fixed-width R||S");
			xvalue* out = xjwtVerify(token, K_EC_PUB, NULL);
			CHECK(out != NULL, "ES256 verify round-trip");
			if ( out != NULL ) xrtValueRelease(out);
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- ES256 篡改拒绝 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("estamper")));
		char* token = xjwtEs256(claims, K_EC_PRIV, 3600);
		if ( token != NULL ) {
			size_t n = strlen(token);
			token[n - 3] = (token[n - 3] == 'A') ? 'B' : 'A';
			CHECK(xjwtVerify(token, K_EC_PUB, NULL) == NULL,
				"ES256 tampered signature rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- ES256 用 RSA 公钥验（密钥类型不匹配）拒绝 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("cross")));
		char* token = xjwtEs256(claims, K_EC_PRIV, 3600);
		if ( token != NULL ) {
			CHECK(xjwtVerify(token, K_RSA_PUB, NULL) == NULL,
				"ES256 token with RSA public key rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ============ openssl 互操作 ============ */

	/* ---- openssl 签的 RS256 令牌 → xjwt 验证 ---- */
	{
		xvalue* out = xjwtVerify(K_TOKEN_RS256_OPENSSL, K_RSA_PUB, NULL);
		CHECK(out != NULL, "openssl-signed RS256 verified by xjwt");
		if ( out != NULL ) {
			xstrview sv;
			CHECK(xrtValueGetString(xrtValueObjectGet(out, xrtStrView("sub")), &sv)
				&& sv.Size == 10 && memcmp(sv.Data, "interop-rs", 10) == 0,
				"openssl RS256 claims readable");
			xrtValueRelease(out);
		}
	}

	/* ---- OpenSSL DER 经 JWS R||S 转换后互认；原始 DER 拒绝 ---- */
	{
		xvalue* out = xjwtVerify(K_TOKEN_ES256_OPENSSL, K_EC_PUB, NULL);
		CHECK(out != NULL, "openssl-signed ES256 JWS verified by xjwt");
		if ( out != NULL ) xrtValueRelease(out);
		CHECK(xjwtVerify(K_TOKEN_ES256_DER_OPENSSL, K_EC_PUB, NULL) == NULL,
			"non-JWS DER ES256 signature rejected");
	}

	/* ---- 签发时分配失败不得返回缺少 exp 的令牌 ---- */
	{
		xvalue* claims = xrtValueObject();
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.KeyPem = "secret"; cfg.ExpireSeconds = 3600;
		CHECK(xrtMemDebugFailAfter(0), "JWT allocation fault armed");
		char* token = xjwtSign(&cfg, claims);
		bool failed = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		CHECK(failed && token == NULL, "JWT signing fails closed on exp allocation");
		CHECK(!xrtValueObjectHas(claims, xrtStrView("exp")) &&
			!xrtValueObjectHas(claims, xrtStrView("iat")),
			"JWT allocation failure leaves caller claims unchanged");
		xrtFree(token);
		xrtValueRelease(claims);
	}

	/* ---- HS/RS/ES 签发逐点 OOM：不发布令牌，不修改输入 ---- */
	{
		const int algorithms[] = {
			XJWT_ALG_HS256, XJWT_ALG_RS256, XJWT_ALG_ES256
		};
		const char* keys[] = {
			"test-secret", K_RSA_PRIV, K_EC_PRIV_PKCS8
		};
		const char* publicKeys[] = {
			"test-secret", K_RSA_PUB, K_EC_PUB
		};
		for ( size_t alg = 0; alg < 3u; alg++ ) {
			xvalue* claims = xrtValueObject();
			xjwtconfig cfg;
			bool armed = true, closed = true, unchanged = true;
			bool completed = false, verified = false;
			int injected = 0;
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("original")));
			xjwtConfigInit(&cfg);
			cfg.Alg = algorithms[alg];
			cfg.KeyPem = keys[alg];
			cfg.Subject = "configured";
			cfg.ExpireSeconds = 3600;
			for ( uint64 point = 0; point < 128u; point++ ) {
				char original[32];
				char* token;
				bool hit;
				if ( !xrtMemDebugFailAfter(point) ) {
					armed = false;
					break;
				}
				token = xjwtSign(&cfg, claims);
				hit = xrtMemDebugFailTriggered();
				xrtMemDebugFailClear();
				if ( hit ) {
					injected++;
					if ( token != NULL ) {
						printf("OOM issued token: alg=%d point=%llu\n",
							algorithms[alg], (unsigned long long)point);
						closed = false;
					}
				} else {
					completed = token != NULL;
					if ( token != NULL ) {
						xvalue* out = xjwtVerify(token, publicKeys[alg], NULL);
						char subject[32];
						verified = out != NULL &&
							xjwtClaimString(out, "sub", subject, sizeof(subject)) &&
							strcmp(subject, "configured") == 0;
						if ( out != NULL ) xrtValueRelease(out);
					}
				}
				unchanged = unchanged &&
					xjwtClaimString(claims, "sub", original, sizeof(original)) &&
					strcmp(original, "original") == 0 &&
					!xrtValueObjectHas(claims, xrtStrView("iat")) &&
					!xrtValueObjectHas(claims, xrtStrView("exp"));
				xrtFree(token);
				xrtClearError();
				if ( !hit ) break;
			}
			CHECK(armed && completed && verified && injected > 0,
				"JWT HS/RS/ES OOM sweep reaches valid signing");
			CHECK(closed, "JWT HS/RS/ES OOM never issues partial token");
			CHECK(unchanged, "JWT HS/RS/ES OOM leaves caller claims intact");
			xrtValueRelease(claims);
		}
	}

	/* ---- 签发字段只进入 token；重复签发及后期失败不污染调用方 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("iss"),
			xrtValueString(xrtStrView("original")));
		const xvalue* input = claims;
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.KeyPem = "secret"; cfg.ExpireSeconds = 3600;
		cfg.Issuer = "first";
		char* token = xjwtSign(&cfg, input);
		CHECK(token != NULL, "JWT signs const claims");
		if ( token != NULL ) {
			xvalue* out = xjwtVerify(token, "secret", NULL);
			char issuer[32];
			CHECK(out != NULL && xjwtClaimString(out, "iss", issuer, sizeof(issuer)) &&
				strcmp(issuer, "first") == 0 &&
				xrtValueObjectHas(out, xrtStrView("exp")),
				"JWT config fields appear in signed claims");
			xrtValueRelease(out);
			xrtFree(token);
		}
		char original[32];
		CHECK(xjwtClaimString(claims, "iss", original, sizeof(original)) &&
			strcmp(original, "original") == 0 &&
			!xrtValueObjectHas(claims, xrtStrView("exp")) &&
			!xrtValueObjectHas(claims, xrtStrView("iat")),
			"JWT successful signing leaves caller claims unchanged");

		cfg.Issuer = "second"; cfg.ExpireSeconds = 0;
		token = xjwtSign(&cfg, input);
		CHECK(token != NULL, "JWT re-signs same claims");
		if ( token != NULL ) {
			xvalue* out = xjwtVerify(token, "secret", NULL);
			char issuer[32];
			CHECK(out != NULL && xjwtClaimString(out, "iss", issuer, sizeof(issuer)) &&
				strcmp(issuer, "second") == 0 &&
				!xrtValueObjectHas(out, xrtStrView("exp")),
				"JWT repeat signing does not reuse injected claims");
			xrtValueRelease(out);
			xrtFree(token);
		}
		cfg.Alg = XJWT_ALG_RS256; cfg.KeyPem = "invalid PEM";
		CHECK(xjwtSign(&cfg, input) == NULL,
			"JWT invalid key fails after claims preparation");
		CHECK(xjwtClaimString(claims, "iss", original, sizeof(original)) &&
			strcmp(original, "original") == 0 &&
			!xrtValueObjectHas(claims, xrtStrView("iat")),
			"JWT signing failure leaves caller claims unchanged");
		xrtValueRelease(claims);
	}

	/* ---- 密钥封装格式覆盖：PKCS#1 RSA / PKCS#8 EC ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("pkcs1")));
		char* token = xjwtRs256(claims, K_RSA_PRIV_PKCS1, 3600);
		CHECK(token != NULL, "RS256 sign with PKCS#1 private key");
		if ( token != NULL ) {
			xvalue* out = xjwtVerify(token, K_RSA_PUB, NULL);
			CHECK(out != NULL, "PKCS#1 RSA key round-trip");
			if ( out != NULL ) xrtValueRelease(out);
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("pkcs8")));
		char* token = xjwtEs256(claims, K_EC_PRIV_PKCS8, 3600);
		CHECK(token != NULL, "ES256 sign with PKCS#8 EC private key");
		if ( token != NULL ) {
			xvalue* out = xjwtVerify(token, K_EC_PUB, NULL);
			CHECK(out != NULL, "PKCS#8 EC key round-trip");
			if ( out != NULL ) xrtValueRelease(out);
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ============ JWKS ============ */

	/* ---- JWKS 解析 ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		CHECK(jwks != NULL, "JWKS parse succeeds");
		xjwtJwksFree(jwks);
		CHECK(xjwtJwksParse("{}") == NULL, "JWKS without keys rejected");
		CHECK(xjwtJwksParse("not json") == NULL, "JWKS invalid JSON rejected");
	}
	{
		xerror* marker = xrtErrorCreate(XERR_STATE, "xrt.jwt.test", 1,
			"prior diagnostic");
		CHECK(marker != NULL, "JWKS prior diagnostic fixture created");
		if ( marker != NULL ) {
			xrtSetError(marker);
			xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
			CHECK(jwks != NULL && xrtGetError() == marker,
				"successful JWKS parse preserves prior diagnostic");
			xjwtJwksFree(jwks);
			xrtClearError();
			xrtErrorFree(marker);
		}
	}

	/* ---- JWKS 解析逐分配点 OOM：不得返回缺钥的部分密钥集 ---- */
	{
		bool armed = true, closed = true, memoryError = true;
		bool completed = false;
		int injected = 0;

		for ( uint64 point = 0; point < 512u; point++ ) {
			xjwtjwks* jwks;
			bool hit;

			xrtClearError();
			if ( !xrtMemDebugFailAfter(point) ) {
				armed = false;
				break;
			}
			jwks = xjwtJwksParse(K_JWKS);
			hit = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			if ( hit ) {
				injected++;
				if ( xrtGetError() == NULL ||
					xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
					printf("OOM lost memory error: point=%llu\n",
						(unsigned long long)point);
					memoryError = false;
				}
				if ( jwks != NULL ) {
					printf("OOM returned JWKS: point=%llu\n",
						(unsigned long long)point);
					closed = false;
				}
			} else {
				completed = jwks != NULL && xrtGetError() == NULL;
			}
			xjwtJwksFree(jwks);
			xrtClearError();
			if ( !hit ) break;
		}
		CHECK(armed && completed && injected > 0,
			"JWKS OOM sweep reaches a complete parse");
		CHECK(closed, "JWKS allocation failure returns no partial set");
		CHECK(memoryError, "JWKS allocation failure preserves memory error");
	}

	/* ---- HS/RS/ES 验签入口逐分配点 OOM：不可交付 claims 或丢失根因 ---- */
	{
		xjwtkey* key = xjwtKeyParse(K_RSA_PUB);
		xjwtkey* ecKey = xjwtKeyParse(K_EC_PUB);
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		xvalue* hsClaims = xrtValueObject();
		char* hsToken = hsClaims != NULL ?
			xjwtHs256(hsClaims, "oom-secret", 3600) : NULL;
		xrtValueRelease(hsClaims);
		CHECK(key != NULL && ecKey != NULL && jwks != NULL &&
			hsToken != NULL,
			"JWT verification OOM fixtures created");
		if ( key != NULL && ecKey != NULL && jwks != NULL &&
			hsToken != NULL ) {
			for ( int route = 0; route < 7; route++ ) {
				bool armed = true, closed = true, memoryError = true;
				bool completed = false;
				int injected = 0;

				for ( uint64 point = 0; point < 512u; point++ ) {
					xvalue* claims;
					bool hit;

					xrtClearError();
					if ( !xrtMemDebugFailAfter(point) ) {
						armed = false;
						break;
					}
					switch ( route ) {
					case 0:
						claims = xjwtVerify(K_TOKEN_RS256_OPENSSL,
							K_RSA_PUB, NULL); break;
					case 1:
						claims = xjwtVerifyKey(K_TOKEN_RS256_OPENSSL,
							key, NULL); break;
					case 2:
						claims = xjwtVerifyJwks(K_TOKEN_RS256_OPENSSL,
							jwks, NULL); break;
					case 3:
						claims = xjwtVerify(hsToken, "oom-secret", NULL);
						break;
					case 4:
						claims = xjwtVerify(K_TOKEN_ES256_OPENSSL,
							K_EC_PUB, NULL); break;
					case 5:
						claims = xjwtVerifyKey(K_TOKEN_ES256_OPENSSL,
							ecKey, NULL); break;
					default:
						claims = xjwtVerifyJwks(K_TOKEN_ES256_OPENSSL,
							jwks, NULL); break;
					}
					hit = xrtMemDebugFailTriggered();
					xrtMemDebugFailClear();
					if ( hit ) {
						injected++;
						if ( claims != NULL ) {
							printf("OOM verified token: route=%d point=%llu\n",
								route, (unsigned long long)point);
							closed = false;
						}
						if ( xrtGetError() == NULL ||
							xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
							printf("OOM lost verify error: route=%d point=%llu\n",
								route, (unsigned long long)point);
							memoryError = false;
						}
					} else {
						completed = claims != NULL;
					}
					xrtValueRelease(claims);
					xrtClearError();
					if ( !hit ) break;
				}
				CHECK(armed && completed && injected > 0,
					"JWT verification OOM sweep reaches a valid token");
				CHECK(closed, "JWT verification allocation failure stays closed");
				CHECK(memoryError,
					"JWT verification allocation failure preserves memory error");
			}
		}
		xjwtKeyFree(key);
		xjwtKeyFree(ecKey);
		xjwtJwksFree(jwks);
		xrtFree(hsToken);
	}

	/* ---- 公钥缓存与可选解码输出逐分配点 OOM ---- */
	{
		static const char sentinel[] = "output sentinel";
		for ( int route = 0; route < 5; route++ ) {
			bool armed = true, closed = true, memoryError = true;
			bool outputs = true, leakFree = true, completed = false;
			int injected = 0;
			for ( uint64 point = 0; point < 512u; point++ ) {
				xjwtkey* key = NULL;
				xvalue* value = NULL;
				int alg = XJWT_ALG_HS512;
				const char* kid = sentinel;
				bool hit;
				xrtClearError();
				long before = g_probeLiveBytes;
				if ( !xrtMemDebugFailAfter(point) ) {
					armed = false;
					break;
				}
				switch ( route ) {
				case 0: key = xjwtKeyParse(K_RSA_PUB); break;
				case 1: key = xjwtKeyParse(K_EC_PUB); break;
				case 2: value = xjwtDecode(K_TOKEN_RS256_OPENSSL, NULL); break;
				case 3: value = xjwtDecode(K_TOKEN_RS256_OPENSSL, &alg); break;
				default:
					value = xjwtDecodeHeader(K_TOKEN_RS256_OPENSSL, &alg, &kid);
					break;
				}
				hit = xrtMemDebugFailTriggered();
				xrtMemDebugFailClear();
				if ( hit ) {
					injected++;
					if ( key != NULL || value != NULL ) {
						printf("OOM returned parsed output: route=%d point=%llu\n",
							route, (unsigned long long)point);
						closed = false;
					}
					if ( xrtGetError() == NULL ||
						xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
						printf("OOM lost parse error: route=%d point=%llu\n",
							route, (unsigned long long)point);
						memoryError = false;
					}
					if ( (route >= 3 && alg != XJWT_ALG_INVALID) ||
						(route == 4 && kid != NULL) ) outputs = false;
				} else completed = key != NULL || value != NULL;
				xjwtKeyFree(key);
				xrtValueRelease(value);
				if ( route == 4 && kid != NULL &&
					kid != sentinel ) xrtFree((void*)kid);
				xrtClearError();
				if ( g_probeLiveBytes != before ) leakFree = false;
				if ( !hit ) break;
			}
			CHECK(armed && completed && injected > 0,
				"JWT key/decode OOM sweep reaches a complete result");
			CHECK(closed, "JWT key/decode OOM returns no partial output");
			CHECK(memoryError, "JWT key/decode OOM preserves memory error");
			CHECK(outputs, "JWT decode OOM clears optional outputs");
			CHECK(leakFree, "JWT key/decode OOM releases all owned memory");
		}
	}
	{
		const char* pems[] = {K_RSA_PUB, K_EC_PUB};
		for ( int i = 0; i < 2; i++ ) {
			xerror* marker = xrtErrorCreate(XERR_MEMORY, "xrt.jwt.test", 2,
				"prior memory diagnostic");
			CHECK(marker != NULL, "JWT cached-key prior diagnostic fixture created");
			if ( marker != NULL ) {
				xrtSetError(marker);
				xjwtkey* key = xjwtKeyParse(pems[i]);
				CHECK(key != NULL && xrtGetError() == marker,
					"JWT cached-key parse preserves a prior memory diagnostic on success");
				xjwtKeyFree(key);
				xrtClearError();
				xrtErrorFree(marker);
			}
		}
		xrtClearError();
		xjwtkey* key = xjwtKeyParse(K_EC_PUB);
		CHECK(key != NULL && xrtGetError() == NULL,
			"JWT EC cached-key success discards RSA probe diagnostics");
		xjwtKeyFree(key);
	}

	/* ---- 解码失败清空输出，非对象与带 NUL 的算法名不能伪装合法字段 ---- */
	{
		int alg = XJWT_ALG_RS256;
		const char* kid = "sentinel";
		CHECK(xjwtDecodeHeader(NULL, &alg, &kid) == NULL &&
			alg == XJWT_ALG_INVALID && kid == NULL &&
			xjwtLastError() == XJWT_ERROR_ARGUMENT,
			"JWT null header decode clears outputs and reports argument error");
		alg = XJWT_ALG_RS256;
		CHECK(xjwtDecode(NULL, &alg) == NULL && alg == XJWT_ALG_INVALID &&
			xjwtLastError() == XJWT_ERROR_ARGUMENT,
			"JWT null claims decode clears alg and reports argument error");
		const char* headers[] = {"[1]", "{"};
		for ( int i = 0; i < 2; i++ ) {
			char* token = jwt_test_sign_json(headers[i], "{}", XJWT_ALG_HS256, "k");
			CHECK(token != NULL, "JWT invalid-header signed fixture created");
			if ( token != NULL ) {
				alg = XJWT_ALG_RS256;
				kid = "sentinel";
				xvalue* value = xjwtDecodeHeader(token, &alg, &kid);
				CHECK(value == NULL && alg == XJWT_ALG_INVALID && kid == NULL,
					"JWT invalid header rejected with empty optional outputs");
				xrtValueRelease(value);
				value = xjwtDecode(token, &alg);
				CHECK(value == NULL && alg == XJWT_ALG_INVALID,
					"JWT claims decode with alg rejects an invalid header");
				xrtValueRelease(value);
			}
			xrtFree(token);
		}
		char* token = jwt_test_sign_json("{\"alg\":\"HS256\"}", "[1]",
			XJWT_ALG_HS256, "k");
		CHECK(token != NULL, "JWT array-claims signed fixture created");
		if ( token != NULL ) {
			xvalue* value = xjwtDecode(token, NULL);
			CHECK(value == NULL, "JWT claims decode rejects a JSON array");
			xrtValueRelease(value);
		}
		xrtFree(token);
		token = jwt_test_sign_json("{\"alg\":\"HS256\\u0000suffix\"}", "{}",
			XJWT_ALG_HS256, "k");
		CHECK(token != NULL, "JWT NUL-alg signed fixture created");
		if ( token != NULL ) {
			alg = XJWT_ALG_RS256;
			xvalue* value = xjwtDecodeHeader(token, &alg, NULL);
			CHECK(value != NULL && alg == XJWT_ALG_INVALID,
				"JWT NUL-containing algorithm does not match a supported name");
			xrtValueRelease(value);
			value = xjwtVerify(token, "k", NULL);
			CHECK(value == NULL && xjwtLastError() == XJWT_ERROR_ALG_MISMATCH,
				"JWT valid HMAC cannot authorize a NUL-containing algorithm");
			xrtValueRelease(value);
		}
		xrtFree(token);
	}

	/* ---- kid 字段存在时须为字符串，空值不等价于缺失 ---- */
	{
		const char* kid = NULL;
		int alg = XJWT_ALG_INVALID;
		xvalue* header = xjwtDecodeHeader(
			"eyJhbGciOiJSUzI1NiIsImtpZCI6NDJ9.e30.AA",
			&alg, &kid);
		CHECK(header == NULL && kid == NULL,
			"JWT numeric kid rejected before JWKS fallback");
		xrtValueRelease(header);
		xrtFree((void*)kid);
		kid = NULL;
		header = xjwtDecodeHeader(
			"eyJhbGciOiJSUzI1NiIsImtpZCI6IiJ9.e30.AA",
			&alg, &kid);
		CHECK(header != NULL && kid != NULL && kid[0] == '\0',
			"JWT empty kid preserved as a present identifier");
		xrtValueRelease(header);
		xrtFree((void*)kid);
		kid = NULL;
		header = xjwtDecodeHeader(
			"eyJhbGciOiJSUzI1NiJ9.e30.AA", &alg, &kid);
		CHECK(header != NULL && kid == NULL && alg == XJWT_ALG_RS256,
			"JWT absent kid remains valid");
		xrtValueRelease(header);
		xrtFree((void*)kid);
		xvalue* claims = xrtValueObject();
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_RS256;
		cfg.KeyPem = K_RSA_PRIV;
		cfg.KeyId = "";
		char* token = xjwtSign(&cfg, claims);
		CHECK(token != NULL, "JWT signing accepts an empty configured kid");
		if ( token != NULL ) {
			xjwtjwks* noKid = xjwtJwksParse(K_JWKS_NOKID);
			CHECK(noKid != NULL, "JWT no-kid key set fixture created");
			xvalue* verified = noKid != NULL ? xjwtVerifyJwks(token, noKid, NULL) : NULL;
			CHECK(verified == NULL && xjwtLastError() == XJWT_ERROR_KEY_NOT_FOUND,
				"JWT empty kid never matches a key with no identifier");
			xrtValueRelease(verified);
			xjwtJwksFree(noKid);

			xvalue* set = xrtJsonParse(xrtStrView(K_JWKS_NOKID));
			xvalue* key = xrtValueArrayGet(xrtValueObjectGet(set, xrtStrView("keys")), 0);
			bool configured = key != NULL && xrtValueObjectSetNew(key,
				xrtStrView("kid"), xrtValueString(xrtStrView("")));
			char* json = configured ? xrtJsonStringify(set, false, NULL) : NULL;
			xjwtjwks* emptyKid = json != NULL ? xjwtJwksParse(json) : NULL;
			CHECK(emptyKid != NULL, "JWT explicit empty-kid key set fixture created");
			verified = emptyKid != NULL ? xjwtVerifyJwks(token, emptyKid, NULL) : NULL;
			CHECK(verified != NULL, "JWT empty kid matches an explicit empty JWK kid");
			xrtValueRelease(verified);
			xjwtJwksFree(emptyKid);
			xrtFree(json);
			xrtValueRelease(set);
		}
		xrtFree(token);
		xrtValueRelease(claims);
	}

	/* ---- NUL kid 不允许按 C 字符串前缀选钥 ---- */
	{
		char* token = jwt_test_sign_json(
			"{\"alg\":\"RS256\",\"kid\":\"rsa-key-1\\u0000suffix\"}", "{}",
			XJWT_ALG_RS256, K_RSA_PRIV);
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		xjwtkey* key = xjwtKeyParse(K_RSA_PUB);
		CHECK(token != NULL && jwks != NULL && key != NULL,
			"JWT NUL-kid signed fixtures created");
		if ( token != NULL && jwks != NULL && key != NULL ) {
			for ( int route = 0; route < 3; route++ ) {
				xvalue* value;
				if ( route == 0 ) value = xjwtVerify(token, K_RSA_PUB, NULL);
				else if ( route == 1 ) value = xjwtVerifyKey(token, key, NULL);
				else value = xjwtVerifyJwks(token, jwks, NULL);
				CHECK(value == NULL && xjwtLastError() == XJWT_ERROR_PARSE,
					"JWT NUL kid rejected before verification or prefix selection");
				xrtValueRelease(value);
			}
		}
		xrtFree(token);
		xjwtJwksFree(jwks);
		xjwtKeyFree(key);
	}
	{
		const char* fields[] = {"kid", "kid", "kty", "crv"};
		const char* values[] = {"rsa-key-1\0suffix", NULL, "RSA\0suffix", "P-256\0suffix"};
		const size_t sizes[] = {16u, 0u, 10u, 12u};
		for ( int i = 0; i < 4; i++ ) {
			xvalue* set = xrtJsonParse(xrtStrView(K_JWKS));
			xvalue* entry = xrtValueArrayGet(xrtValueObjectGet(set, xrtStrView("keys")),
				i == 3 ? 1u : 0u);
			bool changed = entry != NULL && xrtValueObjectSetNew(entry,
				xrtStrView(fields[i]), values[i] == NULL ? xrtValueInt(42) :
				xrtValueString(xrtStrViewN(values[i], sizes[i])));
			char* json = changed ? xrtJsonStringify(set, false, NULL) : NULL;
			xjwtjwks* jwks = json != NULL ? xjwtJwksParse(json) : NULL;
			CHECK(jwks != NULL, "JWKS malformed-selector fixture parses");
			if ( jwks != NULL ) {
				xvalue* value = xjwtVerifyJwks(i == 3 ? K_TOKEN_ES256_OPENSSL :
					K_TOKEN_RS256_OPENSSL, jwks, NULL);
				CHECK(value == NULL && xjwtLastError() == XJWT_ERROR_KEY_NOT_FOUND,
					"JWKS malformed selector cannot authorize a matching prefix token");
				xrtValueRelease(value);
				value = xjwtVerifyJwks(i == 3 ? K_TOKEN_RS256_OPENSSL :
					K_TOKEN_ES256_OPENSSL, jwks, NULL);
				CHECK(value != NULL,
					"JWKS retains the other valid key after skipping a malformed selector");
				xrtValueRelease(value);
			}
			xjwtJwksFree(jwks);
			xrtFree(json);
			xrtValueRelease(set);
		}
	}

	/* ---- JWKS + RS256（带 kid）---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		if ( jwks != NULL ) {
			xjwtconfig cfg; xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_RS256; cfg.KeyPem = K_RSA_PRIV;
			cfg.KeyId = "rsa-key-1"; cfg.ExpireSeconds = 3600;
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("jwks-user")));
			char* token = xjwtSign(&cfg, claims);
			if ( token != NULL ) {
				xvalue* out = xjwtVerifyJwks(token, jwks, NULL);
				CHECK(out != NULL, "JWKS verify RS256 with kid");
				if ( out != NULL ) xrtValueRelease(out);
				xrtFree(token);
			}
			xrtValueRelease(claims);
		}
		xjwtJwksFree(jwks);
	}

	/* ---- JWKS + ES256（带 kid）---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		if ( jwks != NULL ) {
			xjwtconfig cfg; xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_ES256; cfg.KeyPem = K_EC_PRIV;
			cfg.KeyId = "ec-key-1"; cfg.ExpireSeconds = 3600;
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("jwks-ec")));
			char* token = xjwtSign(&cfg, claims);
			if ( token != NULL ) {
				xvalue* out = xjwtVerifyJwks(token, jwks, NULL);
				CHECK(out != NULL, "JWKS verify ES256 with kid");
				if ( out != NULL ) xrtValueRelease(out);
				xrtFree(token);
			}
			xrtValueRelease(claims);
		}
		xjwtJwksFree(jwks);
	}

	/* ---- JWKS 验 openssl 令牌（kid = rsa-key-1 / ec-key-1）---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		if ( jwks != NULL ) {
			xvalue* out = xjwtVerifyJwks(K_TOKEN_RS256_OPENSSL, jwks, NULL);
			CHECK(out != NULL, "JWKS verify openssl RS256 token");
			if ( out != NULL ) xrtValueRelease(out);
			out = xjwtVerifyJwks(K_TOKEN_ES256_OPENSSL, jwks, NULL);
			CHECK(out != NULL, "JWKS verify openssl ES256 token");
			if ( out != NULL ) xrtValueRelease(out);
		}
		xjwtJwksFree(jwks);
	}

	/* ---- JWKS kid 不匹配拒绝 ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		if ( jwks != NULL ) {
			xjwtconfig cfg; xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_RS256; cfg.KeyPem = K_RSA_PRIV;
			cfg.KeyId = "unknown-kid"; cfg.ExpireSeconds = 3600;
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("nomatch")));
			char* token = xjwtSign(&cfg, claims);
			if ( token != NULL ) {
				CHECK(xjwtVerifyJwks(token, jwks, NULL) == NULL,
					"JWKS unknown kid rejected");
				xrtFree(token);
			}
			xrtValueRelease(claims);
		}
		xjwtJwksFree(jwks);
	}

	/* ---- xjwtDecodeHeader ---- */
	{
		int alg = XJWT_ALG_INVALID;
		const char* kid = NULL;
		xvalue* hdr = xjwtDecodeHeader(K_TOKEN_RS256_OPENSSL, &alg, &kid);
		CHECK(hdr != NULL, "decodeHeader returns header");
		CHECK(alg == XJWT_ALG_RS256, "decodeHeader alg = RS256");
		CHECK(kid != NULL && strcmp(kid, "rsa-key-1") == 0,
			"decodeHeader kid extracted");
		if ( kid != NULL ) xrtFree((void*)kid);
		if ( hdr != NULL ) xrtValueRelease(hdr);
	}

	/* ============ 审计修复回归 ============ */

	/* ---- H1：字符串 exp 拒绝（防绕过过期检查） ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("poc")));
		xrtValueObjectSetNew(claims, xrtStrView("exp"),
			xrtValueString(xrtStrView("1000000000")));  /* 字符串，2001 年 */
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "poc-secret";
		char* token = xjwtSign(&cfg, claims);  /* 不注入 exp */
		if ( token != NULL ) {
			xjwtcheck check; xjwtCheckInit(&check);
			check.NowOverride = 1900000000;  /* 2030 年 */
			CHECK(xjwtVerify(token, "poc-secret", &check) == NULL,
				"H1 string exp rejected at future clock");
			xrtFree(token);
		} else {
			CHECK(false, "H1 string-exp token sign failed");
		}
		xrtValueRelease(claims);
	}

	/* ---- H1：字符串 nbf 拒绝 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("nbf"),
			xrtValueString(xrtStrView("9999999999")));
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "poc-secret";
		char* token = xjwtSign(&cfg, claims);
		if ( token != NULL ) {
			CHECK(xjwtVerify(token, "poc-secret", NULL) == NULL,
				"H1 string nbf rejected");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- H1：浮点 exp 拒绝（JSON 数字但非整数） ---- */
	{
		xjwtjwks* unused = NULL; (void)unused;
		/* 手工拼一个浮点 exp 的 token：header + claims 走 xjwt 分段签名不可行，
		 * 这里直接测 xjwtClaimsValid 对浮点 exp 的行为 */
		xvalue* claims = xrtJsonParse(xrtStrView(
			"{\"exp\":999999999999.5}"));
		if ( claims != NULL ) {
			CHECK(!xjwtClaimsValid(claims, NULL),
				"H1 float exp rejected by claims check");
			xrtValueRelease(claims);
		}
	}

	/* ---- H2：RSA-8192（1024 字节签名）签发不溢出 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("bigkey")));
		char* token = xjwtRs256(claims, K_RSA8K_PRIV, 3600);
		CHECK(token != NULL, "H2 RSA-8192 sign returns token");
		if ( token != NULL ) {
			xvalue* out = xjwtVerify(token, K_RSA8K_PUB, NULL);
			CHECK(out != NULL, "H2 RSA-8192 verify round-trip");
			if ( out != NULL ) xrtValueRelease(out);
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- M1：kid 含 JSON 特殊字符 → 正确转义且往返一致 ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("kidq")));
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "poc-secret";
		cfg.KeyId = "a\"b\\c";
		char* token = xjwtSign(&cfg, claims);
		CHECK(token != NULL, "M1 quote-kid sign returns token");
		if ( token != NULL ) {
			CHECK(xjwtVerify(token, "poc-secret", NULL) != NULL,
				"M1 quote-kid self-verify passes");
			const char* kid = NULL;
			xvalue* hdr = xjwtDecodeHeader(token, NULL, &kid);
			CHECK(kid != NULL && strcmp(kid, "a\"b\\c") == 0,
				"M1 quote-kid round-trips exactly");
			if ( kid ) xrtFree((void*)kid);
			if ( hdr ) xrtValueRelease(hdr);
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- M1：超长 kid（120 字符）不受缓冲限制 ---- */
	{
		char aKid[121];
		memset(aKid, 'k', 120);
		aKid[120] = 0;
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("kidl")));
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "poc-secret";
		cfg.KeyId = aKid;
		char* token = xjwtSign(&cfg, claims);
		CHECK(token != NULL, "M1 long-kid sign returns token");
		if ( token != NULL ) {
			const char* kid = NULL;
			xvalue* hdr = xjwtDecodeHeader(token, NULL, &kid);
			CHECK(kid != NULL && strcmp(kid, aKid) == 0,
				"M1 long-kid round-trips fully");
			if ( kid ) xrtFree((void*)kid);
			if ( hdr ) xrtValueRelease(hdr);
			CHECK(xjwtVerify(token, "poc-secret", NULL) != NULL,
				"M1 long-kid self-verify passes");
			xrtFree(token);
		}
		xrtValueRelease(claims);
	}

	/* ---- M2：JWKS 非 P-256 曲线条目被跳过 ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(
			"{\"keys\":[{\"kty\":\"EC\",\"kid\":\"p384-key\",\"crv\":\"P-384\","
			"\"x\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\","
			"\"y\":\"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\"}]}");
		CHECK(jwks != NULL, "M2 P-384-only JWKS parses (entry skipped)");
		if ( jwks != NULL ) {
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("nop")));
			char* token = xjwtEs256(claims, K_EC_PRIV, 3600);
			if ( token != NULL ) {
				CHECK(xjwtVerifyJwks(token, jwks, NULL) == NULL,
					"M2 no usable key after P-384 skip");
				xrtFree(token);
			}
			xrtValueRelease(claims);
		}
		xjwtJwksFree(jwks);
	}

	/* ---- M3：HS256 令牌走 JWKS 被算法族拒绝 ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		if ( jwks != NULL ) {
			xjwtconfig cfg; xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "jwks-secret";
			cfg.KeyId = "rsa-key-1";  /* kid 能匹配上 */
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("hsviajwks")));
			char* token = xjwtSign(&cfg, claims);
			if ( token != NULL ) {
				CHECK(xjwtVerifyJwks(token, jwks, NULL) == NULL,
					"M3 HS256 token rejected by JWKS path");
				xrtFree(token);
			}
			xrtValueRelease(claims);
		}
		xjwtJwksFree(jwks);
	}

	/* ---- M3：RS256 令牌 kid 匹配到 EC 密钥 → 族不匹配拒绝 ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
		if ( jwks != NULL ) {
			xjwtconfig cfg; xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_RS256; cfg.KeyPem = K_RSA_PRIV;
			cfg.KeyId = "ec-key-1";  /* 指向 EC 条目 */
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("crossfamily")));
			char* token = xjwtSign(&cfg, claims);
			if ( token != NULL ) {
				CHECK(xjwtVerifyJwks(token, jwks, NULL) == NULL,
					"M3 RS256 token + EC key rejected (family mismatch)");
				xrtFree(token);
			}
			xrtValueRelease(claims);
		}
		xjwtJwksFree(jwks);
	}

	/* ============ LOW 审计修复回归 ============ */

	/* ---- LOW2：签名输出容量不足直接失败（不截断） ---- */
	{
		unsigned char aSmall[4];
		size_t sz = 0;
		CHECK(!xjwt__sign(XJWT_ALG_HS256, "x", 1, "k", aSmall, sizeof(aSmall), &sz),
			"LOW2 HMAC with 4-byte output rejected");
		CHECK(!xjwt__sign(XJWT_ALG_RS256, "x", 1, K_RSA_PRIV,
			aSmall, sizeof(aSmall), &sz),
			"LOW2 RSA with 4-byte output rejected");
		unsigned char aTiny[63];
		CHECK(!xjwt__sign(XJWT_ALG_ES256, "x", 1, K_EC_PRIV,
			aTiny, sizeof(aTiny), &sz),
			"LOW2 ES256 with 63-byte output rejected");
	}

	/* ---- LOW3：JWKS 超过 16 把密钥整体拒绝 ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS_17);
		CHECK(jwks == NULL, "LOW3 17-key JWKS rejected");
		xjwtJwksFree(jwks);
	}

	/* ---- LOW4：kid 严格匹配（无 kid 条目不再吃掉任意 token kid） ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(K_JWKS_NOKID);
		CHECK(jwks != NULL, "LOW4 no-kid JWKS parses");
		if ( jwks != NULL ) {
			/* 用对应私钥签发，但 token 带 kid —— 严格匹配下必须拒绝 */
			xjwtconfig cfg; xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_RS256; cfg.KeyPem = K_RSA_PRIV;
			cfg.KeyId = "whatever"; cfg.ExpireSeconds = 3600;
			xvalue* claims = xrtValueObject();
			xrtValueObjectSetNew(claims, xrtStrView("sub"),
				xrtValueString(xrtStrView("strictkid")));
			char* token = xjwtSign(&cfg, claims);
			if ( token != NULL ) {
				CHECK(xjwtVerifyJwks(token, jwks, NULL) == NULL,
					"LOW4 token kid with no-kid JWKS rejected (strict)");
				xrtFree(token);
			}
			/* token 不带 kid → 回退取第一把，应当通过 */
			cfg.KeyId = NULL;
			token = xjwtSign(&cfg, claims);
			if ( token != NULL ) {
				CHECK(xjwtVerifyJwks(token, jwks, NULL) != NULL,
					"LOW4 kid-less token takes first key");
				xrtFree(token);
			}
			xrtValueRelease(claims);
		}
		xjwtJwksFree(jwks);
	}

	/* ---- LOW5：公钥缓存 xjwtKeyParse / xjwtVerifyKey ---- */
	{
		/* RSA 正路径：openssl 令牌 + 缓存公钥 */
		xjwtkey* key = xjwtKeyParse(K_RSA_PUB);
		CHECK(key != NULL, "LOW5 RSA key parsed");
		if ( key != NULL ) {
			xvalue* out = xjwtVerifyKey(K_TOKEN_RS256_OPENSSL, key, NULL);
			CHECK(out != NULL, "LOW5 cached RSA key verifies openssl token");
			if ( out != NULL ) xrtValueRelease(out);
			/* ES256 令牌 + RSA 缓存 → 族不匹配拒绝 */
			CHECK(xjwtVerifyKey(K_TOKEN_ES256_OPENSSL, key, NULL) == NULL,
				"LOW5 ES token with RSA cached key rejected");
			xjwtKeyFree(key);
		}
		/* EC 正路径 */
		xjwtkey* eckey = xjwtKeyParse(K_EC_PUB);
		CHECK(eckey != NULL, "LOW5 EC key parsed");
		if ( eckey != NULL ) {
			xvalue* out = xjwtVerifyKey(K_TOKEN_ES256_OPENSSL, eckey, NULL);
			CHECK(out != NULL, "LOW5 cached EC key verifies openssl token");
			if ( out != NULL ) xrtValueRelease(out);
			xjwtKeyFree(eckey);
		}
		/* HS256 令牌 + 缓存公钥 → 拒绝 */
		{
			xjwtkey* rk = xjwtKeyParse(K_RSA_PUB);
			if ( rk != NULL ) {
				xvalue* claims = xrtValueObject();
				xrtValueObjectSetNew(claims, xrtStrView("sub"),
					xrtValueString(xrtStrView("hscache")));
				char* token = xjwtHs256(claims, "k", 3600);
				if ( token != NULL ) {
					CHECK(xjwtVerifyKey(token, rk, NULL) == NULL,
						"LOW5 HS token with cached key rejected");
					xrtFree(token);
				}
				xrtValueRelease(claims);
				xjwtKeyFree(rk);
			}
		}
		/* 非公钥 PEM → 解析失败（私钥 PEM 也不收） */
		CHECK(xjwtKeyParse("not a pem") == NULL, "LOW5 garbage PEM rejected");
		CHECK(xjwtKeyParse(K_RSA_PRIV) == NULL, "LOW5 private PEM rejected");
	}

	/* ---- RSA 公钥 DER 结构与 JWA 最低密钥长度 ---- */
	{
		xpemblock block;
		size_t derSize = 0;
		unsigned char* der = NULL;
		CHECK(xrtPemFind(K_RSA_PUB, strlen(K_RSA_PUB), "PUBLIC KEY", &block),
			"RSA SPKI fixture found");
		if ( xrtPemFind(K_RSA_PUB, strlen(K_RSA_PUB), "PUBLIC KEY", &block) )
			der = xrtPemDecodeNew(&block, &derSize);
		CHECK(der != NULL, "RSA SPKI fixture decoded");
		if ( der != NULL ) {
			xdercursor outer, body, algorithm;
			xdervalue value;
			const unsigned char *oid = NULL, *parameterTag = NULL,
				*bitString = NULL;
			xbytesview encodedKey = { NULL, 0 };
			uint8 unusedBits = 0;
			bool parsed = xrtDerInit(&outer, der, derSize) &&
				xrtDerRead(&outer, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &body) &&
				xrtDerRead(&body, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &algorithm) &&
				xrtDerRead(&algorithm, &value) == XDER_VALUE;
			if ( parsed ) {
				oid = (const unsigned char*)value.Value.Data;
				parsed = xrtDerRead(&algorithm, &value) == XDER_VALUE;
				if ( parsed ) parameterTag = (const unsigned char*)value.Raw.Data;
				parsed = parsed && xrtDerRead(&body, &value) == XDER_VALUE &&
					xrtDerBitString(&value, &encodedKey, &unusedBits) &&
					unusedBits == 0;
				if ( parsed ) bitString = (const unsigned char*)value.Value.Data;
			}
			CHECK(parsed && oid != NULL && parameterTag != NULL &&
				bitString != NULL,
				"RSA SPKI structure available for mutation tests");
			if ( parsed ) {
				str pem = xrtPemEncodeNew("RSA PUBLIC KEY",
					encodedKey.Data, encodedKey.Size);
				xjwtkey* key = pem != NULL ? xjwtKeyParse(pem) : NULL;
				CHECK(key != NULL, "valid PKCS#1 RSA public key accepted");
				if ( key != NULL ) {
					xvalue* out = xjwtVerifyKey(K_TOKEN_RS256_OPENSSL, key, NULL);
					CHECK(out != NULL, "PKCS#1 RSA public key verifies RS256");
					if ( out != NULL ) xrtValueRelease(out);
					xjwtKeyFree(key);
				}
				xrtFree(pem);

				unsigned char* changed = (unsigned char*)xrtMalloc(derSize + 1u);
				CHECK(changed != NULL, "RSA DER mutation buffer allocated");
				if ( changed != NULL ) {
					memcpy(changed, der, derSize);
					changed[(size_t)(oid - der) + 8u] = 0x0au;
					CHECK(xrtDerValidate(changed, derSize),
						"wrong-OID SPKI remains valid DER");
					pem = xrtPemEncodeNew("PUBLIC KEY", changed, derSize);
					CHECK(pem != NULL && xjwtKeyParse(pem) == NULL,
						"SPKI with wrong RSA algorithm OID rejected");
					xrtFree(pem);

					memcpy(changed, der, derSize);
					changed[(size_t)(parameterTag - der)] = 0x04u;
					CHECK(xrtDerValidate(changed, derSize),
						"wrong-parameter SPKI remains valid DER");
					pem = xrtPemEncodeNew("PUBLIC KEY", changed, derSize);
					CHECK(pem != NULL && xjwtKeyParse(pem) == NULL,
						"SPKI with non-NULL RSA parameters rejected");
					xrtFree(pem);

					memcpy(changed, der, derSize);
					changed[(size_t)(bitString - der)] = 1u;
					changed[derSize - 1u] &= 0xfeu;
					CHECK(xrtDerValidate(changed, derSize),
						"nonzero-unused-bits SPKI remains valid DER");
					pem = xrtPemEncodeNew("PUBLIC KEY", changed, derSize);
					CHECK(pem != NULL && xjwtKeyParse(pem) == NULL,
						"SPKI with nonzero unused BIT STRING bits rejected");
					xrtFree(pem);

					memcpy(changed, der, derSize);
					changed[derSize] = 0u;
					pem = xrtPemEncodeNew("PUBLIC KEY", changed, derSize + 1u);
					CHECK(pem != NULL && xjwtKeyParse(pem) == NULL,
						"SPKI with trailing DER byte rejected");
					xrtFree(pem);
					xrtFree(changed);
				}
			}
			xrtFree(der);
		}

		unsigned char weakModulus[129] = { 0 };
		xbuffer integers, weakDer;
		weakModulus[1] = 0x80u;
		weakModulus[128] = 1u;
		xrtBufferInit(&integers);
		xrtBufferInit(&weakDer);
		bool weakBuilt = xrtDerAppend(&integers, XASN1_UNIVERSAL,
			XASN1_INTEGER, false,
			(xbytesview){ weakModulus, sizeof(weakModulus) }) &&
			xrtDerAppendUInt64(&integers, 65537u) &&
			xrtDerAppend(&weakDer, XASN1_UNIVERSAL,
				XASN1_SEQUENCE, true, xrtBufferView(&integers));
		CHECK(weakBuilt, "1024-bit RSA public DER fixture built");
		if ( weakBuilt ) {
			str pem = xrtPemEncodeNew("RSA PUBLIC KEY", weakDer.Data, weakDer.Size);
			CHECK(pem != NULL && xjwtKeyParse(pem) == NULL,
				"JWA rejects 1024-bit RSA public key");
			xrtFree(pem);
			str n = xjwt__base64url_encode(weakModulus + 1u, 128u);
			if ( n != NULL ) {
				char json[256];
				int len = snprintf(json, sizeof(json),
					"{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"weak\",\"n\":\"%s\",\"e\":\"AQAB\"}]}", n);
				CHECK(len > 0 && (size_t)len < sizeof(json),
					"1024-bit RSA JWK fixture built");
				if ( len > 0 && (size_t)len < sizeof(json) ) {
					xjwtjwks* jwks = xjwtJwksParse(json);
					CHECK(jwks != NULL && jwks->nKeys == 0,
						"JWA skips 1024-bit RSA JWK");
					xjwtJwksFree(jwks);
				}
				xrtFree(n);
			}
		}
		xrtBufferUnit(&weakDer);
		xrtBufferUnit(&integers);
	}

	/* ---- RSA 私钥 PKCS#8 算法标识、完整性与长度下限 ---- */
	{
		xpemblock block;
		size_t derSize = 0;
		unsigned char* der = NULL;
		if ( xrtPemFind(K_RSA_PRIV, strlen(K_RSA_PRIV), "PRIVATE KEY", &block) )
			der = xrtPemDecodeNew(&block, &derSize);
		CHECK(der != NULL, "RSA PKCS#8 fixture decoded");
		if ( der != NULL ) {
			xdercursor outer, fields, algorithm;
			xdervalue value;
			const unsigned char *oid = NULL, *parameterTag = NULL;
			bool parsed = xrtDerInit(&outer, der, derSize) &&
				xrtDerRead(&outer, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &fields) &&
				xrtDerRead(&fields, &value) == XDER_VALUE &&
				xrtDerRead(&fields, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &algorithm) &&
				xrtDerRead(&algorithm, &value) == XDER_VALUE;
			if ( parsed ) {
				oid = (const unsigned char*)value.Value.Data;
				if ( xrtDerRead(&algorithm, &value) == XDER_VALUE )
					parameterTag = (const unsigned char*)value.Raw.Data;
			}
			CHECK(oid != NULL && parameterTag != NULL,
				"RSA PKCS#8 algorithm identifier located");
			if ( oid != NULL && parameterTag != NULL ) {
				unsigned char* changed = (unsigned char*)xrtMalloc(derSize + 1u);
				CHECK(changed != NULL, "RSA private DER mutation buffer allocated");
				if ( changed != NULL ) {
					xvalue* claims = xrtValueObject();
					memcpy(changed, der, derSize);
					changed[(size_t)(oid - der) + 8u] = 0x0au;
					CHECK(xrtDerValidate(changed, derSize),
						"wrong-OID PKCS#8 remains valid DER");
					str pem = xrtPemEncodeNew("PRIVATE KEY", changed, derSize);
					char* token = pem != NULL ? xjwtRs256(claims, pem, 60) : NULL;
					CHECK(pem != NULL && token == NULL,
						"PKCS#8 with wrong RSA algorithm OID rejected");
					xrtFree(token);
					xrtFree(pem);

					memcpy(changed, der, derSize);
					changed[(size_t)(parameterTag - der)] = 0x04u;
					CHECK(xrtDerValidate(changed, derSize),
						"wrong-parameter PKCS#8 remains valid DER");
					pem = xrtPemEncodeNew("PRIVATE KEY", changed, derSize);
					token = pem != NULL ? xjwtRs256(claims, pem, 60) : NULL;
					CHECK(pem != NULL && token == NULL,
						"PKCS#8 with non-NULL RSA parameters rejected");
					xrtFree(token);
					xrtFree(pem);

					memcpy(changed, der, derSize);
					changed[derSize] = 0u;
					pem = xrtPemEncodeNew("PRIVATE KEY", changed, derSize + 1u);
					token = pem != NULL ? xjwtRs256(claims, pem, 60) : NULL;
					CHECK(pem != NULL && token == NULL,
						"PKCS#8 with trailing DER byte rejected");
					xrtFree(token);
					xrtFree(pem);
					xrtValueRelease(claims);
					xrtFree(changed);
				}
			}
			xrtSecureZero(der, derSize);
			xrtFree(der);
		}

		unsigned char weakModulus[129] = { 0 };
		xbuffer integers, weakDer;
		weakModulus[1] = 0x80u;
		weakModulus[128] = 1u;
		xrtBufferInit(&integers);
		xrtBufferInit(&weakDer);
		bool weakBuilt = xrtDerAppendUInt64(&integers, 0u) &&
			xrtDerAppend(&integers, XASN1_UNIVERSAL, XASN1_INTEGER,
				false, (xbytesview){ weakModulus, sizeof(weakModulus) }) &&
			xrtDerAppendUInt64(&integers, 65537u);
		for ( int i = 0; i < 6 && weakBuilt; i++ )
			weakBuilt = xrtDerAppendUInt64(&integers, 3u);
		if ( weakBuilt )
			weakBuilt = xrtDerAppend(&weakDer, XASN1_UNIVERSAL,
				XASN1_SEQUENCE, true, xrtBufferView(&integers));
		CHECK(weakBuilt, "1024-bit RSA private DER fixture built");
		if ( weakBuilt ) {
			str pem = xrtPemEncodeNew("RSA PRIVATE KEY", weakDer.Data, weakDer.Size);
			xvalue* claims = xrtValueObject();
			char* token = pem != NULL ? xjwtRs256(claims, pem, 60) : NULL;
			CHECK(pem != NULL && token == NULL,
				"JWA rejects 1024-bit RSA signing key");
			xrtFree(token);
			xrtValueRelease(claims);
			xrtFree(pem);
		}
		if ( integers.Data != NULL ) xrtSecureZero(integers.Data, integers.Size);
		if ( weakDer.Data != NULL ) xrtSecureZero(weakDer.Data, weakDer.Size);
		xrtBufferUnit(&weakDer);
		xrtBufferUnit(&integers);
	}

	/* ---- ES256 公钥必须明确指定 P-256，且点、DER 均有效 ---- */
	{
		xpemblock block;
		size_t derSize = 0;
		unsigned char* der = NULL;
		if ( xrtPemFind(K_EC_PUB, strlen(K_EC_PUB), "PUBLIC KEY", &block) )
			der = xrtPemDecodeNew(&block, &derSize);
		CHECK(der != NULL, "EC SPKI fixture decoded");
		if ( der != NULL ) {
			xdercursor outer, fields, algorithm;
			xdervalue value;
			xbytesview point = { NULL, 0 };
			const unsigned char *algorithmOid = NULL, *curveOid = NULL;
			uint8 unused = 0;
			bool parsed = xrtDerInit(&outer, der, derSize) &&
				xrtDerRead(&outer, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &fields) &&
				xrtDerRead(&fields, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &algorithm) &&
				xrtDerRead(&algorithm, &value) == XDER_VALUE;
			if ( parsed ) {
				algorithmOid = (const unsigned char*)value.Value.Data;
				parsed = xrtDerRead(&algorithm, &value) == XDER_VALUE;
				if ( parsed ) curveOid = (const unsigned char*)value.Value.Data;
				parsed = parsed && xrtDerRead(&fields, &value) == XDER_VALUE &&
					xrtDerBitString(&value, &point, &unused);
			}
			CHECK(parsed && algorithmOid != NULL && curveOid != NULL &&
				point.Size == 65u && unused == 0u,
				"EC SPKI mutation offsets located");
			if ( parsed && point.Size == 65u ) {
				unsigned char* changed = (unsigned char*)xrtMalloc(derSize + 1u);
				CHECK(changed != NULL, "EC SPKI mutation buffer allocated");
				if ( changed != NULL ) {
					memcpy(changed, der, derSize);
					changed[(size_t)(algorithmOid - der) + 6u] = 2u;
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_public_der_rejected(changed, derSize),
						"EC SPKI wrong algorithm OID rejected");
					memcpy(changed, der, derSize);
					changed[(size_t)(curveOid - der) + 7u] = 8u;
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_public_der_rejected(changed, derSize),
						"EC SPKI wrong named curve rejected");
					memcpy(changed, der, derSize);
					changed[(size_t)(point.Data - der)] = 2u;
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_public_der_rejected(changed, derSize),
						"EC SPKI non-uncompressed point rejected");
					memcpy(changed, der, derSize);
					changed[derSize] = 0u;
					CHECK(ec_public_der_rejected(changed, derSize + 1u),
						"EC SPKI trailing DER rejected");
					xrtFree(changed);
				}
				unsigned char zeros[32] = { 0 };
				str zero = xjwt__base64url_encode(zeros, sizeof(zeros));
				str x = xjwt__base64url_encode(point.Data + 1u, 32u);
				str y = xjwt__base64url_encode(point.Data + 33u, 32u);
				CHECK(zero != NULL && x != NULL && y != NULL,
					"EC JWKS coordinates encoded");
				if ( zero != NULL && x != NULL && y != NULL ) {
					char json[512];
					int len = snprintf(json, sizeof(json),
						"{\"keys\":[{\"kty\":\"EC\",\"kid\":\"stale\",\"crv\":\"P-256\",\"x\":\"%s\",\"y\":\"%s\"},"
						"{\"kty\":\"EC\",\"crv\":\"P-256\",\"x\":\"%s\",\"y\":\"%s\"}]}",
						zero, zero, x, y);
					CHECK(len > 0 && (size_t)len < sizeof(json),
						"EC mixed JWKS fixture built");
					if ( len > 0 && (size_t)len < sizeof(json) ) {
						xjwtjwks* jwks = xjwtJwksParse(json);
						CHECK(jwks != NULL && jwks->nKeys == 1 &&
							jwks->keys[0].kid[0] == 0 &&
							memcmp(jwks->keys[0].ec65, point.Data, 65u) == 0,
							"off-curve JWKS key skipped without stale kid");
						xjwtJwksFree(jwks);
					}
				}
				xrtFree(zero);
				xrtFree(x);
				xrtFree(y);
			}
			xrtFree(der);
		}
	}

	/* ---- SEC1 私钥可选曲线和公钥必须与标量一致 ---- */
	{
		xpemblock block;
		size_t derSize = 0;
		unsigned char* der = NULL;
		if ( xrtPemFind(K_EC_PRIV, strlen(K_EC_PRIV), "EC PRIVATE KEY", &block) )
			der = xrtPemDecodeNew(&block, &derSize);
		CHECK(der != NULL, "EC SEC1 fixture decoded");
		if ( der != NULL ) {
			xdercursor outer, fields, optional;
			xdervalue value;
			xbytesview scalar = { NULL, 0 }, point = { NULL, 0 };
			const unsigned char *version = NULL, *curveOid = NULL;
			uint8 unused = 0;
			bool parsed = xrtDerInit(&outer, der, derSize) &&
				xrtDerRead(&outer, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &fields) &&
				xrtDerRead(&fields, &value) == XDER_VALUE;
			if ( parsed ) {
				version = (const unsigned char*)value.Value.Data;
				parsed = xrtDerRead(&fields, &value) == XDER_VALUE &&
					xrtDerOctets(&value, &scalar) &&
					xrtDerRead(&fields, &value) == XDER_VALUE &&
					xrtDerEnter(&value, &optional) &&
					xrtDerRead(&optional, &value) == XDER_VALUE;
				if ( parsed ) curveOid = (const unsigned char*)value.Value.Data;
				parsed = parsed && xrtDerRead(&fields, &value) == XDER_VALUE &&
					xrtDerEnter(&value, &optional) &&
					xrtDerRead(&optional, &value) == XDER_VALUE &&
					xrtDerBitString(&value, &point, &unused);
			}
			CHECK(parsed && version != NULL && curveOid != NULL &&
				scalar.Size == 32u && point.Size == 65u && unused == 0u,
				"EC SEC1 mutation offsets located");
			if ( parsed && scalar.Size == 32u && point.Size == 65u ) {
				unsigned char* changed = (unsigned char*)xrtMalloc(derSize + 1u);
				CHECK(changed != NULL, "EC SEC1 mutation buffer allocated");
				if ( changed != NULL ) {
					memcpy(changed, der, derSize);
					changed[(size_t)(version - der)] = 2u;
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_private_der_rejected("EC PRIVATE KEY", changed, derSize),
						"SEC1 wrong version rejected");
					memcpy(changed, der, derSize);
					changed[(size_t)(curveOid - der) + 7u] = 8u;
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_private_der_rejected("EC PRIVATE KEY", changed, derSize),
						"SEC1 wrong named curve rejected");
					memcpy(changed, der, derSize);
					memset(changed + (size_t)(scalar.Data - der), 0, 32u);
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_private_der_rejected("EC PRIVATE KEY", changed, derSize),
						"SEC1 zero private scalar rejected");
					unsigned char one[32] = { 0 }, alternate[65];
					one[31] = 1u;
					bool alternateReady = xrtP256Public(one, alternate);
					CHECK(alternateReady,
						"alternate P-256 public point generated");
					if ( alternateReady ) {
						memcpy(changed, der, derSize);
						memcpy(changed + (size_t)(point.Data - der), alternate, 65u);
						CHECK(xrtDerValidate(changed, derSize) &&
							ec_private_der_rejected("EC PRIVATE KEY", changed, derSize),
							"SEC1 mismatched valid public point rejected");
					}
					memcpy(changed, der, derSize);
					changed[derSize] = 0u;
					CHECK(ec_private_der_rejected("EC PRIVATE KEY", changed,
						derSize + 1u), "SEC1 trailing DER rejected");
					xrtFree(changed);
				}
			}
			xrtSecureZero(der, derSize);
			xrtFree(der);
		}
	}

	/* ---- PKCS#8 私钥算法标识必须是 P-256 ---- */
	{
		xpemblock block;
		size_t derSize = 0;
		unsigned char* der = NULL;
		if ( xrtPemFind(K_EC_PRIV_PKCS8, strlen(K_EC_PRIV_PKCS8),
			"PRIVATE KEY", &block) )
			der = xrtPemDecodeNew(&block, &derSize);
		CHECK(der != NULL, "EC PKCS#8 fixture decoded");
		if ( der != NULL ) {
			xdercursor outer, fields, algorithm;
			xdervalue value;
			const unsigned char *algorithmOid = NULL, *curveOid = NULL;
			bool parsed = xrtDerInit(&outer, der, derSize) &&
				xrtDerRead(&outer, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &fields) &&
				xrtDerRead(&fields, &value) == XDER_VALUE &&
				xrtDerRead(&fields, &value) == XDER_VALUE &&
				xrtDerEnter(&value, &algorithm) &&
				xrtDerRead(&algorithm, &value) == XDER_VALUE;
			if ( parsed ) {
				algorithmOid = (const unsigned char*)value.Value.Data;
				parsed = xrtDerRead(&algorithm, &value) == XDER_VALUE;
				if ( parsed ) curveOid = (const unsigned char*)value.Value.Data;
			}
			CHECK(parsed && algorithmOid != NULL && curveOid != NULL,
				"EC PKCS#8 mutation offsets located");
			if ( parsed ) {
				unsigned char* changed = (unsigned char*)xrtMalloc(derSize + 1u);
				CHECK(changed != NULL, "EC PKCS#8 mutation buffer allocated");
				if ( changed != NULL ) {
					memcpy(changed, der, derSize);
					changed[(size_t)(algorithmOid - der) + 6u] = 2u;
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_private_der_rejected("PRIVATE KEY", changed, derSize),
						"EC PKCS#8 wrong algorithm OID rejected");
					memcpy(changed, der, derSize);
					changed[(size_t)(curveOid - der) + 7u] = 8u;
					CHECK(xrtDerValidate(changed, derSize) &&
						ec_private_der_rejected("PRIVATE KEY", changed, derSize),
						"EC PKCS#8 wrong named curve rejected");
					memcpy(changed, der, derSize);
					changed[derSize] = 0u;
					CHECK(ec_private_der_rejected("PRIVATE KEY", changed,
						derSize + 1u), "EC PKCS#8 trailing DER rejected");
					xrtFree(changed);
				}
			}
			xrtSecureZero(der, derSize);
			xrtFree(der);
		}
	}

	/* ---- PKCS#8 可选 Attributes：RSA/EC 签发与畸形字段拒绝 ---- */
	{
		static const unsigned char attr[] = {
			0x30, 0x0b, 0x06, 0x03, 0x2a, 0x03, 0x04,
			0x31, 0x04, 0x0c, 0x02, 'o', 'k'
		};
		static const unsigned char emptyValues[] = {
			0x30, 0x07, 0x06, 0x03, 0x2a, 0x03, 0x04,
			0x31, 0x00
		};
		const char* fixtures[2] = { K_RSA_PRIV, K_EC_PRIV_PKCS8 };
		const char* publicKeys[2] = { K_RSA_PUB, K_EC_PUB };
		for ( int i = 0; i < 2; i++ ) {
			size_t derSize = 0;
			bytes der = pkcs8_with_attributes(fixtures[i],
				(xbytesview){ attr, sizeof(attr) }, &derSize);
			CHECK(der != NULL && xrtDerValidate(der, derSize),
				"PKCS#8 with attribute encoded as valid DER");
			if ( der != NULL ) {
				str pem = xrtPemEncodeNew("PRIVATE KEY", der, derSize);
				xvalue* claims = xrtValueObject();
				char* token = pem != NULL ? (i == 0 ?
					xjwtRs256(claims, pem, 60) :
					xjwtEs256(claims, pem, 60)) : NULL;
				CHECK(token != NULL,
					"PKCS#8 with attribute signs RSA or ES256 token");
				if ( token != NULL ) {
					xvalue* verified = xjwtVerify(token, publicKeys[i], NULL);
					CHECK(verified != NULL,
						"PKCS#8 with attribute signature verifies");
					if ( verified != NULL ) xrtValueRelease(verified);
				}
				xrtFree(token);
				xrtValueRelease(claims);
				xrtFree(pem);

				bytes changed = (bytes)xrtMalloc(derSize);
				CHECK(changed != NULL, "PKCS#8 attribute mutation buffer allocated");
				if ( changed != NULL ) {
					size_t tag = derSize - sizeof(attr) - 2u;
					memcpy(changed, der, derSize);
					changed[tag] = 0xa1u;
					CHECK(xrtDerValidate(changed, derSize) &&
						(i == 0 ? rsa_private_der_rejected(changed, derSize) :
							ec_private_der_rejected("PRIVATE KEY", changed, derSize)),
						"PKCS#8 wrong attribute context tag rejected");
					memcpy(changed, der, derSize);
					changed[tag + 4u] = 0x04u;
					CHECK(xrtDerValidate(changed, derSize) &&
						(i == 0 ? rsa_private_der_rejected(changed, derSize) :
							ec_private_der_rejected("PRIVATE KEY", changed, derSize)),
						"PKCS#8 attribute without OID rejected");
					xrtSecureZero(changed, derSize);
					xrtFree(changed);
				}
				xrtSecureZero(der, derSize);
				xrtFree(der);
			}

			der = pkcs8_with_attributes(fixtures[i],
				(xbytesview){ emptyValues, sizeof(emptyValues) }, &derSize);
			CHECK(der != NULL && xrtDerValidate(der, derSize) &&
				(i == 0 ? rsa_private_der_rejected(der, derSize) :
					ec_private_der_rejected("PRIVATE KEY", der, derSize)),
				"PKCS#8 attribute with empty value set rejected");
			if ( der != NULL ) {
				xrtSecureZero(der, derSize);
				xrtFree(der);
			}
		}

		unsigned char unsorted[2u * sizeof(attr)];
		memcpy(unsorted, attr, sizeof(attr));
		unsorted[6] = 0x05u;
		memcpy(unsorted + sizeof(attr), attr, sizeof(attr));
		size_t derSize = 0;
		bytes der = pkcs8_with_attributes(K_RSA_PRIV,
			(xbytesview){ unsorted, sizeof(unsorted) }, &derSize);
		CHECK(der != NULL && xrtDerValidate(der, derSize) &&
			rsa_private_der_rejected(der, derSize),
			"PKCS#8 unsorted attribute SET rejected");
		if ( der != NULL ) {
			xrtSecureZero(der, derSize);
			xrtFree(der);
		}
	}

	/* ---- RFC 5958 OneAsymmetricKey：外层公钥、版本与私钥一致性 ---- */
	{
		static const unsigned char attr[] = {
			0x30, 0x0b, 0x06, 0x03, 0x2a, 0x03, 0x04,
			0x31, 0x04, 0x0c, 0x02, 'o', 'k'
		};
		const char* privateKeys[2] = { K_RSA_PRIV, K_EC_PRIV_PKCS8 };
		const char* publicKeys[2] = { K_RSA_PUB, K_EC_PUB };
		for ( int i = 0; i < 2; i++ ) {
			size_t publicSize = 0, derSize = 0;
			bytes publicBytes = spki_public_copy(publicKeys[i], &publicSize);
			bytes der;

			CHECK(publicBytes != NULL && publicSize != 0u,
				"OneAsymmetricKey public fixture extracted");
			if ( publicBytes == NULL ) continue;
			for ( int withAttributes = 0; withAttributes < 2;
				withAttributes++ ) {
				der = pkcs8_with_options(privateKeys[i],
					withAttributes ? (xbytesview){ attr, sizeof(attr) } :
						(xbytesview){ NULL, 0 },
					(xbytesview){ publicBytes, publicSize }, 1u, &derSize);
				CHECK(der != NULL && xrtDerValidate(der, derSize),
					"OneAsymmetricKey v2 encoded as valid DER");
				if ( der != NULL ) {
					str pem = xrtPemEncodeNew("PRIVATE KEY", der, derSize);
					xvalue* claims = xrtValueObject();
					char* token = pem != NULL ? (i == 0 ?
						xjwtRs256(claims, pem, 60) :
						xjwtEs256(claims, pem, 60)) : NULL;
					CHECK(token != NULL,
						"OneAsymmetricKey v2 signs RSA or ES256 token");
					if ( token != NULL ) {
						xvalue* verified = xjwtVerify(token,
						publicKeys[i], NULL);
						CHECK(verified != NULL,
							"OneAsymmetricKey v2 signature verifies");
						if ( verified != NULL ) xrtValueRelease(verified);
					}
					xrtFree(token);
					xrtValueRelease(claims);
					xrtFree(pem);
					if ( !withAttributes ) {
						size_t tag = 0, unused = 0;
						bool located = one_asym_public_offsets(
							der, derSize, &tag, &unused);
						bytes changed = (bytes)xrtMalloc(derSize);
						CHECK(located && changed != NULL,
							"OneAsymmetricKey public field located");
						if ( located && changed != NULL ) {
							memcpy(changed, der, derSize);
							changed[tag] = 0xa1u;
							CHECK((i == 0 ? rsa_private_der_rejected(
									changed, derSize) :
								 ec_private_der_rejected("PRIVATE KEY",
									changed, derSize)),
								"OneAsymmetricKey rejects explicit public tag");
							memcpy(changed, der, derSize);
							changed[unused] = 1u;
							CHECK(xrtDerValidate(changed, derSize) &&
								(i == 0 ? rsa_private_der_rejected(
									changed, derSize) :
								 ec_private_der_rejected("PRIVATE KEY",
									changed, derSize)),
								"OneAsymmetricKey rejects nonzero unused bits");
						}
						if ( changed != NULL ) {
							xrtSecureZero(changed, derSize);
							xrtFree(changed);
						}
					}
					xrtSecureZero(der, derSize);
					xrtFree(der);
				}
			}
			der = pkcs8_with_options(privateKeys[i],
				(xbytesview){ NULL, 0 },
				(xbytesview){ publicBytes, publicSize }, 0u, &derSize);
			CHECK(der != NULL && xrtDerValidate(der, derSize) &&
				(i == 0 ? rsa_private_der_rejected(der, derSize) :
				 ec_private_der_rejected("PRIVATE KEY", der, derSize)),
				"OneAsymmetricKey rejects v1 with an outer public key");
			if ( der != NULL ) {
				xrtSecureZero(der, derSize);
				xrtFree(der);
			}
			der = pkcs8_with_options(privateKeys[i],
				(xbytesview){ NULL, 0 },
				(xbytesview){ NULL, 0 }, 1u, &derSize);
			CHECK(der != NULL && xrtDerValidate(der, derSize) &&
				(i == 0 ? rsa_private_der_rejected(der, derSize) :
				 ec_private_der_rejected("PRIVATE KEY", der, derSize)),
				"OneAsymmetricKey rejects v2 without an outer public key");
			if ( der != NULL ) {
				xrtSecureZero(der, derSize);
				xrtFree(der);
			}
			if ( i == 0 ) {
				publicBytes[publicSize - 1u] ^= 2u;
			} else {
				uint8 otherScalar[32] = { 0 };
				otherScalar[31] = 1u;
				CHECK(xrtP256Public(otherScalar, publicBytes),
					"OneAsymmetricKey alternate EC public key created");
			}
			der = pkcs8_with_options(privateKeys[i],
				(xbytesview){ NULL, 0 },
				(xbytesview){ publicBytes, publicSize }, 1u, &derSize);
			CHECK(der != NULL && xrtDerValidate(der, derSize) &&
				(i == 0 ? rsa_private_der_rejected(der, derSize) :
				 ec_private_der_rejected("PRIVATE KEY", der, derSize)),
				"OneAsymmetricKey rejects mismatched outer public key");
			if ( der != NULL ) {
				xrtSecureZero(der, derSize);
				xrtFree(der);
			}
			xrtFree(publicBytes);
		}
	}

	/* ============ 二轮审计修复回归 ============ */

	/* ---- M-A：JWKS 部分条目零泄漏（计数分配器实测） ---- */
	{
		extern long g_probeLiveBytes;   /* test_jwt.c 顶部的计数分配器 */
		/* xrt 分配器对大块池化且高水位滞留：预热到稳态后测净增长 */
		for ( int i = 0; i < 400; i++ ) {
			xjwtjwks* k = xjwtJwksParse(
				"{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"noE\","
				"\"n\":\"AQABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
				"AAAAAAAAAAAA\"}]}");
			xjwtJwksFree(k);
		}
		long base = g_probeLiveBytes;
		for ( int i = 0; i < 50; i++ ) {
			xjwtjwks* k = xjwtJwksParse(
				"{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"noE\","
				"\"n\":\"AQABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
				"AAAAAAAAAAAA\"}]}");
			xjwtJwksFree(k);
		}
		CHECK(g_probeLiveBytes == base, "M-A malformed JWKS steady-state zero leak");
		/* 混合条目：坏条目 + 好条目（验证跨条目槽位复用正确）；同样预热 */
		for ( int i = 0; i < 400; i++ ) {
			xjwtjwks* k = xjwtJwksParse(K_JWKS_MIXED);
			if ( k != NULL ) {
				xvalue* out = xjwtVerifyJwks(K_TOKEN_RS256_OPENSSL, k, NULL);
				if ( out != NULL ) xrtValueRelease(out);
			}
			xjwtJwksFree(k);
		}
		base = g_probeLiveBytes;
		for ( int i = 0; i < 50; i++ ) {
			xjwtjwks* k = xjwtJwksParse(K_JWKS_MIXED);
			if ( k != NULL ) {
				/* 坏条目被跳过，好条目可用（openssl 令牌 kid 精确命中） */
				xvalue* out = xjwtVerifyJwks(K_TOKEN_RS256_OPENSSL, k, NULL);
				if ( out != NULL ) xrtValueRelease(out);
			}
			xjwtJwksFree(k);
		}
		CHECK(g_probeLiveBytes == base, "M-A mixed JWKS steady-state zero leak");
	}

	/* ---- M-B：超长 kid 不再退化成"无 kid" ---- */
	{
		/* 签发侧守卫：≥256 直接拒绝 */
		char aKid[299];
		memset(aKid, 'z', sizeof(aKid) - 1);
		aKid[sizeof(aKid) - 1] = 0;   /* 298 字符 */
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("bigkid")));
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "k";
		cfg.KeyId = aKid;
		CHECK(xjwtSign(&cfg, claims) == NULL,
			"M-B sign with 298-char KeyId rejected");

		/* 验证侧：手工构造带 300 字符 kid 的令牌（签名无效无妨——
		 * 拒绝发生在 header 解码阶段，先于验签） */
		char aHdr[400];
		memset(aHdr, 0, sizeof(aHdr));
		strcpy(aHdr, "{\"alg\":\"HS256\",\"typ\":\"JWT\",\"kid\":\"");
		memset(aHdr + strlen(aHdr), 'z', 300);
		strcat(aHdr, "\"}");
		char* h64 = xjwt__base64url_encode(aHdr, strlen(aHdr));
		char* c64 = xjwt__base64url_encode("{\"sub\":\"x\"}", 12);
		CHECK(h64 != NULL && c64 != NULL, "M-B fixture built");
		if ( h64 != NULL && c64 != NULL ) {
			size_t n = strlen(h64) + 1 + strlen(c64) + 1 + 6;
			char* token = (char*)xrtMalloc(n + 1);
			if ( token != NULL ) {
				strcpy(token, h64);
				strcat(token, ".");
				strcat(token, c64);
				strcat(token, ".AAAAAA");
				const char* kid = (const char*)1;
				xvalue* hdr = xjwtDecodeHeader(token, NULL, &kid);
				CHECK(hdr == NULL && kid == NULL,
					"M-B DecodeHeader rejects 300-char kid");
				if ( hdr != NULL ) xrtValueRelease(hdr);
				CHECK(xjwtVerify(token, "k", NULL) == NULL,
					"M-B long-kid token rejected by PEM path");
				xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
				if ( jwks != NULL ) {
					CHECK(xjwtVerifyJwks(token, jwks, NULL) == NULL,
						"M-B long-kid token rejected by JWKS (no fallback)");
					xjwtJwksFree(jwks);
				}
				xrtFree(token);
			}
		}
		xrtFree(h64);
		xrtFree(c64);
		xrtValueRelease(claims);
	}

	/* ---- M-C：非对象 claims 双向拒绝 ---- */
	{
		xvalue* arr = xrtJsonParse(xrtStrView("[1,2,3]"));
		if ( arr != NULL ) {
			CHECK(!xjwtClaimsValid(arr, NULL),
				"M-C array claims rejected by ClaimsValid");
			xjwtconfig cfg; xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "k";
			CHECK(xjwtSign(&cfg, arr) == NULL,
				"M-C array claims rejected by Sign");
			xrtValueRelease(arr);
		}
		xvalue* scalar = xrtJsonParse(xrtStrView("42"));
		if ( scalar != NULL ) {
			CHECK(!xjwtClaimsValid(scalar, NULL),
				"M-C scalar claims rejected");
			xrtValueRelease(scalar);
		}
	}

	/* ---- L-2：JWKS 条目 kid ≥128 被跳过 ---- */
	{
		xjwtjwks* jwks = xjwtJwksParse(
			"{\"keys\":[{\"kty\":\"RSA\",\"kid\":\""
			"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
			"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
			"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk\","
			"\"n\":\"AQAB\",\"e\":\"AQAB\"}]}");
		CHECK(jwks != NULL, "L-2 long-kid JWKS parses");
		if ( jwks != NULL ) {
			/* 条目被跳过 → 无可用密钥 → 任何 token 拒绝 */
			CHECK(xjwtVerifyJwks(K_TOKEN_RS256_OPENSSL, jwks, NULL) == NULL,
				"L-2 long-kid entry skipped (no usable key)");
			xjwtJwksFree(jwks);
		}
	}

	/* ---- fuzz：畸形输入不崩溃（确定性 LCG，2000 例） ---- */
	{
		unsigned int seed = 0x9E3779B9u;
		static const char aAlphabet[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
		char aBuf[80];
		for ( int iter = 0; iter < 2000; iter++ ) {
			seed = seed * 1664525u + 1013904223u;
			size_t len = (size_t)(seed % 40) + (iter % 7 == 0 ? 60 : 3);
			if ( len >= sizeof(aBuf) ) len = sizeof(aBuf) - 1;
			for ( size_t i = 0; i < len; i++ ) {
				seed = seed * 1664525u + 1013904223u;
				aBuf[i] = aAlphabet[seed % (sizeof(aAlphabet) - 1u)];
			}
			aBuf[len] = 0;
			/* 四条解码/验证入口全喂一遍；返回值只做释放，不校验语义 */
			int alg = 0;
			const char* kid = NULL;
			xvalue* h = xjwtDecodeHeader(aBuf, &alg, &kid);
			if ( h != NULL ) xrtValueRelease(h);
			if ( kid != NULL ) xrtFree((void*)kid);
			xvalue* c = xjwtDecode(aBuf, NULL);
			if ( c != NULL ) xrtValueRelease(c);
			xvalue* v = xjwtVerify(aBuf, "fuzz-secret", NULL);
			if ( v != NULL ) xrtValueRelease(v);
			if ( g_fuzzKey != NULL ) {
				xvalue* vk = xjwtVerifyKey(aBuf, g_fuzzKey, NULL);
				if ( vk != NULL ) xrtValueRelease(vk);
			}
		}
		CHECK(1, "fuzz 2000 malformed tokens no crash");
	}

	/* ============ API 评审修复回归 ============ */

	/* ---- ③：aud 数组（RFC 7519 §4.1.3）---- */
	{
		/* claims 里手放 aud 数组，验证任一命中 */
		xvalue* claims = xrtValueObject();
		xvalue* arr = xrtValueArray();
		xrtValueArrayAppendNew(arr, xrtValueString(xrtStrView("other-api")));
		xrtValueArrayAppendNew(arr, xrtValueString(xrtStrView("myapp-web")));
		xrtValueObjectSetNew(claims, xrtStrView("aud"), arr);
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "k";
		char* token = xjwtSign(&cfg, claims);
		xrtValueRelease(claims);
		CHECK(token != NULL, "aud-array token signs");
		if ( token != NULL ) {
			xjwtcheck ck; xjwtCheckInit(&ck);
			ck.Audience = "myapp-web";
			xvalue* out = xjwtVerify(token, "k", &ck);
			CHECK(out != NULL, "aud array: any-match accepted");
			if ( out != NULL ) xrtValueRelease(out);
			ck.Audience = "not-in-list";
			CHECK(xjwtVerify(token, "k", &ck) == NULL,
				"aud array: no-match rejected");
			/* 非字符串元素混入数组：其余元素命中仍应通过（宽容解析） */
			xrtClearError();
			ck.Audience = "myapp-web";
			out = xjwtVerify(token, "k", &ck);
			CHECK(out != NULL, "aud array re-verify stable");
			if ( out != NULL ) xrtValueRelease(out);
			xrtFree(token);
		}
		/* 数组元素全不匹配且含非字符串 → 拒绝（fail-closed） */
		{
			xvalue* c2 = xrtValueObject();
			xvalue* a2 = xrtValueArray();
			xrtValueArrayAppendNew(a2, xrtValueInt(42));
			xrtValueObjectSetNew(c2, xrtStrView("aud"), a2);
			xjwtconfig c2cfg; xjwtConfigInit(&c2cfg);
			c2cfg.Alg = XJWT_ALG_HS256; c2cfg.KeyPem = "k";
			char* t2 = xjwtSign(&c2cfg, c2);
			xrtValueRelease(c2);
			if ( t2 != NULL ) {
				xjwtcheck ck; xjwtCheckInit(&ck);
				ck.Audience = "anything";
				CHECK(xjwtVerify(t2, "k", &ck) == NULL,
					"aud array of non-strings rejected");
				xrtFree(t2);
			}
		}
	}

	/* ---- ②：xjwtLastError ---- */
	{
		xrtClearError();
		CHECK(xjwtLastError() == 0, "no xjwt error -> 0");
		/* 过期 → EXPIRED */
		xvalue* claims = xrtValueObject();
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "k"; cfg.ExpireSeconds = -60;
		char* t = xjwtSign(&cfg, claims);
		xrtValueRelease(claims);
		if ( t != NULL ) {
			CHECK(xjwtVerify(t, "k", NULL) == NULL, "expired rejected");
			CHECK(xjwtLastError() == XJWT_ERROR_EXPIRED,
				"xjwtLastError = EXPIRED");
			xrtFree(t);
		}
		/* 篡改 → 非 EXPIRED 的签名类错误 */
		claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("tamper2")));
		t = xjwtHs256(claims, "k", 3600);
		xrtValueRelease(claims);
		if ( t != NULL ) {
			size_t n = strlen(t);
			t[n - 2] = (t[n - 2] == 'A') ? 'B' : 'A';
			xrtClearError();
			CHECK(xjwtVerify(t, "k", NULL) == NULL, "tampered rejected");
			CHECK(xjwtLastError() != 0 && xjwtLastError() != XJWT_ERROR_EXPIRED,
				"xjwtLastError distinguishes signature errors");
			xrtFree(t);
		}
		/* JWKS 未知 kid → KEY_NOT_FOUND（轮换重试判据） */
		{
			xjwtjwks* jwks = xjwtJwksParse(K_JWKS);
			if ( jwks != NULL ) {
				xrtClearError();
				CHECK(xjwtVerifyJwks(K_TOKEN_RS256_OPENSSL, jwks, NULL) != NULL,
					"openssl token via JWKS still ok");
				/* 自签一个 kid 不存在的令牌 */
				xvalue* c3 = xrtValueObject();
				xjwtconfig c3cfg; xjwtConfigInit(&c3cfg);
				c3cfg.Alg = XJWT_ALG_RS256; c3cfg.KeyPem = K_RSA_PRIV;
				c3cfg.KeyId = "rotated-away"; c3cfg.ExpireSeconds = 3600;
				char* t3 = xjwtSign(&c3cfg, c3);
				xrtValueRelease(c3);
				if ( t3 != NULL ) {
					xrtClearError();
					CHECK(xjwtVerifyJwks(t3, jwks, NULL) == NULL,
						"unknown kid rejected");
					CHECK(xjwtLastError() == XJWT_ERROR_KEY_NOT_FOUND,
						"xjwtLastError = KEY_NOT_FOUND (rotation signal)");
					xrtFree(t3);
				}
			}
			xjwtJwksFree(jwks);
		}
	}

	/* ---- ④：xjwtClaimString ---- */
	{
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("alice")));
		xrtValueObjectSetNew(claims, xrtStrView("num"), xrtValueInt(7));
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = "k";
		char* token = xjwtSign(&cfg, claims);
		xrtValueRelease(claims);
		if ( token != NULL ) {
			xvalue* out = xjwtVerify(token, "k", NULL);
			CHECK(out != NULL, "claimString fixture verifies");
			if ( out != NULL ) {
				char buf[64];
				CHECK(xjwtClaimString(out, "sub", buf, sizeof(buf)) &&
					strcmp(buf, "alice") == 0,
					"xjwtClaimString extracts sub");
				char small[4];
				CHECK(xjwtClaimString(out, "sub", small, sizeof(small)) &&
					strcmp(small, "ali") == 0,
					"xjwtClaimString truncates safely");
				CHECK(!xjwtClaimString(out, "missing", buf, sizeof(buf)),
					"xjwtClaimString missing key -> false");
				CHECK(!xjwtClaimString(out, "num", buf, sizeof(buf)),
					"xjwtClaimString non-string -> false");
				xrtValueRelease(out);
			}
			xrtFree(token);
		}
	}

	CHECK(jwt_time_bounds_run(false) == 0, "RFC 7519 time bounds and leeway");
	printf("\n%d pass, %d fail\n", s_pass, s_fail);
	xjwtKeyFree(g_fuzzKey);
	return s_fail > 0 ? 1 : 0;
}
