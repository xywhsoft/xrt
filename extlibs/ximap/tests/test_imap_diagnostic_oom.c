#define main diagnosticExistingClientMain
#include "test_imap_client.c"
#undef main

typedef struct diagnostictrace {
    uint64 Owner;
    bool Active, ProtocolBeforeMemory, PrimaryMemory, ProtocolAfterMemory;
} diagnostictrace;

typedef struct diagnosticserver {
    xnetlistener* Listener;
    double Deadline;
    bool Body, Good, Binary, SeenOperation, Success;
} diagnosticserver;

typedef struct diagnosticsink {
    unsigned Calls;
    size_t Size;
    unsigned char Data[8];
} diagnosticsink;

typedef struct diagnosticresult {
    bool Valid, Triggered, PrimaryMemory, PrimaryAfterResponse, Masked;
} diagnosticresult;

/* Only the immutable owner is read by other threads. Observe errors without
 * allocating, replacing errors or arming faults outside the calling thread. */
static void diagnosticObserve(const xerror* error, ptr data)
{
    diagnostictrace* trace = (diagnostictrace*)data;
    if (xrtThreadCurrentId() != trace->Owner || !trace->Active) return;
    if (xrtErrorKind(error) == XERR_PROTOCOL) {
        if (trace->PrimaryMemory) trace->ProtocolAfterMemory = true;
        else trace->ProtocolBeforeMemory = true;
    }
    if (xrtErrorKind(error) == XERR_MEMORY && !trace->ProtocolBeforeMemory)
        trace->PrimaryMemory = true;
}

static bool diagnosticSend(xnetstream* stream, cstr text, double deadline)
{ return testImapSend(stream, text, strlen(text), deadline); }

/* 1: exact command; -1: EOF/error; 0: wrong bytes or timeout. */
static int diagnosticReceive(xnetstream* stream, cstr expected, double deadline)
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

static int32 diagnosticServer(ptr data)
{
    diagnosticserver* server = (diagnosticserver*)data;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, server->Deadline, NULL);
    bool valid = stream != NULL && diagnosticSend(stream, "* PREAUTH ready\r\n", server->Deadline) &&
        diagnosticReceive(stream, "A00000001 CAPABILITY\r\n", server->Deadline) == 1 &&
        diagnosticSend(stream, "* CAPABILITY IMAP4rev1\r\nA00000001 OK capabilities\r\n", server->Deadline);
    if (valid && server->Body) valid =
        diagnosticReceive(stream, "A00000002 SELECT \"INBOX\"\r\n", server->Deadline) == 1 &&
        diagnosticSend(stream, "* 1 EXISTS\r\nA00000002 OK selected\r\n", server->Deadline);
    if (valid && !server->Body) valid =
        diagnosticReceive(stream, "A00000002 APPEND \"INBOX\" {0}\r\n", server->Deadline) == 1 &&
        diagnosticSend(stream, "+ continue\r\n", server->Deadline);
    if (valid) {
        int received = diagnosticReceive(stream, server->Body ?
            "A00000003 FETCH 1 BODY.PEEK[]\r\n" : "\r\n", server->Deadline);
        server->SeenOperation = received == 1; valid = received != 0;
        if (server->SeenOperation) {
            cstr response = server->Body ? (server->Good ?
                "* 1 FETCH (BODY[] {1}\r\nx)\r\nA00000003 OK fetched\r\n" : server->Binary ?
                "* 1 FETCH (BODY[] ~{1}\r\nx)\r\nA00000003 OK fetched\r\n" :
                "* 0 FETCH (BODY[] {1}\r\nx)\r\nA00000003 OK fetched\r\n") : (server->Good ?
                "A00000002 OK [APPENDUID 51 77] appended\r\n" :
                "A00000002 OK [APPENDUID 0 1] invalid\r\n");
            /* A failed client can abort after consuming the response, before
             * the peer's send reports completion. Check EOF independently. */
            bool sent = diagnosticSend(stream, response, server->Deadline);
            if (server->Good) valid = sent &&
                diagnosticReceive(stream, server->Body ? "A00000004 NOOP\r\n" : "A00000003 NOOP\r\n", server->Deadline) == 1 &&
                diagnosticSend(stream, server->Body ? "A00000004 OK noop\r\n" : "A00000003 OK noop\r\n", server->Deadline) &&
                diagnosticReceive(stream, server->Body ? "A00000005 LOGOUT\r\n" : "A00000004 LOGOUT\r\n", server->Deadline) == 1 &&
                diagnosticSend(stream, server->Body ? "* BYE closing\r\nA00000005 OK logout\r\n" :
                    "* BYE closing\r\nA00000004 OK logout\r\n", server->Deadline);
            else valid = diagnosticReceive(stream, "x", server->Deadline) == -1;
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

static bool diagnosticWrite(xbytesview data, ptr user)
{
    diagnosticsink* sink = (diagnosticsink*)user;
    if (data.Size > sizeof(sink->Data) - sink->Size) return false;
    memcpy(sink->Data + sink->Size, data.Data, data.Size);
    sink->Size += data.Size; sink->Calls++; return true;
}

/* fault=-3: prior MEMORY/binary metadata control, -2: valid reuse control,
 * -1: normal invalid response, 0..31: configured one-shot allocation slots. */
static diagnosticresult diagnosticCase(const ximapclientconfig* config,
    xnetlistener* listener, diagnostictrace* trace, bool body, int fault)
{
    diagnosticserver server;
    xthread* thread; ximapclient* client;
    ximapappendconfig append; ximapappendresult result, original;
    ximapmailboxinfo info; ximapresponseview last;
    diagnosticsink sink = {0}; unsigned char original_data[sizeof(sink.Data)];
    size_t written = 0x1234u; xerror* prior = NULL;
    diagnosticresult outcome = {0}; xerrkind kind; ximapclientstate state;
    bool returned, valid, joined = false, saved_bad_tag;
    server.Listener = listener; server.Deadline = __xrtWaitAfter(10000);
    server.Body = body; server.Good = fault == -2; server.Binary = fault == -3;
    server.SeenOperation = false; server.Success = false;
    thread = xrtThreadCreate(diagnosticServer, &server, 0);
    testRequire(thread != NULL, "diagnostic peer creation failed");
    client = xrtImapClientOpen(config, 10000, NULL);
    testRequire(client != NULL, "diagnostic public Open failed");
    if (body) testRequire(xrtImapClientSelect(client, XRT_STR_LITERAL("INBOX"), &info, 10000, NULL),
        "diagnostic BODY selection failed");
    else {
        xrtImapAppendConfigInit(&append); append.Mailbox = XRT_STR_LITERAL("INBOX");
        testRequire(xrtImapClientAppendBegin(client, &append, 10000, NULL), "diagnostic APPEND Begin failed");
    }
    memset(&result, 0xA5, sizeof(result)); memcpy(&original, &result, sizeof(result));
    memset(sink.Data, 0xA5, sizeof(sink.Data)); memcpy(original_data, sink.Data, sizeof(original_data));
    trace->ProtocolBeforeMemory = false; trace->PrimaryMemory = false; trace->ProtocolAfterMemory = false;
    xrtClearError();
    if (fault == -3) {
        prior = xrtErrorCreate(XERR_MEMORY, "imap.test", 73, "prior error");
        testRequire(prior != NULL, "diagnostic prior error creation failed"); xrtSetError(prior);
    }
    trace->Active = true;
    if (fault >= 0) testRequire(xrtMemDebugFailAfter((uint64)fault), "diagnostic fault setup failed");
    returned = body ? xrtImapClientBodyWrite(client, 1u, XRT_STR_LITERAL(""), false, true, 100u,
        diagnosticWrite, &sink, &written, 10000, NULL) : xrtImapClientAppendEnd(client, &result, 10000, NULL);
    trace->Active = false; kind = xrtErrorKind(xrtGetError());
    outcome.Triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); state = xrtImapClientState(client);
    testRequire(xrtImapClientLastResponse(client, &last), "diagnostic saved response missing");
    saved_bad_tag = testMailViewEqual(last.Source, XRT_STR_LITERAL("A00000002 OK [APPENDUID 0 1] invalid"));
    outcome.PrimaryMemory = trace->PrimaryMemory;
    outcome.Masked = trace->PrimaryMemory && (kind != XERR_MEMORY || trace->ProtocolAfterMemory);
    if (fault == -2) {
        valid = returned && kind == XERR_NONE && !outcome.Triggered && !trace->PrimaryMemory &&
            state == (body ? XIMAP_CLIENT_SELECTED : XIMAP_CLIENT_AUTHENTICATED);
        if (body) valid = valid && written == 1u && sink.Calls == 1u && sink.Size == 1u && sink.Data[0] == 'x' &&
            testMailViewEqual(last.Source, XRT_STR_LITERAL("A00000003 OK fetched"));
        else valid = valid && result.Present && result.UidValidity == 51u && result.Uid == 77u &&
            testMailViewEqual(last.Source, XRT_STR_LITERAL("A00000002 OK [APPENDUID 51 77] appended"));
        valid = xrtImapClientNoop(client, 10000, NULL) && valid;
        valid = xrtImapClientLogout(client, 10000, NULL) && valid;
        valid = valid && xrtImapClientState(client) == XIMAP_CLIENT_CLOSED;
    } else {
        valid = !returned && (kind == XERR_MEMORY || kind == XERR_PROTOCOL) && !outcome.Masked;
        if (body) valid = valid && written == 0x1234u && sink.Calls == 0 && sink.Size == 0 &&
            memcmp(sink.Data, original_data, sizeof(original_data)) == 0 &&
            testMailViewEqual(last.Source, XRT_STR_LITERAL("A00000002 OK selected"));
        else valid = valid && memcmp(&result, &original, sizeof(result)) == 0;
        if (fault < 0) valid = valid && kind == XERR_PROTOCOL && !outcome.Triggered && !trace->PrimaryMemory &&
            xrtErrorCode(xrtGetError()) == XMAIL_ERROR_PROTOCOL &&
            strcmp(xrtErrorDomain(xrtGetError()), "xrt.mail") == 0 &&
            strcmp(xrtErrorMessage(xrtGetError()), body ? "invalid IMAP BODY literal response" :
                "invalid IMAP APPEND completion") == 0 &&
            state == (body ? XIMAP_CLIENT_CLOSED : XIMAP_CLIENT_FAILED) && (body || saved_bad_tag);
    }
    if (state == XIMAP_CLIENT_FAILED || state == XIMAP_CLIENT_CLOSED || fault == -2) {
        joined = __xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK;
        valid = valid && joined && server.Success && xrtThreadExitCode(thread) == 0;
    } else valid = false;
    xrtClearError(); xrtErrorFree(prior);
    testRequire(xrtImapClientAbort(client), "diagnostic Abort failed"); xrtImapClientDestroy(client);
    if (!joined) testRequire(__xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK, "diagnostic peer join failed");
    valid = valid && server.Success && xrtThreadExitCode(thread) == 0;
    outcome.PrimaryAfterResponse = outcome.PrimaryMemory && server.SeenOperation && (body || saved_bad_tag);
    xrtThreadDestroy(thread); outcome.Valid = valid;
    if (!valid) fprintf(stderr, "diagnostic body=%d fault=%d primary=%d protocol_after=%d kind=%d state=%d\n",
        body, fault, outcome.PrimaryMemory, trace->ProtocolAfterMemory, (int)kind, (int)state);
    return outcome;
}

static bool diagnosticReport(bool valid, cstr name)
{
    printf("[%s] IMAP diagnostic %s\n", valid ? "PASS" : "FAIL", name);
    xrtClearError(); return valid;
}

int main(void)
{
    xnetengineconfig ec; xnetlistenconfig lc; xnetresolverconfig rc; ximapclientconfig config;
    xnetengine* engine; xnetlistener* listener; xnetresolver* resolver; xnetenginestats stats;
    diagnostictrace trace = {0}; bool all = true;
    testRequire(xrtMemDebugEnable(true), "diagnostic memory debug unavailable"); trace.Owner = xrtThreadCurrentId();
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec); testRequire(engine != NULL && xrtNetEngineStart(engine), "diagnostic Engine failed");
    xrtNetListenConfigInit(&lc); testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "diagnostic address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestImapAddress), "diagnostic Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testImapResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc); testRequire(resolver != NULL, "diagnostic Resolver failed");
    xrtImapClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "imap.test"; config.Net.Port = TestImapAddress.Port; xrtSetErrorHandler(diagnosticObserve, &trace);
    for (unsigned mode = 0; mode < 2u; mode++) {
        bool sweep = true, last_triggered = true; unsigned primary = 0, remote_primary = 0, masked = 0, rounds = 0;
        for (unsigned round = 0; round < 8u; round++) {
        for (int fault = round ? 0 : (mode ? -3 : -2); fault < 32; fault++) {
            diagnosticresult r = diagnosticCase(&config, listener, &trace, mode != 0, fault);
            if (fault == -3) { if (!diagnosticReport(r.Valid, "BODY binary metadata replaces prior MEMORY")) all = false; }
            else if (fault == -2) { if (!diagnosticReport(r.Valid, mode ? "BODY valid bytes and session reuse" : "APPEND valid UID and session reuse")) all = false; }
            else if (fault == -1) { if (!diagnosticReport(r.Valid, mode ? "BODY invalid FETCH normal diagnostic" : "APPEND invalid UID normal diagnostic")) all = false; }
            else { sweep = sweep && r.Valid; primary += r.PrimaryMemory ? 1u : 0u;
                remote_primary += r.PrimaryAfterResponse ? 1u : 0u; masked += r.Masked ? 1u : 0u; last_triggered = r.Triggered; }
        }
        rounds++;
        if (remote_primary != 0) break;
        }
        sweep = sweep && primary != 0 && remote_primary != 0 && !last_triggered;
        printf("diagnostic mode=%u slots=32 rounds=%u primary=%u remote_primary=%u masked=%u\n", mode, rounds, primary, remote_primary, masked);
        if (!diagnosticReport(sweep, mode ? "BODY primary diagnostic OOM preserved" : "APPEND primary diagnostic OOM preserved")) all = false;
    }
    testRequire(xrtNetListenerClose(listener), "diagnostic Listener close failed"); double deadline = __xrtWaitAfter(10000);
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) { testRequire(!__xrtWaitExpired(deadline), "diagnostic Listener timeout"); xrtThreadYield(); }
    xrtNetListenerDestroy(listener); testRequire(xrtNetResolverDestroy(resolver), "diagnostic Resolver destroy failed");
    for (;;) { xnetretireresult next = xrtNetEngineTryStop(engine); if (next == XNET_RETIRE_READY) break;
        testRequire(next == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline), "diagnostic Engine timeout"); xrtThreadYield(); }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 && stats.PendingCommands == 0 &&
        stats.ActiveTimers == 0 && stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "diagnostic retained Engine resources");
    xrtSetErrorHandler(NULL, NULL); xrtClearError(); testMemoryDebugDrain("diagnostic resource leak"); return all ? 0 : 1;
}
