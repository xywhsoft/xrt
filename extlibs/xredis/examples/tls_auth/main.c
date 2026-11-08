#include "material.h"
#include <stdlib.h>

/* The example selects RSA signing; production TLS-auth accepts any separately
 * selected native/external identity. Keys are file contents, never argv values. */
int main(int argc, char** argv)
{
    if (argc == 1) { puts("Usage: xredis-tls-auth HOST PORT SERVER_CA.pem CLIENT_CHAIN.pem RSA_KEY.pem [PASSWORD_ENV_NAME]"); return 0; }
    if (argc != 6 && argc != 7) return 2;
    char* end = NULL; unsigned long port = strtoul(argv[2], &end, 10); if (*end || !port || port > UINT16_MAX) return 2;
    xnetengine* engine = NULL; xnetresolver* resolver = NULL; xx509store* store = NULL;
    xtlsidentity* identity = NULL; xtlsverifier* verifier = NULL; xredisclient* client = NULL;
    xfuture *ready = NULL, *request = NULL, *closed = NULL; int code = 1; size_t trust_size = 0;
    bytes trust = xredisExampleAuthRead(argv[3], 64u * 1024u, &trust_size);
    identity = xredisExampleAuthIdentity(argv[4], argv[5]); store = xrtX509StoreCreate();
    if (!trust || !identity || !store || !xrtX509StoreAddData(store, (xbytesview){trust, trust_size}, NULL)) goto done;
    xtlsverifierconfig verify; xrtTlsVerifierConfigInit(&verify); verify.Store = store;
    xtlsverifylimits limits; xrtTlsVerifyLimitsInit(&limits); verifier = xrtTlsVerifierCreateLimited(&verify, &limits); if (!verifier) goto done;
    xnetengineconfig network; xrtNetEngineConfigInit(&network); network.Workers = 1;
    engine = xrtNetEngineCreate(&network); if (!engine || !xrtNetEngineStart(engine)) goto done;
    resolver = xrtNetResolverCreate(NULL); if (!resolver) goto done;
    xredisclienttlsconfig tls; xredisClientTlsConfigInit(&tls); tls.Session.Verifier = verifier;
    xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity;
    xredisclientinitconfig init; xredisClientInitConfigInit(&init);
    if (argc == 7) { cstr password = getenv(argv[6]); if (!password) goto done; init.Flags = XREDIS_INIT_AUTH; init.Password = (xbytesview){(cbytes)password, strlen(password)}; }
    if (!xredisClientTlsAuthDialV1(engine, resolver, (xstrview){argv[1], strlen(argv[1])}, (uint16)port, 0, NULL, &init, &tls, &auth, &client)) goto done;
    /* Admission retained both, before DNS. */
    xrtTlsIdentityRelease(identity); identity = NULL; xrtTlsVerifierRelease(verifier); verifier = NULL;
    ready = xredisClientReady(client); if (!ready || xrtFutureWaitFor(ready, 10000) != XWAIT_OK) goto done;
    const xredisreadyresult* state = xrtFutureValue(ready); if (!state || state->Status != XREDIS_READY_OK) goto done;
    xbytesview ping = XRT_BYTES_LITERAL("PING"); if (xredisClientCall(client, &ping, 1, 5000, &request) != XREDIS_ADMISSION_ACCEPTED) goto done;
    const xredisrequestresult* result = xrtFutureValue(request); xredisreplyvalue value;
    if (!result || result->Status != XREDIS_REQUEST_OK || !xredisReplyGet(result->Reply, 0, &value) ||
        value.Data.Size != 4u || memcmp(value.Data.Data, "PONG", 4)) goto done;
    puts("[PASS] TLS client proof, trusted server, Redis initialization and PONG"); code = 0;
done:
    if (client) { closed = xredisClientClosed(client); if (!xredisClientClose(client) || !closed || xrtFutureWaitFor(closed, 10000) != XWAIT_OK) code = 1; xredisClientRelease(client); }
    if (resolver && !xrtNetResolverDestroy(resolver)) code = 1;
    if (engine && (!xrtNetEngineStop(engine) || !xrtNetEngineDestroy(engine))) code = 1;
    xrtFutureDestroy(closed); xrtFutureDestroy(request); xrtFutureDestroy(ready);
    xrtTlsIdentityRelease(identity); xrtTlsVerifierRelease(verifier); xrtX509StoreFree(store);
    if (trust) { xrtSecureZero(trust, trust_size); xrtFree(trust); }
    if (code) fputs("TLS authentication example failed; inspect typed readiness and explicit credentials.\n", stderr);
    return code;
}
