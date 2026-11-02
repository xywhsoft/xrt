/* Real RSA proofs with an in-memory peer. Selection-only tests substitute a
 * parsed profile vector explicitly; independent OpenSSL tests verify real CAs. */
#include "../test.h"
#include "../../src/internal/xrt_tls_client_auth.h"
#include "../fixtures/x509_profile_vectors.h"

static const xtlsclientauthconfigv1* TestAuthConfig;
static bool TestAuthReleaseCaller;
static xtlssession* testAuthCreate(const xtlsclientconfig* config, xnetbufpool* pool)
{
    if (!TestAuthConfig) return xrtTlsClientCreate(config, pool);
    xtlssession* session = xrtTlsClientCreateAuthV1(config, TestAuthConfig, pool);
    if (TestAuthReleaseCaller) {
        xrtTlsIdentityRelease((xtlsidentity*)TestAuthConfig->Identity);
        TestAuthReleaseCaller = false;
    }
    return session;
}
#define xrtTlsClientCreate testAuthCreate
#define main testTlsClientAuthLegacyRegression
#include "test_tls_client_server_hello.c"
#undef main
#undef xrtTlsClientCreate

typedef struct auth_signer {
    xtlsidentity* Key;
    unsigned Supports, Queries, Signs, Drops, Mode;
    const xerror* Primary;
} auth_signer;

static void testAuthCheckedError(auth_signer* state)
{
    xerrordesc error = {0}; error.Kind = XERR_CANCELLED; error.Code = 172901;
    error.Domain = "test.client-signer"; error.Message = "checked client signing failure";
    xrtSetErrorTake(xrtErrorBuild(&error)); state->Primary = xrtGetError();
    testRequire(state->Primary != NULL, "checked signer diagnostic");
}
static bool testAuthSupports(ptr context, xtlsversion version, xtlssignature scheme)
{
    auth_signer* state = context; state->Supports++;
    if (state->Mode == 1u) { testAuthCheckedError(state); return true; }
    return xrtTlsIdentityCanSign(state->Key, version, scheme);
}
static bool testAuthSign(ptr context, xtlsversion version, xtlssignature scheme,
    xbytesview message, void* output, size_t capacity, size_t* size)
{
    auth_signer* state = context;
    if (output) state->Signs++; else state->Queries++;
    if ((!output && state->Mode == 4u) || (output && state->Mode == 2u)) {
        testAuthCheckedError(state); return false;
    }
    bool ok = xrtTlsIdentitySign(state->Key, version, scheme, message, output, capacity, size);
    if (ok && output && state->Mode == 3u) testAuthCheckedError(state);
    return ok;
}
static void testAuthDrop(ptr context)
{
    auth_signer* state = context; state->Drops++;
    xrtTlsIdentityRelease(state->Key); state->Key = NULL;
}
static xtlsidentity* testAuthIdentity(auth_signer* state)
{
    state->Key = testTlsClientIdentity();
    xbytesview certificate = {X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)};
    xtlsidentityconfig config = {0}; config.Certificates = &certificate; config.CertificateCount = 1u;
    config.Type = XTLS_IDENTITY_RSA; config.Supports = testAuthSupports;
    config.Sign = testAuthSign; config.Release = testAuthDrop; config.Context = state;
    xtlsidentity* result = xrtTlsIdentityCreate(&config);
    testRequire(result != NULL, "client forwarding signer creation"); return result;
}
static void testAuthBalanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount, "client auth exact allocation rollback");
}
static bool testAuthVisit(xrtownershipref child, ptr context)
{
    unsigned* visits = context; testRequire(child.Data && child.Ops, "real auth ownership edge");
    (*visits)++; return true;
}

static size_t testAuthRequest(xbytesview authorities, xbytesview filters, uint16 scheme,
    void* output, size_t capacity)
{
    uint8 extensions[4096], body[4100]; xtlswriter writer;
    uint16 certificates[] = {XTLS_SIGNATURE_RSA_PKCS1_SHA256, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256};
    testRequire(xrtTlsWriterInit(&writer, extensions, sizeof(extensions)) &&
        xrtTlsWriterIds(&writer, XTLS_EXTENSION_SIGNATURE_ALGORITHMS, &scheme, 1u) &&
        xrtTlsWriterIds(&writer, XTLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT, certificates, 2u), "auth request algorithms");
    if (authorities.Size) testRequire(xrtTlsWriterExtension(&writer, XTLS_EXTENSION_CERTIFICATE_AUTHORITIES, authorities), "auth CA hints");
    if (filters.Size) testRequire(xrtTlsWriterExtension(&writer, XTLS_EXTENSION_OID_FILTERS, filters), "auth OID filters");
    xbytesview data = xrtTlsWriterData(&writer), empty = {NULL, 0};
    size_t size = xrtTls13CertificateRequestSize(empty, data);
    testRequire(size && size <= sizeof(body) && xrtTls13CertificateRequestEncode(empty, data, body, sizeof(body)) &&
        xrtTlsHandshakeSize(size) <= capacity && xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CERTIFICATE_REQUEST,
            (xbytesview){body, size}, output, capacity), "auth request encoding");
    return xrtTlsHandshakeSize(size);
}

/* Verify every outgoing message independently of the session's committed
 * transcript, including the actual client proof and both Finished hashes. */
static void testAuthProof(test_tls_client_server_hello* fixture, xtlssession* server,
    xtlstranscript transcript, xbytesview server_finished, xbytesview client_traffic)
{
    testRequire(__xrtTlsTranscriptUpdate(&transcript, server_finished), "auth peer server Finished transcript");
    uint8 wire[4096], expected[64], digest[32]; size_t wire_size = 0;
    while (xrtTlsSessionSendSize(fixture->Client)) {
        xnetspan span; testRequire(xrtTlsSessionSendFront(fixture->Client, &span) &&
            span.Size <= sizeof(wire) - wire_size, "auth peer outgoing flight size");
        memcpy(wire + wire_size, span.Data, span.Size); wire_size += span.Size;
        testRequire(xrtTlsSessionSendConsume(fixture->Client, span.Size), "auth peer output consume");
    }
    testRequire(xrtTlsSessionFeed(server, wire, wire_size) == XTLS_OK, "auth peer flight feed");
    xtlssessionrecord record; testRequire(__xrtTlsSessionRecordNext(server, &record) == XTLS_OK &&
        record.Type == XTLS_RECORD_HANDSHAKE, "auth peer protected flight");
    size_t offset = 0; unsigned messages = 0;
    while (offset < record.Data.Size) {
        xtlshandshake message; xbytesview encoded = {record.Data.Data + offset, record.Data.Size - offset};
        testRequire(xrtTlsHandshakeParse(encoded, &message, NULL) == XTLS_OK, "auth peer handshake parse");
        encoded.Size = message.EncodedSize;
        if (!messages) {
            xtlscertificatemessage certificate; xtlscertificatecursor cursor; xtlscertificateentry entry;
            testRequire(message.Type == XTLS_HANDSHAKE_CERTIFICATE &&
                xrtTlsCertificateParse(XTLS_VERSION_13, message.Body, &certificate) &&
                xrtTlsCertificateEntries(&certificate, &cursor) && xrtTlsCertificatesRead(&cursor, &entry) == XTLS_ITEM_VALUE &&
                entry.Data.Size == sizeof(X509_LEGACY_RSA_CERT) &&
                !memcmp(entry.Data.Data, X509_LEGACY_RSA_CERT, entry.Data.Size) &&
                xrtTlsCertificatesRead(&cursor, &entry) == XTLS_ITEM_DONE, "auth peer exact certificate chain");
        } else if (messages == 1u) {
            xtlscertificateverify verify; xx509pubkey key;
            testRequire(message.Type == XTLS_HANDSHAKE_CERTIFICATE_VERIFY &&
                xrtTlsCertificateVerifyParse(message.Body, &verify) &&
                xrtTlsIdentityPublicKey(fixture->State->Auth->Identity, &key) &&
                __xrtTlsTranscriptDigest(&transcript, digest, sizeof(digest)) &&
                xrtTls13CertificateVerifySignature(XTLS_CLIENT, (xtlssignature)verify.Scheme,
                    (xbytesview){digest, sizeof(digest)}, verify.Signature, &key), "auth peer verifies actual RSA client proof");
        } else {
            size_t size = testTlsClientFinishedMessage(&transcript, client_traffic, expected, sizeof(expected));
            testRequire(messages == 2u && encoded.Size == size && !memcmp(encoded.Data, expected, size),
                "auth peer client Finished includes Certificate and proof");
        }
        testRequire(__xrtTlsTranscriptUpdate(&transcript, encoded), "auth peer message transcript");
        offset += encoded.Size; messages++;
    }
    testRequire(messages == 3u && __xrtTlsTranscriptDigest(&transcript, digest, sizeof(digest)) &&
        __xrtTlsTranscriptDigest(&fixture->State->Transcript, expected, sizeof(digest)) &&
        !memcmp(digest, expected, sizeof(digest)), "auth peer final transcript agreement");
    testRequire(__xrtTlsSessionRecordFinish(server, false) == XTLS_OK, "auth peer record consume");
    __xrtTlsTranscriptClear(&transcript);
}

/* Modes: normal, supports failure, false/checked signing, query failure,
 * output backpressure, bad server Finished, absent request. */
static void testAuthFlight(unsigned mode, size_t prefix, bool fault, bool stale)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    auth_signer signer = {0}; signer.Mode = mode < 5u ? mode : 0u;
    xtlsclientauthconfigv1 config; xrtTlsClientAuthConfigInitV1(&config);
    config.Identity = testAuthIdentity(&signer); TestAuthConfig = &config; TestAuthReleaseCaller = true;
    uint8 request[128], finished[64], digest_before[32], digest_after[32], client_traffic[32];
    size_t request_size = testAuthRequest((xbytesview){0}, (xbytesview){0}, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256, request, sizeof(request));
    test_tls_client_server_hello fixture;
    xtlssession* server;
    if (mode == 1u) {
        uint8 extensions[256]; size_t size;
        server = testTlsClientCertificateBegin(&fixture, true, extensions, sizeof(extensions), &size);
        testRequire(testTlsClientServerProtect(&fixture, server, request, request_size) == XTLS_ERROR &&
            signer.Signs == 0u && signer.Queries == 0u && xrtGetError() == signer.Primary,
            "checked Supports failure stops certificate selection");
        goto cleanup;
    }
    server = testTlsClientFinishedBeginRequest(&fixture, TEST_TLS_ENCRYPTED_EXTENSIONS_NO_ALPN,
        mode == 7u ? NULL : request, mode == 7u ? 0u : request_size);
    TestAuthConfig = NULL;
    testRequire(!signer.Drops && !signer.Signs && !xrtTlsClientCertificateQueuedV1(fixture.Client),
        "session retains identity without signing before server Finished");
    testRequire(!fixture.State->Auth->Transcript && !fixture.State->Auth->TranscriptCapacity,
        "TLS 1.3 never allocates the TLS 1.2 raw signing transcript");
    xtlstranscript transcript = fixture.State->Transcript;
    memcpy(client_traffic, fixture.State->ClientHandshakeTraffic, sizeof(client_traffic));
    size_t size = testTlsClientFinishedMessage(&transcript,
        (xbytesview){fixture.State->ServerHandshakeTraffic, fixture.State->HashSize}, finished, sizeof(finished));
    if (mode == 6u) finished[size - 1u] ^= 1u;
    testRequire(__xrtTlsTranscriptDigest(&transcript, digest_before, sizeof(digest_before)), "auth before digest");
    const xerror* previous = NULL;
    if (stale) { xrtSetErrorKind(XERR_STATE); previous = xrtGetError(); testRequire(previous != NULL, "auth stale error"); }
    size_t fill = 0;
    if (mode == 5u) {
        size_t record_size;
        testRequire(__xrtTlsClientAuthRecordsSize13(&fixture.Client->WriteKey,
            fixture.State->Auth->Certificate13 + 8u + fixture.State->Auth->SignatureCapacity + size, &record_size), "auth preflight size");
        fill = xrtTlsContextLimits(fixture.Context)->SendLimit - record_size + 1u;
        bytes data = xrtMalloc(fill); testRequire(data != NULL, "auth output fill allocation"); memset(data, 0xA5, fill);
        testRequire(__xrtTlsSessionSendTake(fixture.Client, data, fill) == XTLS_OK, "auth output fill transfer");
    }
    testRequire(__xrtTlsSessionRecordProtect(server, XTLS_RECORD_HANDSHAKE, (xbytesview){finished, size}, 0) == XTLS_OK,
        "auth server Finished protection");
    xnetspan span;
    while (xrtTlsSessionSendSize(server)) {
        testRequire(xrtTlsSessionSendFront(server, &span) && xrtTlsSessionFeed(fixture.Client, span.Data, span.Size) == XTLS_OK &&
            xrtTlsSessionSendConsume(server, span.Size), "auth Finished feed before OOM");
    }
    if (fault) testRequire(xrtMemDebugFailAfter(prefix), "auth flight OOM arm");
    xtlsresult result = xrtTlsClientDrive(fixture.Client);
    bool hit = fault && xrtMemDebugFailTriggered(); if (fault) xrtMemDebugFailClear();
    if (mode == 5u) {
        testRequire(result == XTLS_AGAIN && !signer.Signs && !signer.Queries &&
            !xrtTlsClientCertificateQueuedV1(fixture.Client) && fixture.State->Step == XTLS_CLIENT_WAIT_FINISHED &&
            __xrtTlsTranscriptDigest(&fixture.State->Transcript, digest_after, sizeof(digest_after)) &&
            !memcmp(digest_before, digest_after, sizeof(digest_before)) && xrtTlsSessionSendConsume(fixture.Client, fill),
            "auth backpressure performs no signer call or transcript commit");
        result = xrtTlsClientDrive(fixture.Client);
    }
    if (hit || mode == 2u || mode == 3u || mode == 4u || mode == 6u || mode == 7u) {
        testRequire(result == XTLS_ERROR && xrtTlsSessionState(fixture.Client) == XTLS_STATE_FAILED &&
            !xrtTlsClientCertificateQueuedV1(fixture.Client) && signer.Signs <= 1u &&
            __xrtTlsTranscriptDigest(&fixture.State->Transcript, digest_after, sizeof(digest_after)) &&
            !memcmp(digest_before, digest_after, sizeof(digest_before)), "auth failed flight never commits epoch or transcript");
        if (mode >= 6u) testRequire(!signer.Signs && !signer.Queries, "unverified or absent request never invokes signer");
        if (mode == 2u || mode == 3u || mode == 4u)
            testRequire(xrtErrorIs(xrtGetError(), XERR_CANCELLED) != NULL, "auth signer failure cause survives TLS failure");
        if (mode == 4u) testRequire(signer.Queries == 1u && !signer.Signs, "failed size query does not sign");
        unsigned calls = signer.Signs;
        testRequire(xrtTlsClientDrive(fixture.Client) == XTLS_ERROR && signer.Signs == calls,
            "terminal auth failure cannot repeat a private-key operation");
    } else {
        testRequire(result == XTLS_OK && xrtTlsSessionState(fixture.Client) == XTLS_STATE_READY &&
            signer.Signs == 1u && signer.Queries == 1u && xrtTlsClientCertificateQueuedV1(fixture.Client), "auth flight signs exactly once");
        if (stale) testRequire(xrtGetError() == previous, "successful signer preserves the old ambient error");
        xrtClearError(); testAuthProof(&fixture, server, transcript, (xbytesview){finished, size},
            (xbytesview){client_traffic, sizeof(client_traffic)});
        result = xrtTlsClientDrive(fixture.Client);
        testRequire((result == XTLS_OK || result == XTLS_AGAIN) && signer.Signs == 1u, "auth ready drive never re-signs");
    }
cleanup:
    TestAuthConfig = NULL; TestAuthReleaseCaller = false;
    xrtSecureZero(client_traffic, sizeof(client_traffic));
    xrtClearError(); xrtTlsSessionDestroy(server); testTlsClientServerFixtureUnit(&fixture);
    testRequire(signer.Drops == 1u && !signer.Key, "client auth releases retained signer once");
    testAuthBalanced(before);
}

static void testAuthSelection(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    auth_signer signer = {0}; xtlsidentity* identity = testAuthIdentity(&signer);
    xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity; auth.RequireCertificate = false;
    xtlsclientconfig config; xrtTlsClientConfigInit(&config); config.Context = testTlsClientServerContext();
    config.Verifier = testTlsClientVerifier(); config.ServerName = XRT_STR_LITERAL("example.com");
    xtlssession* session = xrtTlsClientCreateAuthV1(&config, &auth, NULL); testRequire(session != NULL, "selection session");
    xrtTlsIdentityRelease(identity); xrtTlsContextRelease((xtlscontext*)config.Context);
    xrtTlsVerifierRelease((xtlsverifier*)config.Verifier);
    xtlsclientstate* state = __xrtTlsSessionRoleData(session); state->Version = XTLS_VERSION_13;
    uint8 request[4096], authorities[1024], filters[256];
    xx509cert profile; testRequire(xrtX509Parse(X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &profile), "parsed profile-only vector");
    xx509cert original = state->Auth->Leaf;
    for (unsigned mode = 0; mode < 15u; mode++) {
        state->Auth->Leaf = original; xbytesview ca = {0}, oid = {0}; uint16 scheme = XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;
        bool selected = true, valid = true;
        if (mode == 1u || mode == 2u || mode == 3u) {
            xbytesview name = mode == 2u ? profile.Subject : original.Issuer;
            if (mode == 3u) { static const uint8 bad[] = {0x30, 1u, 0}; name = (xbytesview){bad, sizeof(bad)}; valid = false; }
            size_t size = xrtTlsAuthoritiesSize(&name, 1u);
            testRequire(size <= sizeof(authorities) && xrtTlsAuthoritiesEncode(&name, 1u, authorities, sizeof(authorities)), "selection CA encoding");
            ca = (xbytesview){authorities, size}; if (mode == 2u) selected = false;
        }
        if (mode == 4u) { scheme = XTLS_SIGNATURE_ED25519; selected = false; }
        if (mode >= 5u) {
            /* oid_filters: full DER OID, then the extension's DER value. */
            static const uint8 ku[] = {0, 12u, 5u, 6u, 3u, 0x55u, 0x1Du, 15u, 0, 4u, 3u, 2u, 7u, 0x80u};
            static const uint8 eku[] = {0, 20u, 5u, 6u, 3u, 0x55u, 0x1Du, 37u, 0, 12u, 0x30u, 10u, 6u, 8u, 0x2Bu, 6u, 1u, 5u, 5u, 7u, 3u, 2u};
            const uint8* data = mode >= 8u && mode <= 10u ? eku : ku;
            size_t size = data == eku ? sizeof(eku) : sizeof(ku); memcpy(filters, data, size);
            if (mode != 5u) state->Auth->Leaf.Extensions = profile.Extensions;
            if (mode == 5u) selected = false; /* No KeyUsage extension in the real legacy leaf. */
            if (mode == 7u) { filters[size - 2u] = 6u; filters[size - 1u] = 0x40u; selected = false; }
            if (mode == 9u) { filters[size - 1u] = 99u; selected = false; }
            if (mode == 10u) { /* Forbidden anyExtendedKeyUsage. */
                static const uint8 any[] = {0, 16u, 5u, 6u, 3u, 0x55u, 0x1Du, 37u, 0, 8u, 0x30u, 6u, 6u, 4u, 0x55u, 0x1Du, 37u, 0};
                memcpy(filters, any, sizeof(any)); size = sizeof(any); valid = false;
            }
            if (mode == 11u) { filters[7] = 99u; } /* Unknown OID is ignored. */
            if (mode == 12u) { /* Duplicate OID is rejected even when both match. */
                memcpy(filters + size, filters + 2u, size - 2u); size += size - 2u; filters[1] = (uint8)(size - 2u); valid = false;
            }
            if (mode == 13u) { filters[3] = 5u; valid = false; } /* Not an OID DER tag. */
            if (mode == 14u) { filters[1]--; valid = false; }
            oid = (xbytesview){filters, size};
        }
        size_t size = testAuthRequest(ca, oid, scheme, request, sizeof(request));
        xtlshandshake message; xtls13certificaterequest parsed;
        testRequire(xrtTlsHandshakeParse((xbytesview){request, size}, &message, NULL) == XTLS_OK &&
            xrtTls13CertificateRequestParse(message.Body, &parsed), "selection request framing");
        bool result = __xrtTlsClientAuthSelect13(state, &parsed);
        if (result != valid || (valid && state->Auth->Selected != selected)) {
            fprintf(stderr, "selection mode=%u result=%d selected=%d expected=%d/%d\n", mode, result, state->Auth->Selected, valid, selected);
            testRequire(false, "CA/name/scheme/OID selection result");
        }
        testRequire(!signer.Signs && !signer.Queries, "selection never signs"); xrtClearError();
    }
    state->Auth->Leaf = original;
    unsigned visits = 0; xrtownershipscope freeze = {0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze) && !xrtTlsSessionOwnershipTrace(session, testAuthVisit, &visits) &&
        !xrtGetError() && xrtOwnershipScopeEnd(&freeze), "opaque client signer is not a false ownership leaf");
    xrtTlsSessionDestroy(session); testRequire(signer.Drops == 1u, "selection releases identity"); testAuthBalanced(before);
}

static void testAuthFactory(void)
{
    auth_signer signer = {0}; xtlsidentity* identity = testAuthIdentity(&signer);
    xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity;
    xtlsclientconfig config; xrtTlsClientConfigInit(&config); config.Context = testTlsClientServerContext();
    config.Verifier = testTlsClientVerifier(); config.ServerName = XRT_STR_LITERAL("example.com");
    testRequire(auth.RequireCertificate && auth.Size == sizeof(auth) && !xrtTlsClientCertificateQueuedV1(NULL), "auth V1 defaults");
    for (unsigned mode = 0; mode < 8u; mode++) {
        xtlsclientauthconfigv1 bad = auth; xtlsclientconfig session_config = config;
        if (mode == 0u) bad.Size--;
        if (mode == 1u) bad.Identity = NULL;
        if (mode == 2u) bad.MaxCertificates = 0;
        if (mode == 3u) bad.MaxCertificates = XTLS_CLIENT_AUTH_CERTIFICATES_MAX + 1u;
        if (mode == 4u) bad.MaxCertificateBytes = sizeof(X509_LEGACY_RSA_CERT) - 1u;
        if (mode == 5u) bad.MaxTranscriptBytes = XTLS_CLIENT_AUTH_BYTES_MAX + 1u;
        if (mode == 6u) session_config.ResumeOnly = true;
        if (mode == 7u) { uint8 secret[32]; session_config.Resume = testTlsClientServerResume(secret); testRequire(session_config.Resume != NULL, "auth explicit resume fixture"); }
        testRequire(!xrtTlsClientCreateAuthV1(&session_config, &bad, NULL) && xrtGetError(), "auth invalid limits or unbound resumption rejected");
        if (mode == 7u) xrtTlsResumeRelease((xtlsresume*)session_config.Resume);
        xrtClearError();
    }
    xtlssession* valid = xrtTlsClientCreateAuthV1(&config, &auth, NULL);
    testRequire(valid != NULL, "valid auth factory baseline"); xrtTlsSessionDestroy(valid);
    unsigned failures = 0u, successes = 0u;
    for (size_t point = 0; point < 96u; point++) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); testRequire(xrtMemDebugFailAfter(point), "auth factory OOM arm");
        xtlssession* session = xrtTlsClientCreateAuthV1(&config, &auth, NULL);
        bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        testRequire(!hit || (!session && xrtErrorIs(xrtGetError(), XERR_MEMORY)), "auth factory OOM is failure atomic");
        if (hit) failures++; else { testRequire(session != NULL, "unfaulted auth factory succeeds"); successes++; }
        xrtClearError(); xrtTlsSessionDestroy(session); testRequire(!signer.Drops, "factory failure keeps caller identity"); testAuthBalanced(before);
    }
    testRequire(failures > 1u && successes > 0u, "factory OOM sweep reaches real allocations and success");
    xrtTlsContextRelease((xtlscontext*)config.Context); xrtTlsVerifierRelease((xtlsverifier*)config.Verifier);
    xrtTlsIdentityRelease(identity); testRequire(signer.Drops == 1u, "factory identity final release");
}

#if defined(TEST_TLS_AUTH_EMBEDDED)
int testTlsClientAuth13Standalone(void)
#else
int main(void)
#endif
{
    testRequire(xrtMemDebugEnable(true), "client auth memory instrumentation");
    testAuthFactory(); testAuthSelection();
    for (unsigned mode = 0; mode < 8u; mode++) testAuthFlight(mode, 0u, false, false);
    testAuthFlight(0u, 0u, false, true);
    for (size_t point = 0; point < 128u; point++) testAuthFlight(0u, point, true, false);
    puts("TLS client auth: real proof/transcript, CA/OID policy, checked failures, sign-once backpressure, 224 OOM prefixes balanced");
    return 0;
}
