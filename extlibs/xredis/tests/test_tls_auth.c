/* Native fixture cannot request client certificates. Optional mode exercises
 * lifetime/transport faults here; independent Redis requires real proofs. */
#define main xredisTlsAuthLegacyRegression
#include "test_tls.c"
#undef main

typedef struct redis_auth_signer {
    xtlsidentity* Key;
    xatomic32 Signs, Queries, Drops;
} redis_auth_signer;
static bool authSupports(ptr context, xtlsversion version, xtlssignature scheme)
{ return xrtTlsIdentityCanSign(((redis_auth_signer*)context)->Key, version, scheme); }
static bool authSign(ptr context, xtlsversion version, xtlssignature scheme,
    xbytesview message, void* output, size_t capacity, size_t* size)
{
    redis_auth_signer* state = context;
    xrtAtomic32FetchAdd(output ? &state->Signs : &state->Queries, 1u, XMEMORY_RELAXED);
    return xrtTlsIdentitySign(state->Key, version, scheme, message, output, capacity, size);
}
static void authDrop(ptr context)
{
    redis_auth_signer* state = context; xrtTlsIdentityRelease(state->Key); state->Key = NULL;
    xrtAtomic32Store(&state->Drops, 1u, XMEMORY_RELEASE);
}
static xtlsidentity* authIdentity(redistlspeer* peer, redis_auth_signer* state)
{
    state->Key = xrtTlsIdentityRetain(peer->Identity); xbytesview certificate;
    testRequire(state->Key && xrtTlsIdentityCertificate(state->Key, 0, &certificate), "auth fixture immutable certificate");
    xtlsidentityconfig config = {0}; config.Type = XTLS_IDENTITY_RSA;
    config.Certificates = &certificate; config.CertificateCount = 1;
    config.Supports = authSupports; config.Sign = authSign; config.Release = authDrop; config.Context = state;
    xtlsidentity* identity = xrtTlsIdentityCreate(&config); testRequire(identity != NULL, "auth fixture real forwarding signer"); return identity;
}
static void authReleased(redis_auth_signer* state)
{
    testRequire(xrtAtomic32Load(&state->Drops, XMEMORY_ACQUIRE) == 1u && !state->Key &&
        !xrtAtomic32Load(&state->Signs, XMEMORY_ACQUIRE) && !xrtAtomic32Load(&state->Queries, XMEMORY_ACQUIRE),
        "optional/no-request or failed pre-auth path releases once without signing");
}
static bool authConnect(redistlspeer* peer, unsigned mode, const xredisclientconfig* config,
    const xredisclientinitconfig* init, const xredisclienttlsconfig* tls,
    const xtlsclientauthconfigv1* auth, xredisclient** output)
{
    if (mode == 0u) return xredisClientTlsAuthDialV1(peer->Dial.Init.Peer.Engine, peer->Dial.Resolver,
        XRT_STR_LITERAL("redis.fixture"), peer->Dial.Init.Peer.Address.Port, 0, config, init, tls, auth, output);
    if (mode == 1u) return xredisClientTlsAuthConnectV1(peer->Dial.Init.Peer.Engine, &peer->Dial.Init.Peer.Address,
        0, config, init, tls, auth, output);
    xnetaddr values[] = {peer->Dial.RefusedAddress, peer->Dial.Init.Peer.Address};
    xnetaddrlist* addresses = xrtNetAddrListCreate(values, 2); testRequire(addresses != NULL, "auth prepared endpoints");
    bool result = xredisClientTlsAuthDialAddressesV1(peer->Dial.Init.Peer.Engine, addresses, 0, config, init, tls, auth, output);
    xrtNetAddrListDestroy(addresses); return result;
}
static void authBasic(void)
{
    for (unsigned mode = 0; mode < 3u; mode++) {
        redistlspeer peer; redisTlsPeerStart(&peer, false, false, mode == 0u ? REDIS_DIAL_BLOCK : 0);
        redis_auth_signer state = {0}; xtlsidentity* identity = authIdentity(&peer, &state);
        xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity; auth.RequireCertificate = false;
        xredisclienttlsconfig tls; redisTlsPeerConfig(&peer, &tls); tls.WriteBytes = 3;
        char name[] = "redis.fixture", protocol[] = "r\0p";
        xstrview alpn = {protocol, sizeof(protocol) - 1u}; tls.Session.VerifyName = (xstrview){name, sizeof(name) - 1u};
        tls.Session.Protocols = &alpn; tls.Session.ProtocolCount = 1;
        xredisclientconfig config; xredisClientConfigInit(&config); config.ReadBytes = 2; config.ReadFrames = 1; config.WorkRequests = 1;
        if (mode == 0u) xrtSetErrorKind(XERR_STATE);
        const xerror* previous = xrtGetError(); xredisclient* client = NULL;
        testRequire(authConnect(&peer, mode, &config, NULL, &tls, &auth, &client), "auth factory admission");
        testRequire(xrtGetError() == previous, "auth factory isolates old thread errors"); xrtClearError();
        xrtTlsIdentityRelease(identity); memset(&auth, 0xA5, sizeof(auth)); memset(&tls, 0xA5, sizeof(tls));
        memset(name, 0xA5, sizeof(name)); memset(protocol, 0xA5, sizeof(protocol));
        if (!mode) { redisPeerWait(&peer.Dial.Entered, 1); testRequire(!xrtAtomic32Load(&state.Drops, XMEMORY_ACQUIRE), "identity retained before blocked DNS returns"); }
        xrtAtomic32Store(&peer.Dial.Gate, 1u, XMEMORY_RELEASE);
        xfuture* ready = xredisClientReady(client), *pipeline[32];
        for (size_t index = 0; index < 32; index++) {
            xbytesview args[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("auth\0\r\n")};
            testRequire(xredisClientCallAsync(client, args, 2, 5000, &pipeline[index]) == XREDIS_ADMISSION_ACCEPTED, "auth binary pipeline admission");
        }
        testRequire(tlsReady(ready)->Status == XREDIS_READY_OK, "auth optional full TLS/HELLO readiness");
        for (size_t index = 0; index < 32; index++) {
            const xredisrequestresult* result = tlsRequest(pipeline[index]); xredisreplyvalue value;
            testRequire(result->Status == XREDIS_REQUEST_OK && xredisReplyGet(result->Reply, 0, &value) && value.Data.Size == 7u &&
                !memcmp(value.Data.Data, "auth\0\r\n", 7), "auth exact partial-write pipeline correlation");
        }
        redisDialClientClose(client); redisTlsPeerStop(&peer); authReleased(&state);
        xrtFutureDestroy(ready); for (size_t index = 0; index < 32; index++) xrtFutureDestroy(pipeline[index]);
    }
}
static void authFailures(void)
{
    for (unsigned mode = 0; mode < 6u; mode++) {
        redistlspeer peer; redisTlsPeerStart(&peer, mode == 2u, mode == 0u, mode >= 3u ? REDIS_DIAL_BLOCK : 0);
        redis_auth_signer state = {0}; xtlsidentity* identity = authIdentity(&peer, &state);
        xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity; auth.RequireCertificate = mode == 1u;
        xredisclienttlsconfig tls; redisTlsPeerConfig(&peer, &tls);
        xredisclientinitconfig init; xredisClientInitConfigInit(&init); if (mode == 2u || mode == 4u) init.Timeout = 30;
        xredisclient* client = NULL; testRequire(authConnect(&peer, 0u, NULL, &init, &tls, &auth, &client), "auth failing setup admitted");
        xrtTlsIdentityRelease(identity); memset(&auth, 0, sizeof(auth)); memset(&tls, 0, sizeof(tls));
        xfuture* ready = xredisClientReady(client), *request = NULL, *closed = NULL; xbytesview ping = XRT_BYTES_LITERAL("PING");
        testRequire(xredisClientCallAsync(client, &ping, 1, 5000, &request) == XREDIS_ADMISSION_ACCEPTED, "auth pre-ready request");
        if (mode >= 3u) {
            redisPeerWait(&peer.Dial.Entered, 1);
            if (mode != 4u) { closed = xredisClientClosed(client); testRequire(xredisClientClose(client), "auth close blocked DNS"); }
            if (mode == 5u) { xredisClientRelease(client); client = NULL; }
        }
        xredisreadystatus expected = mode <= 1u ? XREDIS_READY_DISCONNECTED : mode == 2u || mode == 4u ? XREDIS_READY_TIMEOUT : XREDIS_READY_CLOSED;
        const xredisreadyresult* result = tlsReady(ready);
        testRequire(result->Status == expected && !result->Hello && tlsRequest(request)->Delivery == XREDIS_NOT_SENT &&
            !xrtAtomic32Load(&peer.Dial.Init.Peer.Commands, XMEMORY_ACQUIRE), "auth failed preflight sends no Redis command and publishes no readiness");
        if (client) redisDialClientClose(client); else testRequire(xrtFutureWaitFor(closed, 10000) == XWAIT_OK, "auth runtime reference close barrier");
        if (mode >= 3u) testRequire(!xrtAtomic32Load(&peer.Dial.Exited, XMEMORY_ACQUIRE), "auth closed without fabricating resolver quiescence");
        redisTlsPeerStop(&peer); authReleased(&state); xrtFutureDestroy(closed); xrtFutureDestroy(ready); xrtFutureDestroy(request);
    }
}
static void authInvalid(void)
{
    redistlspeer peer; redisTlsPeerStart(&peer, false, false, 0);
    redis_auth_signer state = {0}; xtlsidentity* identity = authIdentity(&peer, &state);
    xtlsclientauthconfigv1 defaults; xrtTlsClientAuthConfigInitV1(&defaults); defaults.Identity = identity;
    xredisclienttlsconfig tls; redisTlsPeerConfig(&peer, &tls);
    for (unsigned mode = 0; mode < 9u; mode++) {
        xtlsclientauthconfigv1 auth = defaults; xredisclienttlsconfig config = tls; const xtlsclientauthconfigv1* input = &auth;
        if (mode == 0u) input = NULL;
        if (mode == 1u) auth.Size--;
        if (mode == 2u) auth.Identity = NULL;
        if (mode == 3u) auth.MaxCertificates = 0;
        if (mode == 4u) auth.MaxCertificateBytes = 1;
        if (mode == 5u) auth.MaxTranscriptBytes = 0;
        if (mode == 6u) config.Session.ResumeOnly = true;
        if (mode == 7u) input = (const xtlsclientauthconfigv1*)(uintptr_t)(UINTPTR_MAX - 1u);
        if (mode == 8u) auth.MaxCertificates = XTLS_CLIENT_AUTH_CERTIFICATES_MAX + 1u;
        xredisclient* client = (xredisclient*)(uintptr_t)1;
        testRequire(!authConnect(&peer, 0u, NULL, NULL, &config, input, &client) && client == (xredisclient*)(uintptr_t)1,
            "auth invalid config or resume admitted/changed output"); xrtClearError();
    }
    xtlsclientauthconfigv1 snapshot = defaults;
    testRequire(!authConnect(&peer, 0u, NULL, NULL, &tls, &defaults, (xredisclient**)&defaults.Identity) &&
        !memcmp(&snapshot, &defaults, sizeof(defaults)), "auth descriptor/output overlap rejected atomically"); xrtClearError();
    testRequire(!xredisClientTlsAuthDialAddressesV1(peer.Dial.Init.Peer.Engine, NULL, 0, NULL, NULL, &tls, &defaults, NULL), "auth NULL endpoint list"); xrtClearError();
    testRequire(!xredisClientTlsAuthConnectV1(peer.Dial.Init.Peer.Engine, NULL, 0, NULL, NULL, &tls, &defaults, NULL), "auth NULL numeric endpoint"); xrtClearError();
    xrtTlsIdentityRelease(identity); redisTlsPeerStop(&peer); authReleased(&state);
}
static void authAllocations(void)
{
    unsigned failed = 0, created = 0;
    for (size_t limit = 1; limit < 160000; limit += 4001) {
        redistlspeer peer; redisTlsPeerStart(&peer, false, false, 0);
        redis_auth_signer state = {0}; xtlsidentity* identity = authIdentity(&peer, &state);
        xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity; auth.RequireCertificate = false;
        xredisclienttlsconfig tls; redisTlsPeerConfig(&peer, &tls);
        xallocationbudget* budget = NULL; xallocationscope scope = {0};
        testRequire(xrtAllocationBudgetCreate(limit, &budget) && xrtAllocationBudgetEnter(budget, &scope), "auth factory budget");
        xredisclient* client = (xredisclient*)(uintptr_t)1;
        bool success = authConnect(&peer, 0u, NULL, NULL, &tls, &auth, &client);
        if (success) created++; else { failed++; testRequire(client == (xredisclient*)(uintptr_t)1, "auth OOM changed output"); }
        xrtClearError(); testRequire(xrtAllocationBudgetLeave(&scope), "auth quota exit"); xrtTlsIdentityRelease(identity);
        if (success) { xfuture* ready = xredisClientReady(client); testRequire(tlsReady(ready)->Status == XREDIS_READY_OK, "auth quota survivor readiness"); redisDialClientClose(client); xrtFutureDestroy(ready); }
        redisTlsPeerStop(&peer); authReleased(&state);
        xallocationbudgetinfo info = {0}; info.Size = sizeof(info);
        testRequire(xrtAllocationBudgetInfo(budget, &info) && !info.LiveBytes && !info.LiveCount, "auth factory budget rollback"); xrtAllocationBudgetFree(budget);
    }
    testRequire(failed && created, "auth quota sweep reaches both failure and success");
}
int main(void)
{
    authBasic(); authFailures(); authInvalid(); authAllocations();
    puts("[PASS] xredis TLS authentication: retained signer/configs before DNS; three factories; binary pipeline; no-request policy; faults/quota/close/lifetimes");
    return 0;
}
