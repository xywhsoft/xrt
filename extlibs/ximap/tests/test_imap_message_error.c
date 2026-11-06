#define main messageErrorExistingMain
#include "test_imap_client.c"
#undef main

typedef struct messageerrorserver {
    xnetlistener* Listener;
    double Deadline;
    unsigned Mode;
    bool Good, Success;
} messageerrorserver;

typedef struct messageerrorsink {
    unsigned Calls;
    size_t Size;
    unsigned char Data[8];
    xerror* Error;
} messageerrorsink;

static bool messageErrorSend(xnetstream* stream, cstr text, double deadline)
{ return testImapSend(stream, text, strlen(text), deadline); }

/* 1: exact command; -1: EOF/error; 0: wrong bytes or timeout. */
static int messageErrorReceive(xnetstream* stream, cstr expected, double deadline)
{
    for (size_t i = 0; expected[i] != 0; i++) {
        xnetbytes* bytes;
        xbytesview view;
        xrtClearError(); bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
        if (bytes == NULL) return xrtErrorKind(xrtGetError()) != XERR_NONE &&
            xrtErrorKind(xrtGetError()) != XERR_TIMEOUT ? -1 : 0;
        view = xrtNetBytesView(bytes);
        bool equal = view.Size == 1u && view.Data[0] == (unsigned char)expected[i];
        xrtNetBytesDestroy(bytes); if (!equal) return 0;
    }
    return 1;
}

static int32 messageErrorServer(ptr data)
{
    messageerrorserver* server = (messageerrorserver*)data;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, server->Deadline, NULL);
    bool valid = stream != NULL && messageErrorSend(stream, "* PREAUTH ready\r\n", server->Deadline) &&
        messageErrorReceive(stream, "A00000001 CAPABILITY\r\n", server->Deadline) == 1 &&
        messageErrorSend(stream, "* CAPABILITY IMAP4rev1\r\nA00000001 OK capabilities\r\n", server->Deadline) &&
        messageErrorReceive(stream, "A00000002 SELECT \"INBOX\"\r\n", server->Deadline) == 1 &&
        messageErrorSend(stream, "* 1 EXISTS\r\nA00000002 OK selected\r\n", server->Deadline);
    if (valid && server->Mode == 3u) valid =
        messageErrorReceive(stream, "A00000003 NOOP\r\n", server->Deadline) == 1 &&
        messageErrorSend(stream, "A00000003 OK noop\r\n", server->Deadline) &&
        messageErrorReceive(stream, "A00000004 LOGOUT\r\n", server->Deadline) == 1 &&
        messageErrorSend(stream, "* BYE closing\r\nA00000004 OK logout\r\n", server->Deadline);
    else if (valid) {
        valid = messageErrorReceive(stream, "A00000003 FETCH 1 BODY.PEEK[]\r\n", server->Deadline) == 1;
        if (valid) {
            cstr response = server->Mode == 0 ?
                "* 1 FETCH (BODY[] {2}\r\nxx)\r\nA00000003 OK fetched\r\n" :
                (server->Mode == 1 && !server->Good) ?
                "* 1 FETCH (BODY[] {1}\r\nx)\r\n* 2 FETCH (BODY[] {1}\r\ny)\r\nA00000003 OK fetched\r\n" :
                "* 1 FETCH (BODY[] {1}\r\nx)\r\nA00000003 OK fetched\r\n";
            /* The receiver may abort before the peer's send completion. Require
             * peer EOF independently rather than requiring that send to succeed. */
            bool sent = messageErrorSend(stream, response, server->Deadline);
            if (server->Good) valid = sent &&
                messageErrorReceive(stream, "A00000004 NOOP\r\n", server->Deadline) == 1 &&
                messageErrorSend(stream, "A00000004 OK noop\r\n", server->Deadline) &&
                messageErrorReceive(stream, "A00000005 LOGOUT\r\n", server->Deadline) == 1 &&
                messageErrorSend(stream, "* BYE closing\r\nA00000005 OK logout\r\n", server->Deadline);
            else valid = messageErrorReceive(stream, "x", server->Deadline) == -1;
        }
    }
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) valid = xrtNetStreamClose(stream) && valid;
        (void)__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, server->Deadline, NULL);
        while (xrtNetStreamState(stream) != XNET_STREAM_CLOSED && !__xrtWaitExpired(server->Deadline))
            xrtThreadYield();
        valid = xrtNetStreamState(stream) == XNET_STREAM_CLOSED && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError(); server->Success = valid; return valid ? 0 : 1;
}

static bool messageErrorWrite(xbytesview data, ptr user)
{
    messageerrorsink* sink = (messageerrorsink*)user;
    if (data.Size > sizeof(sink->Data) - sink->Size) return false;
    memcpy(sink->Data + sink->Size, data.Data, data.Size);
    sink->Size += data.Size; sink->Calls++;
    if (sink->Error != NULL) { xrtSetError(sink->Error); return false; }
    return true;
}

/* mode: 0 byte budget, 1 duplicate literal, 2 explicit sink error. */
static bool messageErrorCase(const ximapclientconfig* config, xnetlistener* listener,
    unsigned mode, xerrkind previous, bool good, cstr name)
{
    messageerrorserver server;
    messageerrorsink sink = {0}; xthread* thread; ximapclient* client;
    ximapmailboxinfo info; ximapresponseview last;
    xerror* prior = NULL; size_t written = 0x1234u;
    bool returned, valid, joined; xerrkind kind; ximapclientstate state;
    server.Listener = listener; server.Deadline = __xrtWaitAfter(10000);
    server.Mode = mode; server.Good = good; server.Success = false;
    thread = xrtThreadCreate(messageErrorServer, &server, 0);
    testRequire(thread != NULL, "message error peer creation failed");
    client = xrtImapClientOpen(config, 10000, NULL);
    testRequire(client != NULL, "message error public Open failed");
    testRequire(xrtImapClientSelect(client, XRT_STR_LITERAL("INBOX"), &info, 10000, NULL),
        "message error Select failed");
    memset(sink.Data, 0xA5, sizeof(sink.Data));
    if (mode == 2) {
        sink.Error = xrtErrorCreate(XERR_IO, "imap.sink", 91, "sink rejected bytes");
        testRequire(sink.Error != NULL, "message sink error creation failed");
    }
    xrtClearError();
    if (previous != XERR_NONE) {
        prior = xrtErrorCreate(previous, "imap.test", 73, "prior error");
        testRequire(prior != NULL, "message prior error creation failed"); xrtSetError(prior);
    }
    returned = xrtImapClientBodyWrite(client, 1u, XRT_STR_LITERAL(""), false, true,
        mode == 0 ? (good ? 2u : 1u) : 100u, messageErrorWrite, &sink, &written, 10000, NULL);
    kind = xrtErrorKind(xrtGetError()); state = xrtImapClientState(client);
    valid = good ? returned && xrtGetError() == prior && written == (mode == 0 ? 2u : 1u) &&
        state == XIMAP_CLIENT_SELECTED : !returned && written == 0x1234u && state == XIMAP_CLIENT_CLOSED;
    if (!good) {
        if (mode == 2) valid = valid && xrtGetError() == sink.Error && kind == XERR_IO &&
            xrtErrorCode(xrtGetError()) == 91 && strcmp(xrtErrorDomain(xrtGetError()), "imap.sink") == 0 &&
            strcmp(xrtErrorMessage(xrtGetError()), "sink rejected bytes") == 0;
        else valid = valid && xrtGetError() != prior && kind == (mode == 0 ? XERR_RANGE : XERR_PROTOCOL) &&
            xrtErrorCode(xrtGetError()) == (mode == 0 ? XMAIL_ERROR_LIMIT : XMAIL_ERROR_PROTOCOL) &&
            strcmp(xrtErrorDomain(xrtGetError()), "xrt.mail") == 0 &&
            strcmp(xrtErrorMessage(xrtGetError()), mode == 0 ? "IMAP BODY literal exceeds the byte limit" :
                "IMAP BODY command returned multiple literals") == 0;
    }
    size_t expected = mode == 0 ? (good ? 2u : 0u) : 1u;
    valid = valid && sink.Size == expected && (expected == 0 ? sink.Calls == 0 :
        (sink.Calls >= 1u && sink.Calls <= expected));
    for (size_t i = 0; i < sizeof(sink.Data); i++)
        valid = valid && sink.Data[i] == (i < expected ? (unsigned char)'x' : 0xA5u);
    testRequire(xrtImapClientLastResponse(client, &last), "message error saved response missing");
    valid = valid && testMailViewEqual(last.Source, good ? XRT_STR_LITERAL("A00000003 OK fetched") :
        XRT_STR_LITERAL("A00000002 OK selected"));
    if (good) {
        valid = xrtImapClientNoop(client, 10000, NULL) && valid;
        valid = xrtImapClientLogout(client, 10000, NULL) && valid;
        valid = valid && xrtImapClientState(client) == XIMAP_CLIENT_CLOSED;
    }
    /* Join terminal peers before explicit Abort so EOF proves library recovery. */
    joined = __xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK;
    valid = valid && joined && server.Success && xrtThreadExitCode(thread) == 0;
    if (!valid) fprintf(stderr, "message error mode=%u prior=%d good=%d returned=%d kind=%d state=%d written=%zu calls=%u bytes=%zu peer=%d joined=%d\n",
        mode, (int)previous, good, returned, (int)kind, (int)state, written, sink.Calls, sink.Size, server.Success, joined);
    xrtClearError(); xrtErrorFree(prior); xrtErrorFree(sink.Error);
    testRequire(xrtImapClientAbort(client), "message error Abort failed"); xrtImapClientDestroy(client);
    if (!joined) testRequire(__xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK, "message error peer join failed");
    xrtThreadDestroy(thread);
    printf("[%s] IMAP message error %s\n", valid ? "PASS" : "FAIL", name);
    xrtClearError(); return valid;
}

/* mode: 3 invalid size range, 4 valid owned bytes, 5 optional size, 6 diagnostic OOM. */
static bool messageBytesCase(const ximapclientconfig* config, xnetlistener* listener,
    unsigned mode, xerrkind previous, cstr name)
{
    messageerrorserver server;
    xthread* thread; ximapclient* client; ximapmailboxinfo info; ximapresponseview last;
    xerror* prior = NULL; size_t size = 0x1234u; bytes owned;
    bool preflight = mode == 3u || mode == 6u, fault = mode == 6u;
    bool valid, triggered, joined; xerrkind kind;
    server.Listener = listener; server.Deadline = __xrtWaitAfter(10000);
    server.Mode = preflight ? 3u : mode; server.Good = true; server.Success = false;
    thread = xrtThreadCreate(messageErrorServer, &server, 0);
    testRequire(thread != NULL, "message bytes peer creation failed");
    client = xrtImapClientOpen(config, 10000, NULL);
    testRequire(client != NULL, "message bytes public Open failed");
    testRequire(xrtImapClientSelect(client, XRT_STR_LITERAL("INBOX"), &info, 10000, NULL),
        "message bytes Select failed");
    xrtClearError();
    if (previous != XERR_NONE) {
        prior = xrtErrorCreate(previous, "imap.test", 73, "prior error");
        testRequire(prior != NULL, "message bytes prior error creation failed"); xrtSetError(prior);
    }
    if (fault) testRequire(xrtMemDebugFailAfter(0), "message bytes diagnostic fault setup failed");
    owned = xrtImapClientBodyBytes(client, 1u, XRT_STR_LITERAL(""), false, true, 1u,
        preflight ? (size_t*)(uintptr_t)(UINTPTR_MAX - sizeof(size_t) + 1u) :
            mode == 5u ? NULL : &size, 10000, NULL);
    kind = xrtErrorKind(xrtGetError()); triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    valid = xrtImapClientState(client) == XIMAP_CLIENT_SELECTED && triggered == fault;
    if (preflight) valid = valid && owned == NULL && size == 0x1234u && xrtGetError() != prior &&
        kind == (fault ? XERR_MEMORY : XERR_ARGUMENT) &&
        xrtErrorCode(xrtGetError()) == (fault ? 1 : 0) &&
        strcmp(xrtErrorDomain(xrtGetError()), fault ? "xrt.memory" : "xrt.mail") == 0 &&
        strcmp(xrtErrorMessage(xrtGetError()), fault ? "memory allocation failed" : "invalid argument") == 0;
    else valid = valid && owned != NULL && owned[0] == 'x' && owned[1] == 0 &&
        size == (mode == 5u ? 0x1234u : 1u) && xrtGetError() == prior;
    testRequire(xrtImapClientLastResponse(client, &last), "message bytes saved response missing");
    valid = valid && testMailViewEqual(last.Source, preflight ? XRT_STR_LITERAL("A00000002 OK selected") :
        XRT_STR_LITERAL("A00000003 OK fetched"));
    /* Exact NOOP tags prove a preflight rejection neither sends FETCH nor
     * consumes its command tag. Successful owned bytes outlive client teardown. */
    valid = xrtImapClientNoop(client, 10000, NULL) && valid;
    valid = xrtImapClientLogout(client, 10000, NULL) && valid;
    valid = valid && xrtImapClientState(client) == XIMAP_CLIENT_CLOSED;
    joined = __xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK;
    valid = valid && joined && server.Success && xrtThreadExitCode(thread) == 0;
    if (!valid) fprintf(stderr, "message bytes mode=%u prior=%d kind=%d triggered=%d size=%zu peer=%d joined=%d\n",
        mode, (int)previous, (int)kind, triggered, size, server.Success, joined);
    xrtClearError(); xrtErrorFree(prior);
    testRequire(xrtImapClientAbort(client), "message bytes Abort failed"); xrtImapClientDestroy(client);
    if (owned != NULL) valid = valid && !preflight && owned[0] == 'x' && owned[1] == 0;
    xrtFree(owned);
    if (!joined) testRequire(__xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK, "message bytes peer join failed");
    xrtThreadDestroy(thread);
    printf("[%s] IMAP message error %s\n", valid ? "PASS" : "FAIL", name);
    xrtClearError(); return valid;
}

int main(void)
{
    xnetengineconfig ec; xnetlistenconfig lc; xnetresolverconfig rc; ximapclientconfig config;
    xnetengine* engine; xnetlistener* listener; xnetresolver* resolver; xnetenginestats stats;
    bool all = true;
    testRequire(xrtMemDebugEnable(true), "message error memory debug unavailable");
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec); testRequire(engine != NULL && xrtNetEngineStart(engine), "message error Engine failed");
    xrtNetListenConfigInit(&lc); testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "message error address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestImapAddress), "message error Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testImapResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc); testRequire(resolver != NULL, "message error Resolver failed");
    xrtImapClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "imap.test"; config.Net.Port = TestImapAddress.Port;
    for (unsigned mode = 0; mode < 2u; mode++) {
        cstr names[4];
        names[0] = mode == 0 ? "limit without prior error" : "duplicate without prior error";
        names[1] = mode == 0 ? "limit replaces prior VALUE" : "duplicate replaces prior VALUE";
        names[2] = mode == 0 ? "limit replaces prior MEMORY" : "duplicate replaces prior MEMORY";
        names[3] = mode == 0 ? "valid two bytes preserves prior error and reuses session" : "valid one byte preserves prior error and reuses session";
        for (unsigned previous = 0; previous < 3u; previous++)
            if (!messageErrorCase(&config, listener, mode, previous == 0 ? XERR_NONE :
                previous == 1 ? XERR_VALUE : XERR_MEMORY, false, names[previous])) all = false;
        if (!messageErrorCase(&config, listener, mode, XERR_VALUE, true, names[3])) all = false;
    }
    if (!messageErrorCase(&config, listener, 2u, XERR_VALUE, false, "sink supplied error replaces prior VALUE exactly")) all = false;
    if (!messageBytesCase(&config, listener, 3u, XERR_NONE, "BodyBytes invalid output range without prior error")) all = false;
    if (!messageBytesCase(&config, listener, 3u, XERR_VALUE, "BodyBytes invalid output range replaces prior VALUE")) all = false;
    if (!messageBytesCase(&config, listener, 3u, XERR_MEMORY, "BodyBytes invalid output range replaces prior MEMORY")) all = false;
    if (!messageBytesCase(&config, listener, 4u, XERR_VALUE, "BodyBytes owned NUL bytes survive client and preserve prior VALUE")) all = false;
    if (!messageBytesCase(&config, listener, 5u, XERR_MEMORY, "BodyBytes optional size preserves prior MEMORY and ownership")) all = false;
    if (!messageBytesCase(&config, listener, 6u, XERR_VALUE, "BodyBytes output refusal diagnostic OOM preserves new MEMORY")) all = false;
    testRequire(xrtNetListenerClose(listener), "message error Listener close failed"); double deadline = __xrtWaitAfter(10000);
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) { testRequire(!__xrtWaitExpired(deadline), "message error Listener timeout"); xrtThreadYield(); }
    xrtNetListenerDestroy(listener); testRequire(xrtNetResolverDestroy(resolver), "message error Resolver destroy failed");
    for (;;) { xnetretireresult next = xrtNetEngineTryStop(engine); if (next == XNET_RETIRE_READY) break;
        testRequire(next == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline), "message error Engine timeout"); xrtThreadYield(); }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 && stats.PendingCommands == 0 &&
        stats.ActiveTimers == 0 && stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "message error retained Engine resources");
    xrtClearError(); testMemoryDebugDrain("message error resource leak"); return all ? 0 : 1;
}
