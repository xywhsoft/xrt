#define main testImapExistingClientMain
#include "test_imap_client.c"
#undef main

typedef enum mailboxfield {
    MAILBOX_FIELD_NONE, MAILBOX_FIELD_EXISTS, MAILBOX_FIELD_RECENT,
    MAILBOX_FIELD_UNSEEN, MAILBOX_FIELD_VALIDITY, MAILBOX_FIELD_NEXT,
    MAILBOX_FIELD_MODSEQ, MAILBOX_FIELD_EXPUNGE
} mailboxfield;

typedef struct mailboxcase {
    cstr Name;
    cstr Line;
    mailboxfield Field;
    uint64 Value;
    xmailnext Next;
    xerrkind Error;
} mailboxcase;

static void numbersSnapshot(ximapmailboxinfo* info)
{
    xrtImapMailboxInfoInit(info);
    info->Exists = UINT32_MAX; info->Recent = 11u; info->Unseen = 12u;
    info->UidValidity = 13u; info->UidNext = 14u; info->HighestModSeq = 15u;
    info->Present = XIMAP_MAILBOX_EXISTS | XIMAP_MAILBOX_ACCESS; info->ReadOnly = true;
}

static bool numbersResult(bool valid, cstr name)
{
    printf("[%s] IMAP mailbox numbers %s\n", valid ? "PASS" : "FAIL", name);
    xrtClearError();
    return valid;
}

static bool numbersPure(void)
{
    static const mailboxcase cases[] = {
        { "UIDNEXT zero", "* OK [UIDNEXT 0] next", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "UIDNEXT over32", "* OK [UIDNEXT 4294967296] next", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_RANGE },
        { "UIDVALIDITY zero", "* OK [UIDVALIDITY 0] stable", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "UIDVALIDITY over32", "* OK [UIDVALIDITY 4294967296] stable", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_RANGE },
        { "UNSEEN sequence zero", "* OK [UNSEEN 0] unseen", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "UNSEEN sequence over32", "* OK [UNSEEN 4294967296] unseen", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_RANGE },
        { "EXISTS over32", "* 4294967296 EXISTS", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_RANGE },
        { "RECENT over32", "* 4294967296 RECENT", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_RANGE },
        { "EXPUNGE zero", "* 0 EXPUNGE", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "EXPUNGE over32", "* 4294967296 EXPUNGE", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_RANGE },
        { "UIDNEXT one", "* OK [UIDNEXT 1] next", MAILBOX_FIELD_NEXT, 1u, XMAIL_NEXT_ITEM, XERR_NONE },
        { "UIDNEXT max32", "A1 OK [UIDNEXT 4294967295] next", MAILBOX_FIELD_NEXT, UINT32_MAX, XMAIL_NEXT_ITEM, XERR_NONE },
        { "UIDVALIDITY one", "* OK [UIDVALIDITY 1] stable", MAILBOX_FIELD_VALIDITY, 1u, XMAIL_NEXT_ITEM, XERR_NONE },
        { "UIDVALIDITY max32", "* OK [UIDVALIDITY 4294967295] stable", MAILBOX_FIELD_VALIDITY, UINT32_MAX, XMAIL_NEXT_ITEM, XERR_NONE },
        { "UNSEEN sequence one", "* OK [UNSEEN 1] unseen", MAILBOX_FIELD_UNSEEN, 1u, XMAIL_NEXT_ITEM, XERR_NONE },
        { "UNSEEN sequence max32", "* OK [UNSEEN 4294967295] unseen", MAILBOX_FIELD_UNSEEN, UINT32_MAX, XMAIL_NEXT_ITEM, XERR_NONE },
        { "EXISTS zero", "* 0 EXISTS", MAILBOX_FIELD_EXISTS, 0, XMAIL_NEXT_ITEM, XERR_NONE },
        { "EXISTS max32", "* 4294967295 EXISTS", MAILBOX_FIELD_EXISTS, UINT32_MAX, XMAIL_NEXT_ITEM, XERR_NONE },
        { "RECENT zero", "* 0 RECENT", MAILBOX_FIELD_RECENT, 0, XMAIL_NEXT_ITEM, XERR_NONE },
        { "RECENT max32", "* 4294967295 RECENT", MAILBOX_FIELD_RECENT, UINT32_MAX, XMAIL_NEXT_ITEM, XERR_NONE },
        { "EXPUNGE one", "* 1 EXPUNGE", MAILBOX_FIELD_EXPUNGE, 0, XMAIL_NEXT_ITEM, XERR_NONE },
        { "EXPUNGE max32", "* 4294967295 EXPUNGE", MAILBOX_FIELD_EXPUNGE, 0, XMAIL_NEXT_ITEM, XERR_NONE },
        { "legacy MODSEQ zero", "* OK [HIGHESTMODSEQ 0] legacy", MAILBOX_FIELD_MODSEQ, 0, XMAIL_NEXT_ITEM, XERR_NONE },
        { "legacy MODSEQ max64", "* OK [HIGHESTMODSEQ 18446744073709551615] legacy", MAILBOX_FIELD_MODSEQ, UINT64_MAX, XMAIL_NEXT_ITEM, XERR_NONE },
        { "unknown code stays generic", "* OK [X-VENDOR 18446744073709551616] extension", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_END, XERR_NONE },
        { "unknown numeric extension text", "* 18446744073709551615 X-VENDOR payload", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_END, XERR_NONE },
        { "UIDNEXT nonnumber", "* OK [UIDNEXT invalid] next", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "UIDNEXT plus sign", "* OK [UIDNEXT +1] next", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "UIDNEXT over64", "* OK [UIDNEXT 18446744073709551616] next", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "EXISTS trailing data", "* 1 EXISTS extra", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "RECENT trailing data", "* 1 RECENT extra", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL },
        { "EXPUNGE trailing data", "* 1 EXPUNGE extra", MAILBOX_FIELD_NONE, 0, XMAIL_NEXT_ERROR, XERR_PROTOCOL }
    };
    bool all = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const mailboxcase* c = &cases[i];
        ximapmailboxinfo info, expected;
        ximapresponseview response, original_response;
        xmailnext next;
        bool valid;
        numbersSnapshot(&info); memcpy(&expected, &info, sizeof(info));
        testRequire(xrtImapResponseParse((xstrview){ c->Line, strlen(c->Line) }, &response),
            "mailbox numeric fixture response parse failed");
        memcpy(&original_response, &response, sizeof(response));
        if (c->Field == MAILBOX_FIELD_EXISTS) { expected.Exists = c->Value; expected.Present |= XIMAP_MAILBOX_EXISTS; }
        if (c->Field == MAILBOX_FIELD_RECENT) { expected.Recent = c->Value; expected.Present |= XIMAP_MAILBOX_RECENT; }
        if (c->Field == MAILBOX_FIELD_UNSEEN) { expected.Unseen = c->Value; expected.Present |= XIMAP_MAILBOX_UNSEEN; }
        if (c->Field == MAILBOX_FIELD_VALIDITY) { expected.UidValidity = c->Value; expected.Present |= XIMAP_MAILBOX_UID_VALIDITY; }
        if (c->Field == MAILBOX_FIELD_NEXT) { expected.UidNext = c->Value; expected.Present |= XIMAP_MAILBOX_UID_NEXT; }
        if (c->Field == MAILBOX_FIELD_MODSEQ) { expected.HighestModSeq = c->Value; expected.Present |= XIMAP_MAILBOX_HIGHEST_MODSEQ; }
        if (c->Field == MAILBOX_FIELD_EXPUNGE) expected.Exists--;
        xrtClearError(); next = xrtImapMailboxInfoUpdate(&response, &info);
        valid = next == c->Next && xrtErrorKind(xrtGetError()) == c->Error &&
            memcmp(&info, &expected, sizeof(info)) == 0 &&
            memcmp(&response, &original_response, sizeof(response)) == 0;
        if (next == XMAIL_NEXT_ERROR) valid = valid && xrtErrorCode(xrtGetError()) == XMAIL_ERROR_PROTOCOL &&
            strcmp(xrtErrorDomain(xrtGetError()), "xrt.mail") == 0;
        if (!valid) fprintf(stderr, "mailbox pure case=%zu next=%d expected=%d kind=%d expected_kind=%d\n",
            i, (int)next, (int)c->Next, (int)xrtErrorKind(xrtGetError()), (int)c->Error);
        if (!numbersResult(valid, c->Name)) all = false;
    }
    {
        ximapnumberview number;
        bool valid = xrtImapNumberParse(XRT_STR_LITERAL("18446744073709551615 EXISTS"), &number) == XMAIL_NEXT_ITEM &&
            number.Number == UINT64_MAX;
        if (!numbersResult(valid, "generic number max64 remains supported")) all = false;
    }
    {
        ximapmailboxstatusview status;
        ximapstatuscursor cursor;
        ximapstatusitem item;
        bool valid = xrtImapStatusParse(XRT_STR_LITERAL("STATUS INBOX (UNSEEN 0)"), &status) &&
            xrtImapStatusCursorInit(&cursor, &status) && xrtImapStatusNext(&cursor, &item) == XMAIL_NEXT_ITEM &&
            testMailViewEqual(item.Name, XRT_STR_LITERAL("UNSEEN")) && item.Value.Number == 0 &&
            xrtImapStatusNext(&cursor, &item) == XMAIL_NEXT_END;
        if (!numbersResult(valid, "STATUS UNSEEN zero is a count")) all = false;
    }
    return all;
}

typedef struct mailboxserver {
    xnetlistener* Listener;
    double Deadline;
    unsigned Mode;
    bool Success;
} mailboxserver;

static bool numbersSend(xnetstream* stream, cstr text, double deadline)
{ return testImapSend(stream, text, strlen(text), deadline); }

static bool numbersReceive(xnetstream* stream, cstr text, double deadline)
{ return testImapReceive(stream, text, strlen(text), deadline); }

static int32 numbersServer(ptr data)
{
    mailboxserver* server = (mailboxserver*)data;
    unsigned mode = server->Mode;
    double deadline = server->Deadline;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener, deadline, NULL);
    bool valid = stream != NULL;
    if (valid) valid = numbersSend(stream, "* PREAUTH imap.test ready\r\n", deadline) &&
        numbersReceive(stream, "A00000001 CAPABILITY\r\n", deadline) &&
        numbersSend(stream, "* CAPABILITY IMAP4rev1 CONDSTORE\r\nA00000001 OK capabilities\r\n", deadline);
    if (valid && mode < 6u) valid = numbersReceive(stream, (mode == 1u || mode == 4u) ?
            "A00000002 EXAMINE \"INBOX\"\r\n" : "A00000002 SELECT \"INBOX\"\r\n", deadline);
    if (valid && mode < 6u) {
        cstr reply = mode == 0u ? "* 7 EXISTS\r\n* OK [UIDNEXT 0] bad\r\nA00000002 OK selected\r\n" :
            mode == 1u ? "* 7 EXISTS\r\nA00000002 OK [UIDVALIDITY 4294967296] bad\r\n" :
            mode == 2u ? "* 7 EXISTS\r\n* 0 EXPUNGE\r\nA00000002 OK selected\r\n" :
            mode == 3u ? "* 4294967295 EXISTS\r\n* 0 RECENT\r\n* OK [UIDVALIDITY 4294967295] stable\r\n"
                "* OK [UIDNEXT 1] next\r\n* OK [UNSEEN 1] unseen\r\n"
                "* OK [HIGHESTMODSEQ 18446744073709551615] legacy\r\n* 4294967295 EXPUNGE\r\n"
                "A00000002 OK [READ-WRITE] selected\r\n" :
            mode == 4u ? "* 0 EXISTS\r\n* 0 RECENT\r\n* OK [UIDVALIDITY 1] stable\r\n"
                "* OK [UIDNEXT 1] next\r\nA00000002 OK [READ-ONLY] selected\r\n" :
            "* 7 EXISTS\r\nA00000002 NO denied\r\n";
        valid = numbersSend(stream, reply, deadline);
    }
    if (valid && mode < 3u) {
        xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, deadline, NULL);
        valid = bytes == NULL && xrtErrorKind(xrtGetError()) != XERR_TIMEOUT;
        xrtNetBytesDestroy(bytes);
    } else if (valid && mode >= 6u) valid = numbersReceive(stream, "A00000002 NOOP\r\n", deadline) &&
        numbersSend(stream, "A00000002 OK reusable\r\n", deadline) &&
        numbersReceive(stream, "A00000003 LOGOUT\r\n", deadline) &&
        numbersSend(stream, "* BYE signing off\r\nA00000003 OK logout\r\n", deadline);
    else if (valid) valid = numbersReceive(stream, "A00000003 NOOP\r\n", deadline) &&
        numbersSend(stream, "A00000003 OK reusable\r\n", deadline) &&
        numbersReceive(stream, "A00000004 LOGOUT\r\n", deadline) &&
        numbersSend(stream, "* BYE signing off\r\nA00000004 OK logout\r\n", deadline);
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) valid = xrtNetStreamClose(stream) && valid;
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, deadline, NULL) ||
            xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError(); server->Success = valid; return valid ? 0 : 1;
}

static bool numbersSession(const ximapclientconfig* config, xnetlistener* listener, unsigned mode)
{
    static const char* const names[] = { "SELECT rejects bad untagged UID", "EXAMINE rejects bad tagged UID",
        "SELECT validates with NULL output", "SELECT valid maxima and legacy MODSEQ",
        "EXAMINE empty mailbox", "SELECT NO remains reusable", "SELECT invalid mailbox preserves output",
        "SELECT line limit preserves output", "SELECT quote OOM preserves output" };
    mailboxserver server;
    xthread* thread;
    ximapclient* client;
    ximapmailboxinfo info, original_info;
    ximapclientconfig bounded = *config;
    ximapresponseview before, after;
    char last[256];
    bool returned, valid, joined = false, recovered, triggered;
    xerrkind kind;
    ximapclientstate state;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    server.Listener = listener; server.Deadline = deadline; server.Mode = mode; server.Success = false;
    thread = xrtThreadCreate(numbersServer, &server, 0);
    testRequire(thread != NULL, "mailbox number peer create failed");
    if (mode == 7u) bounded.CommandLineLimit = 32u;
    client = xrtImapClientOpen(&bounded, 10000, NULL);
    testRequire(client != NULL && xrtImapClientState(client) == XIMAP_CLIENT_AUTHENTICATED,
        "mailbox number public Open failed");
    testRequire(xrtImapClientLastResponse(client, &before) && before.Source.Size < sizeof(last),
        "mailbox number response snapshot failed");
    memcpy(last, before.Source.Data, before.Source.Size);
    numbersSnapshot(&info); memcpy(&original_info, &info, sizeof(info)); xrtClearError();
    if (mode == 8u) testRequire(xrtMemDebugFailAfter(0), "mailbox number quote OOM setup failed");
    returned = (mode == 1u || mode == 4u) ?
        xrtImapClientExamine(client, XRT_STR_LITERAL("INBOX"), &info, 10000, NULL) :
        xrtImapClientSelect(client, mode == 6u ? XRT_STR_LITERAL("bad\r\n") : mode == 7u ?
            XRT_STR_LITERAL("1234567890123456789012345678901234567890123456789012345678901234") :
            XRT_STR_LITERAL("INBOX"), mode == 2u ? NULL : &info, 10000, NULL);
    kind = xrtErrorKind(xrtGetError()); state = xrtImapClientState(client);
    triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    valid = returned == (mode == 3u || mode == 4u) && kind ==
        (mode < 3u ? XERR_PROTOCOL : mode == 5u ? XERR_PERMISSION : mode == 6u ? XERR_ARGUMENT :
         mode == 7u ? XERR_RANGE : mode == 8u ? XERR_MEMORY : XERR_NONE) &&
        state == (mode < 3u ? XIMAP_CLIENT_FAILED : mode >= 5u ? XIMAP_CLIENT_AUTHENTICATED : XIMAP_CLIENT_SELECTED) &&
        triggered == (mode == 8u);
    if (mode < 3u) valid = valid && xrtErrorCode(xrtGetError()) == XMAIL_ERROR_PROTOCOL &&
        strcmp(xrtErrorDomain(xrtGetError()), "xrt.mail") == 0;
    testRequire(xrtImapClientLastResponse(client, &after), "mailbox number final response missing");
    if (mode == 0u || mode == 2u) valid = valid && after.Source.Size == before.Source.Size &&
        memcmp(after.Source.Data, last, before.Source.Size) == 0;
    if (mode >= 6u) valid = valid && after.Source.Size == before.Source.Size &&
        memcmp(after.Source.Data, last, before.Source.Size) == 0 &&
        memcmp(&info, &original_info, sizeof(info)) == 0 && xrtImapClientTag(client).Size == 0 &&
        xrtImapClientLiteralRemaining(client) == 0;
    if (mode == 1u) valid = valid && testMailViewEqual(after.Source,
        XRT_STR_LITERAL("A00000002 OK [UIDVALIDITY 4294967296] bad"));
    if (mode == 0u) valid = valid && info.Exists == 7u && info.UidNext == 0 &&
        (info.Present & XIMAP_MAILBOX_UID_NEXT) == 0;
    if (mode == 1u) valid = valid && info.Exists == 7u && info.UidValidity == 0 &&
        (info.Present & XIMAP_MAILBOX_UID_VALIDITY) == 0;
    if (mode == 3u) valid = valid && info.Exists == (uint64)UINT32_MAX - 1u && info.Recent == 0 &&
        info.UidValidity == UINT32_MAX && info.UidNext == 1u && info.Unseen == 1u &&
        info.HighestModSeq == UINT64_MAX && !info.ReadOnly && info.Present == UINT32_C(0x7f);
    if (mode == 4u) valid = valid && info.Exists == 0 && info.Recent == 0 && info.UidValidity == 1u &&
        info.UidNext == 1u && info.ReadOnly && (info.Present & XIMAP_MAILBOX_ACCESS) != 0;
    if (mode < 3u && state == XIMAP_CLIENT_FAILED) {
        joined = __xrtThreadWaitUntil(thread, deadline) == XWAIT_OK;
        valid = valid && joined && server.Success && xrtThreadExitCode(thread) == 0;
    }
    if (mode < 3u) {
        xrtClearError(); recovered = !xrtImapClientNoop(client, 10000, NULL) && xrtErrorKind(xrtGetError()) == XERR_STATE;
    } else recovered = xrtImapClientNoop(client, 10000, NULL) && xrtImapClientLogout(client, 10000, NULL);
    if (!valid || !recovered) fprintf(stderr, "mailbox session mode=%u returned=%d kind=%d state=%d valid=%d recovery=%d\n",
        mode, returned, (int)kind, (int)state, valid, recovered);
    xrtClearError(); testRequire(xrtImapClientAbort(client), "mailbox number Abort failed");
    xrtImapClientDestroy(client);
    if (!joined) testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK, "mailbox number peer join failed");
    valid = valid && recovered && server.Success && xrtThreadExitCode(thread) == 0;
    xrtThreadDestroy(thread);
    return numbersResult(valid, names[mode]);
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
    bool valid;
    double deadline;
    testRequire(xrtMemDebugEnable(true), "mailbox number memory diagnostics failed");
    valid = numbersPure();
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "mailbox number Engine failed");
    xrtNetListenConfigInit(&lc);
    testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "mailbox number address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestImapAddress), "mailbox number Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testImapResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc);
    testRequire(resolver != NULL, "mailbox number Resolver failed");
    xrtImapClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "imap.test"; config.Net.Port = TestImapAddress.Port;
    for (unsigned mode = 0; mode < 9u; mode++) if (!numbersSession(&config, listener, mode)) valid = false;
    deadline = __xrtWaitAfter(INT64_C(10000));
    testRequire(xrtNetListenerClose(listener), "mailbox number Listener close failed");
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "mailbox number Listener retirement timed out"); xrtThreadYield();
    }
    xrtNetListenerDestroy(listener);
    testRequire(xrtNetResolverDestroy(resolver), "mailbox number Resolver retirement failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine);
        if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline), "mailbox number Engine retirement failed");
        xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 && stats.PendingCommands == 0 &&
        stats.ActiveTimers == 0 && stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "mailbox number Engine resources remain");
    xrtClearError(); testMemoryDebugDrain("mailbox number leaked resources");
    return valid ? 0 : 1;
}
