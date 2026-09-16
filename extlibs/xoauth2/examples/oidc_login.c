/*
	OIDC 登录组合示例：xoauth2（OAuth 传输）+ xjwt（id_token 验签）。

	两个库互相不知道对方的存在——耦合只发生在本文件的应用层胶水，
	边界数据是纯 C 字符串（id_token / JWKS JSON）与 xrt 核心的 xvalue。
	本示例用 mock 传输离线可跑；真实部署把 mock_http 换成
	xoauth2HttpXrt（或你自己的 HTTP 实现）即可。

	编译（在 extlibs/xoauth2 目录）：
	  gcc -std=c11 -I. -I../xjwt -I../../single \
	      -o /tmp/oidc_demo examples/oidc_login.c xoauth2.c ../xjwt/xjwt.c \
	      -lws2_32 -lbcrypt -ladvapi32 -liphlpapi
*/
#define XRT_MODULE_ALL       /* 示例简化：全模块（工程中按需选闭包） */
#define XRT_IMPLEMENTATION   /* xrt 实现集中在本 TU，两个库 TU 纯声明消费 */
#include <xrt.h>
#include "../xoauth2.h"
#include "xjwt.h"
#include <stdio.h>
#include <string.h>

/* 组合测试夹具：EC P-256 密钥 + 匹配 JWKS（生产中来自 IdP） */
#include "../tests/oidc_keys.h"

/* ---- mock 传输：替 IdP 应答三个端点 ---- */
static char* dupstr(const char* s)
{
	char* p = (char*)xrtMalloc(strlen(s) + 1);
	strcpy(p, s);
	return p;
}

static bool mock_http(const char* sMethod, const char* sUrl, const char* sBody,
                      const char* sAuthHeader, char** psBody, int* piStatus,
                      void* pContext)
{
	(void)sMethod; (void)sBody; (void)sAuthHeader; (void)pContext;
	*piStatus = 200;
	if ( strstr(sUrl, "/jwks") != NULL )
		*psBody = dupstr(OIDC_JWKS);
	else if ( strstr(sUrl, "/token") != NULL ) {
		/* 演示：IdP 返回 access_token + id_token。
		 * 真实场景 id_token 由 IdP 用它的私钥签发。 */
		static char sTokenJson[4096];
		xvalue* claims = xrtValueObject();
		xrtValueObjectSetNew(claims, xrtStrView("iss"),
			xrtValueString(xrtStrView("https://idp.example")));
		xrtValueObjectSetNew(claims, xrtStrView("aud"),
			xrtValueString(xrtStrView("demo-client-id")));
		xrtValueObjectSetNew(claims, xrtStrView("sub"),
			xrtValueString(xrtStrView("user-42")));
		xrtValueObjectSetNew(claims, xrtStrView("email"),
			xrtValueString(xrtStrView("alice@example.com")));
		{
			/* nonce 由调用方经 extern 传入（见下方 g_Nonce） */
			extern char g_Nonce[128];
			xrtValueObjectSetNew(claims, xrtStrView("nonce"),
				xrtValueString(xrtStrView(g_Nonce)));
		}
		{
			xjwtconfig cfg;
			xjwtConfigInit(&cfg);
			cfg.Alg = XJWT_ALG_ES256;
			cfg.KeyPem = OIDC_EC_PRIV;
			cfg.KeyId = "oidc-ec-1";
			cfg.ExpireSeconds = 600;
			char* idTok = xjwtSign(&cfg, claims);
			snprintf(sTokenJson, sizeof(sTokenJson),
				"{\"access_token\":\"acc-demo\",\"token_type\":\"Bearer\","
				"\"expires_in\":3600,\"id_token\":\"%s\"}", idTok);
			xrtFree(idTok);
		}
		xrtValueRelease(claims);
		*psBody = dupstr(sTokenJson);
	}
	else
		*psBody = dupstr("{\"sub\":\"user-42\",\"email\":\"alice@example.com\"}");
	return true;
}

char g_Nonce[128];   /* BeginLogin 生成后、token 签发前由胶水保存 */

int main(void)
{
	xoauth2client oauth;

	/* ① 客户端配置（真实场景用 xoauth2UseGoogle/UseMicrosoft 预设） */
	{
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp.example/authorize";
		cfg.TokenUrl = "https://idp.example/token";
		cfg.UserInfoUrl = "https://idp.example/userinfo";
		cfg.JwksUrl = "https://idp.example/jwks";
		cfg.Issuer = "https://idp.example";
		cfg.ClientId = "demo-client-id";
		cfg.RedirectUri = "https://app.example/cb";
		cfg.Scope = "openid email profile";
		cfg.UseNonce = true;
		cfg.Http = mock_http;
		xoauth2UseCustom(&oauth, &cfg);
	}

	/* ② 登录入口：302 到授权页（nonce 已自动生成并进 URL） */
	char* url = xoauth2BeginLogin(&oauth);
	printf("authorize: %.60s...nonce=%.8s...\n", url, oauth.sNonce);
	strcpy(g_Nonce, oauth.sNonce);   /* 演示：mock IdP 签发时要回显 */

	/* ③ 回调：code + state 换 token（state 常时校验后即焚毁） */
	xoauth2token* tok = xoauth2CompleteLogin(&oauth, "auth-code", oauth.sState);
	if ( tok == NULL ) {
		printf("token exchange failed: err=%d\n", xoauth2LastError());
		return 1;
	}
	xrtFree(url);
	printf("token: access=%.12s... id_token=%zu chars\n",
		tok->AccessToken, strlen(tok->IdToken));

	/* ④ ===== 应用层胶水：xoauth2 取数 + xjwt 验签（两库在此会师）===== */
	int st = 0;
	char* jwksJson = xoauth2HttpGet(&oauth, oauth.Config.JwksUrl, NULL, &st);
	xjwtjwks* keys = xjwtJwksParse(jwksJson);
	xjwtcheck ck;
	xjwtCheckInit(&ck);
	ck.Issuer = oauth.Config.Issuer;      /* issuer/JWKS 是 xoauth2 的预设知识，
	                                       * 但只是一根字符串，喂给谁都不产生依赖 */
	ck.Audience = oauth.Config.ClientId;
	xvalue* claims = xjwtVerifyJwks(tok->IdToken, keys, &ck);
	if ( claims == NULL ) {
		printf("id_token verify failed: err=%d\n", xjwtLastError());
		return 1;
	}
	char sub[32], email[64], nonce[128];
	xjwtClaimString(claims, "sub", sub, sizeof(sub));
	xjwtClaimString(claims, "email", email, sizeof(email));
	xjwtClaimString(claims, "nonce", nonce, sizeof(nonce));
	if ( !xoauth2NonceConsume(&oauth, nonce) ) {   /* 常时比对 + 一次性焚毁 */
		printf("nonce mismatch (replay?)\n");
		return 1;
	}
	printf("verified: sub=%s email=%s nonce=ok\n", sub, email);

	/* ⑤ userinfo（Bearer + 对象 claims；非 OIDC provider 同样适用） */
	xvalue* ui = xoauth2GetUserInfo(&oauth, tok->AccessToken);
	if ( ui != NULL ) {
		xjwtClaimString(ui, "email", email, sizeof(email));
		printf("userinfo: email=%s\n", email);
		xrtValueRelease(ui);
	}

	/* ⑥ 清理（各自释放各自的） */
	xrtValueRelease(claims);
	xjwtJwksFree(keys);
	xrtFree(jwksJson);
	xoauth2TokenFree(tok);
	xoauth2ClientUnit(&oauth);
	printf("done\n");
	return 0;
}
