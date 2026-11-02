/* Sequential cold connection cycles. Python is only an independent OpenSSL
 * fixture launcher; the measured client/crypto/dial/HELLO/PING/close are C. */
#include <xredis.h>
#include <xrt/process.h>
#include "../examples/tls_auth/material.h"
#include <stdlib.h>

static bool authBenchWait(xfuture* future)
{ return future && xrtFutureWaitFor(future, 10000) == XWAIT_OK; }
static int authBenchCompare(const void* left, const void* right)
{ double a = *(const double*)left, b = *(const double*)right; return (a > b) - (a < b); }
static int authBenchClient(unsigned long cycles, cstr port_text, cstr ca, cstr cert, cstr key)
{
    char* end = NULL; unsigned long port = strtoul(port_text, &end, 10);
    if (!cycles || cycles > 10000u || *end || !port || port > UINT16_MAX) return 2;
    xnetengine* engine = NULL; xx509store* store = NULL; xtlsverifier* verifier = NULL; xtlsidentity* identity = NULL;
    xredisclient* client = NULL; xfuture *ready = NULL, *request = NULL, *closed = NULL;
    size_t size = 0; bytes trust = xredisExampleAuthRead(ca, 65536u, &size); double* samples = xrtCalloc(cycles, sizeof(double)); int code = 1;
    identity = xredisExampleAuthIdentity(cert, key); store = xrtX509StoreCreate();
    if (!trust || !samples || !identity || !store || !xrtX509StoreAddData(store, (xbytesview){trust, size}, NULL)) goto done;
    xtlsverifierconfig verify; xrtTlsVerifierConfigInit(&verify); verify.Store = store;
    xtlsverifylimits limits; xrtTlsVerifyLimitsInit(&limits); verifier = xrtTlsVerifierCreateLimited(&verify, &limits); if (!verifier) goto done;
    static const xtlsversion versions[] = {XTLS_VERSION_13};
    static const xtlscipher ciphers[] = {XTLS_AES_128_GCM_SHA256};
    static const uint16 groups[] = {XTLS_GROUP_X25519};
    static const xtlssignature signatures[] = {XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256};
    xtlspolicy policy; xrtTlsPolicyInit(&policy); policy.Versions = versions; policy.VersionCount = 1;
    policy.Ciphers = ciphers; policy.CipherCount = 1; policy.Groups = groups; policy.GroupCount = 1;
    policy.Signatures = signatures; policy.SignatureCount = 1;
    xtlscontextconfig context_config; xrtTlsContextConfigInit(&context_config); context_config.Policy = &policy;
    xtlscontext* context = xrtTlsContextCreate(&context_config); if (!context) goto done;
    xredisclienttlsconfig tls; xredisClientTlsConfigInit(&tls); tls.Session.Context = context; tls.Session.Verifier = verifier;
    tls.Session.ServerName = (xstrview)XRT_STR_LITERAL("localhost"); tls.Stream.CloseTimeout = 200;
    xtlsclientauthconfigv1 auth; xrtTlsClientAuthConfigInitV1(&auth); auth.Identity = identity;
    xnetengineconfig options; xrtNetEngineConfigInit(&options); options.Workers = 1;
    engine = xrtNetEngineCreate(&options);
    if (!engine || !xrtNetEngineStart(engine)) { xrtTlsContextRelease(context); goto done; }
    xnetaddr address; if (!xrtNetAddrLoopback(&address, XNET_FAMILY_IPV4, (uint16)port)) { xrtTlsContextRelease(context); goto done; }
    double start = xrtTimer();
    for (unsigned long index = 0; index < cycles; index++) {
        double began = xrtTimer();
        if (!xredisClientTlsAuthConnectV1(engine, &address, 0, NULL, NULL, &tls, &auth, &client)) { xrtTlsContextRelease(context); goto done; }
        ready = xredisClientReady(client); if (!authBenchWait(ready)) { xrtTlsContextRelease(context); goto done; }
        const xredisreadyresult* state = xrtFutureValue(ready);
        if (!state || state->Status != XREDIS_READY_OK || !state->Negotiated) {
            fprintf(stderr, "auth cycle readiness=%d\n", state ? (int)state->Status : -1); xrtTlsContextRelease(context); goto done;
        }
        xbytesview ping = XRT_BYTES_LITERAL("PING");
        if (xredisClientCall(client, &ping, 1, 5000, &request) != XREDIS_ADMISSION_ACCEPTED) { xrtTlsContextRelease(context); goto done; }
        const xredisrequestresult* reply = xrtFutureValue(request); xredisreplyvalue value;
        if (!reply || reply->Status != XREDIS_REQUEST_OK || !xredisReplyGet(reply->Reply, 0, &value) ||
            value.Data.Size != 4u || memcmp(value.Data.Data, "PONG", 4)) { xrtTlsContextRelease(context); goto done; }
        closed = xredisClientClosed(client);
        if (!xredisClientClose(client) || !authBenchWait(closed)) { xrtTlsContextRelease(context); goto done; }
        xredisClientRelease(client); client = NULL;
        xrtFutureDestroy(ready); ready = NULL; xrtFutureDestroy(request); request = NULL; xrtFutureDestroy(closed); closed = NULL;
        samples[index] = (xrtTimer() - began) * 1000000.0;
    }
    double elapsed = xrtTimer() - start; xrtTlsContextRelease(context);
    qsort(samples, cycles, sizeof(double), authBenchCompare);
    size_t p50 = ((size_t)cycles * 50u + 99u) / 100u - 1u, p99 = ((size_t)cycles * 99u + 99u) / 100u - 1u;
    printf("completed: %lu\nauthenticated_cycles_per_sec: %.9f\ncycle_p50_us: %.9f\ncycle_p99_us: %.9f\ncycle_max_us: %.9f\n",
        cycles, (double)cycles / elapsed, samples[p50], samples[p99], samples[cycles - 1u]);
    puts("workload: sequential-full-tls13-rsa-client-proof+HELLO3+PING+physical-close");
    puts("timing_scope: native-client-wall-cycle; fixture-start/key-import/engine-start excluded; peer-work and first-cycle OS work included"); code = 0;
done:
    if (client) { if (!closed) closed = xredisClientClosed(client); (void)xredisClientClose(client); if (!authBenchWait(closed)) code = 1; xredisClientRelease(client); }
    if (engine && (!xrtNetEngineStop(engine) || !xrtNetEngineDestroy(engine))) code = 1;
    xrtFutureDestroy(ready); xrtFutureDestroy(request); xrtFutureDestroy(closed);
    xrtTlsIdentityRelease(identity); xrtTlsVerifierRelease(verifier); xrtX509StoreFree(store); xrtFree(samples);
    if (trust) { xrtSecureZero(trust, size); xrtFree(trust); }
    return code;
}
int main(int argc, char** argv)
{
    if (argc == 7 && !strcmp(argv[1], "--client")) {
        char* end = NULL; unsigned long cycles = strtoul(argv[2], &end, 10); if (*end) return 2;
        return authBenchClient(cycles, argv[3], argv[4], argv[5], argv[6]);
    }
    if (argc != 2) return 2;
    cstr python = getenv("XREDIS_BENCH_PYTHON");
    if (!python) {
#if defined(_WIN32)
        python = "python";
#else
        python = "python3";
#endif
    }
    cstr args[] = {"extlibs/xredis/bench/support/tls_auth_peer.py", "--client", argv[0], "--cycles", argv[1]};
    xprocessconfig config; xrtProcessConfigInit(&config); config.Program = python; config.Args = args; config.ArgCount = 5;
    config.HideWindow = true; config.NewGroup = true; config.Stdout.Mode = XPROCESS_IO_PIPE; config.Stderr.Mode = XPROCESS_IO_PIPE;
    xprocessrunoptions options; xrtProcessRunOptionsInit(&options); options.Timeout = 110000; options.StdoutLimit = options.StderrLimit = 65536;
    xprocessresult result = {0}; bool ok = xrtProcessRun(&config, &options, &result) && xrtProcessResultSuccess(&result) &&
        !result.StdoutTruncated && !result.StderrTruncated;
    if (result.StdoutSize) fwrite(result.Stdout, 1, result.StdoutSize, stdout);
    if (result.StderrSize) fwrite(result.Stderr, 1, result.StderrSize, stderr);
    xrtProcessResultUnit(&result); return ok ? 0 : 1;
}
