/*
	xoauth2 API 评审示例：一个 Web 服务的第三方登录全景（五场景）。

	S1 GitHub 授权登录（纯 OAuth）		S2 Google OIDC（数据耦合组合 xjwt）
	S3 token 生命周期（过期→刷新→替换）	S4 错误全景（错误码→HTTP 状态映射）
	S5 会话重置（ClientUnit→重新预设复用）

	mock 传输离线可跑。编译（在 extlibs/xoauth2 目录）：
	  gcc -std=c11 -I. -I../xjwt -I../../single -o /tmp/oauth2_review \
	      examples/api_review.c xoauth2.c ../xjwt/xjwt.c \
	      -lws2_32 -lbcrypt -ladvapi32 -liphlpapi
*/
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include "../xoauth2.h"
#include "xjwt.h"
#include <stdio.h>
#include <string.h>
#include "../tests/oidc_keys.h"   /* 仅 S2 的 IdP 夹具 */

/* ---- mock 传输：按 URL 前缀路由，可编程响应 ---- */
static int    g_ReplyStatus = 200;
static const char* g_ReplyBody = "{}";
static const char* g_RefreshBody;      /* S3 用：refresh 请求的独立应答 */
static char   g_LastMethod[8];

static bool mock_http(const char* sMethod, const char* sUrl, const char* sBody,
                      const char* sAuthHeader, char** psBody, int* piStatus,
                      void* pContext)
{
	(void)sBody; (void)sAuthHeader; (void)pContext;
	snprintf(g_LastMethod, sizeof(g_LastMethod), "%s", sMethod);
	*piStatus = g_ReplyStatus;
	if ( strstr(sUrl, "/refresh-aware-token") && g_RefreshBody )
		*psBody = (char*)xrtStrDup(g_RefreshBody);
	else
		*psBody = (char*)xrtStrDup(g_ReplyBody);
	return true;
}

static void mock_set(int iStatus, const char* sBody)
{
	g_ReplyStatus = iStatus;
	g_ReplyBody = sBody;
}

/* ============ S1：GitHub 第三方登录（最常见场景） ============ */
static void scenario_github(void)
{
	xoauth2client oauth;
	char* url;
	xoauth2token* tok;

	/* 预设 + 挂传输（两步——摩擦点记录①） */
	xoauth2UseGithub(&oauth, "gh-client-id", "gh-secret", "https://app.example/cb");
	oauth.Config.Http = mock_http;

	/* 登录入口：拿到重定向 URL，交给路由层 302 */
	url = xoauth2BeginLogin(&oauth);
	printf("S1 authorize url: %.52s...\n", url);
	xrtFree(url);

	/* 回调：路由把 query 里的 code/state 交来（state 由库自动比对） */
	mock_set(200,
		"{\"access_token\":\"gho_abc123\",\"token_type\":\"bearer\","
		"\"scope\":\"read:user\"}");
	tok = xoauth2CompleteLogin(&oauth, "returned-code", oauth.sState);
	if ( tok == NULL ) { printf("S1 FAIL err=%d\n", xoauth2LastError()); return; }
	printf("S1 token ok: %.12s... (no expires_in -> %lld)\n",
		tok->AccessToken, (long long)tok->ExpiresIn);

	/* 用 access token 调 provider API（GitHub: GET api.github.com/user） */
	{
		int st = 0;
		mock_set(200, "{\"login\":\"alice\",\"id\":42}");
		oauth.Config.UserInfoUrl = "https://api.github.com/user";  /* 预设未带——摩擦点② */
		xvalue* user = xoauth2GetUserInfo(&oauth, tok->AccessToken);
		(void)st;
		if ( user ) {
			char aLogin[32];
			xjwtClaimString(user, "login", aLogin, sizeof(aLogin));
			printf("S1 github api user: %s\n", aLogin);
			xrtValueRelease(user);
		}
	}
	xoauth2TokenFree(tok);
	xoauth2ClientUnit(&oauth);
}

/* ============ S2：Google OIDC（数据耦合组合，第二次手写验手感） ============ */
static void scenario_oidc(void)
{
	xoauth2client oauth;
	xoauth2token* tok;
	char* jwksJson;
	xjwtjwks* keys;
	xvalue* claims;

	xoauth2UseGoogle(&oauth, "g-id", "g-secret", "https://app.example/cb");
	oauth.Config.Http = mock_http;

	char* url = xoauth2BeginLogin(&oauth);
	xrtFree(url);

	/* mock IdP：用夹具密钥签 id_token（回显 nonce） */
	{
		xvalue* c = xrtValueObject();
		xrtValueObjectSetNew(c, xrtStrView("iss"),
			xrtValueString(xrtStrView("https://accounts.google.com")));
		xrtValueObjectSetNew(c, xrtStrView("aud"),
			xrtValueString(xrtStrView("g-id")));
		xrtValueObjectSetNew(c, xrtStrView("sub"),
			xrtValueString(xrtStrView("google-u-9")));
		xrtValueObjectSetNew(c, xrtStrView("nonce"),
			xrtValueString(xrtStrView(oauth.sNonce)));
		xjwtconfig jc; xjwtConfigInit(&jc);
		jc.Alg = XJWT_ALG_ES256;
		jc.KeyPem = OIDC_EC_PRIV;
		jc.KeyId = "oidc-ec-1";
		jc.ExpireSeconds = 600;
		char* idTok = xjwtSign(&jc, c);
		xrtValueRelease(c);
		static char buf[2048];
		snprintf(buf, sizeof(buf),
			"{\"access_token\":\"ya29.x\",\"id_token\":\"%s\","
			"\"expires_in\":3599}", idTok);
		xrtFree(idTok);
		mock_set(200, buf);
	}
	tok = xoauth2CompleteLogin(&oauth, "g-code", oauth.sState);
	if ( !tok ) { printf("S2 FAIL err=%d\n", xoauth2LastError()); return; }

	/* 组合胶水（与 README 相同 8 行——第二次写，验顺滑度） */
	mock_set(200, OIDC_JWKS);
	jwksJson = xoauth2HttpGet(&oauth, oauth.Config.JwksUrl, NULL, &(int){0});
	keys = xjwtJwksParse(jwksJson);
	{
		xjwtcheck ck; xjwtCheckInit(&ck);
		ck.Issuer = oauth.Config.Issuer;
		ck.Audience = oauth.Config.ClientId;
		claims = xjwtVerifyJwks(tok->IdToken, keys, &ck);
	}
	if ( claims ) {
		char sub[32], nonce[128];
		xjwtClaimString(claims, "sub", sub, sizeof(sub));
		xjwtClaimString(claims, "nonce", nonce, sizeof(nonce));
		printf("S2 verified sub=%s nonce=%s\n", sub,
			xoauth2NonceConsume(&oauth, nonce) ? "ok" : "FAIL");
		xrtValueRelease(claims);
	}
	xjwtJwksFree(keys);
	xrtFree(jwksJson);
	xoauth2TokenFree(tok);
	xoauth2ClientUnit(&oauth);
}

/* ============ S3：token 生命周期（过期→刷新→替换） ============ */
static void scenario_lifecycle(void)
{
	xoauth2client oauth;
	xoauth2config cfg;
	xoauth2token* tok;
	xoauth2token* fresh;

	xoauth2ConfigInit(&cfg);
	cfg.AuthorizeUrl = "https://idp/a";
	cfg.TokenUrl = "https://idp/refresh-aware-token";
	cfg.ClientId = "cid";
	cfg.ClientSecret = "sec";
	cfg.RedirectUri = "https://app/cb";
	cfg.Http = mock_http;
	xoauth2UseCustom(&oauth, &cfg);

	/* 拿到即将过期的 token（expires_in=30） */
	mock_set(200, "{\"access_token\":\"old\",\"refresh_token\":\"rt-1\","
		"\"expires_in\":30}");
	{
		char* u = xoauth2BeginLogin(&oauth);
		xrtFree(u);
	}
	tok = xoauth2CompleteLogin(&oauth, "c", oauth.sState);
	if ( !tok ) { printf("S3 FAIL\n"); return; }

	/* 每请求的中间件式判断 */
	if ( xoauth2TokenExpiring(tok, 60) ) {
		printf("S3 token expiring in 30s (leeway 60) -> refresh\n");
		/* 刷新（旧 token 调用方负责释放——文档明确） */
		static const char* sRefreshReply =
			"{\"access_token\":\"new\",\"refresh_token\":\"rt-2\","
			"\"expires_in\":3600}";
		g_RefreshBody = sRefreshReply;
		mock_set(200, sRefreshReply);
		fresh = xoauth2Refresh(&oauth, tok->RefreshToken);
		g_RefreshBody = NULL;
		xoauth2TokenFree(tok);        /* 敏感字段已被库清零 */
		if ( fresh ) {
			tok = fresh;
			printf("S3 refreshed: %.8s... expiring(now)=%d\n",
				tok->AccessToken, (int)xoauth2TokenExpiring(tok, 60));
		}
	}
	xoauth2TokenFree(tok);
	xoauth2ClientUnit(&oauth);
}

/* ============ S4：错误全景（错误码 → HTTP 状态映射） ============ */
static int http_of(int err)
{
	switch ( err ) {
	case XOAUTH2_ERROR_STATE_MISMATCH: return 400;   /* 请求被篡改 */
	case XOAUTH2_ERROR_TOKEN_DENIED:   return 401;   /* code 过期/无效 */
	case XOAUTH2_ERROR_TOKEN_ENDPOINT:
	case XOAUTH2_ERROR_TOKEN_RESPONSE: return 502;   /* 上游异常 */
	case XOAUTH2_ERROR_NETWORK:        return 504;   /* 网关超时 */
	}
	return 500;
}

static void scenario_errors(void)
{
	xoauth2client oauth;
	xoauth2config cfg;
	xoauth2ConfigInit(&cfg);
	cfg.AuthorizeUrl = "https://idp/a";
	cfg.TokenUrl = "https://idp/t";
	cfg.ClientId = "cid";
	cfg.RedirectUri = "https://app/cb";
	cfg.Http = mock_http;
	xoauth2UseCustom(&oauth, &cfg);

	/* 逐个触发并打印映射 */
	{
		char* u = xoauth2BeginLogin(&oauth);
		xrtFree(u);
		xrtClearError();
		(void)xoauth2CompleteLogin(&oauth, "c", "tampered-state");
		printf("S4 state mismatch     -> HTTP %d\n", http_of(xoauth2LastError()));
	}
	{
		char* u = xoauth2BeginLogin(&oauth);
		xrtFree(u);
		mock_set(400, "{\"error\":\"invalid_grant\"}");
		xrtClearError();
		(void)xoauth2CompleteLogin(&oauth, "c", oauth.sState);
		printf("S4 provider denied    -> HTTP %d\n", http_of(xoauth2LastError()));
	}
	{
		char* u = xoauth2BeginLogin(&oauth);
		xrtFree(u);
		mock_set(502, "Bad Gateway");
		xrtClearError();
		(void)xoauth2CompleteLogin(&oauth, "c", oauth.sState);
		printf("S4 upstream 502       -> HTTP %d\n", http_of(xoauth2LastError()));
	}
	xoauth2ClientUnit(&oauth);
}

/* ============ S5：会话重置（ClientUnit → 重新预设复用） ============ */
static void scenario_reset(void)
{
	xoauth2client oauth;
	xoauth2UseMicrosoft(&oauth, "ms-id", "ms-secret", "https://app/cb",
		"contoso.onmicrosoft.com");
	printf("S5 tenant issuer: %.44s...\n", oauth.Config.Issuer);
	{
		char* u = xoauth2BeginLogin(&oauth);
		xrtFree(u);
	}
	printf("S5 before unit: state[0]=%d\n", (int)oauth.sState[0]);
	xoauth2ClientUnit(&oauth);
	printf("S5 after  unit: state[0]=%d issuer=%s\n",
		(int)oauth.sState[0], oauth.Config.Issuer ? "?" : "(null)");
	/* 重新预设后复用（Unit 语义：必须重新 Use*） */
	xoauth2UseMicrosoft(&oauth, "ms-id", "ms-secret", "https://app/cb", "common");
	printf("S5 re-preset issuer: %.40s...\n", oauth.Config.Issuer);
	xoauth2ClientUnit(&oauth);
}

int main(void)
{
	printf("==== S1 GitHub ====\n");     scenario_github();
	printf("==== S2 OIDC ====\n");       scenario_oidc();
	printf("==== S3 lifecycle ====\n");  scenario_lifecycle();
	printf("==== S4 errors ====\n");     scenario_errors();
	printf("==== S5 reset ====\n");      scenario_reset();
	return 0;
}
