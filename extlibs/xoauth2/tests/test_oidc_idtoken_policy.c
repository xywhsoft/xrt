/* Independent Python JWS fixtures drive the actual offline application. */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include "../xoauth2.h"
#include "../../xjwt/xjwt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* fixture_line(void)
{
    char buffer[16384];
    if ( fgets(buffer, sizeof(buffer), stdin) == NULL ) return NULL;
    size_t size = strlen(buffer);
    if ( size == 0 || buffer[size - 1u] != '\n' ) return NULL;
    buffer[--size] = 0;
    if ( size != 0 && buffer[size - 1u] == '\r' ) buffer[--size] = 0;
    return size != 0 ? xrtStrDup(buffer) : NULL;
}

static char* fixture_sign(const xjwtconfig* config, const xvalue* claims)
{
    (void)config;
    char* json = xrtJsonStringify(claims, false, NULL);
    if ( json == NULL ) return NULL;
    printf("FIXTURE %s\n", json); fflush(stdout);
    xrtFree(json);
    return fixture_line();
}

static str fixture_duplicate(cstr text)
{
    if ( strstr(text, "\"keys\"") != NULL ||
         strcmp(text, "{\"sub\":\"user-42\",\"email\":\"alice@example.com\"}") == 0 )
        return fixture_line();
    return xrtStrDup(text);
}

#define xjwtSign fixture_sign
#define xrtStrDup fixture_duplicate
#define main oidcExampleMain
#include "../examples/oidc_login.c"
#undef main
#undef xrtStrDup
#undef xjwtSign

static char* refresh_reply;
static bool refresh_http(const char* method, const char* url, const char* body,
    const char* auth, char** response, int* status, void* context)
{
    (void)method; (void)url; (void)body; (void)auth; (void)context;
    *status = 200;
    *response = xrtStrDup(refresh_reply);
    return *response != NULL;
}

static int refresh_policy(void)
{
    int result = 1;
    xoauth2client oauth = {0}; xoauth2config config;
    xoauth2token *initial = NULL, *replacement = NULL;
    xvalue *identity = NULL, *updated = NULL;
    xjwtjwks* keys = NULL;
    char *url = NULL, *jwks = NULL, *initial_json = NULL, *refresh_json = NULL;
    xoauth2ConfigInit(&config);
    config.AuthorizeUrl = "https://idp.example/authorize";
    config.TokenUrl = "https://idp.example/token";
    config.ClientId = "demo-client-id"; config.Issuer = "https://idp.example";
    config.RedirectUri = "https://app.example/cb"; config.UseNonce = true;
    config.Http = refresh_http;
    xoauth2UseCustom(&oauth, &config);
    url = xoauth2BeginLogin(&oauth);
    if ( url == NULL ) goto done;
    printf("FIXTURE {\"iss\":\"https://idp.example\",\"aud\":\"demo-client-id\",\"sub\":\"user-42\",\"nonce\":\"%s\"}\n", oauth.sNonce);
    fflush(stdout);
    jwks = fixture_line(); initial_json = fixture_line(); refresh_json = fixture_line();
    if ( jwks == NULL || initial_json == NULL || refresh_json == NULL ) goto done;
    keys = xjwtJwksParse(jwks);
    if ( keys == NULL ) goto done;
    refresh_reply = initial_json;
    initial = xoauth2CompleteLogin(&oauth, "fixture-code", oauth.sState);
    if ( initial == NULL || initial->RefreshToken == NULL ) goto done;
    oidcExamplePolicy policy; oidcExamplePolicyInit(&policy);
    policy.SigningAlg = XJWT_ALG_ES256;
    const char* trusted[] = {"trusted-api"};
    policy.TrustedAudiences = trusted; policy.TrustedAudienceCount = 1;
    identity = oidcExampleVerifyIdToken(&oauth, initial, keys, &policy, NULL);
    if ( identity == NULL || oauth.sNonce[0] != 0 ) goto done;
    refresh_reply = refresh_json;
    replacement = xoauth2Refresh(&oauth, initial->RefreshToken);
    if ( replacement == NULL || !oidcExampleRequireBearer(replacement) ) goto done;
    if ( replacement->IdToken != NULL ) {
        updated = oidcExampleVerifyIdToken(&oauth, replacement, keys, &policy, identity);
        if ( updated == NULL ) goto done;
    }
    result = 0;
done:
    refresh_reply = NULL;
    xrtValueRelease(updated); xrtValueRelease(identity); xjwtJwksFree(keys);
    xoauth2TokenFree(replacement); xoauth2TokenFree(initial); xoauth2ClientUnit(&oauth);
    xrtFree(refresh_json); xrtFree(initial_json); xrtFree(jwks); xrtFree(url);
    return result;
}

int main(int argc, char** argv)
{
    if ( !xrtMemDebugEnable(true) ) return 3;
    int result = argc == 2 && strcmp(argv[1], "refresh") == 0 ? refresh_policy() : oidcExampleMain();
    printf("POLICY_RESULT %d\n", result);
    xrtClearError();
    xmemdebugsnapshot memory; xrtMemDebugSnapshot(&memory);
    if ( memory.LiveCount || memory.LiveBytes || memory.InvalidFreeCount || memory.DoubleFreeCount || g_Nonce[0] ) {
        fprintf(stderr, "POLICY_CLEANUP live=%zu bytes=%zu invalid=%llu double=%llu nonce=%d\n",
            memory.LiveCount, memory.LiveBytes, (unsigned long long)memory.InvalidFreeCount,
            (unsigned long long)memory.DoubleFreeCount, g_Nonce[0] != 0);
        return 4;
    }
    return result;
}
