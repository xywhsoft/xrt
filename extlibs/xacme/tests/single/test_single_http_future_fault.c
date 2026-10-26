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

static const char* Mode;
static bool Tls;
static max_align_t RoutingToken;
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
    if (Fail) { make_cause(); require(xrtPromiseReject(Promise, Cause), "worker failure published"); }
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
    if (stage("connect") && strstr(Mode, "-direct-") != NULL) { make_cause(); return NULL; }
    return result(stage("connect"), false, &RoutingToken);
}
static xfuture* tls_dial(xnetengine* Engine, xnetresolver* Resolver, cstr Host, uint16 Port,
    const xtlsclientconfig* Config, const xtlsdialconfig* Dial, const xtlsstreamevents* Events, ptr Data)
{ (void)Engine; (void)Resolver; (void)Host; (void)Port; (void)Config; (void)Dial; (void)Events; (void)Data; return dial(); }
static xfuture* tcp_dial(xnetengine* Engine, xnetresolver* Resolver, cstr Host, uint16 Port,
    const xnetdialconfig* Config, const xnetstreamevents* Events, ptr Data)
{ (void)Engine; (void)Resolver; (void)Host; (void)Port; (void)Config; (void)Events; (void)Data; return dial(); }
static xtlsstream* tls_ref(xtlsstream* Stream) { require(Stream == (xtlsstream*)&RoutingToken, "TLS routing token"); return Stream; }
static xnetstream* tcp_ref(xnetstream* Stream) { require(Stream == (xnetstream*)&RoutingToken, "TCP routing token"); return Stream; }
static xfuture* tls_send(xtlsstream* Stream, const void* Data, size_t Size)
{
    (void)Stream; (void)Data; require(Size > 0, "request sent"); Sends++;
    if (stage("send") && strstr(Mode, "-direct-") != NULL) { make_cause(); return NULL; }
    return result(stage("send"), false, NULL);
}
static xnetresult tcp_send(xnetstream* Stream, const void* Data, size_t Size)
{
    (void)Stream; (void)Data; require(Size > 0, "request sent"); Sends++;
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
    if (wrapped == NULL && suffix("wrap-fault")) {
        require(MetadataCause == NULL, "one metadata allocation failure");
        MetadataCause = xrtErrorRef(xrtGetError());
    }
    return wrapped;
}
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
    if (Argc == 3) { run(Argv[1], strcmp(Argv[2], "tls") == 0); return 0; }
    require(Argc == 1, "usage: test_http_fault [case tcp|tls]");
    arguments();
    for (size_t I = 0; I < sizeof(Cases) / sizeof(Cases[0]); I++) for (unsigned T = 0; T < 2; T++) {
        if (T == 0 && strcmp(Cases[I], "end-pending") == 0) continue;
        if (T == 0 && strncmp(Cases[I], "end-direct-", 11u) == 0) continue;
        run(Cases[I], T != 0);
    }
    return 0;
}
