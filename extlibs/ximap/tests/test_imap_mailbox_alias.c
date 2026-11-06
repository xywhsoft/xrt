#define main aliasExistingClientMain
#include "test_imap_client.c"
#undef main

typedef struct mailboxstorage {
    unsigned char Prefix[64];
    ximapmailboxinfo Info;
    unsigned char Tail[128];
} mailboxstorage;

static bool aliasResult(bool valid, cstr name)
{
    printf("[%s] IMAP mailbox alias %s\n", valid ? "PASS" : "FAIL", name);
    xrtClearError();
    return valid;
}

static void aliasParse(cstr text, ximapresponseview* response)
{
    testRequire(xrtImapResponseParse((xstrview){ text, strlen(text) }, response),
        "mailbox alias public response parse failed");
}

static bool aliasReject(ximapresponseview* response, ximapmailboxinfo* info,
    const void* backing, size_t size, bool oom, cstr name)
{
    unsigned char snapshot[512];
    ximapresponseview original;
    xmailnext next;
    xerrkind kind;
    bool triggered, valid;
    testRequire(size <= sizeof(snapshot), "mailbox alias snapshot capacity failed");
    memcpy(snapshot, backing, size); memcpy(&original, response, sizeof(original));
    xrtClearError();
    if (oom) testRequire(xrtMemDebugFailAfter(0), "mailbox alias diagnostic OOM setup failed");
    next = xrtImapMailboxInfoUpdate(response, info);
    kind = xrtErrorKind(xrtGetError()); triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    valid = next == XMAIL_NEXT_ERROR && kind == (oom ? XERR_MEMORY : XERR_ARGUMENT) &&
        triggered == oom && memcmp(snapshot, backing, size) == 0 &&
        memcmp(&original, response, sizeof(original)) == 0;
    if (!oom) valid = valid && xrtErrorCode(xrtGetError()) == 0 &&
        strcmp(xrtErrorDomain(xrtGetError()), "xrt.mail") == 0;
    if (!valid) fprintf(stderr, "mailbox alias %s next=%d kind=%d triggered=%d unchanged=%d\n",
        name, (int)next, (int)kind, triggered, memcmp(snapshot, backing, size) == 0);
    return aliasResult(valid, name);
}

/* All borrowed texts originate in public parser results. A detached Source or
 * Text is copied from another parser result for the identical logical line. */
static bool aliasBorrowed(unsigned mode, bool oom, cstr name)
{
    mailboxstorage storage;
    ximapresponseview response, separate;
    cstr line = mode == 4u ? "* 1 X-VENDOR" : "* 1 EXISTS";
    size_t length = strlen(line), offset = offsetof(mailboxstorage, Info);
    char* source;
    memset(&storage, 0, sizeof(storage)); xrtImapMailboxInfoInit(&storage.Info);
    if (mode == 1u) {
        testRequire(offsetof(ximapmailboxinfo, ReadOnly) + sizeof(storage.Info.ReadOnly) < sizeof(storage.Info),
            "mailbox alias public layout lacks trailing padding");
        source = (char*)&storage + offset + sizeof(storage.Info) - 1u;
    } else source = (char*)&storage + offset - (length - 1u);
    memcpy(source, line, length);
    testRequire(storage.Info.Present == 0 && !storage.Info.ReadOnly,
        "mailbox alias fixture damaged active flags");
    testRequire(xrtImapResponseParse((xstrview){ source, length }, &response),
        "mailbox alias overlapping text parse failed");
    aliasParse(line, &separate);
    if (mode == 2u) response.Text = separate.Text;
    if (mode == 3u) response.Source = separate.Source;
    if (mode == 1u || mode == 2u) testRequire(
        xrtMemRangesOverlap(&storage.Info, sizeof(storage.Info), response.Source.Data, response.Source.Size) &&
        !xrtMemRangesOverlap(&storage.Info, sizeof(storage.Info), response.Text.Data, response.Text.Size),
        "mailbox alias Source-only fixture overlaps Text");
    if (mode == 3u) testRequire(
        !xrtMemRangesOverlap(&storage.Info, sizeof(storage.Info), response.Source.Data, response.Source.Size) &&
        xrtMemRangesOverlap(&storage.Info, sizeof(storage.Info), response.Text.Data, response.Text.Size),
        "mailbox alias Text-only fixture overlaps Source");
    return aliasReject(&response, &storage.Info, &storage, sizeof(storage), oom, name);
}

static bool aliasInvalidRange(unsigned mode, cstr name)
{
    ximapresponseview response;
    ximapmailboxinfo info;
    aliasParse("* 1 EXISTS", &response); xrtImapMailboxInfoInit(&info);
    if (mode == 0u) response.Source = (xstrview){ NULL, 1u };
    if (mode == 1u) response.Source = (xstrview){ (const char*)(uintptr_t)UINTPTR_MAX, 2u };
    if (mode == 2u) response.Text = (xstrview){ NULL, 1u };
    if (mode == 3u) response.Text = (xstrview){ (const char*)(uintptr_t)UINTPTR_MAX, 2u };
    return aliasReject(&response, &info, &info, sizeof(info), false, name);
}

static bool aliasObject(bool oom, cstr name)
{
    union { ximapresponseview Response; ximapmailboxinfo Info; } storage;
    memset(&storage, 0, sizeof(storage)); aliasParse("* 1 EXISTS", &storage.Response);
    return aliasReject(&storage.Response, &storage.Info, &storage, sizeof(storage), oom, name);
}

static bool aliasAdjacentSource(bool after, cstr name)
{
    mailboxstorage storage;
    ximapresponseview response, original;
    static const char line[] = "* 1 EXISTS";
    size_t length = sizeof(line) - 1u, offset = offsetof(mailboxstorage, Info);
    char* source;
    bool valid;
    memset(&storage, 0, sizeof(storage)); xrtImapMailboxInfoInit(&storage.Info);
    source = (char*)&storage + (after ? offset + sizeof(storage.Info) : offset - length);
    memcpy(source, line, length);
    testRequire(xrtImapResponseParse((xstrview){ source, length }, &response), "mailbox alias adjacent parse failed");
    memcpy(&original, &response, sizeof(original)); xrtClearError();
    valid = xrtImapMailboxInfoUpdate(&response, &storage.Info) == XMAIL_NEXT_ITEM &&
        storage.Info.Exists == 1u && storage.Info.Present == XIMAP_MAILBOX_EXISTS &&
        memcmp(source, line, length) == 0 && memcmp(&response, &original, sizeof(response)) == 0 &&
        xrtErrorKind(xrtGetError()) == XERR_NONE;
    return aliasResult(valid, name);
}

static bool aliasAdjacentObject(bool after, cstr name)
{
    struct { ximapresponseview Response; ximapmailboxinfo Info; } forward;
    struct { ximapmailboxinfo Info; ximapresponseview Response; } reverse;
    ximapresponseview* response = after ? &forward.Response : &reverse.Response;
    ximapmailboxinfo* info = after ? &forward.Info : &reverse.Info;
    ximapresponseview original;
    bool valid;
    memset(&forward, 0, sizeof(forward)); memset(&reverse, 0, sizeof(reverse));
    testRequire(after ? (uintptr_t)info == (uintptr_t)response + sizeof(*response) :
        (uintptr_t)response == (uintptr_t)info + sizeof(*info), "mailbox alias objects are not adjacent");
    aliasParse("* 1 EXISTS", response); xrtImapMailboxInfoInit(info); memcpy(&original, response, sizeof(original));
    xrtClearError(); valid = xrtImapMailboxInfoUpdate(response, info) == XMAIL_NEXT_ITEM &&
        info->Exists == 1u && info->Present == XIMAP_MAILBOX_EXISTS &&
        memcmp(&original, response, sizeof(original)) == 0 && xrtErrorKind(xrtGetError()) == XERR_NONE;
    return aliasResult(valid, name);
}

static bool aliasDisjoint(bool unknown, cstr name)
{
    ximapresponseview response;
    ximapmailboxinfo info, expected;
    xerror* prior;
    bool valid;
    aliasParse(unknown ? "* 1 X-VENDOR payload" : "* 1 EXISTS", &response);
    xrtImapMailboxInfoInit(&info); info.Exists = 9u; info.Present = XIMAP_MAILBOX_EXISTS;
    memcpy(&expected, &info, sizeof(info)); if (!unknown) expected.Exists = 1u;
    prior = xrtErrorCreate(XERR_VALUE, "mailbox.test", 73, "preserved error");
    testRequire(prior != NULL, "mailbox alias prior error creation failed"); xrtSetError(prior);
    valid = xrtImapMailboxInfoUpdate(&response, &info) == (unknown ? XMAIL_NEXT_END : XMAIL_NEXT_ITEM) &&
        memcmp(&info, &expected, sizeof(info)) == 0 && xrtGetError() == prior &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE && xrtErrorCode(xrtGetError()) == 73;
    xrtClearError(); xrtErrorFree(prior);
    return aliasResult(valid, name);
}

static bool aliasEmptySource(void)
{
    ximapresponseview response;
    ximapmailboxinfo info;
    bool valid;
    aliasParse("* 1 EXISTS", &response); response.Source = (xstrview){ NULL, 0 };
    xrtImapMailboxInfoInit(&info); xrtClearError();
    valid = xrtImapMailboxInfoUpdate(&response, &info) == XMAIL_NEXT_ITEM && info.Exists == 1u &&
        xrtErrorKind(xrtGetError()) == XERR_NONE;
    return aliasResult(valid, "empty Source with separate Text remains valid");
}

static bool aliasNumericErrorOom(void)
{
    ximapresponseview response;
    ximapmailboxinfo info;
    aliasParse("* OK [UIDNEXT 0] invalid", &response); xrtImapMailboxInfoInit(&info);
    /* The existing invalid-number diagnostic also allocates; verify its OOM
     * fallback directly, without attributing untested high-level behavior. */
    return aliasReject(&response, &info, &info, sizeof(info), true, "numeric diagnostic OOM preserves output");
}

typedef struct aliasdiagtrace {
    uint64 Owner;
    bool Active;
    bool ProtocolBeforeMemory;
    bool PrimaryMemory;
} aliasdiagtrace;

typedef struct aliasdiagserver {
    xnetlistener* Listener;
    double Deadline;
    bool Tagged;
    bool SeenSelect;
    bool Success;
} aliasdiagserver;

/* Observe public error notifications without changing errors, allocating or
 * arming faults in another thread. Existing protocol before a cleanup OOM is
 * distinguished from a primary MEMORY later replaced by protocol reporting. */
static void aliasDiagObserve(const xerror* error, ptr data)
{
    aliasdiagtrace* trace = (aliasdiagtrace*)data;
    if (xrtThreadCurrentId() != trace->Owner || !trace->Active) return;
    if (xrtErrorKind(error) == XERR_PROTOCOL && !trace->PrimaryMemory) trace->ProtocolBeforeMemory = true;
    if (xrtErrorKind(error) == XERR_MEMORY && !trace->ProtocolBeforeMemory) trace->PrimaryMemory = true;
}

static bool aliasDiagSend(xnetstream* stream, cstr text, double deadline)
{ return testImapSend(stream, text, strlen(text), deadline); }

/* 1 = complete exact command; -1 = observed EOF/error; 0 = bad bytes/timeout. */
static int aliasDiagReceive(xnetstream* stream, cstr expected, double deadline)
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

static int32 aliasDiagServer(ptr data)
{
    aliasdiagserver* server = (aliasdiagserver*)data;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, server->Deadline, NULL);
    bool valid = stream != NULL && aliasDiagSend(stream, "* PREAUTH ready\r\n", server->Deadline) &&
        aliasDiagReceive(stream, "A00000001 CAPABILITY\r\n", server->Deadline) == 1 &&
        aliasDiagSend(stream, "* CAPABILITY IMAP4rev1\r\nA00000001 OK capabilities\r\n", server->Deadline);
    if (valid) {
        int received = aliasDiagReceive(stream, "A00000002 SELECT \"INBOX\"\r\n", server->Deadline);
        server->SeenSelect = received == 1;
        valid = received != 0;
        if (server->SeenSelect) {
            /* The client may retire after consuming the invalid response but
             * before the peer observes send completion. Still require EOF. */
            (void)aliasDiagSend(stream, server->Tagged ?
                "A00000002 OK [UIDNEXT 0] invalid\r\n" :
                "* OK [UIDNEXT 0] invalid\r\nA00000002 OK selected\r\n", server->Deadline);
            valid = aliasDiagReceive(stream, "x", server->Deadline) == -1;
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

static bool aliasDiagnostic(void)
{
    xnetengineconfig ec; xnetlistenconfig lc; xnetresolverconfig rc; ximapclientconfig config;
    xnetengine* engine; xnetlistener* listener; xnetresolver* resolver; xnetenginestats stats;
    aliasdiagtrace trace = {0}; bool all = true;
    trace.Owner = xrtThreadCurrentId();
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec); testRequire(engine != NULL && xrtNetEngineStart(engine), "mailbox diagnostic Engine failed");
    xrtNetListenConfigInit(&lc); testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "mailbox diagnostic address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestImapAddress), "mailbox diagnostic Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testImapResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc); testRequire(resolver != NULL, "mailbox diagnostic Resolver failed");
    xrtImapClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "imap.test"; config.Net.Port = TestImapAddress.Port;
    xrtSetErrorHandler(aliasDiagObserve, &trace);
    for (unsigned mode = 0; mode < 2u; mode++) {
        bool sweep = true; unsigned primary = 0, primary_after_response = 0, masked = 0, rounds = 0;
        bool last_triggered = true;
        for (unsigned round = 0; round < 8u; round++) {
        for (int n = round ? 0 : -1; n < 32; n++) {
            aliasdiagserver server; xthread* thread; ximapclient* client; ximapmailboxinfo info;
            ximapresponseview last; xerrkind kind; ximapclientstate state;
            bool returned, triggered, valid, joined = false, saved_bad_tag;
            server.Listener = listener; server.Deadline = __xrtWaitAfter(10000); server.Tagged = mode != 0;
            server.SeenSelect = false; server.Success = false;
            thread = xrtThreadCreate(aliasDiagServer, &server, 0); testRequire(thread != NULL, "mailbox diagnostic peer failed");
            client = xrtImapClientOpen(&config, 10000, NULL); testRequire(client != NULL, "mailbox diagnostic public Open failed");
            trace.ProtocolBeforeMemory = false; trace.PrimaryMemory = false; xrtClearError();
            trace.Active = true;
            if (n >= 0) testRequire(xrtMemDebugFailAfter((uint64)n), "mailbox diagnostic fault setup failed");
            returned = xrtImapClientSelect(client, XRT_STR_LITERAL("INBOX"), &info, 10000, NULL);
            trace.Active = false; kind = xrtErrorKind(xrtGetError()); triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear(); state = xrtImapClientState(client);
            testRequire(xrtImapClientLastResponse(client, &last), "mailbox diagnostic response missing");
            saved_bad_tag = testMailViewEqual(last.Source, XRT_STR_LITERAL("A00000002 OK [UIDNEXT 0] invalid"));
            valid = !returned && (kind == XERR_MEMORY || kind == XERR_PROTOCOL);
            if (trace.PrimaryMemory) { primary++; if (kind != XERR_MEMORY) { valid = false; masked++; } }
            if (state == XIMAP_CLIENT_FAILED) {
                joined = __xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK;
                valid = valid && joined && server.Success && xrtThreadExitCode(thread) == 0;
            }
            if (n < 0) valid = valid && kind == XERR_PROTOCOL && state == XIMAP_CLIENT_FAILED &&
                !triggered && !trace.PrimaryMemory && (mode == 0u || saved_bad_tag) &&
                strcmp(xrtErrorMessage(xrtGetError()), mode ? "invalid IMAP mailbox response code" :
                    "invalid IMAP mailbox status response") == 0;
            xrtClearError(); testRequire(xrtImapClientAbort(client), "mailbox diagnostic Abort failed"); xrtImapClientDestroy(client);
            if (!joined) testRequire(__xrtThreadWaitUntil(thread, server.Deadline) == XWAIT_OK, "mailbox diagnostic peer join failed");
            valid = valid && server.Success && xrtThreadExitCode(thread) == 0;
            if (trace.PrimaryMemory && server.SeenSelect && (mode == 0u || saved_bad_tag)) primary_after_response++;
            xrtThreadDestroy(thread);
            if (n < 0) {
                if (!valid) fprintf(stderr, "mailbox normal mode=%u kind=%d state=%d triggered=%d primary=%d joined=%d peer=%d seen=%d saved=%d\n",
                    mode, (int)kind, (int)state, triggered, trace.PrimaryMemory, joined, server.Success, server.SeenSelect, saved_bad_tag);
                if (!aliasResult(valid, mode ? "tagged invalid response normal diagnostic" : "untagged invalid response normal diagnostic")) all = false;
            } else {
                if (!valid) fprintf(stderr, "mailbox diagnostic mode=%u fault=%d primary_memory=%d kind=%d state=%d\n",
                    mode, n, trace.PrimaryMemory, (int)kind, (int)state);
                sweep = sweep && valid; last_triggered = triggered;
            }
        }
        rounds++;
        if (primary_after_response != 0) break;
        }
        sweep = sweep && primary != 0 && primary_after_response != 0 && !last_triggered;
        printf("mailbox diagnostic mode=%u slots=32 rounds=%u primary=%u response_primary=%u masked=%u\n",
            mode, rounds, primary, primary_after_response, masked);
        if (!aliasResult(sweep, mode ? "tagged primary diagnostic OOM preserved" : "untagged primary diagnostic OOM preserved")) all = false;
    }
    xrtSetErrorHandler(NULL, NULL);
    testRequire(xrtNetListenerClose(listener), "mailbox diagnostic Listener close failed");
    double deadline = __xrtWaitAfter(10000);
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) { testRequire(!__xrtWaitExpired(deadline), "mailbox diagnostic Listener timeout"); xrtThreadYield(); }
    xrtNetListenerDestroy(listener); testRequire(xrtNetResolverDestroy(resolver), "mailbox diagnostic Resolver failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine); if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline), "mailbox diagnostic Engine timeout"); xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 && stats.PendingCommands == 0 &&
        stats.ActiveTimers == 0 && stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "mailbox diagnostic retained Engine resources");
    return all;
}

int main(void)
{
    bool valid = true;
    testRequire(xrtMemDebugEnable(true), "mailbox alias memory diagnostics failed");
    if (!aliasBorrowed(0u, false, "borrowed numeric text overlap")) valid = false;
    if (!aliasBorrowed(1u, false, "one-byte Source-only overlap")) valid = false;
    if (!aliasBorrowed(2u, false, "detached Text with overlapping Source")) valid = false;
    if (!aliasBorrowed(3u, false, "detached Source with overlapping Text")) valid = false;
    if (!aliasBorrowed(4u, false, "unknown numeric overlap is an argument error")) valid = false;
    if (!aliasInvalidRange(0u, "nonempty NULL Source")) valid = false;
    if (!aliasInvalidRange(1u, "wrapping Source range")) valid = false;
    if (!aliasInvalidRange(2u, "nonempty NULL Text")) valid = false;
    if (!aliasInvalidRange(3u, "wrapping Text range")) valid = false;
    if (!aliasObject(false, "response object overlap")) valid = false;
    if (!aliasAdjacentSource(true, "Source immediately after output")) valid = false;
    if (!aliasAdjacentSource(false, "Source immediately before output")) valid = false;
    if (!aliasAdjacentObject(true, "response object immediately before output")) valid = false;
    if (!aliasAdjacentObject(false, "response object immediately after output")) valid = false;
    if (!aliasDisjoint(false, "known disjoint update preserves prior error")) valid = false;
    if (!aliasDisjoint(true, "unknown disjoint update preserves prior error")) valid = false;
    if (!aliasEmptySource()) valid = false;
    if (!aliasBorrowed(0u, true, "overlap diagnostic OOM preserves input and output")) valid = false;
    if (!aliasNumericErrorOom()) valid = false;
    if (!aliasObject(true, "object overlap diagnostic OOM")) valid = false;
    if (!aliasDiagnostic()) valid = false;
    xrtClearError(); testMemoryDebugDrain("mailbox alias leaked resources");
    return valid ? 0 : 1;
}
