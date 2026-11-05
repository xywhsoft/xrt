#define main testPop3ExistingAuthMain
#include "test_pop3_auth.c"
#undef main

/* Reuse the real resolver/send/receive helpers; every Client is opened by the
 * public API and every result comes from a real loopback peer. */
typedef struct pop3contractserver {
    xnetlistener* Listener;
    double Deadline;
    unsigned Mode;
    bool Success;
} pop3contractserver;

typedef struct pop3contractsink {
    unsigned Mode;
    unsigned Calls;
    size_t Bytes;
    xerror* Stale;
    xerror* Raised;
} pop3contractsink;

static bool contractSend(xnetstream* stream, cstr text, double deadline)
{
    return testPop3Send(stream, text, strlen(text), deadline);
}

static bool contractReceive(xnetstream* stream, cstr text, double deadline)
{
    return testPop3Receive(stream, text, strlen(text), deadline);
}

static bool contractPeerClosed(xnetstream* stream, double deadline)
{
    xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
    bool closed = bytes == NULL && xrtErrorKind(xrtGetError()) != XERR_TIMEOUT;
    xrtNetBytesDestroy(bytes);
    xrtClearError();
    return closed;
}

static int32 contractServer(ptr data)
{
    pop3contractserver* server = (pop3contractserver*)data;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener,
        server->Deadline, NULL);
    bool valid = stream != NULL;
    unsigned mode = server->Mode;
    if (valid) valid = contractSend(stream, "+OK contract ready\r\n", server->Deadline);
    if (valid && mode >= 3u) {
        valid = contractReceive(stream, "USER user\r\n", server->Deadline) &&
            contractSend(stream, "+OK user\r\n", server->Deadline) &&
            contractReceive(stream, "PASS pass\r\n", server->Deadline) &&
            contractSend(stream, "+OK locked\r\n", server->Deadline);
    }
    if (valid && mode >= 3u && mode <= 6u) {
        valid = contractReceive(stream, "RETR 1\r\n", server->Deadline) &&
            contractSend(stream, mode == 3u ? "+OK body\r\nabc\r\nnext\r\n.\r\n" :
                "+OK body\r\nabc\r\n.\r\n", server->Deadline);
    } else if (valid && (mode == 7u || mode == 9u)) {
        valid = contractReceive(stream, "LIST 1\r\n", server->Deadline) &&
            contractSend(stream, mode == 7u ? "+OK 2 17\r\n" : "+OK 1 17\r\n",
                server->Deadline);
    } else if (valid && (mode == 8u || mode == 10u)) {
        valid = contractReceive(stream, "UIDL 1\r\n", server->Deadline) &&
            contractSend(stream, mode == 8u ? "+OK 2 stable\r\n" : "+OK 1 stable\r\n",
                server->Deadline);
    }
    if (valid && (mode <= 2u || mode == 6u || mode >= 9u)) {
        valid = contractReceive(stream, "NOOP\r\n", server->Deadline) &&
            contractSend(stream, "+OK recovered\r\n", server->Deadline);
    }
    if (valid) valid = contractPeerClosed(stream, server->Deadline);
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) {
            valid = xrtNetStreamClose(stream) && valid;
        }
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE,
            server->Deadline, NULL) || xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    server->Success = valid;
    return valid ? 0 : 1;
}

static bool contractSink(xbytesview data, ptr user)
{
    pop3contractsink* sink = (pop3contractsink*)user;
    sink->Calls++;
    sink->Bytes += data.Size;
    if (sink->Mode == 5u) {
        xrtSetErrorTake(xrtErrorRef(sink->Raised));
        return false;
    }
    if ((sink->Mode == 3u && sink->Calls == 2u) ||
        (sink->Mode == 4u && sink->Calls == 1u)) {
        /* A recoverable operation during an earlier successful fragment leaves
         * an ambient error. A later failure must report its own cause. */
        xrtSetErrorTake(xrtErrorRef(sink->Stale));
    }
    return sink->Mode != 4u || sink->Calls != 2u;
}

static bool contractRun(const xpop3clientconfig* config, xnetlistener* listener,
    unsigned mode)
{
    static const char* names[] = {
        "AuthLine invalid timeout preserves session", "Line invalid timeout preserves session",
        "Close invalid timeout preserves session", "message limit replaces stale error",
        "sink fallback replaces prior fragment error", "sink error identity is preserved",
        "successful streaming remains reusable", "LIST rejects mismatched message",
        "UIDL rejects mismatched message", "LIST matching response remains reusable",
        "UIDL matching response remains reusable"
    };
    pop3contractserver server = {0};
    pop3contractsink sink = {0};
    xpop3client* client;
    xthread* thread;
    xpop3reply reply;
    bool valid = true, result;
    size_t written = 777u;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    xpop3listview list, list_before;
    xpop3uidlview uidl, uidl_before;
    xstrview line = XRT_STR_LITERAL("unchanged");
    sink.Mode = mode;
    sink.Stale = xrtErrorCreate(XERR_VALUE, "test.pop3.stale", 701, "stale prior error");
    sink.Raised = xrtErrorCreate(XERR_MEMORY, "test.pop3.sink", 702, "specific sink error");
    testRequire(sink.Stale != NULL && sink.Raised != NULL, "POP3 contract errors unavailable");
    server.Listener = listener; server.Deadline = deadline; server.Mode = mode;
    thread = xrtThreadCreate(contractServer, &server, 0);
    testRequire(thread != NULL, "POP3 contract peer thread unavailable");
    client = xrtPop3ClientOpen(config, 10000, NULL);
    testRequire(client != NULL, "POP3 contract Open failed");
    if (mode >= 3u) testRequire(xrtPop3ClientLogin(client,
        XRT_STR_LITERAL("user"), XRT_STR_LITERAL("pass"), true, 10000, NULL),
        "POP3 contract login failed");
    xrtClearError();
    if (mode <= 2u) {
        if (mode == 0u) result = xrtPop3ClientAuthLine(client,
            XRT_STR_LITERAL("probe"), -2, NULL);
        else if (mode == 1u) result = xrtPop3ClientLine(client, &line, -2, NULL);
        else result = xrtPop3ClientClose(client, -2);
        valid = !result && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
            xrtPop3ClientState(client) == XPOP3_CLIENT_AUTHORIZATION &&
            testMailViewEqual(line, XRT_STR_LITERAL("unchanged"));
    } else if (mode <= 6u) {
        result = xrtPop3ClientRetrWrite(client, 1u, mode == 3u ? 5u : 0u,
            contractSink, &sink, &written, 10000, NULL);
        if (mode == 3u) valid = !result && written == 777u && sink.Calls == 2u &&
            xrtErrorKind(xrtGetError()) == XERR_RANGE &&
            xrtErrorCode(xrtGetError()) == XMAIL_ERROR_LIMIT;
        else if (mode == 4u) valid = !result && written == 777u && sink.Calls == 2u &&
            xrtErrorKind(xrtGetError()) == XERR_IO &&
            xrtErrorCode(xrtGetError()) == XMAIL_ERROR_CALLBACK;
        else if (mode == 5u) valid = !result && written == 777u && sink.Calls == 1u &&
            xrtGetError() == sink.Raised;
        else valid = result && written == 5u && sink.Calls == 2u && sink.Bytes == 5u &&
            xrtPop3ClientState(client) == XPOP3_CLIENT_TRANSACTION;
        if (mode < 6u) valid = valid && xrtPop3ClientState(client) == XPOP3_CLIENT_CLOSED;
    } else {
        memset(&list, 0xa5, sizeof(list)); memcpy(&list_before, &list, sizeof(list));
        memset(&uidl, 0xa5, sizeof(uidl)); memcpy(&uidl_before, &uidl, sizeof(uidl));
        result = (mode == 7u || mode == 9u) ?
            xrtPop3ClientList(client, 1u, &list, 10000, NULL) :
            xrtPop3ClientUidl(client, 1u, &uidl, 10000, NULL);
        if (mode == 7u || mode == 8u) {
            valid = !result && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL &&
                xrtErrorCode(xrtGetError()) == XMAIL_ERROR_PROTOCOL &&
                xrtPop3ClientState(client) == XPOP3_CLIENT_FAILED &&
                memcmp(&list, &list_before, sizeof(list)) == 0 &&
                memcmp(&uidl, &uidl_before, sizeof(uidl)) == 0 &&
                xrtPop3ClientLastReply(client, &reply) &&
                testMailViewEqual(reply.Source, mode == 7u ?
                    XRT_STR_LITERAL("+OK 2 17") : XRT_STR_LITERAL("+OK 2 stable"));
        } else valid = result && xrtPop3ClientState(client) == XPOP3_CLIENT_TRANSACTION &&
            (mode == 9u ? list.Message == 1u && list.Bytes == 17u :
                uidl.Message == 1u && testMailViewEqual(uidl.Id, XRT_STR_LITERAL("stable")));
    }
    if (!valid) fprintf(stderr, "POP3 contract mode=%u state=%d kind=%d code=%d calls=%u written=%zu\n",
        mode, xrtPop3ClientState(client), xrtErrorKind(xrtGetError()),
        xrtErrorCode(xrtGetError()), sink.Calls, written);
    xrtClearError();
    if (valid && (mode <= 2u || mode == 6u || mode >= 9u)) {
        valid = xrtPop3ClientCommand(client, XRT_STR_LITERAL("NOOP"), XRT_STR_LITERAL(""),
            &reply, 10000, NULL) && reply.Ok &&
            testMailViewEqual(reply.Text, XRT_STR_LITERAL("recovered"));
    }
    testRequire(xrtPop3ClientAbort(client), "POP3 contract abort failed");
    xrtPop3ClientDestroy(client);
    testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK,
        "POP3 contract peer did not retire");
    valid = server.Success && xrtThreadExitCode(thread) == 0 && valid;
    xrtThreadDestroy(thread);
    xrtClearError(); xrtErrorFree(sink.Stale); xrtErrorFree(sink.Raised);
    printf("[%s] POP3 contract %s\n", valid ? "PASS" : "FAIL", names[mode]);
    return valid;
}

int main(void)
{
    xnetengineconfig engine_config;
    xnetlistenconfig listen_config;
    xnetresolverconfig resolver_config;
    xpop3clientconfig config;
    xnetengine* engine;
    xnetlistener* listener;
    xnetresolver* resolver;
    xnetenginestats stats;
    bool valid = true;
    double deadline;
    testRequire(xrtMemDebugEnable(true), "POP3 contract memory diagnostics unavailable");
    xrtNetEngineConfigInit(&engine_config);
    engine_config.Backend = XNET_PORT_SELECT; engine_config.Workers = 1u;
    engine = xrtNetEngineCreate(&engine_config);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "POP3 contract Engine failed");
    xrtNetListenConfigInit(&listen_config);
    testRequire(xrtNetAddrLoopback(&listen_config.Address, XNET_FAMILY_IPV4, 0),
        "POP3 contract address failed");
    listener = xrtNetListen(engine, &listen_config, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestPop3Address),
        "POP3 contract Listener failed");
    xrtNetResolverConfigInit(&resolver_config);
    resolver_config.Workers = 1u; resolver_config.Lookup = testPop3Resolve;
    resolver_config.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&resolver_config);
    testRequire(resolver != NULL, "POP3 contract Resolver failed");
    xrtPop3ClientConfigInit(&config);
    config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "pop3.test"; config.Net.Port = TestPop3Address.Port;
    config.ReadCapabilities = false;
    for (unsigned mode = 0; mode < 11u; mode++) {
        if (!contractRun(&config, listener, mode)) valid = false;
    }
    deadline = __xrtWaitAfter(INT64_C(10000));
    testRequire(xrtNetListenerClose(listener), "POP3 contract Listener close failed");
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "POP3 contract Listener retirement timed out");
        xrtThreadYield();
    }
    xrtNetListenerDestroy(listener);
    testRequire(xrtNetResolverDestroy(resolver), "POP3 contract Resolver retirement failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine);
        if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline),
            "POP3 contract Engine retirement failed");
        xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 &&
        stats.WaitErrors == 0 && xrtNetEngineDestroy(engine),
        "POP3 contract Engine resources remain");
    xrtClearError(); testMemoryDebugDrain("POP3 contract leaked resources");
    return valid ? 0 : 1;
}
