/* Signed OIDC code flow over real HTTPS. Keep secrets in runtime environment. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include "../xoauth2.h"
#include "xjwt.h"
#include "oidc_example_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* live_ca(const char* path)
{
    FILE* file = fopen(path, "rb"); long size; char* text = NULL;
    if ( file == NULL ) return NULL;
    if ( fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && size < 65536 &&
         fseek(file, 0, SEEK_SET) == 0 ) {
        text = xrtMalloc((size_t)size + 1u);
        if ( text != NULL ) {
            if ( fread(text, 1u, (size_t)size, file) != (size_t)size ) { xrtFree(text); text = NULL; }
            else text[size] = 0;
        }
    }
    fclose(file);
    return text;
}

static bool live_field(const xvalue* object, const char* name, char* text, size_t capacity)
{
    xstrview view;
    if ( !oidcExampleString(xrtValueObjectGet(object, xrtStrView(name)), &view) || view.Size >= capacity ) return false;
    memcpy(text, view.Data, view.Size); text[view.Size] = 0;
    return true;
}

static bool live_line(char* text, size_t capacity)
{
    if ( fgets(text, (int)capacity, stdin) == NULL ) return false;
    size_t size = strlen(text);
    if ( size == 0 || text[size - 1u] != '\n' ) return false;
    text[--size] = 0;
    if ( size != 0 && text[size - 1u] == '\r' ) text[--size] = 0;
    return size != 0;
}

static bool live_userinfo(xoauth2client* oauth, const char* access, const xvalue* identity)
{
    xvalue* user = xoauth2GetUserInfo(oauth, access);
    bool match = user != NULL && oidcExampleSameText(
        xrtValueObjectGet(user, xrtStrView("sub")), xrtValueObjectGet(identity, xrtStrView("sub")));
    xrtValueRelease(user);
    if ( !match && xrtGetError() == NULL ) oidcExampleFail(OIDC_EXAMPLE_CLAIMS, "userinfo subject differs from verified identity");
    return match;
}

int main(int argc, char** argv)
{
    if ( argc == 1 ) {
        puts("Usage: oidc_live ISSUER CLIENT_ID REDIRECT_URI CA_PEM|system basic|body|public [RS256|ES256]");
        puts("Set XOAUTH2_CLIENT_SECRET for confidential clients; enter callback code and state as two lines.");
        return 0;
    }
    if ( argc != 6 && argc != 7 ) return 2;
    bool public_client = strcmp(argv[5], "public") == 0;
    if ( !public_client && strcmp(argv[5], "basic") != 0 && strcmp(argv[5], "body") != 0 ) return 2;
    int result = 1, status = 0;
    const char* stage = "HTTPS transport";
    const char* secret = public_client ? NULL : getenv("XOAUTH2_CLIENT_SECRET");
    if ( !public_client && (secret == NULL || secret[0] == 0) ) {
        fputs("XOAUTH2_CLIENT_SECRET is required\n", stderr); return 2;
    }
    oidcExamplePolicy policy; oidcExamplePolicyInit(&policy);
    if ( argc == 7 ) {
        if ( strcmp(argv[6], "ES256") == 0 ) policy.SigningAlg = XJWT_ALG_ES256;
        else if ( strcmp(argv[6], "RS256") != 0 ) return 2;
    }
    xoauth2client oauth = {0}; xoauth2config config;
    xoauth2httpxrt* http = NULL;
    xoauth2token *token = NULL, *replacement = NULL;
    xvalue *metadata = NULL, *identity = NULL, *updated = NULL;
    xjwtjwks* keys = NULL;
    char *ca = NULL, *json = NULL, *url = NULL;
    char issuer[2048], authorize[2048], endpoint[2048], userinfo[2048], jwks[2048];
    char discovery[4096], code[8192] = {0}, state[128] = {0};
    if ( strcmp(argv[4], "system") != 0 && (ca = live_ca(argv[4])) == NULL ) goto done;
    http = xoauth2HttpXrtCreate(NULL, ca, 10000000u);
    if ( http == NULL ) goto done;
    xoauth2ConfigInit(&config); config.Http = xoauth2HttpXrt; config.HttpContext = http;
    xoauth2UseCustom(&oauth, &config);
    stage = "trusted discovery";
    size_t size = strlen(argv[1]);
    int written = snprintf(discovery, sizeof(discovery), "%s%s.well-known/openid-configuration",
        argv[1], size && argv[1][size - 1] == '/' ? "" : "/");
    if ( written < 0 || (size_t)written >= sizeof(discovery) || strncmp(argv[1], "https://", 8) ) goto done;
    json = xoauth2HttpGet(&oauth, discovery, NULL, &status);
    if ( json == NULL || (metadata = xrtJsonParse(xrtStrView(json))) == NULL ||
         !live_field(metadata, "issuer", issuer, sizeof(issuer)) || strcmp(issuer, argv[1]) ||
         !live_field(metadata, "authorization_endpoint", authorize, sizeof(authorize)) ||
         !live_field(metadata, "token_endpoint", endpoint, sizeof(endpoint)) ||
         !live_field(metadata, "userinfo_endpoint", userinfo, sizeof(userinfo)) ||
         !live_field(metadata, "jwks_uri", jwks, sizeof(jwks)) ||
         strncmp(authorize, "https://", 8) || strncmp(endpoint, "https://", 8) ||
         strncmp(userinfo, "https://", 8) || strncmp(jwks, "https://", 8) ) goto done;
    xrtFree(json); json = NULL;
    config.AuthorizeUrl = authorize; config.TokenUrl = endpoint; config.UserInfoUrl = userinfo;
    config.JwksUrl = jwks; config.Issuer = issuer; config.ClientId = argv[2];
    config.ClientSecret = secret; config.RedirectUri = argv[3];
    config.AuthStyle = strcmp(argv[5], "basic") == 0 ? XOAUTH2_AUTH_BASIC : XOAUTH2_AUTH_BODY;
    config.UsePkce = true; config.UseNonce = true; config.Scope = "openid profile email offline_access";
    xoauth2UseCustom(&oauth, &config);
    stage = "browser authorization and callback";
    url = xoauth2BeginLogin(&oauth);
    if ( url == NULL ) goto done;
    printf("AUTHORIZE %s\n", url); fflush(stdout);
    if ( !live_line(code, sizeof(code)) || !live_line(state, sizeof(state)) ) goto done;
    stage = "token exchange";
    token = xoauth2CompleteLogin(&oauth, code, state);
    xrtSecureZero(code, sizeof(code)); xrtSecureZero(state, sizeof(state));
    if ( token == NULL || token->IdToken == NULL ) goto done;
    stage = "provider JWKS";
    json = xoauth2HttpGet(&oauth, jwks, NULL, &status);
    if ( json == NULL || (keys = xjwtJwksParse(json)) == NULL ) goto done;
    stage = "signed ID token application policy";
    identity = oidcExampleVerifyIdToken(&oauth, token, keys, &policy, NULL);
    if ( identity == NULL ) goto done;
    stage = "userinfo subject binding";
    if ( !live_userinfo(&oauth, token->AccessToken, identity) ) goto done;
    puts("PASS verified ID token, nonce and userinfo subject");
    if ( token->RefreshToken != NULL ) {
        stage = "refresh";
        replacement = xoauth2Refresh(&oauth, token->RefreshToken);
        if ( replacement == NULL || !oidcExampleRequireBearer(replacement) ) goto done;
        stage = "refreshed identity";
        if ( replacement->IdToken != NULL ) {
            updated = oidcExampleVerifyIdToken(&oauth, replacement, keys, &policy, identity);
            if ( updated == NULL ) goto done;
        }
        if ( !live_userinfo(&oauth, replacement->AccessToken, identity) ) goto done;
        puts("PASS refreshed token and initial identity binding");
    }
    result = 0;
done:
    if ( result ) fprintf(stderr, "OIDC live example failed at %s (kind=%d code=%d)\n",
        stage, (int)xrtErrorKind(xrtGetError()), xrtErrorCode(xrtGetError()));
    xrtValueRelease(updated); xrtValueRelease(identity); xrtValueRelease(metadata); xjwtJwksFree(keys);
    xoauth2TokenFree(replacement); xoauth2TokenFree(token);
    xrtFree(json); xrtFree(url); xoauth2ClientUnit(&oauth);
    bool cleaned = false;
    for ( unsigned attempt = 0; attempt < 3u && !cleaned; attempt++ ) cleaned = xoauth2HttpXrtCleanup(http);
    if ( cleaned ) xoauth2HttpXrtDestroy(http);
    else { result = 1; fputs("OIDC transport cleanup did not complete\n", stderr); }
    if ( !xoauth2HttpXrtCleanupPending(10000000u, NULL) ) result = 1;
    xrtFree(ca); xrtSecureZero(code, sizeof(code)); xrtSecureZero(state, sizeof(state));
    return result;
}
