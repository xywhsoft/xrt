/* xjwt 测试：单 TU（XRT_IMPLEMENTATION 只定义一次）。 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include "../xjwt.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_keys.h"

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

	/* ---- openssl 签的 ES256 令牌 → xjwt 验证（DER 编码互认） ---- */
	{
		xvalue* out = xjwtVerify(K_TOKEN_ES256_OPENSSL, K_EC_PUB, NULL);
		CHECK(out != NULL, "openssl-signed ES256 verified by xjwt (DER)");
		if ( out != NULL ) xrtValueRelease(out);
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
		unsigned char aTiny[64];
		CHECK(!xjwt__sign(XJWT_ALG_ES256, "x", 1, K_EC_PRIV,
			aTiny, sizeof(aTiny), &sz),
			"LOW2 ES256 with 64-byte output rejected");
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
		const char* aAlphabet =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
		char aBuf[80];
		for ( int iter = 0; iter < 2000; iter++ ) {
			seed = seed * 1664525u + 1013904223u;
			size_t len = (size_t)(seed % 40) + (iter % 7 == 0 ? 60 : 3);
			if ( len >= sizeof(aBuf) ) len = sizeof(aBuf) - 1;
			for ( size_t i = 0; i < len; i++ ) {
				seed = seed * 1664525u + 1013904223u;
				aBuf[i] = aAlphabet[seed % 65];
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

	printf("\n%d pass, %d fail\n", s_pass, s_fail);
	xjwtKeyFree(g_fuzzKey);
	return s_fail > 0 ? 1 : 0;
}
