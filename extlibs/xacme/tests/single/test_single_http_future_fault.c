/* Actual futures, HTTP parser/buffers and allocator; controlled transport edges.
 * Routing tokens below are never dereferenced as streams. Actual socket/TLS
 * shutdown and factory ownership are checked by the independent wire probe. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XACME_MODULE_ACME_HTTP
#include <xacme/features.h>
#define XRT_IMPLEMENTATION
#define XRT_MODULE_MEMORY_DEBUG
#include "../../../../single/xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <xrt/detail/wait.h>

static const char* Mode;
static bool Tls;
/* The token needs natural scalar alignment, not a C11-only stddef typedef. */
static union { long double Alignment; void* Pointer; } RoutingToken;
static xerror* Cause;
static xerror* MetadataCause;
static xpromise* Pending;
static unsigned Dials, Sends, Recvs, Ends, Aborts, Closes, Destroys;
static void require(bool Result, const char* Message)
{
    if (!Result) { fprintf(stderr, "[FAIL] ACME_HTTP %s %s: %s\n", Tls ? "tls" : "tcp", Mode, Message); exit(1); }
}
static bool stage(const char* Name) { return strncmp(Mode, Name, strlen(Name)) == 0; }
static bool suffix(const char* Name) { size_t A = strlen(Mode), B = strlen(Name); return A >= B && strcmp(Mode + A - B, Name) == 0; }
/* Controlled monotonic time charges actual transport submissions; Core futures,
 * owned byte payloads, cancellation and allocator behavior remain real. */
static bool BudgetRun;
static double BudgetClock;
static unsigned BudgetSleeps, BudgetSlept;
static int64 DialTimeouts[3];
static void budget_charge(unsigned Ms) { if (BudgetRun) BudgetClock += (double)Ms / 1000.0; }
static unsigned budget_send_ms(void)
{
    return strcmp(Mode, "budget-chunks") == 0 ? 30u : strcmp(Mode, "budget-send-expired") == 0 ? 70u :
        strcmp(Mode, "budget-head-body") == 0 ? 20u : 40u;
}
static void budget_tcp_config(xnetdialconfig* Config)
{
    xrtNetDialConfigInit(Config);
    if (BudgetRun && strcmp(Mode, "budget-config-spent") == 0) budget_charge(105u);
}
static void budget_tls_config(xtlsdialconfig* Config)
{
    xrtTlsDialConfigInit(Config);
    if (BudgetRun && strcmp(Mode, "budget-config-spent") == 0) budget_charge(105u);
}
static double budget_after(int64 Ms)
{
    if (!BudgetRun) return __xrtWaitAfter(Ms);
    return Ms == XRT_WAIT_FOREVER ? INFINITY : Ms < 0 ? NAN : BudgetClock + (double)Ms / 1000.0;
}
static bool budget_expired(double Deadline)
{ return BudgetRun ? BudgetClock >= Deadline : __xrtWaitExpired(Deadline); }
static int64 budget_remaining(double Deadline)
{
    if (!BudgetRun) return __xrtWaitRemaining(Deadline);
    if (Deadline == INFINITY) return XRT_WAIT_FOREVER;
    return Deadline <= BudgetClock ? 0 : (int64)ceil((Deadline - BudgetClock) * 1000.0);
}
static void budget_sleep(int64 Ms)
{
    if (!BudgetRun) { xrtSleep(Ms); return; }
    require(Ms >= 0 && Ms <= 1000, "bounded retry delay");
    BudgetSleeps++; BudgetSlept += (unsigned)Ms; budget_charge((unsigned)Ms);
    if (strcmp(Mode, "budget-retry-wrap-oom") == 0)
        require(xrtMemDebugFailAfter(0), "budget diagnostic allocation failure armed");
}
static void make_cause(void)
{
    require(Cause == NULL, "one primary failure");
    if (suffix("memory")) {
        require(xrtMemDebugFailAfter(0), "allocation injection enabled");
        require(xrtMalloc(1) == NULL && xrtMemDebugFailTriggered(), "real allocation failure");
        xrtMemDebugFailClear();
    } else xrtSetErrorInfo(suffix("protocol") ? XERR_PROTOCOL : suffix("state") ? XERR_STATE : XERR_IO,
        "test.acme.http.worker", 2801, "transport failed after dispatch");
    Cause = xrtErrorRef(xrtGetError());
    require(Cause != NULL, "primary cause retained");
}
static xfuture* result(bool Fail, bool Closed, ptr Value)
{
    xfuture* Future = NULL;
    xpromise* Promise = xrtPromiseCreate(&Future, NULL);
    require(Promise != NULL && Future != NULL, "real future created");
    if (Fail && suffix("pending")) {
        require(Pending == NULL, "one pending producer"); Pending = Promise; return Future;
    }
    if (Fail) { if (!BudgetRun || Cause == NULL) make_cause(); require(xrtPromiseReject(Promise, Cause), "worker failure published"); }
    else if (Closed) require(xrtPromiseClose(Promise), "EOF published");
    else require(xrtPromiseResolve(Promise, Value), "success published");
    xrtPromiseDestroy(Promise);
    /* The consumer cannot rely on a producer's thread-local diagnostic. */
    xrtClearError();
    if (Fail && stage("connect") && suffix("wrap-fault"))
        require(xrtMemDebugFailAfter(0), "connect context allocation failure armed");
    return Future;
}
static xfuture* dial(void)
{
    Dials++;
    if (BudgetRun) {
        if (strcmp(Mode, "budget-retry-wrap-oom") == 0)
            require(Dials == 1u, "expired budget cannot submit another dial before diagnostic OOM");
        budget_charge(strcmp(Mode, "budget-dial-spent") == 0 ? 105u :
            strstr(Mode, "retry") ? 60u : strcmp(Mode, "budget-chunks") == 0 ? 20u :
            strcmp(Mode, "budget-head-body") == 0 ? 20u : 40u);
        bool Fail = strstr(Mode, "retry") && (Dials == 1u ||
            (strcmp(Mode, "budget-retry-two") == 0 && Dials == 2u));
        return result(Fail, false, &RoutingToken);
    }
    if (stage("connect") && strstr(Mode, "-direct-") != NULL) { make_cause(); return NULL; }
    return result(stage("connect"), false, &RoutingToken);
}
static xfuture* tls_dial(xnetengine* Engine, xnetresolver* Resolver, cstr Host, uint16 Port,
    const xtlsclientconfig* Config, const xtlsdialconfig* Dial, const xtlsstreamevents* Events, ptr Data)
{ (void)Engine; (void)Resolver; (void)Host; (void)Port; (void)Config; (void)Events; (void)Data; if (BudgetRun) { require(Dials < 3u, "bounded dial count"); DialTimeouts[Dials] = Dial->Timeout; } return dial(); }
static xfuture* tcp_dial(xnetengine* Engine, xnetresolver* Resolver, cstr Host, uint16 Port,
    const xnetdialconfig* Config, const xnetstreamevents* Events, ptr Data)
{ (void)Engine; (void)Resolver; (void)Host; (void)Port; (void)Events; (void)Data; if (BudgetRun) { require(Dials < 3u, "bounded dial count"); DialTimeouts[Dials] = Config->Timeout; } return dial(); }
static xtlsstream* tls_ref(xtlsstream* Stream) { require(Stream == (xtlsstream*)&RoutingToken, "TLS routing token"); if (BudgetRun && strcmp(Mode, "budget-presend-spent") == 0) budget_charge(70u); return Stream; }
static xnetstream* tcp_ref(xnetstream* Stream) { require(Stream == (xnetstream*)&RoutingToken, "TCP routing token"); if (BudgetRun && strcmp(Mode, "budget-presend-spent") == 0) budget_charge(70u); return Stream; }
static xfuture* tls_send(xtlsstream* Stream, const void* Data, size_t Size)
{
    (void)Stream; (void)Data; require(Size > 0, "request sent"); Sends++;
    if (BudgetRun) { budget_charge(budget_send_ms()); return result(false, false, NULL); }
    if (stage("send") && strstr(Mode, "-direct-") != NULL) { make_cause(); return NULL; }
    return result(stage("send"), false, NULL);
}
static xnetresult tcp_send(xnetstream* Stream, const void* Data, size_t Size)
{
    (void)Stream; (void)Data; require(Size > 0, "request sent"); Sends++;
    if (BudgetRun) { budget_charge(budget_send_ms()); return XNET_RESULT_OK; }
    if (stage("send") && strstr(Mode, "-direct-") != NULL) {
        make_cause();
        if (strstr(Mode, "-current-") == NULL) xrtClearError();
        return XNET_RESULT_ERROR;
    }
    return stage("send") ? XNET_RESULT_AGAIN : XNET_RESULT_OK;
}
static xfuture* tcp_wait(xnetstream* Stream, xnetstreamwait Wait)
{ (void)Stream; require(Wait == XNET_STREAM_WAIT_WRITE, "write wait"); return result(true, false, NULL); }
static void bytes_free(ptr Value, ptr Context) { (void)Context; xrtNetBytesDestroy((xnetbytes*)Value); }
static xfuture* bytes_result(const char* Text)
{
    xnetwspan Span;
    xnetbytes* Bytes = __xrtNetBytesAlloc(strlen(Text), &Span);
    require(Bytes != NULL, "real owned network bytes"); memcpy(Span.Data, Text, Span.Size);
    xfuture* Future = NULL; xpromise* Promise = xrtPromiseCreate(&Future, NULL);
    require(Promise != NULL && xrtPromiseResolveOwned(Promise, Bytes, bytes_free, NULL), "owned receive published");
    xrtPromiseDestroy(Promise); return Future;
}
static xfuture* recv_data(void)
{
    Recvs++;
    if (BudgetRun) {
        bool Split = strcmp(Mode, "budget-head-body") == 0;
        budget_charge(Split && Recvs == 2u ? 50u : 40u);
        if (strcmp(Mode, "budget-late-memory") == 0) return result(true, false, NULL);
        if (Split && Recvs == 1u) return bytes_result("HTTP/1.1 200 OK\r\nLocation: /partial\r\nContent-Length: 2\r\n\r\n");
        require(Recvs == (Split ? 2u : 1u), "bounded budget receive count");
        return bytes_result(Split ? "{}" : "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}");
    }
    if (stage("head") || (stage("body") && Recvs == 2)) {
        if (strstr(Mode, "-direct-") != NULL) { make_cause(); return NULL; }
        return result(true, false, NULL);
    }
    if (stage("end") && Recvs == 2) return result(false, true, NULL);
    require(Recvs == 1, "bounded receive sequence");
    if (stage("body")) return bytes_result("HTTP/1.1 200 OK\r\nLocation: /issued\r\nReplay-Nonce: nonce\r\nContent-Length: 2\r\n\r\n");
    if (stage("end")) return bytes_result("HTTP/1.1 200 OK\r\nLocation: /issued\r\nReplay-Nonce: nonce\r\nConnection: close\r\n\r\n{}");
    return bytes_result("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}");
}
static xfuture* tls_recv(xtlsstream* Stream, size_t Size) { (void)Stream; (void)Size; return recv_data(); }
static xfuture* tcp_recv(xnetstream* Stream, size_t Size) { (void)Stream; (void)Size; return recv_data(); }
static xfuture* tls_wait(xtlsstream* Stream, xtlsstreamwait Wait)
{
    (void)Stream; require(Wait == XTLS_STREAM_WAIT_END, "authenticated EOF wait"); Ends++;
    if (strstr(Mode, "-direct-") != NULL) { make_cause(); return NULL; }
    return result(true, false, NULL);
}
static bool tcp_stats(const xnetstream* Stream, xnetstreamstats* Stats)
{ (void)Stream; memset(Stats, 0, sizeof(*Stats)); Stats->ReadEnded = true; return true; }
static const xerror* tcp_error(const xnetstream* Stream)
{
    (void)Stream; Ends++;
    require(strstr(Mode, "-current-") == NULL, "fresh direct error takes precedence over older stream state");
    if (Cause == NULL) make_cause();
    xrtClearError();
    return Cause;
}
static bool cleanup(bool Abort)
{
    if (Abort) {
        Aborts++; xrtSetErrorInfo(XERR_STATE, "test.acme.http.cleanup", 2802, "cleanup secondary failure");
        if (suffix("wrap-fault")) require(xrtMemDebugFailAfter(0), "uncertain-write context allocation failure armed");
    }
    else Closes++;
    return true;
}
static bool tls_abort(xtlsstream* Stream) { (void)Stream; return cleanup(true); }
static bool tcp_abort(xnetstream* Stream) { (void)Stream; return cleanup(true); }
static bool tls_close(xtlsstream* Stream) { (void)Stream; return cleanup(false); }
static bool tcp_close(xnetstream* Stream) { (void)Stream; return cleanup(false); }
static void tls_destroy(xtlsstream* Stream) { (void)Stream; Destroys++; }
static void tcp_destroy(xnetstream* Stream) { (void)Stream; Destroys++; }
static xerror* observe_wrap(const xerror* cause, xerrkind kind, cstr domain, int32 code, cstr message)
{
    xerror* wrapped = xrtErrorWrap(cause, kind, domain, code, message);
    if (wrapped == NULL && (suffix("wrap-fault") || (BudgetRun && strcmp(Mode, "budget-retry-wrap-oom") == 0))) {
        require(MetadataCause == NULL, "one metadata allocation failure");
        MetadataCause = xrtErrorRef(xrtGetError());
    }
    return wrapped;
}
#define xrtNetDialConfigInit budget_tcp_config
#define xrtTlsDialConfigInit budget_tls_config
#define __xrtWaitAfter budget_after
#define __xrtWaitExpired budget_expired
#define __xrtWaitRemaining budget_remaining
#define xrtSleep budget_sleep
#define xrtErrorWrap observe_wrap
#define xrtTlsDialAsync tls_dial
#define xrtNetDialAsync tcp_dial
#define xrtTlsStreamRef tls_ref
#define xrtNetStreamRef tcp_ref
#define xrtTlsStreamSendAsync tls_send
#define xrtNetStreamSend tcp_send
#define xrtNetStreamWaitAsync tcp_wait
#define xrtTlsStreamRecvAsync tls_recv
#define xrtNetStreamRecvAsync tcp_recv
#define xrtTlsStreamWaitAsync tls_wait
#define xrtNetStreamStats tcp_stats
#define xrtNetStreamError tcp_error
#define xrtTlsStreamAbort tls_abort
#define xrtNetStreamAbort tcp_abort
#define xrtTlsStreamClose tls_close
#define xrtNetStreamClose tcp_close
#define xrtTlsStreamDestroy tls_destroy
#define xrtNetStreamDestroy tcp_destroy
#include "../../src/acme/xacme_http.c"
#undef xrtErrorWrap
#undef xrtNetDialConfigInit
#undef xrtTlsDialConfigInit
#undef __xrtWaitAfter
#undef __xrtWaitExpired
#undef __xrtWaitRemaining
#undef xrtSleep

static void clean(void)
{
    if (Pending != NULL) { require(xrtPromiseClose(Pending), "pending producer retired"); xrtPromiseDestroy(Pending); Pending = NULL; }
    xrtErrorFree(Cause); Cause = NULL;
    xrtErrorFree(MetadataCause); MetadataCause = NULL; xrtClearError();
    xmemdebugsnapshot Memory; xrtMemDebugSnapshot(&Memory);
    require(Memory.LiveCount == 0 && Memory.LiveBytes == 0 && Memory.InvalidFreeCount == 0 && Memory.DoubleFreeCount == 0, "no leaked allocation or invalid free");
}
static bool has_cause(const xerror* error, const xerror* cause)
{
    for (unsigned i = 0; error && i < 16u; ++i, error = xrtErrorCause(error))
        if (error == cause) return true;
    return false;
}
static bool empty_response(const xacmehttpresponse* response)
{
    return response->iStatus == 0 && response->sLocation == NULL && response->sReplayNonce == NULL &&
        response->sRetryAfter == NULL && response->sLink == NULL && response->sContentType == NULL &&
        response->sBody == NULL && response->iBodySize == 0;
}
static bool exchange(xacmehttp* http, xacmehttpresponse* response)
{
    return xacmeHttpExchangeOnceV(http, "POST", Tls ? "https://ca.example/order" : "http://ca.example/order",
        "application/jose+json", XRT_STR_LITERAL("{}"), NULL, 0u, response);
}
static void arguments(void)
{
    xacmehttp http;
    memset(&http, 0, sizeof(http));
    Mode = "arguments"; Tls = false;
    http.pEngine = (xnetengine*)&RoutingToken; http.pResolver = (xnetresolver*)&RoutingToken;
    http.pVerifier = (xtlsverifier*)&RoutingToken; http.uTimeoutMs = 1000;
    for (unsigned i = 0; i < 3u; ++i) {
        xacmehttpresponse response;
        memset(&response, 0xa5, sizeof(response));
        bool ok = xacmeHttpExchangeOnceV(i == 0 ? NULL : &http, i == 1 ? NULL : "POST",
            i == 2 ? NULL : "http://ca.example/order", NULL, XRT_STR_LITERAL("{}"), NULL, 0u, &response);
        require(!ok && empty_response(&response) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
            "invalid input clears every response output before returning");
        require(Dials == 0, "invalid input cannot start a transport");
        clean();
    }
    static const int64 InvalidTimeouts[] = { -2, INT64_MIN };
    for (size_t i = 0; i < sizeof(InvalidTimeouts) / sizeof(InvalidTimeouts[0]); i++) {
        xacmehttp Invalid; memset(&Invalid, 0xa5, sizeof(Invalid));
        require(!xacmeHttpInit(&Invalid, NULL, NULL, InvalidTimeouts[i]) &&
            xrtErrorKind(xrtGetError()) == XERR_ARGUMENT && xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_ARGUMENT &&
            !Invalid.pEngine && !Invalid.bEngineOwned && !Invalid.pResolver && !Invalid.pVerifier,
            "invalid init timeout is rejected before creating transport resources");
        clean();
    }
    puts("[PASS] ACME_HTTP argument outputs");
}
static void run(const char* Name, bool UseTls)
{
    Mode = Name; Tls = UseTls; Dials = Sends = Recvs = Ends = Aborts = Closes = Destroys = 0;
    xacmehttp Http; memset(&Http, 0, sizeof(Http));
    Http.pEngine = (xnetengine*)&RoutingToken; Http.pResolver = (xnetresolver*)&RoutingToken;
    Http.pVerifier = (xtlsverifier*)&RoutingToken; Http.uTimeoutMs = suffix("pending") ? 5 : 1000;
    xacmehttpresponse Response;
    memset(&Response, 0xa5, sizeof(Response));
    if (suffix("io")) xrtSetErrorInfo(XERR_MEMORY, "test.caller", 2803, "stale caller error");
    bool Ok = exchange(&Http, &Response);
    printf("[diagnostic] ACME_HTTP %s %s success=%u kind=%u same-cause=%u uncertain=%u dial=%u send=%u recv=%u abort=%u destroy=%u\n",
        Tls ? "tls" : "tcp", Name, Ok, (unsigned)xrtErrorKind(xrtGetError()),
        Cause != NULL && has_cause(xrtGetError(), Cause), Http.bWriteUncertain, Dials, Sends, Recvs, Aborts, Destroys);
    require(Dials == 1 && Sends <= 1, "one attempt with no automatic replay");
    if (stage("control")) {
        require(Ok && Response.iStatus == 200 && Response.sBody && strcmp(Response.sBody, "{}") == 0, "normal response control");
        require(Aborts == 0 && Closes == 1 && Destroys == 1 && !Http.bWriteUncertain, "successful stream released");
        xacmeHttpResponseUnit(&Response);
    } else {
        require(!Ok && empty_response(&Response), "failure exposes no partial response");
        require(Sends == (stage("connect") ? 0u : 1u) &&
            Recvs == (stage("connect") || stage("send") ? 0u : stage("head") ? 1u : 2u),
            "failure reached the selected transport stage");
        if (Cause && !suffix("wrap-fault")) require(has_cause(xrtGetError(), Cause), "exact worker cause survives cleanup");
        require(xrtErrorKind(xrtGetError()) == (suffix("memory") || suffix("wrap-fault") ? XERR_MEMORY : suffix("pending") ? XERR_TIMEOUT : XERR_IO),
            "failure retains its real category");
        if (stage("send") && !suffix("wrap-fault"))
            require(xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_UNCERTAIN &&
                xrtErrorCode(xrtErrorCause(xrtGetError())) == XACME_HTTP_ERROR_SEND,
                "send failure keeps its stage context below the unknown-outcome marker");
        require(Http.bWriteUncertain == !stage("connect"), "write outcome remains unknown after request start");
        require(stage("connect") ? (Aborts == 0 && Destroys == 0) : (Aborts == 1 && Destroys == 1),
            "failed acquired stream aborted and released once");
        if (suffix("wrap-fault")) {
            require(xrtMemDebugFailTriggered(), "real context allocation failure consumed");
            require(MetadataCause != NULL && has_cause(xrtGetError(), MetadataCause),
                "metadata OOM is retained across cleanup and further context wrapping");
            xrtMemDebugFailClear();
        }
        if (Pending != NULL) {
            xcancel* Cancel = xrtPromiseCancelToken(Pending);
            require(Cancel != NULL && xrtCancelRequested(Cancel), "timed-out producer received cancellation request");
            xrtCancelDestroy(Cancel);
        }
    }
    clean();
    if (!stage("control")) {
        Mode = "control"; Dials = Sends = Recvs = Ends = Aborts = Closes = Destroys = 0;
        Http.uTimeoutMs = 1000;
        require(exchange(&Http, &Response), "same context accepts a separately requested operation");
        require(Response.iStatus == 200 && Response.sBody && strcmp(Response.sBody, "{}") == 0 &&
            Dials == 1 && Sends == 1 && Closes == 1 && Destroys == 1 && !Http.bWriteUncertain,
            "recovery has its own single attempt and cleanup");
        xacmeHttpResponseUnit(&Response); clean(); Mode = Name;
    }
    printf("[PASS] ACME_HTTP %s %s\n", Tls ? "tls" : "tcp", Name);
}
static void run_budget(const char* Name, bool UseTls)
{
    Mode = Name; Tls = UseTls; BudgetRun = true; BudgetClock = 100.0;
    Dials = Sends = Recvs = Ends = Aborts = Closes = Destroys = BudgetSleeps = BudgetSlept = 0;
    memset(DialTimeouts, 0, sizeof(DialTimeouts));
    xacmehttp Http; memset(&Http, 0, sizeof(Http));
    Http.pEngine = (xnetengine*)&RoutingToken; Http.pResolver = (xnetresolver*)&RoutingToken;
    Http.pVerifier = (xtlsverifier*)&RoutingToken;
    bool Forever = strcmp(Name, "budget-forever") == 0;
    bool Control = strcmp(Name, "budget-control") == 0;
    bool RetryControl = strcmp(Name, "budget-retry-control") == 0;
    bool Zero = strcmp(Name, "budget-zero") == 0;
    bool Invalid = strcmp(Name, "budget-invalid") == 0;
    bool Retry = strstr(Name, "retry") != NULL;
    bool Chunks = strcmp(Name, "budget-chunks") == 0;
    bool Memory = strcmp(Name, "budget-late-memory") == 0;
    bool WrapOom = strcmp(Name, "budget-retry-wrap-oom") == 0;
    bool PreSend = strcmp(Name, "budget-presend-spent") == 0;
    bool ConfigSpent = strcmp(Name, "budget-config-spent") == 0;
    Http.uTimeoutMs = Forever ? -1 : Zero ? 0 : Invalid ? -2 : Control ? 1000 : RetryControl ? 2000 :
        strcmp(Name, "budget-retry-two") == 0 ? 700 : 100;
    const char* Method = Retry ? "GET" : "POST";
    xacmehttpresponse Response; memset(&Response, 0xa5, sizeof(Response));
    char* Large = Chunks ? (char*)xrtMalloc(32769u) : NULL;
    require(!Chunks || Large != NULL, "large multi-chunk request allocated");
    if (Chunks) memset(Large, 'P', 32769u);
    xstrview Payload = Chunks ? (xstrview){ Large, 32769u } : (xstrview)XRT_STR_LITERAL("{}");
    bool Ok = xacmeHttpExchangeV(&Http, Method, Tls ? "https://ca.example/budget" : "http://ca.example/budget",
        "application/json", Payload, NULL, 0u, &Response);
    xrtFree(Large);
    printf("[budget] ACME_HTTP %s %s ok=%u kind=%u time=%.3f dial=%u send=%u recv=%u sleep=%u/%u\n",
        Tls ? "tls" : "tcp", Name, Ok, (unsigned)xrtErrorKind(xrtGetError()), BudgetClock - 100.0,
        Dials, Sends, Recvs, BudgetSleeps, BudgetSlept);
    if (Control || Forever || RetryControl) {
        require(Ok && Response.iStatus == 200 && strcmp(Response.sBody, "{}") == 0,
            "in-budget/forever request succeeds");
        require(!Http.bWriteUncertain && Closes == 1u && Destroys == 1u && Aborts == 0,
            "successful budget request releases its only acquired stream");
        require(Dials == (RetryControl ? 2u : 1u) && Sends == 1u && Recvs == 1u,
            "control transport submission counts");
        if (RetryControl) require(BudgetSleeps == 1u && BudgetSlept == 500u &&
            DialTimeouts[1] >= 1439 && DialTimeouts[1] <= 1441,
            "retry receives remaining request budget");
        if (Forever) require(DialTimeouts[0] == XRT_WAIT_FOREVER, "forever budget retained");
        xacmeHttpResponseUnit(&Response);
    } else {
        require(!Ok && empty_response(&Response), "expired request cannot publish successful or partial output");
        require(xrtErrorKind(xrtGetError()) == (Invalid ? XERR_ARGUMENT : (Memory || WrapOom) ? XERR_MEMORY : XERR_TIMEOUT),
            "budget failure keeps argument/memory/timeout category");
        bool BeforeAcquire = Zero || Invalid || ConfigSpent || strcmp(Name, "budget-dial-spent") == 0 || Retry;
        require(Aborts == (BeforeAcquire ? 0u : 1u) && Destroys == (BeforeAcquire ? 0u : 1u) && Closes == 0,
            "timeout aborts only an acquired stream exactly once");
        require(Http.bWriteUncertain == (!BeforeAcquire && !PreSend), "started POST retains unknown-write state; no replay");
        if (Zero || Invalid || ConfigSpent) require(Dials == 0 && Sends == 0 && Recvs == 0, "invalid/zero budget starts no transport");
        else if (Retry) {
            require(Dials == (strcmp(Name, "budget-retry-two") == 0 ? 2u : 1u) && Sends == 0 && Recvs == 0,
                "deadline stops retry before a new dial");
            require(BudgetSlept <= (unsigned)(Http.uTimeoutMs - (int64)Dials * 60) + 1u,
                "backoff consumes only remaining budget");
            if (!WrapOom) require(Cause && has_cause(xrtGetError(), Cause), "exhausted retry retains its original producer cause");
        } else require(Dials == 1u, "single write attempt cannot replay");
        if (WrapOom) {
            require(xrtMemDebugFailTriggered() && MetadataCause && has_cause(xrtGetError(), MetadataCause),
                "budget-context OOM keeps its actual immutable MEMORY cause");
            xrtMemDebugFailClear();
        }
        if (PreSend) require(Sends == 0u && Recvs == 0u, "expiry before send is known not to have written");
        if (strcmp(Name, "budget-send-expired") == 0)
            require(Sends == 1u && Recvs == 0, "spent send budget starts no response operation");
        if (strcmp(Name, "budget-head-body") == 0)
            require(Recvs == 2u, "body shares the already partially consumed head budget");
        if (Memory) require(Cause && has_cause(xrtGetError(), Cause), "late MEMORY survives timeout and abort cleanup");
        if (Chunks) require(Sends == 3u && Recvs == 0, "whole send spans three chunks without resetting its budget");
    }
    clean();
    /* An expired exchange does not poison the context or its configured budget. */
    BudgetRun = false; Mode = "control"; Dials = Sends = Recvs = Ends = Aborts = Closes = Destroys = 0;
    Http.uTimeoutMs = 1000;
    require(exchange(&Http, &Response), "same-context explicit recovery after exhausted budget");
    xacmeHttpResponseUnit(&Response); clean(); Mode = Name;
    printf("[PASS] ACME_HTTP %s %s\n", Tls ? "tls" : "tcp", Name);
}
int main(int Argc, char** Argv)
{
    static const char* Cases[] = { "connect-memory", "send-memory", "head-memory", "body-memory", "end-memory",
        "connect-io", "send-io", "head-io", "body-io", "end-io",
        "connect-protocol", "send-protocol", "head-protocol", "body-protocol", "end-protocol",
        "connect-state", "send-state", "head-state", "body-state", "end-state",
        "connect-direct-memory", "send-direct-memory", "head-direct-memory", "body-direct-memory", "end-direct-memory",
        "connect-direct-io", "send-direct-io", "head-direct-io", "body-direct-io", "end-direct-io",
        "send-current-direct-memory", "send-current-direct-io",
        "connect-wrap-fault", "send-wrap-fault", "head-wrap-fault", "body-wrap-fault", "end-wrap-fault",
        "connect-pending", "send-pending", "head-pending", "body-pending", "end-pending", "control" };
    if (Argc == 3) { if (strncmp(Argv[1], "budget-", 7u) == 0) run_budget(Argv[1], strcmp(Argv[2], "tls") == 0); else run(Argv[1], strcmp(Argv[2], "tls") == 0); return 0; }
    require(Argc == 1, "usage: test_http_fault [case tcp|tls]");
    arguments();
    for (size_t I = 0; I < sizeof(Cases) / sizeof(Cases[0]); I++) for (unsigned T = 0; T < 2; T++) {
        if (T == 0 && strcmp(Cases[I], "end-pending") == 0) continue;
        if (T == 0 && strncmp(Cases[I], "end-direct-", 11u) == 0) continue;
        run(Cases[I], T != 0);
    }
    static const char* Budgets[] = { "budget-control", "budget-forever", "budget-zero", "budget-invalid",
        "budget-config-spent", "budget-dial-spent", "budget-presend-spent", "budget-send-expired", "budget-response-expired", "budget-head-body",
        "budget-chunks", "budget-retry-backoff", "budget-retry-two", "budget-retry-control", "budget-retry-wrap-oom", "budget-late-memory" };
    for (size_t I = 0; I < sizeof(Budgets) / sizeof(Budgets[0]); I++) for (unsigned T = 0; T < 2u; T++)
        run_budget(Budgets[I], T != 0);
    return 0;
}
