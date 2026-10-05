/*
	OIDC 登录组合示例：xoauth2（OAuth 传输）+ xjwt（id_token 验签）。

	两个库互相不知道对方的存在——耦合只发生在本文件的应用层胶水，
	边界数据是纯 C 字符串（id_token / JWKS JSON）与 xrt 核心的 xvalue。
	本示例使用离线夹具，并执行授权码 ID token 的应用策略。
	网络部署还须配置可信 discovery/注册算法、传输与回调会话管理。

 * 构建入口见本库 README；使用根目录 tools/build.py。
*/
#if !defined(XOAUTH2_FEATURE_XOAUTH2)
#define XRT_IMPLEMENTATION
#include "../tests/support/runtime.h"
#endif
#include <xoauth2.h>
#include "xjwt.h"
#include <stdio.h>
#include <string.h>

/* 组合测试夹具：EC P-256 密钥 + 匹配 JWKS（生产中来自 IdP） */
#include "../tests/oidc_keys.h"

/* ---- mock 传输：替 IdP 应答三个端点 ---- */
#include "auth_example_support.h"
#include "oidc_example_support.h"
static char g_Nonce[128];
static char* dupstr(const char* text) { return xrtStrDup(text); }

static bool mock_http(const char* method, const char* url, const char* body,
                      const char* auth, char** response, int* status, void* context)
{
    (void)method; (void)body; (void)auth; (void)context;
    *response=NULL; *status=200;
    if(strstr(url,"/jwks")!=NULL) *response=dupstr(OIDC_JWKS);
    else if(strstr(url,"/token")!=NULL) {
        /* 离线 IdP 使用夹具私钥；实际部署只验证 IdP 返回的令牌。 */
        xvalue* claims=xrtValueObject();
        char* idToken=NULL;
        if(claims!=NULL && oauthExampleSetString(claims,"iss","https://idp.example") &&
           oauthExampleSetString(claims,"aud","demo-client-id") &&
           oauthExampleSetString(claims,"sub","user-42") &&
           oauthExampleSetString(claims,"email","alice@example.com") &&
           oauthExampleSetString(claims,"nonce",g_Nonce)) {
            xjwtconfig config; xjwtConfigInit(&config);
            config.Alg=XJWT_ALG_ES256; config.KeyPem=OIDC_EC_PRIV;
            config.KeyId="oidc-ec-1"; config.ExpireSeconds=600;
            idToken=xjwtSign(&config,claims);
            if(idToken!=NULL) {
                char json[4096];
                int length=snprintf(json,sizeof(json),
                    "{\"access_token\":\"acc-demo\",\"token_type\":\"Bearer\",\"expires_in\":3600,\"id_token\":\"%s\"}",idToken);
                if(length>=0 && (size_t)length<sizeof(json)) *response=dupstr(json);
            }
        }
        xrtFree(idToken); xrtValueRelease(claims);
    } else *response=dupstr("{\"sub\":\"user-42\",\"email\":\"alice@example.com\"}");
    return *response!=NULL;
}

int main(void)
{
    int result=1,status=0;
    const char* stage="authorization URL";
    xoauth2client oauth={0}; xoauth2config config;
    char* url=NULL; char* jwksJson=NULL;
    xoauth2token* tok=NULL; xjwtjwks* keys=NULL;
    xvalue* claims=NULL; xvalue* user=NULL;
    char sub[256];
    xoauth2ConfigInit(&config);
    config.AuthorizeUrl="https://idp.example/authorize";
    config.TokenUrl="https://idp.example/token";
    config.UserInfoUrl="https://idp.example/userinfo";
    config.JwksUrl="https://idp.example/jwks";
    config.Issuer="https://idp.example"; config.ClientId="demo-client-id";
    config.RedirectUri="https://app.example/cb"; config.Scope="openid email profile";
    config.UseNonce=true; config.Http=mock_http;
    xoauth2UseCustom(&oauth,&config);
    url=xoauth2BeginLogin(&oauth);
    if(url==NULL || strlen(oauth.sNonce)>=sizeof(g_Nonce)) goto done;
    printf("authorize: %.60s...\n",url);
    strcpy(g_Nonce,oauth.sNonce);
    stage="token exchange";
    tok=xoauth2CompleteLogin(&oauth,"auth-code",oauth.sState);
    if(tok==NULL || tok->IdToken==NULL) goto done;
    printf("token received; id_token=%zu chars\n",strlen(tok->IdToken));
    stage="JWKS retrieval and parse";
    jwksJson=xoauth2HttpGet(&oauth,oauth.Config.JwksUrl,NULL,&status);
    if(jwksJson==NULL || status!=200) goto done;
    keys=xjwtJwksParse(jwksJson);
    if(keys==NULL) goto done;
    stage="id_token verification and required claims";
    oidcExamplePolicy policy; oidcExamplePolicyInit(&policy);
    policy.SigningAlg=XJWT_ALG_ES256; /* 本离线 IdP 显式注册 ES256。 */
    claims=oidcExampleVerifyIdToken(&oauth,tok,keys,&policy,NULL);
    if(claims==NULL || !oauthExampleClaimString(claims,"sub",sub,sizeof(sub))) goto done;
    puts("verified identity; nonce=ok");
    stage="userinfo";
    user=xoauth2GetUserInfo(&oauth,tok->AccessToken);
    char userSub[256];
    if(user==NULL || !oauthExampleClaimString(user,"sub",userSub,sizeof(userSub)) ||
       strcmp(userSub,sub)!=0) goto done;
    puts("userinfo: verified subject matched");
    result=0;
done:
    if(result!=0) fprintf(stderr,"OIDC example failed at %s (kind=%d code=%d)\n",stage,
        (int)xrtErrorKind(xrtGetError()),xrtErrorCode(xrtGetError()));
    xrtValueRelease(user); xrtValueRelease(claims); xjwtJwksFree(keys);
    xrtFree(jwksJson); xrtFree(url); xoauth2TokenFree(tok); xoauth2ClientUnit(&oauth);
    memset(g_Nonce,0,sizeof(g_Nonce));
    return result;
}
