/* Actual Hydra HTTPS client. The driver supplies callback code/state in memory. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "support/runtime.h"
#include <xoauth2.h>
#include <xjwt.h>
#include "../examples/oidc_example_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned Requests;
static bool counted_http(const char* method, const char* url, const char* body,
    const char* auth, char** response, int* status, void* context)
{
    Requests++;
    return xoauth2HttpXrt(method, url, body, auth, response, status, context);
}

static char* read_ca(const char* path)
{
    FILE* file = fopen(path, "rb");
    long size; char* text = NULL;
    if ( file == NULL ) return NULL;
    if ( fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && size < 65536 &&
         fseek(file, 0, SEEK_SET) == 0 ) {
        text = xrtMalloc((size_t)size + 1u);
        if ( text != NULL ) {
            if ( fread(text, 1u, (size_t)size, file) != (size_t)size ) {
                xrtFree(text); text = NULL;
            } else text[size] = 0;
        }
    }
    fclose(file);
    return text;
}

static bool string_field(const xvalue* value, const char* name, char* out, size_t capacity)
{
    xstrview text;
    if ( !xrtValueGetString(xrtValueObjectGet(value, xrtStrView(name)), &text) ||
         text.Size == 0 || text.Size >= capacity || memchr(text.Data, 0, text.Size) != NULL )
        return false;
    memcpy(out, text.Data, text.Size); out[text.Size] = 0;
    return true;
}

static bool line(char* out, size_t capacity)
{
    if ( fgets(out, (int)capacity, stdin) == NULL ) return false;
    size_t size = strlen(out);
    if ( size == 0 || out[size - 1] != '\n' ) return false;
    out[--size] = 0;
    if ( size != 0 && out[size - 1] == '\r' ) out[--size] = 0;
    return size != 0;
}

int main(int argc, char** argv)
{
    if ( argc != 7 ) return 2;
    int result = 1, status = 0;
    const char* stage = "transport initialization";
    const char* mode = argv[5];
    bool negative = strcmp(mode, "pkce") == 0 || strcmp(mode, "secret") == 0;
    char *ca = NULL, *json = NULL, *url = NULL, *tampered = NULL;
    xoauth2httpxrt* http = NULL;
    xoauth2client oauth = {0}; xoauth2config config;
    xoauth2token *token = NULL, *refreshed = NULL, *unexpected = NULL;
    xvalue *discovery = NULL, *claims = NULL, *updated = NULL, *user = NULL, *rejected = NULL;
    xjwtjwks* keys = NULL;
    char issuer[2048], authorize[2048], endpoint[2048], userinfo[2048], jwks[2048];
    char code[4096] = {0}, state[128] = {0}, nonce[128] = {0}, subject[256], user_subject[256];
    char discovery_url[4096];
    if ( !xrtMemDebugEnable(true) ) return 3;
    ca = read_ca(argv[4]);
    if ( ca == NULL || (http = xoauth2HttpXrtCreate(NULL, ca, 10000)) == NULL ) goto done;
    xoauth2ConfigInit(&config); config.Http = counted_http; config.HttpContext = http;
    xoauth2UseCustom(&oauth, &config);
    stage = "discovery";
    size_t length = strlen(argv[1]);
    int written = snprintf(discovery_url, sizeof(discovery_url), "%s%s.well-known/openid-configuration",
        argv[1], length != 0 && argv[1][length - 1] == '/' ? "" : "/");
    if ( written < 0 || (size_t)written >= sizeof(discovery_url) || strncmp(argv[1], "https://", 8) != 0 ) goto done;
    json = xoauth2HttpGet(&oauth, discovery_url, NULL, &status);
    if ( json == NULL || (discovery = xrtJsonParse(xrtStrView(json))) == NULL ||
         !string_field(discovery, "issuer", issuer, sizeof(issuer)) || strcmp(issuer, argv[1]) != 0 ||
         !string_field(discovery, "authorization_endpoint", authorize, sizeof(authorize)) ||
         !string_field(discovery, "token_endpoint", endpoint, sizeof(endpoint)) ||
         !string_field(discovery, "userinfo_endpoint", userinfo, sizeof(userinfo)) ||
         !string_field(discovery, "jwks_uri", jwks, sizeof(jwks)) ||
         strncmp(authorize, "https://", 8) || strncmp(endpoint, "https://", 8) ||
         strncmp(userinfo, "https://", 8) || strncmp(jwks, "https://", 8) ) goto done;
    xrtFree(json); json = NULL;
    config.AuthorizeUrl = authorize; config.TokenUrl = endpoint;
    config.UserInfoUrl = userinfo; config.JwksUrl = jwks;
    config.Issuer = issuer; config.ClientId = argv[2]; config.RedirectUri = argv[3];
    config.ClientSecret = strcmp(argv[6], "public") == 0 ? NULL : getenv("XOAUTH2_CLIENT_SECRET");
    config.AuthStyle = strcmp(argv[6], "basic") == 0 ? XOAUTH2_AUTH_BASIC : XOAUTH2_AUTH_BODY;
    config.Scope = "openid profile email offline_access";
    config.UsePkce = true; config.UseNonce = true;
    xoauth2UseCustom(&oauth, &config);
    stage = "authorization URL";
    url = xoauth2BeginLogin(&oauth);
    if ( url == NULL ) goto done;
    strcpy(nonce, oauth.sNonce);
    printf("AUTHORIZE %s\n", url); fflush(stdout);
    if ( !line(code, sizeof(code)) || !line(state, sizeof(state)) ) goto done;
    stage = "state rejection without network";
    unsigned requests = Requests;
    unexpected = xoauth2CompleteLogin(&oauth, code, "wrong-state");
    if ( unexpected != NULL || xoauth2LastError() != XOAUTH2_ERROR_STATE_MISMATCH || Requests != requests ) goto done;
    if ( strcmp(mode, "pkce") == 0 ) oauth.sVerifier[0] = oauth.sVerifier[0] == 'A' ? 'B' : 'A';
    stage = "authorization code exchange";
    xrtClearError(); token = xoauth2CompleteLogin(&oauth, code, state);
    if ( negative ) {
        if ( token == NULL && xoauth2LastError() == XOAUTH2_ERROR_TOKEN_DENIED &&
             oauth.sState[0] == 0 && oauth.sVerifier[0] == 0 && Requests == requests + 1u ) {
            puts("PASS provider rejected token exchange; session consumed"); result = 0;
        }
        goto done;
    }
    if ( token == NULL || token->IdToken == NULL || token->RefreshToken == NULL || token->TokenType == NULL ||
         strcmp(token->TokenType, "bearer") != 0 || oauth.sState[0] != 0 || oauth.sVerifier[0] != 0 ) goto done;
    stage = "provider JWKS and signed identity";
    json = xoauth2HttpGet(&oauth, jwks, NULL, &status);
    if ( json == NULL || (keys = xjwtJwksParse(json)) == NULL ) goto done;
    oidcExamplePolicy policy; oidcExamplePolicyInit(&policy);
    if ( xoauth2NonceConsume(&oauth, "wrong-nonce") || xoauth2LastError() != XOAUTH2_ERROR_NONCE_MISMATCH ||
         strcmp(oauth.sNonce, nonce) ) goto done;
    xrtClearError();
    claims = oidcExampleVerifyIdToken(&oauth, token, keys, &policy, NULL);
    if ( claims == NULL || !string_field(claims, "sub", subject, sizeof(subject)) ||
         strcmp(subject, "user-42") || oauth.sNonce[0] != 0 ||
         xoauth2NonceConsume(&oauth, nonce) || xoauth2LastError() != XOAUTH2_ERROR_NONCE_MISMATCH ) goto done;
    xrtClearError();
    xjwtcheck check; xjwtCheckInit(&check); check.Issuer = issuer; check.Audience = argv[2];
    stage = "signed identity rejection controls";
    check.Issuer = "https://wrong-issuer.example.test/";
    rejected = xjwtVerifyJwks(token->IdToken, keys, &check);
    if ( rejected != NULL || xjwtLastError() != XJWT_ERROR_ISSUER ) goto done;
    check.Issuer = issuer; check.Audience = "wrong-audience"; xrtClearError();
    rejected = xjwtVerifyJwks(token->IdToken, keys, &check);
    if ( rejected != NULL || xjwtLastError() != XJWT_ERROR_AUDIENCE ) goto done;
    check.Audience = argv[2]; xrtClearError();
    tampered = xrtStrDup(token->IdToken);
    if ( tampered == NULL ) goto done;
    char* signature = strrchr(tampered, '.');
    if ( signature == NULL || signature[1] == 0 ) goto done;
    signature[1] = signature[1] == 'A' ? 'B' : 'A';
    rejected = xjwtVerifyJwks(tampered, keys, &check);
    if ( rejected != NULL || xjwtLastError() != XJWT_ERROR_SIGNATURE ) goto done;
    stage = "userinfo subject binding";
    xrtClearError(); user = xoauth2GetUserInfo(&oauth, token->AccessToken);
    if ( user == NULL || !string_field(user, "sub", user_subject, sizeof(user_subject)) || strcmp(user_subject, subject) ) goto done;
    xrtValueRelease(user); user = NULL;
    stage = "refresh and refreshed userinfo";
    refreshed = xoauth2Refresh(&oauth, token->RefreshToken);
    if ( refreshed == NULL || refreshed->AccessToken == NULL || refreshed->RefreshToken == NULL ||
         !oidcExampleRequireBearer(refreshed) ) goto done;
    if ( refreshed->IdToken != NULL ) {
        updated = oidcExampleVerifyIdToken(&oauth, refreshed, keys, &policy, claims);
        if ( updated == NULL ) goto done;
    }
    user = xoauth2GetUserInfo(&oauth, refreshed->AccessToken);
    if ( user == NULL || !string_field(user, "sub", user_subject, sizeof(user_subject)) || strcmp(user_subject, subject) ) goto done;
    puts("PASS S256 code; registered RS256 policy; issuer/audience/signature controls; nonce controls; userinfo; refresh identity");
    result = 0;
done:
    if ( result != 0 ) fprintf(stderr, "Hydra client failed at %s (oauth=%d jwt=%d kind=%d)\n",
        stage, xoauth2LastError(), xjwtLastError(), (int)xrtErrorKind(xrtGetError()));
    xrtValueRelease(rejected); xrtValueRelease(user); xrtValueRelease(updated); xrtValueRelease(claims);
    xrtValueRelease(discovery); xjwtJwksFree(keys);
    xoauth2TokenFree(unexpected); xoauth2TokenFree(refreshed); xoauth2TokenFree(token);
    xrtFree(tampered); xrtFree(json); xrtFree(url); xoauth2ClientUnit(&oauth);
    if ( !xoauth2HttpXrtCleanup(http) ) result = 1;
    xoauth2HttpXrtDestroy(http);
    if ( !xoauth2HttpXrtCleanupPending(10000, NULL) ) result = 1;
    xrtFree(ca); xrtClearError();
    xmemdebugsnapshot memory; xrtMemDebugSnapshot(&memory);
    if ( memory.LiveCount || memory.LiveBytes || memory.InvalidFreeCount || memory.DoubleFreeCount ) {
        fprintf(stderr, "Hydra client cleanup: live=%zu bytes=%zu invalid=%llu double=%llu\n",
            memory.LiveCount, memory.LiveBytes, (unsigned long long)memory.InvalidFreeCount,
            (unsigned long long)memory.DoubleFreeCount); result = 1;
    }
    xrtSecureZero(code, sizeof(code)); xrtSecureZero(state, sizeof(state)); xrtSecureZero(nonce, sizeof(nonce));
    return result;
}
