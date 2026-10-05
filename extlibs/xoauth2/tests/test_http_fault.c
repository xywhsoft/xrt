/* Actual futures, HTTP parser/buffers and allocator; controlled transport edges.
 * Routing tokens below are never dereferenced as streams. Actual socket/TLS
 * shutdown and factory ownership are checked by the independent wire probe. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XRT_IMPLEMENTATION
#define XRT_MODULE_MEMORY_DEBUG
#include "support/runtime.h"

static const char* Mode;
static bool Tls;
static max_align_t RoutingToken;
static xerror* Cause;
static xpromise* Pending;
static unsigned Dials, Sends, Recvs, Ends, Aborts, Closes, Destroys;
static void require(bool Result, const char* Message)
{
    if (!Result) { fprintf(stderr, "[FAIL] OAUTH_HTTP %s %s: %s\n", Tls ? "tls" : "tcp", Mode, Message); exit(1); }
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
    } else xrtSetErrorInfo(XERR_IO, "test.oauth.http.worker", 2801, "transport failed after dispatch");
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
    return Future;
}
static xfuture* dial(void) { Dials++; return result(stage("connect"), false, &RoutingToken); }
static xfuture* tls_dial(xnetengine* Engine, xnetresolver* Resolver, cstr Host, uint16 Port,
    const xtlsclientconfig* Config, const xtlsdialconfig* Dial, const xtlsstreamevents* Events, ptr Data)
{ (void)Engine; (void)Resolver; (void)Host; (void)Port; (void)Config; (void)Dial; (void)Events; (void)Data; return dial(); }
static xfuture* tcp_dial(xnetengine* Engine, xnetresolver* Resolver, cstr Host, uint16 Port,
    const xnetdialconfig* Config, const xnetstreamevents* Events, ptr Data)
{ (void)Engine; (void)Resolver; (void)Host; (void)Port; (void)Config; (void)Events; (void)Data; return dial(); }
static xtlsstream* tls_ref(xtlsstream* Stream) { require(Stream == (xtlsstream*)&RoutingToken, "TLS routing token"); return Stream; }
static xnetstream* tcp_ref(xnetstream* Stream) { require(Stream == (xnetstream*)&RoutingToken, "TCP routing token"); return Stream; }
static xfuture* tls_send(xtlsstream* Stream, const void* Data, size_t Size)
{ (void)Stream; (void)Data; require(Size > 0, "request sent"); Sends++; return result(stage("send"), false, NULL); }
static xnetresult tcp_send(xnetstream* Stream, const void* Data, size_t Size)
{ (void)Stream; (void)Data; require(Size > 0, "request sent"); Sends++; return stage("send") ? XNET_RESULT_AGAIN : XNET_RESULT_OK; }
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
    if (stage("head") || (stage("body") && Recvs == 2)) return result(true, false, NULL);
    if (stage("end") && Recvs == 2) return result(false, true, NULL);
    require(Recvs == 1, "bounded receive sequence");
    if (stage("body")) return bytes_result("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n");
    if (stage("end")) return bytes_result("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n{}");
    return bytes_result("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}");
}
static xfuture* tls_recv(xtlsstream* Stream, size_t Size) { (void)Stream; (void)Size; return recv_data(); }
static xfuture* tcp_recv(xnetstream* Stream, size_t Size) { (void)Stream; (void)Size; return recv_data(); }
static xfuture* tls_wait(xtlsstream* Stream, xtlsstreamwait Wait)
{ (void)Stream; require(Wait == XTLS_STREAM_WAIT_END, "authenticated EOF wait"); Ends++; return result(true, false, NULL); }
static bool tcp_stats(const xnetstream* Stream, xnetstreamstats* Stats)
{ (void)Stream; memset(Stats, 0, sizeof(*Stats)); Stats->ReadEnded = true; return true; }
static const xerror* tcp_error(const xnetstream* Stream)
{ (void)Stream; Ends++; make_cause(); xrtClearError(); return Cause; }
static bool cleanup(bool Abort)
{
    if (Abort) { Aborts++; xrtSetErrorInfo(XERR_STATE, "test.oauth.http.cleanup", 2802, "cleanup secondary failure"); }
    else Closes++;
    return true;
}
static bool tls_abort(xtlsstream* Stream) { (void)Stream; return cleanup(true); }
static bool tcp_abort(xnetstream* Stream) { (void)Stream; return cleanup(true); }
static bool tls_close(xtlsstream* Stream) { (void)Stream; return cleanup(false); }
static bool tcp_close(xnetstream* Stream) { (void)Stream; return cleanup(false); }
static void tls_destroy(xtlsstream* Stream) { (void)Stream; Destroys++; }
static void tcp_destroy(xnetstream* Stream) { (void)Stream; Destroys++; }
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
#include "support/implementation.c"

static void clean(void)
{
    if (Pending != NULL) { require(xrtPromiseClose(Pending), "pending producer retired"); xrtPromiseDestroy(Pending); Pending = NULL; }
    xrtErrorFree(Cause); Cause = NULL; xrtClearError();
    xmemdebugsnapshot Memory; xrtMemDebugSnapshot(&Memory);
    require(Memory.LiveCount == 0 && Memory.LiveBytes == 0 && Memory.InvalidFreeCount == 0 && Memory.DoubleFreeCount == 0, "no leaked allocation or invalid free");
}
static void run(const char* Name, bool UseTls)
{
    Mode = Name; Tls = UseTls; Dials = Sends = Recvs = Ends = Aborts = Closes = Destroys = 0;
    xoauth2httpxrt Http; memset(&Http, 0, sizeof(Http));
    Http.pEngine = (xnetengine*)&RoutingToken; Http.pResolver = (xnetresolver*)&RoutingToken;
    Http.pVerifier = &RoutingToken; Http.uTimeoutUs = suffix("pending") ? 5000u : 1000000u;
    char* Response = (char*)&RoutingToken; int Status = 999;
    /* An old caller error must not turn the next IO failure into MEMORY. */
    if (suffix("io")) xrtSetErrorInfo(XERR_MEMORY, "test.caller", 2803, "stale caller error");
    bool Ok = xoauth2HttpXrt("POST", Tls ? "https://idp.example/token" : "http://idp.example/token", "code=one", NULL, &Response, &Status, &Http);
    printf("[diagnostic] OAUTH_HTTP %s %s success=%u kind=%u same-cause=%u dial=%u send=%u recv=%u abort=%u destroy=%u\n", Tls ? "tls" : "tcp", Name, Ok, (unsigned)xrtErrorKind(xrtGetError()), Cause != NULL && xrtGetError() == Cause, Dials, Sends, Recvs, Aborts, Destroys);
    require(Dials == 1 && Sends <= 1, "one attempt with no automatic replay");
    if (stage("control")) {
        require(Ok && Status == 200 && Response != NULL && strcmp(Response, "{}") == 0, "normal response control");
        require(Aborts == 0 && Closes == 1 && Destroys == 1, "successful stream released"); xrtFree(Response);
    } else {
        require(!Ok && Response == NULL && Status == 0, "failure exposes no partial response");
        if (suffix("memory")) require(Cause != NULL && xrtGetError() == Cause && xrtErrorKind(Cause) == XERR_MEMORY, "exact worker allocation cause survives cleanup");
        else require(xoauth2LastError() == XOAUTH2_ERROR_NETWORK, "ordinary transport failure keeps NETWORK classification");
        require(stage("connect") ? (Aborts == 0 && Destroys == 0) : (Aborts == 1 && Destroys == 1), "failed acquired stream aborted and released once");
        if (Pending != NULL) {
            xcancel* Cancel = xrtPromiseCancelToken(Pending);
            require(Cancel != NULL && xrtCancelRequested(Cancel), "timed-out producer received cancellation request");
            xrtCancelDestroy(Cancel);
        }
    }
    clean();
    if (!stage("control")) {
        Mode = "control"; Dials = Sends = Recvs = Ends = Aborts = Closes = Destroys = 0;
        Http.uTimeoutUs = 1000000u;
        require(xoauth2HttpXrt("POST", Tls ? "https://idp.example/token" : "http://idp.example/token", "code=new", NULL, &Response, &Status, &Http), "same context accepts a separately requested operation");
        require(Status == 200 && Response != NULL && strcmp(Response, "{}") == 0 && Dials == 1 && Sends == 1 && Closes == 1 && Destroys == 1, "recovery has its own single attempt and cleanup");
        xrtFree(Response); clean(); Mode = Name;
    }
    printf("[PASS] OAUTH_HTTP %s %s\n", Tls ? "tls" : "tcp", Name);
}
int main(int Argc, char** Argv)
{
    static const char* Cases[] = { "connect-memory", "send-memory", "head-memory", "body-memory", "end-memory",
        "connect-io", "send-io", "head-io", "body-io", "end-io", "connect-pending", "send-pending", "head-pending", "body-pending", "end-pending", "control" };
    if (Argc == 3) { run(Argv[1], strcmp(Argv[2], "tls") == 0); return 0; }
    require(Argc == 1, "usage: test_http_fault [case tcp|tls]");
    for (size_t I = 0; I < sizeof(Cases) / sizeof(Cases[0]); I++) for (unsigned T = 0; T < 2; T++) {
        if (T == 0 && strcmp(Cases[I], "end-pending") == 0) continue;
        run(Cases[I], T != 0);
    }
    return 0;
}
