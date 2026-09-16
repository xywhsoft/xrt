/* OIDC 组合测试：xoauth2 + xjwt 数据耦合全链路（应用层胶水的回归锁）。
 * 单 TU 同时编入两个库（互不认知，仅在测试里会师）：
 *   #include "../../xjwt/xjwt.c"    ← xrt 全量实现（XRT_MODULE_ALL）
 *   #include "../xoauth2.c"         <xrt.h> 已含，include guard 生效
 * 验证 README "数据耦合" 一节的 8 行胶水组合：
 * BeginLogin(nonce) → CompleteLogin(id_token) → HttpGet(JWKS) →
 * xjwtJwksParse → xjwtVerifyJwks(iss/aud) → nonce 消费 → userinfo。 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include "../../xjwt/xjwt.c"
#include "../xoauth2.c"

#include <stdio.h>
#include <string.h>
#include "oidc_keys.h"

static int s_pass = 0, s_fail = 0;
#define CHECK(expr, msg) do { \
	if ( !(expr) ) { printf("FAIL: %s\n", msg); s_fail++; } \
	else { s_pass++; } \
} while (0)

/* ---- mock 传输：按 URL 路由（token / jwks / userinfo） ---- */
static const char* g_TokenJson = NULL;   /* 由测试现场用 xjwt 签发填充 */
static char g_SawMethod[8];
static char g_SawAuth[128];

static bool mock_http(const char* sMethod, const char* sUrl, const char* sBody,
                      const char* sAuthHeader, char** psBody, int* piStatus,
                      void* pContext)
{
	(void)pContext; (void)sBody;
	snprintf(g_SawMethod, sizeof(g_SawMethod), "%s", sMethod);
	snprintf(g_SawAuth, sizeof(g_SawAuth), "%s", sAuthHeader ? sAuthHeader : "");
	*piStatus = 200;
	if ( strstr(sUrl, "/jwks") != NULL ) {
		const char* s = OIDC_JWKS;
		*psBody = (char*)xrtMalloc(strlen(s) + 1);
		strcpy(*psBody, s);
		return true;
	}
	if ( strstr(sUrl, "/userinfo") != NULL ) {
		const char* s = "{\"sub\":\"idp-user-7\",\"email\":\"u@example.com\","
			"\"email_verified\":true}";
		*psBody = (char*)xrtMalloc(strlen(s) + 1);
		strcpy(*psBody, s);
		return true;
	}
	if ( strstr(sUrl, "/token") != NULL ) {
		*psBody = (char*)xrtMalloc(strlen(g_TokenJson) + 1);
		strcpy(*psBody, g_TokenJson);
		return true;
	}
	*piStatus = 404;
	*psBody = (char*)xrtMalloc(1);
	(*psBody)[0] = 0;
	return true;
}

/* 用夹具 EC 密钥签一个 id_token（经 xjwt，含 iss/aud/nonce/sub） */
static char* sign_id_token(const char* sIssuer, const char* sAudience,
                           const char* sNonce, const char* sSub)
{
	xvalue* claims = xrtValueObject();
	xrtValueObjectSetNew(claims, xrtStrView("iss"),
		xrtValueString(xrtStrView(sIssuer)));
	xrtValueObjectSetNew(claims, xrtStrView("aud"),
		xrtValueString(xrtStrView(sAudience)));
	xrtValueObjectSetNew(claims, xrtStrView("sub"),
		xrtValueString(xrtStrView(sSub)));
	if ( sNonce != NULL )
		xrtValueObjectSetNew(claims, xrtStrView("nonce"),
			xrtValueString(xrtStrView(sNonce)));
	xjwtconfig cfg;
	xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_ES256;
	cfg.KeyPem = OIDC_EC_PRIV;
	cfg.KeyId = "oidc-ec-1";
	cfg.ExpireSeconds = 600;
	char* sToken = xjwtSign(&cfg, claims);
	xrtValueRelease(claims);
	return sToken;
}

int main(void)
{
	xoauth2client oauth;
	xoauth2token* tok = NULL;
	char* url = NULL;
	char* jwksJson = NULL;
	xjwtjwks* keys = NULL;
	xvalue* claims = NULL;

	/* 1. 客户端：预设 + 指向 mock 的端点 + xoauth2 传输回调 */
	xoauth2UseCustom(&oauth, NULL);
	{
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp.example/authorize";
		cfg.TokenUrl = "https://idp.example/token";
		cfg.UserInfoUrl = "https://idp.example/userinfo";
		cfg.JwksUrl = "https://idp.example/jwks";
		cfg.Issuer = "https://idp.example";
		cfg.ClientId = "my-client-id";
		cfg.ClientSecret = "my-secret";
		cfg.RedirectUri = "https://app/cb";
		cfg.Scope = "openid email profile";
		cfg.UseNonce = true;
		cfg.Http = mock_http;
		xoauth2UseCustom(&oauth, &cfg);
	}

	/* 2. BeginLogin：nonce 已进 URL */
	url = xoauth2BeginLogin(&oauth);
	CHECK(url != NULL && strstr(url, "&nonce=") != NULL,
		"compose: begin with nonce");

	/* 4. 现场签发 id_token（nonce 用客户端刚生成的）并装进 token 响应 */
	{
		char* idTok = sign_id_token("https://idp.example", "my-client-id",
			oauth.sNonce, "idp-user-7");
		CHECK(idTok != NULL, "compose: id_token signed");
		size_t n = strlen(idTok) + 128;
		g_TokenJson = (const char*)xrtMalloc(n);
		snprintf((char*)g_TokenJson, n,
			"{\"access_token\":\"acc-1\",\"token_type\":\"Bearer\","
			"\"expires_in\":3600,\"id_token\":\"%s\"}", idTok);
		xrtFree(idTok);
	}

	/* 5. CompleteLogin：经 mock 传输换到含 id_token 的令牌 */
	tok = xoauth2CompleteLogin(&oauth, "auth-code-1", oauth.sState);
	CHECK(tok != NULL && tok->IdToken != NULL,
		"compose: token with id_token");
	CHECK(strcmp(g_SawMethod, "POST") == 0, "compose: token via POST");

	/* 5. ===== 数据耦合胶水（README 同款 8 行）===== */
	jwksJson = xoauth2HttpGet(&oauth, oauth.Config.JwksUrl, NULL, &(int){0});
	CHECK(jwksJson != NULL && strcmp(g_SawMethod, "GET") == 0,
		"compose: JWKS via GET");
	keys = xjwtJwksParse(jwksJson);
	CHECK(keys != NULL, "compose: jwks parsed by xjwt");
	{
		xjwtcheck ck;
		xjwtCheckInit(&ck);
		ck.Issuer = oauth.Config.Issuer;
		ck.Audience = oauth.Config.ClientId;
		claims = xjwtVerifyJwks(tok->IdToken, keys, &ck);
	}
	CHECK(claims != NULL, "compose: id_token verified (iss/aud/exp/kid)");
	{
		char aSub[32], aNonce[128];
		CHECK(xjwtClaimString(claims, "sub", aSub, sizeof(aSub)) &&
			strcmp(aSub, "idp-user-7") == 0,
			"compose: sub from verified claims");
		CHECK(xjwtClaimString(claims, "nonce", aNonce, sizeof(aNonce)) &&
			xoauth2NonceConsume(&oauth, aNonce),
			"compose: nonce consumed (constant-time + burn)");
		xrtClearError();
		CHECK(!xoauth2NonceConsume(&oauth, aNonce) &&
			xoauth2LastError() == XOAUTH2_ERROR_NONCE_MISMATCH,
			"compose: nonce replay rejected");
	}

	/* 6. userinfo（Bearer 头 + 对象 claims） */
	{
		xvalue* ui = xoauth2GetUserInfo(&oauth, tok->AccessToken);
		CHECK(ui != NULL, "compose: userinfo claims");
		if ( ui != NULL ) {
			char aEmail[64];
			CHECK(xjwtClaimString(ui, "email", aEmail, sizeof(aEmail)) &&
				strcmp(aEmail, "u@example.com") == 0,
				"compose: userinfo email");
			xrtValueRelease(ui);
		}
		CHECK(strncmp(g_SawAuth, "Bearer acc-1", 12) == 0,
			"compose: userinfo bearer");
	}

	/* 7. 负向：篡改 id_token → 验签拒绝 */
	{
		size_t n = strlen(tok->IdToken);
		char* bad = (char*)xrtMalloc(n + 1);
		strcpy(bad, tok->IdToken);
		bad[n - 2] = (bad[n - 2] == 'A') ? 'B' : 'A';
		xjwtcheck ck;
		xjwtCheckInit(&ck);
		ck.Issuer = oauth.Config.Issuer;
		ck.Audience = oauth.Config.ClientId;
		xrtClearError();
		CHECK(xjwtVerifyJwks(bad, keys, &ck) == NULL,
			"compose: tampered id_token rejected");
		xrtFree(bad);
	}
	/* 8. 负向：错误 iss → 拒绝 */
	{
		char* idTok = sign_id_token("https://evil.example", "my-client-id",
			"x", "attacker");
		xjwtcheck ck;
		xjwtCheckInit(&ck);
		ck.Issuer = oauth.Config.Issuer;
		ck.Audience = oauth.Config.ClientId;
		xrtClearError();
		CHECK(xjwtVerifyJwks(idTok, keys, &ck) == NULL,
			"compose: wrong issuer rejected");
		xrtFree(idTok);
	}

	/* 清理 */
	if ( claims ) xrtValueRelease(claims);
	if ( keys ) xjwtJwksFree(keys);
	if ( jwksJson ) xrtFree(jwksJson);
	if ( tok ) xoauth2TokenFree(tok);
	xrtFree((void*)g_TokenJson);
	xoauth2ClientUnit(&oauth);

	printf("\n%d pass, %d fail\n", s_pass, s_fail);
	return s_fail > 0 ? 1 : 0;
}
