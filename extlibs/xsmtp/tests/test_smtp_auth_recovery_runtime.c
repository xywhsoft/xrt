#define main testSmtpExistingAuthMain
#include "test_smtp_auth.c"
#undef main

typedef struct authrecoveryserver {
    xnetlistener* Listener;
    double Deadline;
    unsigned Mode;
    bool Success;
} authrecoveryserver;

static bool recoverySend(xnetstream* stream, cstr text, double deadline)
{
    return testSmtpAuthSend(stream, text, strlen(text), deadline);
}
static bool recoveryReceive(xnetstream* stream, cstr text, double deadline)
{
    return testSmtpAuthReceive(stream, text, strlen(text), deadline);
}
static int32 authRecoveryServer(ptr data)
{
    authrecoveryserver* server = (authrecoveryserver*)data;
    double deadline = server->Deadline;
    unsigned mode = server->Mode;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, deadline, NULL);
    bool valid = stream != NULL;
    if (valid) valid = testSmtpAuthHello(stream,
        "250-smtp.test\r\n250 AUTH LOGIN PLAIN XOAUTH2\r\n",
        sizeof("250-smtp.test\r\n250 AUTH LOGIN PLAIN XOAUTH2\r\n") - 1u, deadline);
    if (valid && (mode <= 5u || mode == 7u)) {
        cstr command = mode == 2u ? "AUTH PLAIN\r\n" :
            mode == 4u ? "AUTH XOAUTH2 dXNlcj11c2VyAWF1dGg9QmVhcmVyIHBhc3MBAQ==\r\n" :
            "AUTH LOGIN\r\n";
        valid = recoveryReceive(stream, command, deadline) &&
            recoverySend(stream, mode == 5u ? "535 denied\r\n" : "334 continue\r\n", deadline);
        if (valid && (mode == 1u || mode == 3u || mode == 7u)) {
            valid = recoveryReceive(stream, "dXNlcg==\r\n", deadline) &&
                recoverySend(stream, "334 continue\r\n", deadline);
        }
        if (valid && (mode == 2u || mode == 3u || mode == 4u || mode == 7u)) {
            valid = recoveryReceive(stream, mode == 2u ? "AHVzZXIAcGFzcw==\r\n" :
                mode == 4u ? "\r\n" : "cGFzcw==\r\n", deadline) &&
                recoverySend(stream, mode == 7u ? "235 accepted\r\n" : "334 continue\r\n", deadline);
        }
    }
    if (valid && mode == 2u) valid =
        recoveryReceive(stream, "AHVzZXIAcGFzcw==\r\n", deadline) &&
        recoverySend(stream, "334 continue\r\n", deadline);
    if (valid && mode <= 4u) {
        xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
        valid = bytes == NULL && xrtErrorKind(xrtGetError()) != XERR_TIMEOUT;
        xrtNetBytesDestroy(bytes);
    } else if (valid) {
        valid = recoveryReceive(stream, "NOOP\r\n", deadline) &&
            recoverySend(stream, "250 reusable\r\n", deadline) && testSmtpAuthQuit(stream, deadline);
    }
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) valid = xrtNetStreamClose(stream) && valid;
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, deadline, NULL) ||
            xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError(); server->Success = valid; return valid ? 0 : 1;
}

static bool authRecoveryRun(const xsmtpclientconfig* config, xnetlistener* listener, unsigned mode)
{
    static const char* const names[] = {
        "LOGIN oversized username closes incomplete exchange",
        "LOGIN oversized password closes incomplete exchange",
        "PLAIN repeated challenge closes incomplete exchange",
        "LOGIN repeated challenge closes incomplete exchange",
        "XOAUTH2 repeated challenge closes incomplete exchange",
        "terminal rejection retains reusable session",
        "invalid config retains reusable session",
        "successful LOGIN retains authenticated session",
        "PLAIN size preflight retains reusable session"
    };
    char oversized[9217];
    authrecoveryserver server;
    xsmtpauthconfig auth;
    xsmtpreply reply;
    xsmtpclient* client;
    xthread* thread;
    xsmtpclientstate state;
    xerrkind kind;
    bool result, valid, recovered = true;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    server.Listener = listener; server.Deadline = deadline; server.Mode = mode; server.Success = false;
    thread = xrtThreadCreate(authRecoveryServer, &server, 0);
    testRequire(thread != NULL, "SMTP auth recovery peer failed");
    client = xrtSmtpClientOpen(config, 10000, NULL);
    testRequire(client != NULL, "SMTP auth recovery public Open failed");
    xrtSmtpAuthConfigInit(&auth); auth.AllowPlaintext = true;
    auth.Method = mode == 2u || mode == 8u ? XSMTP_AUTH_PLAIN :
        mode == 4u ? XSMTP_AUTH_XOAUTH2 : XSMTP_AUTH_LOGIN;
    auth.InitialResponse = mode != 2u;
    auth.Username = XRT_STR_LITERAL("user"); auth.Secret = XRT_STR_LITERAL("pass");
    memset(oversized, 'u', sizeof(oversized));
    if (mode == 0u) auth.Username = (xstrview){ oversized, sizeof(oversized) };
    if (mode == 1u || mode == 8u) auth.Secret = (xstrview){ oversized, sizeof(oversized) };
    if (mode == 6u) auth.Secret = XRT_STR_LITERAL("");
    if (mode != 6u) testRequire(xrtSmtpAuthConfigValid(&auth), "SMTP auth recovery config invalid");
    xrtClearError(); result = xrtSmtpClientAuth(client, &auth, 10000, NULL);
    kind = xrtErrorKind(xrtGetError()); state = xrtSmtpClientState(client);
    valid = xrtSmtpClientLastReply(client, &reply);
    if (mode <= 4u) {
        valid = valid && !result && state == XSMTP_CLIENT_FAILED && reply.Code == 334 &&
            testMailViewEqual(reply.Text, XRT_STR_LITERAL("continue")) &&
            kind == (mode <= 1u ? XERR_RANGE : XERR_PROTOCOL);
        xrtClearError();
        recovered = !xrtSmtpClientNoop(client, 10000, NULL) &&
            xrtErrorKind(xrtGetError()) == XERR_STATE &&
            xrtSmtpClientState(client) == XSMTP_CLIENT_FAILED;
    } else {
        valid = valid && state == XSMTP_CLIENT_READY &&
            (mode == 7u ? result && xrtSmtpClientAuthenticated(client) :
             !result && kind == (mode == 5u ? XERR_PERMISSION : mode == 6u ? XERR_ARGUMENT : XERR_RANGE));
        xrtClearError(); recovered = xrtSmtpClientNoop(client, 10000, NULL) &&
            xrtSmtpClientQuit(client, 10000, NULL);
    }
    if (!valid || !recovered) fprintf(stderr,
        "SMTP auth recovery mode=%u result=%d state=%d kind=%d recovered=%d\n",
        mode, result, state, kind, recovered);
    xrtClearError(); testRequire(xrtSmtpClientAbort(client), "SMTP auth recovery Abort failed");
    xrtSmtpClientDestroy(client);
    testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK, "SMTP auth recovery peer retirement failed");
    valid = valid && recovered && server.Success && xrtThreadExitCode(thread) == 0;
    xrtThreadDestroy(thread); xrtClearError();
    printf("[%s] SMTP auth recovery %s\n", valid ? "PASS" : "FAIL", names[mode]); return valid;
}
int main(void)
{
    xnetengineconfig ec;
    xnetlistenconfig lc;
    xnetresolverconfig rc;
    xsmtpclientconfig config;
    xnetengine* engine;
    xnetlistener* listener;
    xnetresolver* resolver;
    xnetenginestats stats;
    double deadline;
    bool valid = true;
    testRequire(xrtMemDebugEnable(true), "SMTP auth recovery memory diagnostics unavailable");
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "SMTP auth recovery Engine failed");
    xrtNetListenConfigInit(&lc);
    testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "SMTP auth recovery address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestSmtpAuthAddress),
        "SMTP auth recovery Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testSmtpAuthResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc);
    testRequire(resolver != NULL, "SMTP auth recovery Resolver failed");
    xrtSmtpClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "smtp.test"; config.Net.Port = TestSmtpAuthAddress.Port;
    config.Hello = XRT_STR_LITERAL("client.test");
    for (unsigned mode = 0; mode < 9u; mode++) if (!authRecoveryRun(&config, listener, mode)) valid = false;
    deadline = __xrtWaitAfter(INT64_C(10000));
    testRequire(xrtNetListenerClose(listener), "SMTP auth recovery Listener close failed");
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "SMTP auth recovery Listener retirement timed out");
        xrtThreadYield();
    }
    xrtNetListenerDestroy(listener);
    testRequire(xrtNetResolverDestroy(resolver), "SMTP auth recovery Resolver retirement failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine);
        if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline),
            "SMTP auth recovery Engine retirement failed");
        xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 &&
        stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "SMTP auth recovery Engine resources remain");
    xrtClearError(); testMemoryDebugDrain("SMTP auth recovery leaked resources");
    return valid ? 0 : 1;
}
