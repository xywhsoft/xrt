#define main testSmtpExistingAuthMain
#include "test_smtp_auth.c"
#undef main

/* Each opaque Client comes from public Open and talks to a real TCP peer. */
typedef struct smtpdeadlineserver {
    xnetlistener* Listener;
    double Deadline;
    unsigned Mode;
    bool Success;
} smtpdeadlineserver;

static bool deadlineSend(xnetstream* stream, cstr text, double deadline)
{
    return testSmtpAuthSend(stream, text, strlen(text), deadline);
}

static bool deadlineReceive(xnetstream* stream, cstr text, double deadline)
{
    return testSmtpAuthReceive(stream, text, strlen(text), deadline);
}

static bool deadlineChunkMode(unsigned mode)
{
    return mode == 1u || mode == 2u || mode == 4u || mode == 7u;
}

static int32 deadlineServer(ptr data)
{
    smtpdeadlineserver* server = (smtpdeadlineserver*)data;
    xnetstream* stream = __xrtNetListenerAcceptWait(server->Listener,
        server->Deadline, NULL);
    bool valid = stream != NULL;
    if (valid) valid = testSmtpAuthHello(stream,
        "250-smtp.test\r\n250 CHUNKING\r\n",
        sizeof("250-smtp.test\r\n250 CHUNKING\r\n") - 1u, server->Deadline);
    if (valid && (deadlineChunkMode(server->Mode) || server->Mode >= 10u)) {
        valid = deadlineReceive(stream, "MAIL FROM:<sender@test>\r\n", server->Deadline) &&
            deadlineSend(stream, "250 sender\r\n", server->Deadline) &&
            deadlineReceive(stream, "RCPT TO:<receiver@test>\r\n", server->Deadline) &&
            deadlineSend(stream, "250 recipient\r\n", server->Deadline) &&
            (server->Mode >= 10u ?
                (deadlineReceive(stream, "DATA\r\n", server->Deadline) &&
                 deadlineSend(stream, "354 body\r\n", server->Deadline) &&
                 deadlineReceive(stream, "abc\r\n.\r\n", server->Deadline)) :
                deadlineReceive(stream, "BDAT 3 LAST\r\nabc", server->Deadline)) &&
            deadlineSend(stream, "250 stored\r\n", server->Deadline);
    }
    if (valid && server->Mode != 9u) {
        valid = deadlineReceive(stream, "NOOP\r\n", server->Deadline) &&
            deadlineSend(stream, "250 recovered\r\n", server->Deadline) &&
            testSmtpAuthQuit(stream, server->Deadline);
    } else if (valid) {
        xnetbytes* bytes = __xrtNetStreamRecv(stream, 1u, server->Deadline, NULL);
        valid = bytes == NULL && xrtErrorKind(xrtGetError()) != XERR_TIMEOUT;
        xrtNetBytesDestroy(bytes);
    }
    if (stream != NULL) {
        if (xrtNetStreamState(stream) == XNET_STREAM_OPEN) {
            valid = xrtNetStreamClose(stream) && valid;
        }
        valid = (__xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE,
            server->Deadline, NULL) || xrtNetStreamState(stream) == XNET_STREAM_CLOSED) && valid;
        xrtNetStreamDestroy(stream);
    }
    xrtClearError();
    server->Success = valid;
    return valid ? 0 : 1;
}

static bool deadlineRun(const xsmtpclientconfig* config,
    xnetlistener* listener, unsigned mode)
{
    static const char* const names[] = {
        "Receive invalid timeout preserves session and output",
        "BDAT nonempty write invalid timeout preserves block",
        "BDAT empty write rejects invalid timeout",
        "Close invalid timeout preserves session",
        "BDAT wrapper invalid timeout control",
        "Submit invalid timeout control",
        "SubmitEnvelope invalid timeout control",
        "valid BDAT block and infinite NOOP control",
        "valid Send/Receive control",
        "valid Close control",
        "DATA null range preserves writer and session",
        "DATA wrapping range preserves writer and session"
    };
    smtpdeadlineserver server;
    xthread* thread;
    xsmtpclient* client;
    xsmtpreply reply, before, last_before, last_after;
    char last_text[128];
    xsmtpclientstate state;
    xsmtpclientstate observed;
    xerrkind kind = XERR_NONE;
    bool result = false, valid = true, recovered = true;
    double deadline = __xrtWaitAfter(INT64_C(10000));
    server.Listener = listener; server.Deadline = deadline;
    server.Mode = mode; server.Success = false;
    thread = xrtThreadCreate(deadlineServer, &server, 0);
    testRequire(thread != NULL, "SMTP deadline peer creation failed");
    client = xrtSmtpClientOpen(config, INT64_C(10000), NULL);
    testRequire(client != NULL && xrtSmtpClientState(client) == XSMTP_CLIENT_READY,
        "SMTP deadline public Open failed");
    if (deadlineChunkMode(mode) || mode >= 10u) {
        testRequire(xrtSmtpClientMail(client, XRT_STR_LITERAL("sender@test"),
            XRT_STR_LITERAL(""), 10000, NULL) &&
            xrtSmtpClientRcpt(client, XRT_STR_LITERAL("receiver@test"),
                XRT_STR_LITERAL(""), 10000, NULL), "SMTP deadline envelope failed");
        if (mode >= 10u) testRequire(xrtSmtpClientDataBegin(client, 10000, NULL),
            "SMTP deadline DATA begin failed");
        else if (mode != 4u) testRequire(xrtSmtpClientBdatBegin(client, 3u, true,
            10000, NULL), "SMTP deadline BDAT begin failed");
    }
    state = xrtSmtpClientState(client);
    testRequire(xrtSmtpClientLastReply(client, &last_before) &&
        last_before.Text.Size <= sizeof(last_text), "SMTP deadline reply snapshot failed");
    memcpy(last_text, last_before.Text.Data, last_before.Text.Size);
    memset(&reply, 0xA5, sizeof(reply)); memcpy(&before, &reply, sizeof(before));
    xrtClearError();
    if (mode == 0u) result = xrtSmtpClientReceive(client, &reply, -2, NULL);
    else if (mode == 1u || mode == 2u) result = xrtSmtpClientBdatWrite(client,
        mode == 1u ? XRT_BYTES_LITERAL("x") : (xbytesview){ NULL, 0 }, -2, NULL);
    else if (mode == 3u) result = xrtSmtpClientClose(client, -2);
    else if (mode == 4u) result = xrtSmtpClientBdat(client, XRT_BYTES_LITERAL("abc"),
        true, -2, NULL);
    else if (mode == 5u || mode == 6u) {
        xmailmessage message;
        xmailaddress to = { XRT_STR_LITERAL("Receiver"), XRT_STR_LITERAL("receiver@test") };
        xsmtprecipient recipient = { XRT_STR_LITERAL("receiver@test"), XRT_STR_LITERAL("") };
        xsmtpenvelope envelope = { XRT_STR_LITERAL("sender@test"), XRT_STR_LITERAL(""),
            &recipient, 1u };
        xrtMailMessageInit(&message);
        message.From = (xmailaddress){ XRT_STR_LITERAL("Sender"), XRT_STR_LITERAL("sender@test") };
        message.To = &to; message.ToCount = 1u;
        message.Subject = XRT_STR_LITERAL("deadline"); message.Text = XRT_STR_LITERAL("body");
        testRequire(xrtMailMessageValid(&message), "SMTP deadline message control invalid");
        result = mode == 5u ? xrtSmtpSubmit(client, &message, -2, NULL) :
            xrtSmtpSubmitEnvelope(client, &envelope, &message, -2, NULL);
    } else if (mode == 7u) result = xrtSmtpClientBdatWrite(client,
        XRT_BYTES_LITERAL("abc"), 10000, NULL) && xrtSmtpClientBdatEnd(client, 10000, NULL);
    else if (mode == 8u) result = xrtSmtpClientSend(client, XRT_STR_LITERAL("NOOP"),
        10000, NULL) && xrtSmtpClientReceive(client, &reply, 10000, NULL) &&
        reply.Code == 250 && testMailViewEqual(reply.Text, XRT_STR_LITERAL("recovered"));
    else if (mode == 9u) result = xrtSmtpClientClose(client, 10000);
    else result = xrtSmtpClientDataWrite(client, mode == 10u ?
        (xbytesview){ NULL, 1u } :
        (xbytesview){ (cbytes)(uintptr_t)(UINTPTR_MAX - 1u), 4u }, 10000, NULL);
    kind = xrtErrorKind(xrtGetError()); observed = xrtSmtpClientState(client);
    if (mode <= 6u || mode >= 10u) {
        valid = !result && kind == XERR_ARGUMENT && observed == state &&
            memcmp(&reply, &before, sizeof(reply)) == 0 &&
            xrtSmtpClientLastReply(client, &last_after) &&
            last_after.Code == last_before.Code && last_after.Lines == last_before.Lines &&
            last_after.Text.Size == last_before.Text.Size &&
            memcmp(last_after.Text.Data, last_text, last_before.Text.Size) == 0;
    } else valid = result && observed == (mode == 9u ? XSMTP_CLIENT_CLOSED : XSMTP_CLIENT_READY);
    xrtClearError();
    if (mode >= 10u) recovered = xrtSmtpClientDataWrite(client,
        XRT_BYTES_LITERAL("abc\r\n"), 10000, NULL) &&
        xrtSmtpClientDataEnd(client, 10000, NULL);
    else if (mode == 1u || mode == 2u) recovered = xrtSmtpClientBdatWrite(client,
        XRT_BYTES_LITERAL("abc"), 10000, NULL) && xrtSmtpClientBdatEnd(client, 10000, NULL);
    else if (mode == 4u) recovered = xrtSmtpClientBdat(client,
        XRT_BYTES_LITERAL("abc"), true, 10000, NULL);
    if (recovered && mode != 8u && mode != 9u) recovered =
        xrtSmtpClientNoop(client, mode == 7u ? -1 : 10000, NULL);
    if (recovered && mode != 9u) recovered = xrtSmtpClientQuit(client, 10000, NULL);
    if (!valid || !recovered) fprintf(stderr,
        "SMTP deadline mode=%u result=%d state=%d expected=%d kind=%d recovered=%d\n",
        mode, result, observed, state, kind, recovered);
    testRequire(xrtSmtpClientAbort(client), "SMTP deadline Abort failed");
    xrtSmtpClientDestroy(client);
    testRequire(__xrtThreadWaitUntil(thread, deadline) == XWAIT_OK,
        "SMTP deadline peer did not retire");
    valid = valid && recovered && server.Success && xrtThreadExitCode(thread) == 0;
    xrtThreadDestroy(thread); xrtClearError();
    printf("[%s] SMTP deadline %s\n", valid ? "PASS" : "FAIL", names[mode]);
    return valid;
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
    testRequire(xrtMemDebugEnable(true), "SMTP deadline memory diagnostics unavailable");
    xrtNetEngineConfigInit(&ec); ec.Backend = XNET_PORT_SELECT; ec.Workers = 1u;
    engine = xrtNetEngineCreate(&ec);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "SMTP deadline Engine failed");
    xrtNetListenConfigInit(&lc);
    testRequire(xrtNetAddrLoopback(&lc.Address, XNET_FAMILY_IPV4, 0), "SMTP deadline address failed");
    listener = xrtNetListen(engine, &lc, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &TestSmtpAuthAddress),
        "SMTP deadline Listener failed");
    xrtNetResolverConfigInit(&rc); rc.Workers = 1u; rc.Lookup = testSmtpAuthResolve; rc.CacheEntries = 0;
    resolver = xrtNetResolverCreate(&rc);
    testRequire(resolver != NULL, "SMTP deadline Resolver failed");
    xrtSmtpClientConfigInit(&config); config.Net.Engine = engine; config.Net.Resolver = resolver;
    config.Net.Host = "smtp.test"; config.Net.Port = TestSmtpAuthAddress.Port;
    config.Hello = XRT_STR_LITERAL("client.test");
    for (unsigned mode = 0; mode < 12u; mode++) if (!deadlineRun(&config, listener, mode)) valid = false;
    deadline = __xrtWaitAfter(INT64_C(10000));
    testRequire(xrtNetListenerClose(listener), "SMTP deadline Listener close failed");
    while (xrtNetListenerState(listener) != XNET_LISTENER_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "SMTP deadline Listener retirement timed out");
        xrtThreadYield();
    }
    xrtNetListenerDestroy(listener);
    testRequire(xrtNetResolverDestroy(resolver), "SMTP deadline Resolver retirement failed");
    for (;;) {
        xnetretireresult result = xrtNetEngineTryStop(engine);
        if (result == XNET_RETIRE_READY) break;
        testRequire(result == XNET_RETIRE_BUSY && !__xrtWaitExpired(deadline),
            "SMTP deadline Engine retirement failed");
        xrtThreadYield();
    }
    testRequire(xrtNetEngineStats(engine, &stats) && stats.LiveObjects == 0 &&
        stats.WaitErrors == 0 && stats.WakeErrors == 0 && xrtNetEngineDestroy(engine),
        "SMTP deadline Engine resources remain");
    xrtClearError(); testMemoryDebugDrain("SMTP deadline leaked resources");
    return valid ? 0 : 1;
}
