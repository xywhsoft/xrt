#define main testImapExistingClientMain
#include "test_imap_client.c"
#undef main

typedef struct statusserver {
    xnetlistener* Listener;
    double Deadline;
    unsigned Mode;
    bool Success;
} statusserver;
static bool statusSend(xnetstream* stream, cstr text, double deadline)
{
    return testImapSend(stream, text, strlen(text), deadline);
}
static bool statusReceive(xnetstream* stream, cstr text, double deadline)
{
    return testImapReceive(stream, text, strlen(text), deadline);
}
static int32 statusServer(ptr data)
{
    statusserver* server = (statusserver*)data;
    double deadline = server->Deadline;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, deadline, NULL);
    unsigned next = 2u;
    char command[80], response[128];
    bool valid = stream != NULL;
    if (valid) valid = statusSend(stream, server->Mode == 1u ?
        "* PREAUTH ready {3}\r\n" : "* PREAUTH ready\r\n", deadline) &&
        statusReceive(stream, "A00000001 CAPABILITY\r\n", deadline) &&
        statusSend(stream, server->Mode == 2u ?
            "* CAPABILITY IMAP4rev1 IDLE UIDPLUS\r\nA00000001 OK capabilities {3}\r\n" :
            "* CAPABILITY IMAP4rev1 IDLE UIDPLUS\r\nA00000001 OK capabilities\r\n", deadline);
    if (valid && server->Mode == 3u) {
        valid = statusReceive(stream, "A00000002 NOOP\r\n", deadline) &&
            statusSend(stream, "A00000002 OK done {3}\r\n", deadline); next = 3u;
    } else if (valid && server->Mode == 4u) {
        valid = statusReceive(stream, "A00000002 APPEND \"INBOX\" {3}\r\n", deadline) &&
            statusSend(stream, "+ ready {3}\r\n", deadline) &&
            statusReceive(stream, "abc\r\n", deadline) &&
            statusSend(stream, "A00000002 OK [APPENDUID 7 9] stored\r\n", deadline); next = 3u;
    } else if (valid && server->Mode == 5u) {
        valid = statusReceive(stream, "A00000002 FETCH 1 (BODY[1] BODY[2])\r\n", deadline) &&
            statusSend(stream, "* 1 FETCH (BODY[1] {3}\r\nabc BODY[2] {3}\r\ndef)\r\nA00000002 OK fetched\r\n", deadline); next = 3u;
    } else if (valid && server->Mode >= 6u) {
        valid = statusReceive(stream, "A00000002 SELECT \"INBOX\"\r\n", deadline) &&
            statusSend(stream, "* 1 EXISTS\r\nA00000002 OK [READ-WRITE] selected\r\n", deadline) &&
            statusReceive(stream, "A00000003 IDLE\r\n", deadline) &&
            statusSend(stream, "+ idling\r\n* 1 FETCH (BODY[] {3}\r\nabc)\r\n", deadline) &&
            statusReceive(stream, "DONE\r\n", deadline) &&
            statusSend(stream, "A00000003 OK idle complete\r\n", deadline); next = 4u;
    }
    snprintf(command, sizeof(command), "A%08X NOOP\r\n", next);
    snprintf(response, sizeof(response), "A%08X OK reusable\r\n", next);
    valid = valid && statusReceive(stream, command, deadline) && statusSend(stream, response, deadline); next++;
    snprintf(command, sizeof(command), "A%08X LOGOUT\r\n", next);
    snprintf(response, sizeof(response), "* BYE signing off\r\nA%08X OK logout\r\n", next);
    valid = valid && statusReceive(stream, command, deadline) && statusSend(stream, response, deadline);
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) valid = xrtNetStreamClose(stream) && valid;
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, deadline, NULL) ||
            xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError(); server->Success = valid; return valid ? 0 : 1;
}
static bool statusReadExpectedBody(ximapclient* client, cstr expected)
{
    char body[3]; size_t offset = 0;
    while (offset < sizeof(body)) {
        size_t read = 0;
        if (!xrtImapClientReadLiteral(client, body + offset, sizeof(body) - offset, &read, 10000, NULL) ||
            read == 0 || read > sizeof(body) - offset) return false;
        offset += read;
    }
    return memcmp(body, expected, sizeof(body)) == 0 && xrtImapClientLiteralRemaining(client) == 0;
}
static bool statusReadBody(ximapclient* client)
{
    return statusReadExpectedBody(client, "abc");
}
static bool statusFragment(ximapclient* client)
{
    ximapevent event;
    return xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_ITEM &&
        event.Kind == XIMAP_EVENT_FRAGMENT && !event.HasLiteral &&
        testMailViewEqual(event.Source, XRT_STR_LITERAL(")"));
}
static bool statusDone(ximapclient* client)
{
    ximapevent event;
    return xrtImapClientEndIdle(client, 10000, NULL) &&
        xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_END &&
        event.Response.Status == XIMAP_STATUS_OK;
}
static bool statusRun(const ximapclientconfig* config, xnetlistener* listener, unsigned mode)
{
    static const char* const names[] = {
        "plain status text control", "greeting brace suffix is text", "CAPABILITY brace suffix is text",
        "NOOP brace suffix is text", "continuation brace suffix is text", "actual FETCH literal and literal-bearing fragment control",
        "IDLE unread literal does not consume DONE eligibility", "IDLE pending fragment does not consume DONE eligibility",
        "IDLE drained literal control", "IDLE literal invalid timeout control"
    };
    statusserver server;
    xthread* thread;
    ximapclient* client;
    ximapevent event;
    bool valid, recovered;
    xerrkind kind;
    ximapclientstate observed;
    size_t remaining;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    server.Listener = listener; server.Deadline = deadline; server.Mode = mode; server.Success = false;
    thread = xrtThreadCreate(statusServer, &server, 0);
    testRequire(thread != NULL, "IMAP boundary peer creation failed");
    client = xrtImapClientOpen(config, 10000, NULL);
    valid = client != NULL && xrtImapClientState(client) == XIMAP_CLIENT_AUTHENTICATED &&
        xrtImapClientLiteralRemaining(client) == 0;
    recovered = client != NULL;
    if (client != NULL && mode == 3u) valid = xrtImapClientNoop(client, 10000, NULL) &&
        xrtImapClientLiteralRemaining(client) == 0 && valid;
    else if (client != NULL && mode == 4u) {
        ximapappendconfig append; ximapappendresult result;
        xrtImapAppendConfigInit(&append); append.Mailbox = XRT_STR_LITERAL("INBOX");
        append.Size = 3u; append.Literal = XIMAP_LITERAL_SYNC;
        recovered = xrtImapClientAppendBegin(client, &append, 10000, NULL) &&
            xrtImapClientAppendWrite(client, "abc", 3u, 10000, NULL) &&
            xrtImapClientAppendEnd(client, &result, 10000, NULL);
        valid = valid && recovered && result.Present && result.UidValidity == 7u && result.Uid == 9u;
    } else if (client != NULL && mode == 5u) {
        recovered = xrtImapClientBegin(client, XRT_STR_LITERAL("FETCH"), XRT_STR_LITERAL("1 (BODY[1] BODY[2])"), 10000, NULL) &&
            xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_ITEM && event.HasLiteral &&
            event.Literal.Size == 3u && statusReadBody(client) &&
            xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_ITEM &&
            event.Kind == XIMAP_EVENT_FRAGMENT && event.HasLiteral && event.Literal.Size == 3u &&
            testMailViewEqual(event.Source, XRT_STR_LITERAL(" BODY[2] {3}")) &&
            statusReadExpectedBody(client, "def") && statusFragment(client) &&
            xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_END;
        valid = valid && recovered && xrtImapClientLiteralRemaining(client) == 0;
    } else if (client != NULL && mode >= 6u) {
        ximapmailboxinfo info;
        bool setup = xrtImapClientSelect(client, XRT_STR_LITERAL("INBOX"), &info, 10000, NULL) &&
            xrtImapClientBeginIdle(client, 10000, NULL) &&
            xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_ITEM &&
            event.Response.Kind == XIMAP_RESPONSE_CONTINUATION &&
            xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_ITEM && event.HasLiteral &&
            event.Literal.Size == 3u;
        if (setup && (mode == 7u || mode == 8u)) setup = statusReadBody(client);
        if (setup && mode == 8u) {
            recovered = statusFragment(client) && statusDone(client); valid = valid && recovered;
        } else if (setup) {
            size_t before = xrtImapClientLiteralRemaining(client);
            bool result;
            xrtClearError(); result = xrtImapClientEndIdle(client, mode == 9u ? -2 : 10000, NULL);
            kind = xrtErrorKind(xrtGetError());
            valid = valid && !result && kind == (mode == 9u ? XERR_ARGUMENT : XERR_STATE) &&
                xrtImapClientState(client) == XIMAP_CLIENT_SELECTED &&
                xrtImapClientLiteralRemaining(client) == before &&
                testMailViewEqual(xrtImapClientTag(client), XRT_STR_LITERAL("A00000003"));
            xrtClearError(); recovered = (mode == 7u || statusReadBody(client)) &&
                statusFragment(client) && statusDone(client);
        } else { valid = false; recovered = false; }
    }
    kind = xrtErrorKind(xrtGetError()); observed = xrtImapClientState(client);
    remaining = xrtImapClientLiteralRemaining(client);
    xrtClearError();
    if (recovered) recovered = xrtImapClientNoop(client, 10000, NULL);
    if (recovered) recovered = xrtImapClientLogout(client, 10000, NULL);
    if (!valid || !recovered) fprintf(stderr,
        "IMAP boundary mode=%u state=%d kind=%d literal=%zu valid=%d recovered=%d\n",
        mode, observed, kind, remaining, valid, recovered);
    if (client != NULL) {
        xrtClearError(); testRequire(xrtImapClientAbort(client), "IMAP boundary Abort failed");
        xrtImapClientDestroy(client);
    }
    testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK, "IMAP boundary peer retirement failed");
    valid = valid && recovered && server.Success && xrtThreadExitCode(thread) == 0;
    xrtThreadDestroy(thread); xrtClearError();
    printf("[%s] IMAP boundary %s\n", valid ? "PASS" : "FAIL", names[mode]); return valid;
}

/* The peer accepts either the preserved old pending APPEND (for before-fix
 * cleanup) or the corrected next NOOP. Only the latter is valid in whole mode. */
typedef struct outputserver {
    xnetlistener* Listener;
    double Deadline;
    bool Streaming;
    bool Success;
    bool AppendObserved;
} outputserver;
static bool outputLine(xnetstream* stream, char* text, size_t capacity, double deadline)
{
    size_t size = 0;
    while (size + 1u < capacity) {
        xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
        xbytesview view;
        if (bytes == NULL) return false;
        view = xrtNetBytesView(bytes);
        if (view.Size != 1u) { xrtNetBytesDestroy(bytes); return false; }
        text[size++] = (char)view.Data[0]; xrtNetBytesDestroy(bytes);
        if (size >= 2u && text[size - 2u] == '\r' && text[size - 1u] == '\n') {
            text[size] = 0; return true;
        }
    }
    return false;
}
static int32 outputServer(ptr data)
{
    outputserver* server = (outputserver*)data;
    double deadline = server->Deadline;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, deadline, NULL);
    char line[128];
    bool noop_done = false;
    bool valid = stream != NULL;
    if (valid) valid = statusSend(stream, "* PREAUTH ready\r\n", deadline) &&
        statusReceive(stream, "A00000001 CAPABILITY\r\n", deadline) &&
        statusSend(stream, "* CAPABILITY IMAP4rev1 UIDPLUS\r\nA00000001 OK capabilities\r\n", deadline) &&
        outputLine(stream, line, sizeof(line), deadline);
    if (valid && strcmp(line, "A00000002 APPEND \"INBOX\" {3}\r\n") == 0) {
        server->AppendObserved = true;
        valid = statusSend(stream, "+ ready\r\n", deadline) &&
            statusReceive(stream, "abc\r\n", deadline) &&
            statusSend(stream, "A00000002 OK [APPENDUID 7 9] stored\r\n", deadline);
    } else if (valid && !server->Streaming && strcmp(line, "A00000002 NOOP\r\n") == 0) {
        valid = statusSend(stream, "A00000002 OK reusable\r\n", deadline); noop_done = true;
    } else valid = false;
    if (valid && !noop_done) valid = statusReceive(stream, "A00000003 NOOP\r\n", deadline) &&
        statusSend(stream, "A00000003 OK reusable\r\n", deadline);
    valid = valid && statusReceive(stream, noop_done ? "A00000003 LOGOUT\r\n" : "A00000004 LOGOUT\r\n", deadline) &&
        statusSend(stream, noop_done ? "* BYE signing off\r\nA00000003 OK logout\r\n" :
            "* BYE signing off\r\nA00000004 OK logout\r\n", deadline);
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) valid = xrtNetStreamClose(stream) && valid;
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, deadline, NULL) ||
            xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError(); server->Success = valid; return valid ? 0 : 1;
}
static bool outputRun(const ximapclientconfig* config, xnetlistener* listener, bool streaming)
{
    outputserver server;
    xthread* thread;
    ximapclient* client;
    ximapappendconfig append;
    ximapappendresult result;
    ximapappendresult* invalid = (ximapappendresult*)(uintptr_t)(UINTPTR_MAX - 1u);
    ximapresponseview before, after;
    char last[256], tag[16];
    size_t tag_size, remaining;
    bool returned, valid, recovered;
    xerrkind kind;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    server.Listener = listener; server.Deadline = deadline; server.Streaming = streaming;
    server.Success = false; server.AppendObserved = false;
    thread = xrtThreadCreate(outputServer, &server, 0);
    testRequire(thread != NULL, "IMAP boundary output peer creation failed");
    client = xrtImapClientOpen(config, 10000, NULL);
    testRequire(client != NULL && xrtImapClientState(client) == XIMAP_CLIENT_AUTHENTICATED,
        "IMAP boundary output public PREAUTH Open failed");
    xrtImapAppendConfigInit(&append); append.Mailbox = XRT_STR_LITERAL("INBOX");
    append.Size = 3u; append.Literal = XIMAP_LITERAL_SYNC;
    if (streaming) testRequire(xrtImapClientAppendBegin(client, &append, 10000, NULL),
        "IMAP boundary output APPEND setup failed");
    tag_size = xrtImapClientTag(client).Size;
    testRequire(tag_size <= sizeof(tag) && xrtImapClientLastResponse(client, &before) &&
        before.Source.Size <= sizeof(last), "IMAP boundary output snapshot failed");
    if (tag_size != 0) memcpy(tag, xrtImapClientTag(client).Data, tag_size);
    memcpy(last, before.Source.Data, before.Source.Size);
    remaining = xrtImapClientAppendRemaining(client); xrtClearError();
    returned = streaming ? xrtImapClientAppendEnd(client, invalid, 10000, NULL) :
        xrtImapClientAppend(client, &append, "abc", invalid, 10000, NULL);
    kind = xrtErrorKind(xrtGetError());
    valid = !returned && kind == XERR_ARGUMENT &&
        xrtImapClientState(client) == XIMAP_CLIENT_AUTHENTICATED &&
        xrtImapClientAppendRemaining(client) == remaining &&
        xrtImapClientTag(client).Size == tag_size &&
        (tag_size == 0 || memcmp(xrtImapClientTag(client).Data, tag, tag_size) == 0) &&
        xrtImapClientLastResponse(client, &after) && after.Source.Size == before.Source.Size &&
        memcmp(after.Source.Data, last, before.Source.Size) == 0;
    xrtClearError(); recovered = true;
    if (streaming) recovered = xrtImapClientAppendWrite(client, "abc", 3u, 10000, NULL) &&
        xrtImapClientAppendEnd(client, &result, 10000, NULL);
    else if (xrtImapClientTag(client).Size != 0) {
        /* The old whole wrapper sent the header and consumed its literal before
         * rejecting output. Only this explicit recovery completes that exchange. */
        valid = false; recovered = xrtImapClientAppendEnd(client, &result, 10000, NULL);
    }
    if (streaming && recovered) recovered = result.Present && result.UidValidity == 7u && result.Uid == 9u;
    if (recovered) recovered = xrtImapClientNoop(client, 10000, NULL);
    if (recovered) recovered = xrtImapClientLogout(client, 10000, NULL);
    if (!valid || !recovered) fprintf(stderr, "IMAP boundary output streaming=%d kind=%d valid=%d recovered=%d\n",
        streaming, kind, valid, recovered);
    xrtClearError(); testRequire(xrtImapClientAbort(client), "IMAP boundary output Abort failed");
    xrtImapClientDestroy(client);
    testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK, "IMAP boundary output peer retirement failed");
    valid = valid && recovered && server.Success && xrtThreadExitCode(thread) == 0 &&
        server.AppendObserved == streaming;
    xrtThreadDestroy(thread); xrtClearError();
    printf("[%s] IMAP boundary %s\n", valid ? "PASS" : "FAIL", streaming ?
        "AppendEnd invalid output preserves pending upload" : "Append invalid output starts no transaction");
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
    testRequire(xrtMemDebugEnable(true), "IMAP boundary memory diagnostics unavailable");
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "IMAP boundary Engine failed");
    xrtNetListenConfigInit(&lc);
    testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "IMAP boundary address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestImapAddress),
        "IMAP boundary Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testImapResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc);
    testRequire(resolver != NULL, "IMAP boundary Resolver failed");
    xrtImapClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "imap.test"; config.Net.Port = TestImapAddress.Port;
    for (unsigned mode = 0; mode < 10u; mode++) if (!statusRun(&config, listener, mode)) valid = false;
    if (!outputRun(&config, listener, true)) valid = false;
    if (!outputRun(&config, listener, false)) valid = false;
    deadline = __xrtWaitAfter(INT64_C(10000));
    testRequire(xrtNetListenerClose(listener), "IMAP boundary Listener close failed");
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "IMAP boundary Listener retirement timed out");
        xrtThreadYield();
    }
    xrtNetListenerDestroy(listener);
    testRequire(xrtNetResolverDestroy(resolver), "IMAP boundary Resolver retirement failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine);
        if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline),
            "IMAP boundary Engine retirement failed");
        xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 &&
        stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "IMAP boundary Engine resources remain");
    xrtClearError(); testMemoryDebugDrain("IMAP boundary leaked resources"); return valid ? 0 : 1;
}
