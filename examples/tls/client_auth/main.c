/* Self-contained factory/ownership tour. It deliberately stops at ClientHello;
 * run tools/test_tls_client_auth.py for real mutually authenticated peers. */
#include "../embedded_identity.h"
#include <stdio.h>

int main(void)
{
    static const xtlsversion versions[] = {XTLS_VERSION_13};
    static const xtlscipher ciphers[] = {XTLS_AES_128_GCM_SHA256};
    static const uint16 groups[] = {XTLS_GROUP_X25519};
    static const xtlssignature signatures[] = {XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256};
    xtlspolicy policy; xrtTlsPolicyInit(&policy);
    policy.Versions = versions; policy.VersionCount = 1;
    policy.Ciphers = ciphers; policy.CipherCount = 1;
    policy.Groups = groups; policy.GroupCount = 1;
    policy.Signatures = signatures; policy.SignatureCount = 1;
    xtlscontextconfig cc; xrtTlsContextConfigInit(&cc); cc.Policy = &policy;
    xtlscontext* context = xrtTlsContextCreate(&cc);
    xbytesview certificate = {g_ExampleCertDer, sizeof(g_ExampleCertDer)};
    xtlsidentity* identity = xrtTlsIdentityP256(&certificate, 1,
        (xbytesview){g_ExampleKeyDer, sizeof(g_ExampleKeyDer)});
    xx509store* store = xrtX509StoreCreate();
    xtlsverifier* verifier = NULL; xtlssession* session = NULL;
    if (context && identity && store && xrtX509StoreAdd(store, certificate.Data, certificate.Size) == X509_VALUE) {
        xtlsverifierconfig vc; xrtTlsVerifierConfigInit(&vc); vc.Store = store;
        verifier = xrtTlsVerifierCreate(&vc);
        if (verifier) {
            xtlsclientconfig client; xrtTlsClientConfigInit(&client);
            client.Context = context; client.Verifier = verifier; client.ServerName = XRT_STR_LITERAL("localhost");
            xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity;
            session = xrtTlsClientCreateAuthV1(&client, &auth, NULL);
        }
    }
    /* The session now holds its own immutable references. */
    xrtTlsVerifierRelease(verifier); xrtX509StoreFree(store);
    xrtTlsIdentityRelease(identity); xrtTlsContextRelease(context);
    if (!session) {
        fprintf(stderr, "client-auth factory: %s\n", xrtGetError() ? xrtErrorMessage(xrtGetError()) : "failed");
        xrtClearError(); return 1;
    }
    bool ok = xrtTlsSessionSendSize(session) != 0 && !xrtTlsClientCertificateQueuedV1(session);
    printf("client-auth factory: ClientHello=%zu bytes; certificate-queued=%s; caller references released\n",
        xrtTlsSessionSendSize(session), xrtTlsClientCertificateQueuedV1(session) ? "true" : "false");
    xrtTlsSessionDestroy(session); return ok ? 0 : 1;
}
