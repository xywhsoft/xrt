#define main testImapExistingClientMain
#include "test_imap_client.c"
#undef main

/* All opaque clients come from public Open and a real PREAUTH TCP session. */
typedef struct imapdeadlineserver {
    xnetlistener* Listener;
    double Deadline;
    unsigned Mode;
    bool Success;
} imapdeadlineserver;

static bool deadlineSend(xnetstream* stream, cstr text, double deadline)
{
    return testImapSend(stream, text, strlen(text), deadline);
}
static bool deadlineReceive(xnetstream* stream, cstr text, double deadline)
{
    return testImapReceive(stream, text, strlen(text), deadline);
}
static bool deadlineIdleMode(unsigned mode)
{
    return mode == 4u || mode == 5u || mode == 9u || mode == 10u || mode == 14u;
}
static bool deadlineAppendMode(unsigned mode)
{
    return mode == 2u || mode == 3u || mode == 6u || mode == 7u || mode == 13u;
}
static bool deadlineExplicitMode(unsigned mode)
{
    return mode == 0u || mode == 1u || mode == 11u || mode == 12u;
}
static int32 deadlineServer(ptr data)
{
    imapdeadlineserver* server = (imapdeadlineserver*)data;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, server->Deadline, NULL);
    double deadline = server->Deadline;
    unsigned next = 2u;
    char command[80], response[160];
    bool valid = stream != NULL;
    if (valid) valid = deadlineSend(stream, "* PREAUTH imap.test ready\r\n", deadline) &&
        deadlineReceive(stream, "A00000001 CAPABILITY\r\n", deadline) &&
        deadlineSend(stream, "* CAPABILITY IMAP4rev1 IDLE UIDPLUS LITERAL+ APPENDLIMIT=64\r\n"
            "A00000001 OK capabilities\r\n", deadline);
    if (valid && deadlineIdleMode(server->Mode)) {
        valid = deadlineReceive(stream, "A00000002 SELECT \"INBOX\"\r\n", deadline) &&
            deadlineSend(stream, "* 0 EXISTS\r\n* OK [UIDVALIDITY 7] stable\r\n"
                "A00000002 OK [READ-WRITE] selected\r\n", deadline) &&
            deadlineReceive(stream, "A00000003 IDLE\r\n", deadline) &&
            deadlineSend(stream, "+ idling\r\n", deadline) &&
            deadlineReceive(stream, "DONE\r\n", deadline) &&
            deadlineSend(stream, "A00000003 OK idle complete\r\n", deadline);
        next = 4u;
    } else if (valid && deadlineAppendMode(server->Mode)) {
        valid = deadlineReceive(stream, "A00000002 APPEND \"INBOX\" {3}\r\n", deadline) &&
            deadlineSend(stream, "+ ready\r\n", deadline) &&
            deadlineReceive(stream, "abc\r\n", deadline) &&
            deadlineSend(stream, "A00000002 OK [APPENDUID 7 9] stored\r\n", deadline);
        next = 3u;
    } else if (valid && deadlineExplicitMode(server->Mode)) {
        valid = deadlineReceive(stream, "P1 NOOP\r\n", deadline) &&
            deadlineSend(stream, "P1 OK explicit\r\n", deadline);
    }
    if (valid && server->Mode == 17u) {
        xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
        valid = bytes == NULL && xrtErrorKind(xrtGetError()) != XERR_TIMEOUT;
        xrtNetBytesDestroy(bytes);
    } else if (valid) {
        snprintf(command, sizeof(command), "A%08X NOOP\r\n", next);
        snprintf(response, sizeof(response), "A%08X OK reusable\r\n", next);
        valid = deadlineReceive(stream, command, deadline) && deadlineSend(stream, response, deadline);
        next++;
        snprintf(command, sizeof(command), "A%08X LOGOUT\r\n", next);
        snprintf(response, sizeof(response), "* BYE signing off\r\nA%08X OK logout\r\n", next);
        valid = valid && deadlineReceive(stream, command, deadline) && deadlineSend(stream, response, deadline);
    }
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) valid = xrtNetStreamClose(stream) && valid;
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, deadline, NULL) ||
            xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError(); server->Success = valid; return valid ? 0 : 1;
}

static bool deadlineBeginIdle(ximapclient* client)
{
    ximapevent event;
    return xrtImapClientBeginIdle(client, 10000, NULL) &&
        xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_ITEM &&
        event.Response.Kind == XIMAP_RESPONSE_CONTINUATION;
}
static bool deadlineEndIdle(ximapclient* client)
{
    ximapevent event;
    return xrtImapClientEndIdle(client, 10000, NULL) &&
        xrtImapClientNext(client, &event, 10000, NULL) == XMAIL_NEXT_END &&
        event.Response.Status == XIMAP_STATUS_OK;
}
static bool deadlineExplicit(ximapclient* client, bool parts)
{
    ximapevent event;
    bool valid = parts ? xrtImapClientSendParts(client, XRT_STR_LITERAL("P1"),
        XRT_STR_LITERAL("NOOP"), NULL, 0, 10000, NULL) :
        xrtImapClientSend(client, XRT_STR_LITERAL("P1"), XRT_STR_LITERAL("NOOP"),
            XRT_STR_LITERAL(""), 10000, NULL);
    return valid && xrtImapClientReceive(client, &event, 10000, NULL) &&
        event.Response.Kind == XIMAP_RESPONSE_TAGGED &&
        event.Response.Status == XIMAP_STATUS_OK &&
        testMailViewEqual(event.Response.Tag, XRT_STR_LITERAL("P1"));
}

static bool deadlineRun(const ximapclientconfig* config, xnetlistener* listener, unsigned mode)
{
    static const char* const names[] = {
        "Send invalid timeout preserves session",
        "SendParts invalid timeout preserves session",
        "Write nonempty invalid timeout preserves APPEND",
        "Write empty rejects invalid timeout",
        "Continue nonempty invalid timeout preserves IDLE",
        "Continue empty invalid timeout preserves IDLE",
        "AppendWrite invalid timeout delegated path",
        "AppendWrite empty invalid timeout control",
        "Append wrapper invalid timeout control",
        "BeginIdle invalid timeout control",
        "EndIdle invalid timeout preserves DONE eligibility",
        "valid Send and Receive control",
        "valid SendParts and Receive control",
        "valid APPEND and infinite NOOP control",
        "valid IDLE and DONE control",
        "Receive invalid timeout and output control",
        "Close invalid timeout control",
        "valid Close control"
    };
    imapdeadlineserver server;
    ximapclient* client;
    xthread* thread;
    ximapappendconfig append;
    ximapappendresult result, result_before;
    ximapevent event, event_before;
    ximapresponseview last_before, last_after;
    ximapclientstate state, observed;
    xstrview tag;
    char last[256], tag_before[16];
    size_t tag_size, remaining, observed_remaining;
    bool returned, valid, recovered = true;
    xerrkind kind;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    server.Listener = listener; server.Deadline = deadline; server.Mode = mode; server.Success = false;
    thread = xrtThreadCreate(deadlineServer, &server, 0);
    testRequire(thread != NULL, "IMAP deadline peer creation failed");
    client = xrtImapClientOpen(config, 10000, NULL);
    testRequire(client != NULL && xrtImapClientState(client) == XIMAP_CLIENT_AUTHENTICATED,
        "IMAP deadline public PREAUTH Open failed");
    xrtImapAppendConfigInit(&append); append.Mailbox = XRT_STR_LITERAL("INBOX");
    append.Size = 3u; append.Literal = XIMAP_LITERAL_SYNC;
    if (deadlineIdleMode(mode)) {
        ximapmailboxinfo info;
        testRequire(xrtImapClientSelect(client, XRT_STR_LITERAL("INBOX"), &info, 10000, NULL) &&
            xrtImapClientState(client) == XIMAP_CLIENT_SELECTED, "IMAP deadline SELECT failed");
        if (mode != 9u) testRequire(deadlineBeginIdle(client), "IMAP deadline IDLE begin failed");
    } else if (deadlineAppendMode(mode)) testRequire(xrtImapClientAppendBegin(client,
        &append, 10000, NULL), "IMAP deadline APPEND begin failed");
    state = xrtImapClientState(client); remaining = xrtImapClientAppendRemaining(client);
    tag = xrtImapClientTag(client); tag_size = tag.Size;
    testRequire(tag_size <= sizeof(tag_before) && xrtImapClientLastResponse(client, &last_before) &&
        last_before.Source.Size <= sizeof(last), "IMAP deadline snapshot failed");
    if (tag_size != 0) memcpy(tag_before, tag.Data, tag_size);
    memcpy(last, last_before.Source.Data, last_before.Source.Size);
    memset(&event, 0xA5, sizeof(event)); memcpy(&event_before, &event, sizeof(event));
    memset(&result, 0xA5, sizeof(result)); memcpy(&result_before, &result, sizeof(result));
    xrtClearError();
    if (mode == 0u) returned = xrtImapClientSend(client, XRT_STR_LITERAL("P1"),
        XRT_STR_LITERAL("NOOP"), XRT_STR_LITERAL(""), -2, NULL);
    else if (mode == 1u) returned = xrtImapClientSendParts(client, XRT_STR_LITERAL("P1"),
        XRT_STR_LITERAL("NOOP"), NULL, 0, -2, NULL);
    else if (mode == 2u || mode == 3u) returned = xrtImapClientWrite(client,
        mode == 2u ? "x" : NULL, mode == 2u ? 1u : 0, -2, NULL);
    else if (mode == 4u || mode == 5u) returned = xrtImapClientContinue(client,
        mode == 4u ? XRT_STR_LITERAL("DONE") : XRT_STR_LITERAL(""), -2, NULL);
    else if (mode == 6u || mode == 7u) returned = xrtImapClientAppendWrite(client,
        mode == 6u ? "x" : NULL, mode == 6u ? 1u : 0, -2, NULL);
    else if (mode == 8u) returned = xrtImapClientAppend(client, &append, "abc", &result, -2, NULL);
    else if (mode == 9u) returned = xrtImapClientBeginIdle(client, -2, NULL);
    else if (mode == 10u) returned = xrtImapClientEndIdle(client, -2, NULL);
    else if (mode == 11u || mode == 12u) returned = deadlineExplicit(client, mode == 12u);
    else if (mode == 13u) returned = xrtImapClientAppendWrite(client, "abc", 3u, 10000, NULL) &&
        xrtImapClientAppendEnd(client, &result, 10000, NULL);
    else if (mode == 14u) returned = deadlineEndIdle(client);
    else if (mode == 15u) returned = xrtImapClientReceive(client, &event, -2, NULL);
    else if (mode == 16u) returned = xrtImapClientClose(client, -2);
    else returned = xrtImapClientClose(client, 10000);
    kind = xrtErrorKind(xrtGetError()); observed = xrtImapClientState(client);
    observed_remaining = xrtImapClientAppendRemaining(client); tag = xrtImapClientTag(client);
    if (mode <= 10u || mode == 15u || mode == 16u) {
        valid = !returned && kind == XERR_ARGUMENT && observed == state &&
            observed_remaining == remaining && tag.Size == tag_size &&
            (tag_size == 0 || memcmp(tag.Data, tag_before, tag_size) == 0) &&
            memcmp(&event, &event_before, sizeof(event)) == 0 &&
            memcmp(&result, &result_before, sizeof(result)) == 0 &&
            xrtImapClientLastResponse(client, &last_after) &&
            last_after.Source.Size == last_before.Source.Size &&
            memcmp(last_after.Source.Data, last, last_before.Source.Size) == 0;
    } else valid = returned && observed == (mode == 17u ? XIMAP_CLIENT_CLOSED : state);
    xrtClearError();
    if (deadlineAppendMode(mode) && mode != 13u) recovered =
        xrtImapClientAppendWrite(client, "abc", 3u, 10000, NULL) &&
        xrtImapClientAppendEnd(client, &result, 10000, NULL);
    if (deadlineAppendMode(mode) && recovered) recovered = result.Present &&
        result.UidValidity == 7u && result.Uid == 9u;
    if (mode == 9u) recovered = deadlineBeginIdle(client);
    if (deadlineIdleMode(mode) && mode != 14u && recovered) recovered = deadlineEndIdle(client);
    if ((mode == 0u || mode == 1u) && recovered) recovered = deadlineExplicit(client, mode == 1u);
    if (mode != 17u && recovered) recovered = xrtImapClientNoop(client, mode == 13u ? -1 : 10000, NULL);
    if (mode != 17u && recovered) recovered = xrtImapClientLogout(client, 10000, NULL);
    if (!valid || !recovered) fprintf(stderr,
        "IMAP deadline mode=%u result=%d state=%d expected=%d kind=%d remaining=%zu expected_remaining=%zu recovered=%d\n",
        mode, returned, observed, state, kind, observed_remaining, remaining, recovered);
    xrtClearError(); testRequire(xrtImapClientAbort(client), "IMAP deadline Abort failed");
    xrtImapClientDestroy(client);
    testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK, "IMAP deadline peer retirement failed");
    valid = valid && recovered && server.Success && xrtThreadExitCode(thread) == 0;
    xrtThreadDestroy(thread); xrtClearError();
    printf("[%s] IMAP deadline %s\n", valid ? "PASS" : "FAIL", names[mode]); return valid;
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
    testRequire(xrtMemDebugEnable(true), "IMAP deadline memory diagnostics unavailable");
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "IMAP deadline Engine failed");
    xrtNetListenConfigInit(&lc);
    testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "IMAP deadline address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestImapAddress),
        "IMAP deadline Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testImapResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc);
    testRequire(resolver != NULL, "IMAP deadline Resolver failed");
    xrtImapClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "imap.test"; config.Net.Port = TestImapAddress.Port;
    for (unsigned mode = 0; mode < 18u; mode++) if (!deadlineRun(&config, listener, mode)) valid = false;
    deadline = __xrtWaitAfter(INT64_C(10000));
    testRequire(xrtNetListenerClose(listener), "IMAP deadline Listener close failed");
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "IMAP deadline Listener retirement timed out");
        xrtThreadYield();
    }
    xrtNetListenerDestroy(listener);
    testRequire(xrtNetResolverDestroy(resolver), "IMAP deadline Resolver retirement failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine);
        if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline),
            "IMAP deadline Engine retirement failed");
        xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 &&
        stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "IMAP deadline Engine resources remain");
    xrtClearError(); testMemoryDebugDrain("IMAP deadline leaked resources"); return valid ? 0 : 1;
}
