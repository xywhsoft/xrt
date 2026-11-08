#define main testImapExistingClientMain
#include "test_imap_client.c"
#undef main

/* All clients are public Open results. The peer distinguishes a rejected local
 * input from a started exchange by the exact next command on the same socket. */
typedef enum authmode {
    AUTH_PLAIN_LITERAL,
    AUTH_LOGIN_LITERAL,
    AUTH_LOGIN_CANCEL_EOF,
    AUTH_PLAIN_LARGE,
    AUTH_IR_LARGE,
    AUTH_DENIED,
    AUTH_SUCCESS,
    AUTH_LOGIN_CANCEL_DONE,
    AUTH_INVALID_TIMEOUT,
    AUTH_PLAINTEXT_GATE,
    AUTH_UNSUPPORTED,
    AUTH_RAW_OOM,
    AUTH_ENCODED_OOM,
    AUTH_LOGIN_CANCEL_CONTINUE,
    AUTH_MODE_COUNT
} authmode;

typedef struct authserver {
    xnetlistener* Listener;
    double Deadline;
    authmode Mode;
    bool Success;
    bool AuthObserved;
} authserver;

static bool authSend(xnetstream* stream, cstr text, double deadline)
{
    return testImapSend(stream, text, strlen(text), deadline);
}

static bool authReceive(xnetstream* stream, cstr text, double deadline)
{
    return testImapReceive(stream, text, strlen(text), deadline);
}

static bool authLine(xnetstream* stream, char* text, size_t capacity, double deadline)
{
    size_t size = 0;
    while (size + 1u < capacity) {
        xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
        xbytesview view;
        if (bytes == NULL) return false;
        view = xrtNetBytesView(bytes);
        if (view.Size != 1u) { xrtNetBytesDestroy(bytes); return false; }
        text[size++] = (char)view.Data[0];
        xrtNetBytesDestroy(bytes);
        if (size >= 2u && text[size - 2u] == '\r' && text[size - 1u] == '\n') {
            text[size] = 0;
            return true;
        }
    }
    return false;
}

static bool authLogin(authmode mode)
{
    return mode == AUTH_LOGIN_LITERAL || mode == AUTH_LOGIN_CANCEL_EOF ||
        mode == AUTH_LOGIN_CANCEL_DONE || mode == AUTH_LOGIN_CANCEL_CONTINUE;
}

static bool authTerminal(authmode mode)
{
    return mode == AUTH_PLAIN_LITERAL || mode == AUTH_LOGIN_LITERAL ||
        mode == AUTH_LOGIN_CANCEL_EOF || mode == AUTH_LOGIN_CANCEL_CONTINUE;
}

static bool authLocal(authmode mode)
{
    return mode == AUTH_PLAIN_LARGE || mode == AUTH_IR_LARGE ||
        mode == AUTH_INVALID_TIMEOUT || mode == AUTH_PLAINTEXT_GATE ||
        mode == AUTH_UNSUPPORTED || mode == AUTH_RAW_OOM || mode == AUTH_ENCODED_OOM;
}

static bool authEof(xnetstream* stream, double deadline)
{
    xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
    bool valid = bytes == NULL && xrtErrorKind(xrtGetError()) != XERR_TIMEOUT;
    xrtNetBytesDestroy(bytes);
    return valid;
}

static bool authFinish(xnetstream* stream, unsigned next, bool noop_received, double deadline)
{
    char command[80], response[128];
    bool valid = true;
    snprintf(command, sizeof(command), "A%08X NOOP\r\n", next);
    snprintf(response, sizeof(response), "A%08X OK reusable\r\n", next);
    if (!noop_received) valid = authReceive(stream, command, deadline);
    valid = valid && authSend(stream, response, deadline);
    next++;
    snprintf(command, sizeof(command), "A%08X LOGOUT\r\n", next);
    snprintf(response, sizeof(response), "* BYE signing off\r\nA%08X OK logout\r\n", next);
    return valid && authReceive(stream, command, deadline) && authSend(stream, response, deadline);
}

static int32 authServer(ptr data)
{
    authserver* server = (authserver*)data;
    authmode mode = server->Mode;
    double deadline = server->Deadline;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, deadline, NULL);
    char line[512];
    bool valid = stream != NULL;
    if (valid) valid = authSend(stream, "* OK imap.test ready\r\n", deadline) &&
        authReceive(stream, "A00000001 CAPABILITY\r\n", deadline) &&
        authSend(stream, mode == AUTH_UNSUPPORTED ?
            "* CAPABILITY IMAP4rev1\r\nA00000001 OK capabilities\r\n" :
            "* CAPABILITY IMAP4rev1 AUTH=PLAIN SASL-IR\r\nA00000001 OK capabilities\r\n", deadline) &&
        authLine(stream, line, sizeof(line), deadline);
    if (valid && authLocal(mode)) {
        unsigned next = mode == AUTH_IR_LARGE ? 3u : 2u;
        char noop[80];
        snprintf(noop, sizeof(noop), "A%08X NOOP\r\n", next);
        if (mode == AUTH_PLAIN_LARGE && strcmp(line, "A00000002 AUTHENTICATE PLAIN\r\n") == 0) {
            /* Before-fix cleanup: no accepted credential bytes follow. This is
             * an observed command and must never count as a local rejection. */
            server->AuthObserved = true;
            valid = authSend(stream, "+ ready\r\n", deadline) && authEof(stream, deadline);
        } else valid = strcmp(line, noop) == 0 && authFinish(stream, next, true, deadline);
    } else if (valid) {
        valid = strcmp(line, authLogin(mode) ? "A00000002 LOGIN \"user\" \"secret\"\r\n" :
            "A00000002 AUTHENTICATE PLAIN\r\n") == 0;
        server->AuthObserved = valid;
        if (valid && (mode == AUTH_PLAIN_LITERAL || mode == AUTH_LOGIN_LITERAL)) {
            valid = authSend(stream, "* 1 FETCH (BODY[] {3}\r\nabc)\r\n"
                "A00000002 NO denied\r\n", deadline) && authEof(stream, deadline);
        } else if (valid && authLogin(mode)) {
            valid = authSend(stream, "+ unexpected\r\n", deadline) && authReceive(stream, "*\r\n", deadline);
            if (valid && mode == AUTH_LOGIN_CANCEL_DONE) valid =
                authSend(stream, "A00000002 NO cancelled\r\n", deadline) && authFinish(stream, 3u, false, deadline);
            else if (valid && mode == AUTH_LOGIN_CANCEL_CONTINUE) valid =
                authSend(stream, "+ still waiting\r\n", deadline) && authEof(stream, deadline);
            /* EOF case deliberately closes without a tagged cancellation. */
        } else if (valid && mode == AUTH_DENIED) {
            valid = authSend(stream, "A00000002 NO denied\r\n", deadline) && authFinish(stream, 3u, false, deadline);
        } else if (valid && mode == AUTH_SUCCESS) {
            valid = authSend(stream, "+ ready\r\n", deadline) &&
                authReceive(stream, "AHVzZXIAc2VjcmV0\r\n", deadline) &&
                authSend(stream, "A00000002 OK authenticated\r\n", deadline) && authFinish(stream, 3u, false, deadline);
        } else valid = false;
    }
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) valid = xrtNetStreamClose(stream) && valid;
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, deadline, NULL) ||
            xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError();
    server->Success = valid;
    return valid ? 0 : 1;
}

static xerrkind authExpectedError(authmode mode)
{
    if (mode == AUTH_LOGIN_CANCEL_EOF) return XERR_CLOSED;
    if (mode == AUTH_PLAIN_LARGE || mode == AUTH_INVALID_TIMEOUT) return XERR_ARGUMENT;
    if (mode == AUTH_IR_LARGE) return XERR_RANGE;
    if (mode == AUTH_DENIED || mode == AUTH_PLAINTEXT_GATE) return XERR_PERMISSION;
    if (mode == AUTH_UNSUPPORTED) return XERR_UNSUPPORTED;
    if (mode == AUTH_RAW_OOM || mode == AUTH_ENCODED_OOM) return XERR_MEMORY;
    if (mode == AUTH_SUCCESS) return XERR_NONE;
    return XERR_PROTOCOL;
}

static bool authRun(const ximapclientconfig* config, xnetlistener* listener, authmode mode)
{
    static const char* const names[] = {
        "PLAIN unexpected literal retires session", "LOGIN unexpected literal retires session",
        "LOGIN cancellation preserves EOF error", "non-IR encoded size rejects before command",
        "IR line-limit rejection remains reusable", "terminal NO remains reusable",
        "PLAIN success and exact payload", "completed LOGIN cancellation remains reusable",
        "invalid timeout before command", "plaintext credentials gate before command",
        "unsupported capability before command", "raw credential allocation OOM",
        "encoded credential allocation OOM", "LOGIN incomplete cancellation retires session"
    };
    authserver server;
    ximapclientconfig bounded = *config;
    ximapauthconfig auth, original_auth;
    ximapresponseview last_before, last_after;
    ximapclient* client;
    xthread* thread;
    char last[256], username[160];
    xerrkind kind;
    int32 code;
    ximapclientstate state;
    size_t tag_size, literal;
    bool returned, valid, recovered, joined = false, triggered;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    server.Listener = listener; server.Deadline = deadline; server.Mode = mode;
    server.Success = false; server.AuthObserved = false;
    thread = xrtThreadCreate(authServer, &server, 0);
    testRequire(thread != NULL, "IMAP auth peer creation failed");
    if (mode == AUTH_PLAIN_LARGE || mode == AUTH_IR_LARGE) bounded.CommandLineLimit = 128u;
    client = xrtImapClientOpen(&bounded, 10000, NULL);
    testRequire(client != NULL && xrtImapClientState(client) == XIMAP_CLIENT_NOT_AUTHENTICATED,
        "IMAP auth public Open failed");
    testRequire(xrtImapClientLastResponse(client, &last_before) && last_before.Source.Size <= sizeof(last),
        "IMAP auth last-response snapshot failed");
    memcpy(last, last_before.Source.Data, last_before.Source.Size);
    xrtImapAuthConfigInit(&auth); auth.InitialResponse = mode == AUTH_IR_LARGE;
    auth.AllowPlaintext = mode != AUTH_PLAINTEXT_GATE;
    auth.Method = authLogin(mode) ? XIMAP_AUTH_LOGIN : XIMAP_AUTH_PLAIN;
    auth.Username = XRT_STR_LITERAL("user"); auth.Secret = XRT_STR_LITERAL("secret");
    memset(username, 'u', sizeof(username));
    if (mode == AUTH_PLAIN_LARGE || mode == AUTH_IR_LARGE)
        auth.Username = (xstrview){ username, sizeof(username) };
    memcpy(&original_auth, &auth, sizeof(auth));
    xrtClearError();
    if (mode == AUTH_RAW_OOM || mode == AUTH_ENCODED_OOM)
        testRequire(xrtMemDebugFailAfter(mode == AUTH_RAW_OOM ? 0u : 1u), "IMAP auth OOM setup failed");
    returned = xrtImapClientAuth(client, &auth, mode == AUTH_INVALID_TIMEOUT ? -2 : 10000, NULL);
    kind = xrtErrorKind(xrtGetError()); code = xrtErrorCode(xrtGetError());
    state = xrtImapClientState(client); tag_size = xrtImapClientTag(client).Size;
    literal = xrtImapClientLiteralRemaining(client);
    triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    valid = returned == (mode == AUTH_SUCCESS) && kind == authExpectedError(mode) &&
        state == (authTerminal(mode) ? XIMAP_CLIENT_FAILED : mode == AUTH_SUCCESS ?
            XIMAP_CLIENT_AUTHENTICATED : XIMAP_CLIENT_NOT_AUTHENTICATED) &&
        memcmp(&auth, &original_auth, sizeof(auth)) == 0;
    for (size_t i = 0; i < sizeof(username); i++) if (username[i] != 'u') valid = false;
    if (kind == XERR_PROTOCOL) valid = valid && code == XMAIL_ERROR_AUTH &&
        strcmp(xrtErrorDomain(xrtGetError()), "xrt.mail") == 0;
    if (mode == AUTH_RAW_OOM || mode == AUTH_ENCODED_OOM) valid = valid && triggered;
    else valid = valid && !triggered;
    if (authLocal(mode) || authTerminal(mode)) valid = valid &&
        xrtImapClientLastResponse(client, &last_after) && last_after.Source.Size == last_before.Source.Size &&
        memcmp(last_after.Source.Data, last, last_before.Source.Size) == 0;
    if (!authTerminal(mode)) valid = valid && tag_size == 0 && literal == 0;
    if (authTerminal(mode) && state == XIMAP_CLIENT_FAILED) {
        /* Observe peer EOF and its joined native thread before explicit Abort:
         * a state-only fix that leaves the live socket open cannot pass. */
        joined = __xrtThreadWaitUntil(thread, deadline) == XWAIT_OK;
        valid = valid && joined && server.Success && xrtThreadExitCode(thread) == 0;
    }
    xrtClearError();
    recovered = xrtImapClientNoop(client, 10000, NULL);
    if (authTerminal(mode)) recovered = !recovered && xrtErrorKind(xrtGetError()) == XERR_STATE;
    else if (recovered) recovered = xrtImapClientLogout(client, 10000, NULL);
    if (!valid || !recovered) fprintf(stderr,
        "IMAP auth mode=%d returned=%d kind=%d expected=%d code=%d state=%d tag=%zu literal=%zu triggered=%d valid=%d recovery=%d\n",
        (int)mode, returned, (int)kind, (int)authExpectedError(mode), (int)code, (int)state,
        tag_size, literal, triggered, valid, recovered);
    xrtClearError(); testRequire(xrtImapClientAbort(client), "IMAP auth Abort failed");
    xrtImapClientDestroy(client);
    if (!joined) testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK, "IMAP auth peer retirement failed");
    valid = valid && recovered && server.Success && xrtThreadExitCode(thread) == 0 &&
        server.AuthObserved == !authLocal(mode);
    xrtThreadDestroy(thread); xrtClearError();
    printf("[%s] IMAP auth %s\n", valid ? "PASS" : "FAIL", names[mode]);
    return valid;
}

int main(void)
{
    xnetengineconfig ec;
    xnetlistenconfig lc;
    xnetresolverconfig rc;
    ximapclientconfig config;
    xnetengine* engine;
    xnetlistener* listener;
    xnetresolver* resolver;
    xnetenginestats stats;
    bool valid = true;
    double deadline;
    testRequire(xrtMemDebugEnable(true), "IMAP auth memory diagnostics unavailable");
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "IMAP auth Engine failed");
    xrtNetListenConfigInit(&lc);
    testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "IMAP auth address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestImapAddress), "IMAP auth Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testImapResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc);
    testRequire(resolver != NULL, "IMAP auth Resolver failed");
    xrtImapClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "imap.test"; config.Net.Port = TestImapAddress.Port;
    for (authmode mode = AUTH_PLAIN_LITERAL; mode < AUTH_MODE_COUNT; mode++)
        if (!authRun(&config, listener, mode)) valid = false;
    deadline = __xrtWaitAfter(INT64_C(10000));
    testRequire(xrtNetListenerClose(listener), "IMAP auth Listener close failed");
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "IMAP auth Listener retirement timed out");
        xrtThreadYield();
    }
    xrtNetListenerDestroy(listener);
    testRequire(xrtNetResolverDestroy(resolver), "IMAP auth Resolver retirement failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine);
        if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline), "IMAP auth Engine retirement failed");
        xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 &&
        stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "IMAP auth Engine resources remain");
    xrtClearError(); testMemoryDebugDrain("IMAP auth leaked resources");
    return valid ? 0 : 1;
}
