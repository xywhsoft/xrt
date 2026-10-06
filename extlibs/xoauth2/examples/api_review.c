/*
	xoauth2 API 评审示例：一个 Web 服务的第三方登录全景（五场景）。

	S1 GitHub 授权登录（纯 OAuth）		S2 Google OIDC（数据耦合组合 xjwt）
	S3 token 生命周期（过期→刷新→替换）	S4 错误全景（错误码→HTTP 状态映射）
	S5 会话重置（ClientUnit→重新预设复用）

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
#include "../tests/oidc_keys.h"   /* 仅 S2 的 IdP 夹具 */

/* ---- mock 传输：按 URL 前缀路由，可编程响应 ---- */
#include "auth_example_support.h"
#include "oidc_example_support.h"
static int g_ReplyStatus=200;
static const char* g_ReplyBody="{}";
static const char* g_RefreshBody;
static char g_LastMethod[8];

static bool mock_http(const char* method, const char* url, const char* body,
                      const char* auth, char** response, int* status, void* context)
{
    (void)body; (void)auth; (void)context;
    snprintf(g_LastMethod,sizeof(g_LastMethod),"%s",method);
    *status=g_ReplyStatus;
    *response=xrtStrDup(strstr(url,"/refresh-aware-token") && g_RefreshBody
        ? g_RefreshBody : g_ReplyBody);
    return *response!=NULL;
}
static void mock_set(int status, const char* body)
{
    g_ReplyStatus=status; g_ReplyBody=body;
}

/* S1：GitHub OAuth 登录及 userinfo。每个场景统一释放已取得的所有权。 */
static bool scenario_github(void)
{
    bool ok=false;
    xoauth2client oauth={0};
    char* url=NULL;
    xoauth2token* tok=NULL;
    xvalue* user=NULL;
    char login[32];
    xoauth2UseGithub(&oauth,"gh-client-id","gh-secret","https://app.example/cb");
    oauth.Config.Http=mock_http;
    url=xoauth2BeginLogin(&oauth);
    if(url==NULL) goto done;
    printf("S1 authorize url: %.52s...\n",url);
    mock_set(200,"{\"access_token\":\"gho_abc123\",\"token_type\":\"bearer\",\"scope\":\"read:user\"}");
    tok=xoauth2CompleteLogin(&oauth,"returned-code",oauth.sState);
    if(tok==NULL) goto done;
    printf("S1 token received (no expires_in -> %lld)\n",(long long)tok->ExpiresIn);
    mock_set(200,"{\"login\":\"alice\",\"id\":42}");
    oauth.Config.UserInfoUrl="https://api.github.com/user";
    user=xoauth2GetUserInfo(&oauth,tok->AccessToken);
    if(user==NULL || !oauthExampleClaimString(user,"login",login,sizeof(login))) goto done;
    printf("S1 github api user: %s\n",login);
    ok=true;
done:
    xrtValueRelease(user); xoauth2TokenFree(tok); xrtFree(url);
    xoauth2ClientUnit(&oauth);
    return ok;
}

/* S2：Google OIDC，数据只通过字符串和 xvalue 在两库之间传递。 */
static bool scenario_oidc(void)
{
    bool ok=false;
    xoauth2client oauth={0};
    xoauth2token* tok=NULL;
    char* url=NULL; char* idToken=NULL; char* jwksJson=NULL;
    xjwtjwks* keys=NULL;
    xvalue* issued=NULL; xvalue* claims=NULL;
    char response[2048],sub[256];
    int status=0;
    xoauth2UseGoogle(&oauth,"g-id","g-secret","https://app.example/cb");
    oauth.Config.Http=mock_http;
    url=xoauth2BeginLogin(&oauth);
    if(url==NULL) goto done;
    issued=xrtValueObject();
    if(issued==NULL || !oauthExampleSetString(issued,"iss","https://accounts.google.com") ||
       !oauthExampleSetString(issued,"aud","g-id") ||
       !oauthExampleSetString(issued,"sub","google-u-9") ||
       !oauthExampleSetString(issued,"nonce",oauth.sNonce)) goto done;
    xjwtconfig config; xjwtConfigInit(&config);
    config.Alg=XJWT_ALG_ES256; config.KeyPem=OIDC_EC_PRIV;
    config.KeyId="oidc-ec-1"; config.ExpireSeconds=600;
    idToken=xjwtSign(&config,issued);
    if(idToken==NULL) goto done;
    int length=snprintf(response,sizeof(response),
        "{\"access_token\":\"ya29.x\",\"token_type\":\"bearer\",\"id_token\":\"%s\",\"expires_in\":3599}",idToken);
    if(length<0 || (size_t)length>=sizeof(response)) goto done;
    mock_set(200,response);
    tok=xoauth2CompleteLogin(&oauth,"g-code",oauth.sState);
    if(tok==NULL || tok->IdToken==NULL) goto done;
    mock_set(200,OIDC_JWKS);
    jwksJson=xoauth2HttpGet(&oauth,oauth.Config.JwksUrl,NULL,&status);
    if(jwksJson==NULL || status!=200) goto done;
    keys=xjwtJwksParse(jwksJson);
    if(keys==NULL) goto done;
    oidcExamplePolicy policy; oidcExamplePolicyInit(&policy);
    policy.SigningAlg=XJWT_ALG_ES256; /* 显式注册的离线夹具；非真实 Google 服务。 */
    claims=oidcExampleVerifyIdToken(&oauth,tok,keys,&policy,NULL);
    if(claims==NULL || !oauthExampleClaimString(claims,"sub",sub,sizeof(sub))) goto done;
    puts("S2 verified identity; nonce=ok");
    ok=true;
done:
    /* mock 响应可能借用了局部 response；退出后不得保留该指针。 */
    mock_set(200,"{}");
    xrtValueRelease(claims); xrtValueRelease(issued);
    xjwtJwksFree(keys); xrtFree(jwksJson); xrtFree(idToken); xrtFree(url);
    xoauth2TokenFree(tok); xoauth2ClientUnit(&oauth);
    return ok;
}

/* S3：刷新成功后才替换旧 token；失败仍由当前拥有者释放旧 token 一次。 */
static bool scenario_lifecycle(void)
{
    bool ok=false;
    xoauth2client oauth={0}; xoauth2config config;
    xoauth2token* tok=NULL; xoauth2token* fresh=NULL;
    char* url=NULL;
    xoauth2ConfigInit(&config);
    config.AuthorizeUrl="https://idp/a"; config.TokenUrl="https://idp/refresh-aware-token";
    config.ClientId="cid"; config.ClientSecret="sec"; config.RedirectUri="https://app/cb";
    config.Http=mock_http; xoauth2UseCustom(&oauth,&config);
    mock_set(200,"{\"access_token\":\"old\",\"token_type\":\"bearer\",\"refresh_token\":\"rt-1\",\"expires_in\":30}");
    url=xoauth2BeginLogin(&oauth);
    if(url==NULL) goto done;
    tok=xoauth2CompleteLogin(&oauth,"c",oauth.sState);
    if(tok==NULL || !xoauth2TokenExpiring(tok,60)) goto done;
    puts("S3 token expiring -> refresh");
    g_RefreshBody="{\"access_token\":\"new\",\"token_type\":\"bearer\",\"refresh_token\":\"rt-2\",\"expires_in\":3600}";
    fresh=xoauth2Refresh(&oauth,tok->RefreshToken);
    if(fresh==NULL) goto done;
    xoauth2TokenFree(tok); tok=fresh; fresh=NULL;
    if(xoauth2TokenExpiring(tok,60)) goto done;
    puts("S3 refreshed; replacement is not expiring");
    ok=true;
done:
    g_RefreshBody=NULL;
    xoauth2TokenFree(fresh); xoauth2TokenFree(tok); xrtFree(url);
    xoauth2ClientUnit(&oauth);
    return ok;
}

static int http_of(int error)
{
    switch(error) {
    case XOAUTH2_ERROR_STATE_MISMATCH: return 400;
    case XOAUTH2_ERROR_TOKEN_DENIED: return 401;
    case XOAUTH2_ERROR_TOKEN_ENDPOINT:
    case XOAUTH2_ERROR_TOKEN_RESPONSE: return 502;
    case XOAUTH2_ERROR_NETWORK: return 504;
    }
    return 500;
}
static bool scenario_errors(void)
{
    bool ok=false;
    xoauth2client oauth={0}; xoauth2config config;
    xoauth2token* unexpected=NULL;
    char* url=NULL;
    xoauth2ConfigInit(&config);
    config.AuthorizeUrl="https://idp/a"; config.TokenUrl="https://idp/t";
    config.ClientId="cid"; config.RedirectUri="https://app/cb"; config.Http=mock_http;
    xoauth2UseCustom(&oauth,&config);
    const int errors[]={XOAUTH2_ERROR_STATE_MISMATCH,XOAUTH2_ERROR_TOKEN_DENIED,XOAUTH2_ERROR_TOKEN_ENDPOINT};
    const int http[]={400,401,502};
    for(size_t i=0;i<3;i++) {
        url=xoauth2BeginLogin(&oauth);
        if(url==NULL) goto done;
        xrtFree(url); url=NULL;
        mock_set(i==1?400:502,i==1?"{\"error\":\"invalid_grant\"}":"Bad Gateway");
        xrtClearError();
        unexpected=xoauth2CompleteLogin(&oauth,"c",i==0?"tampered-state":oauth.sState);
        int error=xoauth2LastError();
        if(unexpected!=NULL || error!=errors[i] || http_of(error)!=http[i]) goto done;
        printf("S4 expected error=%d -> HTTP %d\n",error,http_of(error));
    }
    ok=true;
done:
    xoauth2TokenFree(unexpected); xrtFree(url); xoauth2ClientUnit(&oauth);
    return ok;
}

static bool scenario_reset(void)
{
    bool ok=false; xoauth2client oauth={0}; char* url=NULL;
    xoauth2UseMicrosoft(&oauth,"ms-id","ms-secret","https://app/cb","contoso.onmicrosoft.com");
    if(oauth.Config.Issuer==NULL) goto done;
    printf("S5 tenant issuer: %.44s...\n",oauth.Config.Issuer);
    url=xoauth2BeginLogin(&oauth);
    if(url==NULL || oauth.sState[0]==0) goto done;
    xrtFree(url); url=NULL;
    xoauth2ClientUnit(&oauth);
    if(oauth.sState[0]!=0 || oauth.Config.Issuer!=NULL) goto done;
    xoauth2UseMicrosoft(&oauth,"ms-id","ms-secret","https://app/cb","common");
    if(oauth.Config.Issuer==NULL) goto done;
    url=xoauth2BeginLogin(&oauth);
    if(url==NULL) goto done;
    puts("S5 unit and re-preset reuse verified");
    ok=true;
done:
    xrtFree(url); xoauth2ClientUnit(&oauth); return ok;
}

int main(void)
{
    const char* names[]={"GitHub","OIDC","token lifecycle","error mapping","session reset"};
    bool (*scenarios[])(void)={scenario_github,scenario_oidc,scenario_lifecycle,scenario_errors,scenario_reset};
    int result=0;
    for(size_t i=0;i<sizeof(scenarios)/sizeof(scenarios[0]);i++) {
        xrtClearError();
        if(!scenarios[i]()) {
            fprintf(stderr,"OAuth example failed at %s (kind=%d code=%d)\n",names[i],
                (int)xrtErrorKind(xrtGetError()),xoauth2LastError());
            result=1; break;
        }
    }
    size_t pending=SIZE_MAX;
    if(!xoauth2HttpXrtCleanupPending(5,&pending) || pending!=0u) result=1;
    return result;
}
