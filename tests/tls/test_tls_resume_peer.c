/* Offline ordinary handshakes and ownership only. No sockets, malformed wire
 * inputs, public endpoints or security/fuzz campaigns. */
#include "../fixtures/tls_server.h"

typedef struct peer_policy {
    size_t Calls;
    size_t ClockCalls;
    xtime Now;
    xtime Observed;
    bool Accept;
} peer_policy;

static xtime clockNow(ptr raw)
{
    peer_policy* policy = raw;
    policy->ClockCalls++;
    return policy->Now;
}

static xtlsverifydecision verifyPeer(const xtlspeer* peer, ptr raw)
{
    peer_policy* policy = raw;
    policy->Calls++;
    policy->Observed = peer->Time;
    testRequire(peer->CertificateCount == 1 && peer->Role == XTLS_SERVER,
        "resume did not supply the actual certificate chain");
    return policy->Accept ? XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}

static const xtlsresume* lookup(ptr raw, const xtlsserverresumerequest* request)
{
    (void)request;
    return raw;
}

static void sessions(xtlscontext* context, xtlsidentity* identity, xtlsverifier* verifier,
    const xtlsresume* offered, const xtlsresume* cached, bool required,
    xtlssession** client, xtlssession** server)
{
    static const xstrview protocols[] = { XRT_STR_INIT("http/1.1") };
    xtlsclientconfig cc; xrtTlsClientConfigInit(&cc);
    cc.Context = context; cc.Verifier = verifier;
    cc.ServerName = XRT_STR_LITERAL("example.com");
    cc.Protocols = protocols; cc.ProtocolCount = 1;
    cc.Resume = offered; cc.ResumeOnly = required;
    xtlsserverconfig sc; xrtTlsServerConfigInit(&sc);
    sc.Context = context; sc.Identity = identity;
    sc.Protocols = protocols; sc.ProtocolCount = 1; sc.RequireProtocol = true;
    sc.Resume = lookup; sc.ResumeContext = (ptr)cached;
    *client = xrtTlsClientCreate(&cc, NULL);
    testRequire(*client != NULL, "client creation failed");
    *server = xrtTlsServerCreate(&sc, NULL);
    testRequire(*server != NULL, "server creation failed");
}

static xtlsresume* issue(xtlssession* client, xtlssession* server,
    test_tls_server_rng* rng, xtlsresume** cached)
{
    testRequire(xrtTlsServerTicketNew(server, cached) == XTLS_OK && *cached,
        "ordinary ticket issue failed");
    testRequire(testTlsServerPostHandshake(client, server, rng), "ticket delivery failed");
    xtlsresume* ticket = xrtTlsClientTakeResume(client);
    testRequire(ticket && xrtTlsResumePeerCertificateCountV1(ticket) == 1,
        "issued ticket lost its certificate owner");
    return ticket;
}

static xtlsresume* fromParts(const xtlsresume* ticket)
{
    xtlsresumeinfo i; testRequire(xrtTlsResumeInfo(ticket, &i), "ticket info failed");
    xtlsresumeconfig c = {0};
    c.Version = i.Version; c.Cipher = i.Cipher; c.Ticket = i.Ticket; c.Secret = i.Secret;
    c.ServerName = i.ServerName; c.Protocol = i.Protocol; c.PeerIdentity = i.PeerIdentity;
    c.Lifetime = i.Lifetime; c.AgeAdd = i.AgeAdd; c.MaxEarlyData = i.MaxEarlyData;
    c.IssuedAt = i.IssuedAt;
    return xrtTlsResumeCreate(&c);
}

static void exercise(void)
{
    peer_policy policy = {0}; policy.Now = xrtNow(); policy.Accept = true;
    xtlsverifierconfig vc; xrtTlsVerifierConfigInit(&vc);
    vc.Verify = verifyPeer; vc.Time = clockNow; vc.Context = &policy;
    xtlsverifylimits limits; xrtTlsVerifyLimitsInit(&limits);
    xtlsverifier* verifier = xrtTlsVerifierCreateFlagsV1(&vc, &limits, XTLS_VERIFY_RESUME_PEER_V1);
    testRequire(verifier && xrtTlsVerifierFlagsV1(verifier) == XTLS_VERIFY_RESUME_PEER_V1,
        "immutable resume verification options were lost");
    xtlscontext* context = testTlsServerContext();
    xtlsidentity* identity = testTlsServerIdentity();
    testRequire(context && identity, "normal fixture setup failed");
    test_tls_server_rng rng = {0x12345678};
    xtlssession *client, *server;
    sessions(context, identity, verifier, NULL, NULL, false, &client, &server);
    testRequire(testTlsServerHandshake(client, server, &rng), "full handshake failed");
    testRequire(policy.Calls == 1 && policy.ClockCalls == 1, "full verification count failed");
    const xx509cert* original = xrtTlsClientCertificate(client, 0);
    xtlsresume *cached, *cached2;
    xtlsresume* ticket = issue(client, server, &rng, &cached);
    xtlsresume* sibling = issue(client, server, &rng, &cached2);
    testRequire(xrtTlsResumePeerCertificateV1(ticket, 0) == original &&
        xrtTlsResumePeerCertificateV1(sibling, 0) == original,
        "tickets copied DER instead of sharing the immutable owner");
    xrtTlsResumeRelease(cached2);
    xrtTlsSessionDestroy(client); xrtTlsSessionDestroy(server);
    xrtTlsResumeRelease(sibling);
    testRequire(xrtTlsResumePeerCertificateV1(ticket, 0) == original,
        "ticket did not outlive the original session");

    policy.Now += XRT_TIME_SECOND;
    sessions(context, identity, verifier, ticket, cached, true, &client, &server);
    testRequire(testTlsServerHandshake(client, server, &rng) && xrtTlsClientResumed(client),
        "required ordinary resumption failed");
    testRequire(policy.Calls == 2 && policy.ClockCalls == 2 && policy.Observed == policy.Now,
        "resumption reused a cached verification/time decision");
    testRequire(xrtTlsClientCertificate(client, 0) == original,
        "resumed session did not share the actual peer owner");
    xtlsresume* next = issue(client, server, &rng, &cached2);
    testRequire(xrtTlsResumePeerCertificateV1(next, 0) == original,
        "successor ticket lost the shared chain");
    xrtTlsSessionDestroy(client); xrtTlsSessionDestroy(server);
    xrtTlsResumeRelease(ticket); xrtTlsResumeRelease(cached);
    ticket = next; cached = cached2;

    xtlsresume* raw = fromParts(ticket);
    testRequire(raw && xrtTlsResumePeerCertificateCountV1(raw) == 0,
        "opaque PeerIdentity was incorrectly treated as a certificate chain");
    sessions(context, identity, verifier, raw, cached, false, &client, &server);
    testRequire(testTlsServerHandshake(client, server, &rng) && !xrtTlsClientResumed(client) &&
        policy.Calls == 3, "missing-chain optional resume did not use a full handshake");
    xrtTlsSessionDestroy(client); xrtTlsSessionDestroy(server);
    xtlsclientconfig required; xrtTlsClientConfigInit(&required);
    required.Context = context; required.Verifier = verifier;
    required.Resume = raw; required.ResumeOnly = true;
    testRequire(!xrtTlsClientCreate(&required, NULL) && xrtGetError(),
        "missing-chain required resume was silently accepted");
    xrtClearError();

    xbytesview der = original->Raw;
    xtlsresume* attached = xrtTlsResumeWithPeerCertificatesV1(raw, &der, 1, &limits);
    testRequire(attached && xrtTlsResumePeerCertificateCountV1(raw) == 0 &&
        xrtTlsResumePeerCertificateV1(attached, 0) != original,
        "explicit import did not make an independent immutable copy");
    sessions(context, identity, verifier, attached, cached, true, &client, &server);
    testRequire(testTlsServerHandshake(client, server, &rng) && xrtTlsClientResumed(client) &&
        policy.Calls == 4, "explicit chain import could not resume normally");
    xrtTlsSessionDestroy(client); xrtTlsSessionDestroy(server);

    /* Ordinary application policy changed after ticket issue: it must be
     * called again, rather than inheriting the old 'accept' decision. */
    policy.Accept = false;
    sessions(context, identity, verifier, attached, cached, true, &client, &server);
    testRequire(!testTlsServerHandshake(client, server, &rng) && policy.Calls == 5 &&
        xrtGetError(), "current application policy was not consulted on resume");
    xrtClearError();
    xrtTlsSessionDestroy(client); xrtTlsSessionDestroy(server);
    policy.Accept = true;

    for (uint64 prefix = 0; prefix < 128; prefix++) {
        xmemdebugsnapshot before, after;
        xrtMemDebugSnapshot(&before);
        testRequire(xrtMemDebugFailAfter(prefix), "allocation injection failed");
        xtlsresume* copy = xrtTlsResumeWithPeerCertificatesV1(raw, &der, 1, &limits);
        bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        testRequire(copy || (hit && xrtGetError()), "import failed without a checked cause");
        xrtClearError(); xrtTlsResumeRelease(copy);
        xrtMemDebugSnapshot(&after);
        testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes,
            "import allocation rollback leaked or freed an input owner");
    }
    xrtTlsResumeRelease(attached); xrtTlsResumeRelease(raw);
    xrtTlsResumeRelease(cached); xrtTlsResumeRelease(ticket);
    xrtTlsIdentityRelease(identity); xrtTlsContextRelease(context);
    xrtTlsVerifierRelease(verifier);
    testRequire(!xrtGetError(), "unexpected retained error");
}

int main(void)
{
    testRequire(xrtMemDebugEnable(true), "memory debug enable failed");
    exercise();
    xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
    exercise(); xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes,
        "resume peer owner lifecycle leaked");
    puts("TLS resume peer: shared chain; fresh current policy/time; explicit import; 128 OOM prefixes; balanced owners");
    return 0;
}
