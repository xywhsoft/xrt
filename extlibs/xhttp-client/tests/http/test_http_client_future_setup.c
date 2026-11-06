#include "../test.h"
#include <xrt/future_bridge.h>
#include "../../../../tests/network/engine_test.h"

/* Use the real Core bridge, Future, error and allocator. Control only the
 * producer boundary to retire the context immediately after Fail publishes. */
typedef struct test_http_setup_call {
    int References;
    unsigned Cancels;
    unsigned Invalid;
} test_http_setup_call;

static test_http_setup_call Call;
static xhttpcallproc Complete;
static ptr Context;
static xerror* WatchError;
static unsigned Mode;
static unsigned Completions;
static bool RealCall;
static union { uint64 Alignment; uint8 Data[64]; } Client, Request;

static bool setup_call_cancel(xhttpcall* call)
{
    if (RealCall) return xrtHttpCallCancel(call);
    if (call != (xhttpcall*)&Call || Call.References == 0) {
        Call.Invalid++;
        return false;
    }
    Call.Cancels++;
    return true;
}

static void setup_call_destroy(xhttpcall* call)
{
    if (RealCall) { xrtHttpCallDestroy(call); return; }
    if (call == NULL) return;
    if (call != (xhttpcall*)&Call || Call.References == 0) {
        Call.Invalid++;
        return;
    }
    Call.References--;
}

static void setup_complete(void)
{
    xhttpcallproc proc = Complete;
    ptr context = Context;
    xhttpcallresult result = {0};

    Complete = NULL;
    Context = NULL;
    Completions++;
    result.Result = XNET_RESULT_CANCELLED;
    proc((xhttpcall*)&Call, &result, context);
    /* The callback borrows the producer's independent Call reference. */
    setup_call_destroy((xhttpcall*)&Call);
}

static xhttpcall* setup_submit(xhttpclient* client, const xhttprequest* request,
    const xhttpcalloptions* options, xhttpcallproc proc, ptr data)
{
    if (RealCall) return xrtHttpClientDo(client, request, options, proc, data);
    (void)client; (void)request; (void)options;
    Call = (test_http_setup_call){2, 0, 0};
    Complete = proc;
    Context = data;
    if (Mode != 0) {
        testRequire(xrtMemDebugFailAfter(0), "HTTP setup watch OOM injection failed");
    }
    return (xhttpcall*)&Call;
}

static bool setup_bridge_watch(xfuturebridge* bridge, xcancelproc proc, ptr data)
{
    if (RealCall) testRequire(xrtMemDebugFailAfter(0), "real HTTP watch OOM injection failed");
    bool result = xrtFutureBridgeWatch(bridge, proc, data);
    if (!result) {
        testRequire(xrtMemDebugFailTriggered(), "HTTP setup watch did not hit real OOM");
        WatchError = xrtErrorRef(xrtGetError());
        xrtMemDebugFailClear();
    }
    return result;
}

static bool setup_bridge_fail(xfuturebridge* bridge)
{
    bool result = xrtFutureBridgeFail(bridge);
    testRequire(result, "HTTP setup failure publication failed");
    if (Mode == 2) setup_complete();
    return result;
}

/* Rename exported definitions to coexist with the linked modular library. */
#define xrtHttpClientDo setup_submit
#define xrtHttpCallCancel setup_call_cancel
#define xrtHttpCallDestroy setup_call_destroy
#define xrtFutureBridgeWatch setup_bridge_watch
#define xrtFutureBridgeFail setup_bridge_fail
#define __xrtHttpClientFutureFinish setup_client_future_finish
#define xrtHttpClientWaitAsync setup_client_wait_async
#define xrtHttpResultRef setup_result_ref
#define xrtHttpResultDestroy setup_result_destroy
#define xrtHttpResultResponse setup_result_response
#define xrtHttpResultTakeResponse setup_result_take_response
#define xrtHttpResultTcp setup_result_tcp
#define xrtHttpResultTakeTcp setup_result_take_tcp
#define xrtHttpResultTls setup_result_tls
#define xrtHttpResultTakeTls setup_result_take_tls
#define xrtHttpResultBuffered setup_result_buffered
#define xrtHttpResultRedirects setup_result_redirects
#define xrtHttpResultUpgraded setup_result_upgraded
#define xrtHttpResultInfo setup_result_info
#define xrtHttpClientDoAsync setup_client_do_async
#define xrtHttpClientDoSync setup_client_do_sync
#include "../../src/http/http_client_future.c"
#undef xrtHttpClientDo
#undef xrtHttpCallCancel
#undef xrtHttpCallDestroy
#undef xrtFutureBridgeWatch
#undef xrtFutureBridgeFail
#undef __xrtHttpClientFutureFinish
#undef xrtHttpClientWaitAsync
#undef xrtHttpResultRef
#undef xrtHttpResultDestroy
#undef xrtHttpResultResponse
#undef xrtHttpResultTakeResponse
#undef xrtHttpResultTcp
#undef xrtHttpResultTakeTcp
#undef xrtHttpResultTls
#undef xrtHttpResultTakeTls
#undef xrtHttpResultBuffered
#undef xrtHttpResultRedirects
#undef xrtHttpResultUpgraded
#undef xrtHttpResultInfo
#undef xrtHttpClientDoAsync
#undef xrtHttpClientDoSync

static bool setup_case(bool options, unsigned mode)
{
    xhttpcalloptions config;
    xfuture* future;
    bool valid;

    Mode = mode;
    Completions = 0;
    WatchError = NULL;
    xrtClearError();
    xrtHttpCallOptionsInit(&config);
    future = setup_client_do_async((xhttpclient*)&Client,
        (const xhttprequest*)&Request, options ? &config : NULL);
    if (mode == 0) {
        valid = future != NULL && xrtFutureCancel(future);
        setup_complete();
        valid = valid && xrtFutureState(future) == XFUTURE_CANCELLED &&
            xrtFutureWaitFor(future, 0) == XWAIT_OK;
        xrtFutureDestroy(future);
    } else {
        valid = future == NULL && WatchError != NULL &&
            xrtGetError() == WatchError && xrtErrorKind(WatchError) == XERR_MEMORY;
        if (mode == 1) setup_complete();
        xrtFutureDestroy(future);
    }
    valid = valid && Call.Invalid == 0 && Call.Cancels == 1 &&
        Call.References == 0 && Completions == 1;
    printf("[%s] HTTP options=%u setup mode=%u invalid=%u cancels=%u refs=%d\n",
        valid ? "PASS" : "FAIL", options ? 1u : 0u, mode,
        Call.Invalid, Call.Cancels, Call.References);
    xrtErrorFree(WatchError);
    WatchError = NULL;
    xrtClearError();
    testMemoryDebugDrain("HTTP setup completion leaked Core resources");
    return valid;
}

static bool setup_decline(xnetlistener* listener, xnetstream* stream, ptr data)
{
    (void)listener; (void)stream; (void)data;
    return false;
}

/* Also retire real Call/transport ownership on two real network Workers. */
static bool setup_real_case(bool options)
{
    xnetengineconfig engine_config;
    xnetlistenconfig listen_config;
    xnetlistenerevents events = {0};
    xhttpcalloptions config;
    xnetengine* engine;
    xnetlistener* listener;
    xhttpclient* client;
    xhttprequest* request;
    xnetaddr address;
    xfuture* future;
    char url[128];
    int length;
    bool valid;
    double deadline;

    /* Both routing transitions are outside the entire engine lifetime. */
    RealCall = true;
    Mode = 1;
    WatchError = NULL;
    xrtNetEngineConfigInit(&engine_config);
    engine_config.Backend = XNET_PORT_SELECT;
    engine_config.Workers = 2;
    engine = xrtNetEngineCreate(&engine_config);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "real HTTP setup engine failed");
    xrtNetListenConfigInit(&listen_config);
    testRequire(xrtNetAddrLoopback(&listen_config.Address, XNET_FAMILY_IPV4, 0),
        "real HTTP setup listener address failed");
    events.Accept = setup_decline;
    listener = xrtNetListen(engine, &listen_config, &events, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &address),
        "real HTTP setup listener failed");
    client = xrtHttpClientCreate(engine, NULL);
    testRequire(client != NULL, "real HTTP setup client failed");
    length = snprintf(url, sizeof(url), "http://127.0.0.1:%u/setup", (unsigned)address.Port);
    testRequire(length > 0 && (size_t)length < sizeof(url), "real HTTP setup URL overflowed");
    request = xrtHttpRequestCreate(XRT_STR_LITERAL("GET"), (xstrview){url, (size_t)length});
    testRequire(request != NULL, "real HTTP setup request failed");
    xrtHttpCallOptionsInit(&config);
    future = setup_client_do_async(client, request, options ? &config : NULL);
    valid = future == NULL && WatchError != NULL && xrtGetError() == WatchError &&
        xrtErrorKind(WatchError) == XERR_MEMORY;
    xrtFutureDestroy(future);
    xrtHttpRequestDestroy(request);
    testRequire(xrtHttpClientDrain(client), "real HTTP setup client drain failed");
    deadline = __xrtWaitAfter(10000);
    while (xrtHttpClientState(client) != XHTTP_CLIENT_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "real HTTP setup client did not retire");
        xrtThreadYield();
    }
    xrtHttpClientDestroy(client);
    testRequire(xrtNetListenerClose(listener), "real HTTP setup listener close failed");
    xrtNetListenerDestroy(listener);
    testRequire(testNetEngineDestroy(engine), "real HTTP setup engine did not retire");
    RealCall = false;
    xrtErrorFree(WatchError);
    WatchError = NULL;
    xrtClearError();
    testMemoryDebugDrain("real HTTP setup failure leaked Core resources");
    printf("[%s] HTTP options=%u real Call setup OOM retired\n",
        valid ? "PASS" : "FAIL", options ? 1u : 0u);
    return valid;
}

int main(void)
{
    bool valid = true;
    for (unsigned options = 0; options != 2; options++) {
        for (unsigned mode = 0; mode != 3; mode++) {
            if (!setup_case(options != 0, mode)) valid = false;
        }
    }
    if (!setup_real_case(false)) valid = false;
    if (!setup_real_case(true)) valid = false;
    return valid ? 0 : 1;
}
