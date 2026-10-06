/* Application policy for signed authorization-code ID tokens; no library ABI. */
#ifndef XOAUTH2_OIDC_EXAMPLE_SUPPORT_H
#define XOAUTH2_OIDC_EXAMPLE_SUPPORT_H

#include <xoauth2.h>
#include "xjwt.h"
#include <string.h>

typedef struct oidcExamplePolicy {
    int SigningAlg;                  /* Registered RS256 or ES256; never from the token. */
    int ClockLeeway;                 /* Nonnegative seconds. */
    int64_t MaxTokenAge;             /* Nonnegative seconds; zero disables this age policy. */
    int64_t NowOverride;             /* Zero uses the clock; nonzero is for deterministic tests. */
    const char* const* TrustedAudiences; /* Additional recipients explicitly trusted by this app. */
    size_t TrustedAudienceCount;
} oidcExamplePolicy;

enum {
    OIDC_EXAMPLE_ARGUMENT = 1,
    OIDC_EXAMPLE_ALGORITHM,
    OIDC_EXAMPLE_CLAIMS,
    OIDC_EXAMPLE_AUDIENCE,
    OIDC_EXAMPLE_TIME,
    OIDC_EXAMPLE_ACCESS_HASH,
    OIDC_EXAMPLE_REFRESH_IDENTITY,
    OIDC_EXAMPLE_TOKEN_TYPE
};

static inline void oidcExamplePolicyInit(oidcExamplePolicy* policy)
{
    if ( policy == NULL ) return;
    memset(policy, 0, sizeof(*policy));
    policy->SigningAlg = XJWT_ALG_RS256;
    policy->ClockLeeway = 30;
    policy->MaxTokenAge = 600;
}

static inline bool oidcExampleFail(int code, const char* message)
{
    xrtSetErrorInfo(XERR_PROTOCOL, "example.oidc", code, message);
    return false;
}

/* This application uses Bearer userinfo; check even when refresh omits id_token. */
static inline bool oidcExampleRequireBearer(const xoauth2token* token)
{
    return (token != NULL && token->TokenType != NULL && strcmp(token->TokenType, "bearer") == 0) ||
        oidcExampleFail(OIDC_EXAMPLE_TOKEN_TYPE, "application requires a bearer token");
}

static inline bool oidcExampleString(const xvalue* value, xstrview* text)
{
    return xrtValueGetString(value, text) && text->Size != 0 &&
        memchr(text->Data, 0, text->Size) == NULL;
}

static inline bool oidcExampleTextEquals(xstrview text, const char* expected)
{
    return expected != NULL && text.Size == strlen(expected) &&
        xrtConstTimeEqual(text.Data, expected, text.Size);
}

static inline bool oidcExampleSameText(const xvalue* first, const xvalue* second)
{
    xstrview a, b;
    return oidcExampleString(first, &a) && oidcExampleString(second, &b) &&
        a.Size == b.Size && xrtConstTimeEqual(a.Data, b.Data, a.Size);
}

static inline bool oidcExampleInteger(const xvalue* value, int64* number)
{
    return xrtValueIs(value, XVALUE_INT) && xrtValueGetInt(value, number);
}

static inline size_t oidcExampleAudienceCount(const xvalue* audience)
{
    return xrtValueIs(audience, XVALUE_STRING) ? 1u :
        (xrtValueIs(audience, XVALUE_ARRAY) ? xrtValueCount(audience) : 0u);
}

static inline const xvalue* oidcExampleAudienceAt(const xvalue* audience, size_t index)
{
    return xrtValueIs(audience, XVALUE_ARRAY) ? xrtValueArrayGet(audience, index) : audience;
}

static inline bool oidcExampleAudienceValid(const xvalue* audience, const char* client,
    const oidcExamplePolicy* policy)
{
    size_t count = oidcExampleAudienceCount(audience);
    bool own = false;
    if ( count == 0 || count > 64u ) return false;
    for ( size_t i = 0; i < count; i++ ) {
        xstrview text;
        if ( !oidcExampleString(oidcExampleAudienceAt(audience, i), &text) ) return false;
        bool trusted = oidcExampleTextEquals(text, client);
        own = own || trusted;
        for ( size_t j = 0; !trusted && j < policy->TrustedAudienceCount; j++ )
            trusted = oidcExampleTextEquals(text, policy->TrustedAudiences[j]);
        if ( !trusted ) return false;
    }
    return own;
}

/* Audience recipients are an unordered set; string and one-element array agree. */
static inline bool oidcExampleAudienceContains(const xvalue* audience, const xvalue* member)
{
    for ( size_t i = 0; i < oidcExampleAudienceCount(audience); i++ )
        if ( oidcExampleSameText(oidcExampleAudienceAt(audience, i), member) ) return true;
    return false;
}

static inline bool oidcExampleSameAudience(const xvalue* first, const xvalue* second)
{
    size_t a = oidcExampleAudienceCount(first), b = oidcExampleAudienceCount(second);
    if ( a == 0 || b == 0 || a > 64u || b > 64u ) return false;
    for ( size_t i = 0; i < a; i++ )
        if ( !oidcExampleAudienceContains(second, oidcExampleAudienceAt(first, i)) ) return false;
    for ( size_t i = 0; i < b; i++ )
        if ( !oidcExampleAudienceContains(first, oidcExampleAudienceAt(second, i)) ) return false;
    return true;
}

static inline bool oidcExampleAccessHash(const xvalue* claims, const char* access)
{
    const xvalue* value = xrtValueObjectGet(claims, xrtStrView("at_hash"));
    if ( value == NULL ) return true; /* Optional in the authorization code flow. */
    xstrview supplied;
    unsigned char digest[32]; char encoded[23]; size_t length;
    xbase64config base64 = {NULL, XBASE64_URL | XBASE64_NO_PADDING};
    if ( !oidcExampleString(value, &supplied) || supplied.Size != 22u )
        return oidcExampleFail(OIDC_EXAMPLE_ACCESS_HASH, "invalid at_hash");
    for ( const unsigned char* p = (const unsigned char*)access; *p; p++ )
        if ( *p > 0x7fu ) return oidcExampleFail(OIDC_EXAMPLE_ACCESS_HASH, "access token is not ASCII");
    if ( !xrtSha256(access, strlen(access), digest) ||
         !xrtBase64Encode(digest, 16u, encoded, sizeof(encoded), &length, &base64) ) return false;
    return (length == supplied.Size && xrtConstTimeEqual(encoded, supplied.Data, length)) ||
        oidcExampleFail(OIDC_EXAMPLE_ACCESS_HASH, "at_hash does not bind the access token");
}

/* Original is borrowed, immutable claims returned by this function at login.
 * Keep that initial object across all refreshes. A refresh may omit id_token;
 * the caller then keeps the initial identity and checks refreshed userinfo sub.
 * This example uses integer NumericDate, signed code flow, and no max_age/acr. */
static inline xvalue* oidcExampleVerifyIdToken(xoauth2client* oauth,
    const xoauth2token* token, const xjwtjwks* keys, const oidcExamplePolicy* policy,
    const xvalue* original)
{
    if ( oauth == NULL || token == NULL || keys == NULL || policy == NULL ||
         token->IdToken == NULL || token->AccessToken == NULL || token->AccessToken[0] == 0 ||
         oauth->Config.Issuer == NULL || strncmp(oauth->Config.Issuer, "https://", 8) != 0 ||
         oauth->Config.Issuer[8] == 0 || oauth->Config.Issuer[8] == '/' ||
         strchr(oauth->Config.Issuer, '?') || strchr(oauth->Config.Issuer, '#') ||
         oauth->Config.ClientId == NULL || oauth->Config.ClientId[0] == 0 ||
         policy->ClockLeeway < 0 || policy->MaxTokenAge < 0 || policy->TrustedAudienceCount > 16u ||
         (policy->TrustedAudienceCount != 0 && policy->TrustedAudiences == NULL) ||
         (policy->SigningAlg != XJWT_ALG_RS256 && policy->SigningAlg != XJWT_ALG_ES256) ) {
        oidcExampleFail(OIDC_EXAMPLE_ARGUMENT, "invalid ID token application policy"); return NULL;
    }
    if ( !oidcExampleRequireBearer(token) ) return NULL;
    for ( size_t i = 0; i < policy->TrustedAudienceCount; i++ )
        if ( policy->TrustedAudiences[i] == NULL || policy->TrustedAudiences[i][0] == 0 ) {
            oidcExampleFail(OIDC_EXAMPLE_ARGUMENT, "empty trusted audience"); return NULL;
        }
    int algorithm = XJWT_ALG_INVALID;
    xvalue* header = xjwtDecodeHeader(token->IdToken, &algorithm, NULL);
    if ( header == NULL ) return NULL;
    xrtValueRelease(header);
    if ( algorithm != policy->SigningAlg ) {
        oidcExampleFail(OIDC_EXAMPLE_ALGORITHM, "ID token algorithm differs from registration"); return NULL;
    }
    int64_t now = policy->NowOverride != 0 ? policy->NowOverride : (int64_t)xrtTimeUnix(xrtNow());
    xjwtcheck check; xjwtCheckInit(&check);
    check.Issuer = oauth->Config.Issuer; check.Audience = oauth->Config.ClientId;
    check.NowOverride = now; check.ClockLeeway = policy->ClockLeeway;
    xvalue* claims = xjwtVerifyJwks(token->IdToken, keys, &check);
    if ( claims == NULL ) return NULL;
    xstrview subject, nonce;
    int64 exp, iat, auth_time = 0;
    if ( !oidcExampleInteger(xrtValueObjectGet(claims, xrtStrView("exp")), &exp) ||
         !oidcExampleInteger(xrtValueObjectGet(claims, xrtStrView("iat")), &iat) ||
         !oidcExampleString(xrtValueObjectGet(claims, xrtStrView("sub")), &subject) || subject.Size > 255u ) {
        oidcExampleFail(OIDC_EXAMPLE_CLAIMS, "missing or invalid required ID token claims"); goto failed;
    }
    for ( size_t i = 0; i < subject.Size; i++ )
        if ( ((const unsigned char*)subject.Data)[i] > 0x7fu ) {
            oidcExampleFail(OIDC_EXAMPLE_CLAIMS, "ID token subject is not ASCII"); goto failed;
        }
    if ( iat > exp || (iat > now && (uint64_t)iat - (uint64_t)now > (uint64_t)policy->ClockLeeway) ||
         (policy->MaxTokenAge != 0 && now > iat && (uint64_t)now - (uint64_t)iat >
            (uint64_t)policy->MaxTokenAge + (uint64_t)policy->ClockLeeway) ) {
        oidcExampleFail(OIDC_EXAMPLE_TIME, "ID token issued-at time is outside the application window"); goto failed;
    }
    const xvalue* audience = xrtValueObjectGet(claims, xrtStrView("aud"));
    const xvalue* party = xrtValueObjectGet(claims, xrtStrView("azp"));
    xstrview text;
    if ( !oidcExampleAudienceValid(audience, oauth->Config.ClientId, policy) ||
         (party != NULL && (!oidcExampleString(party, &text) || !oidcExampleTextEquals(text, oauth->Config.ClientId))) ) {
        oidcExampleFail(OIDC_EXAMPLE_AUDIENCE, "untrusted audience or authorized party"); goto failed;
    }
    const xvalue* authentication = xrtValueObjectGet(claims, xrtStrView("auth_time"));
    if ( authentication != NULL && (!oidcExampleInteger(authentication, &auth_time) ||
         (auth_time > iat && (uint64_t)auth_time - (uint64_t)iat > (uint64_t)policy->ClockLeeway)) ) {
        oidcExampleFail(OIDC_EXAMPLE_TIME, "invalid authentication time"); goto failed;
    }
    if ( !oidcExampleAccessHash(claims, token->AccessToken) ) goto failed;
    const xvalue* nonce_value = xrtValueObjectGet(claims, xrtStrView("nonce"));
    if ( original != NULL ) {
        const xvalue* old_auth = xrtValueObjectGet(original, xrtStrView("auth_time"));
        int64 previous_auth, previous_iat;
        if ( !oidcExampleSameText(xrtValueObjectGet(original, xrtStrView("iss")), xrtValueObjectGet(claims, xrtStrView("iss"))) ||
             !oidcExampleSameText(xrtValueObjectGet(original, xrtStrView("sub")), xrtValueObjectGet(claims, xrtStrView("sub"))) ||
             !oidcExampleSameAudience(xrtValueObjectGet(original, xrtStrView("aud")), audience) ||
             !oidcExampleInteger(xrtValueObjectGet(original, xrtStrView("iat")), &previous_iat) ||
             (previous_iat > iat && (uint64_t)previous_iat - (uint64_t)iat > (uint64_t)policy->ClockLeeway) ||
             (old_auth != NULL && authentication != NULL && (!oidcExampleInteger(old_auth, &previous_auth) || auth_time != previous_auth)) ||
             (nonce_value != NULL && !oidcExampleSameText(nonce_value, xrtValueObjectGet(original, xrtStrView("nonce")))) ) {
            oidcExampleFail(OIDC_EXAMPLE_REFRESH_IDENTITY, "refresh ID token changed the initial identity"); goto failed;
        }
    } else {
        if ( !oidcExampleString(nonce_value, &nonce) || !oidcExampleTextEquals(nonce, oauth->sNonce) ) {
            oidcExampleFail(OIDC_EXAMPLE_CLAIMS, "missing or invalid login nonce"); goto failed;
        }
        if ( !xoauth2NonceConsume(oauth, oauth->sNonce) ) goto failed;
    }
    return claims;
failed:
    xrtValueRelease(claims);
    return NULL;
}
#endif
