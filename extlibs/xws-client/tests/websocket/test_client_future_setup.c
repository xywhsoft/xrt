#include "../test.h"
#include <xrt/future_bridge.h>
#include "../../../../tests/network/engine_test.h"

/* Keep Core's real bridge, Future, error and allocator. Only the submitted
 * producer boundary is controlled, so completion can retire the context at
 * the exact setup publication boundary without timing sleeps. */
typedef struct test_ws_setup_call {
    int References;
    unsigned Cancels;
    unsigned Invalid;
} test_ws_setup_call;

static test_ws_setup_call Call;
static xwsconnectproc Complete;
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
    xwsconnectproc proc = Complete;
    ptr context = Context;

    Complete = NULL;
    Context = NULL;
    Completions++;
    proc((xhttpcall*)&Call, XNET_RESULT_CANCELLED, NULL, NULL, NULL, context);
    /* The producer independently owns its callback's borrowed Call. */
    setup_call_destroy((xhttpcall*)&Call);
}

static xhttpcall* setup_submit(xwsconnectproc proc, ptr data)
{
    Call = (test_ws_setup_call){2, 0, 0};
    Complete = proc;
    Context = data;
    if (Mode != 0) {
        testRequire(xrtMemDebugFailAfter(0), "setup watch OOM injection failed");
    }
    return (xhttpcall*)&Call;
}

static xhttpcall* setup_connect(xhttpclient* client, xstrview url,
    const xwsclientconfig* config, const xwsconnevents* events, ptr event_data,
    xwsconnectproc proc, ptr data)
{
    if (RealCall) return xrtWsConnect(client, url, config, events, event_data, proc, data);
    (void)client; (void)url; (void)config; (void)events; (void)event_data;
    return setup_submit(proc, data);
}

static xhttpcall* setup_connect_request(xhttpclient* client,
    const xhttprequest* request, const xwsclientconfig* config,
    const xwsconnevents* events, ptr event_data, xwsconnectproc proc, ptr data)
{
    if (RealCall) return xrtWsConnectRequest(client, request, config, events, event_data, proc, data);
    (void)request;
    return setup_connect(client, (xstrview){0}, config, events, event_data, proc, data);
}

static bool setup_bridge_watch(xfuturebridge* bridge, xcancelproc proc, ptr data)
{
    if (RealCall) testRequire(xrtMemDebugFailAfter(0), "real Call watch OOM injection failed");
    bool result = xrtFutureBridgeWatch(bridge, proc, data);
    if (!result) {
        testRequire(xrtMemDebugFailTriggered(), "setup watch did not hit real OOM");
        WatchError = xrtErrorRef(xrtGetError());
        xrtMemDebugFailClear();
    }
    return result;
}

static bool setup_bridge_fail(xfuturebridge* bridge)
{
    bool result = xrtFutureBridgeFail(bridge);
    testRequire(result, "setup bridge failure publication failed");
    if (Mode == 2) setup_complete();
    return result;
}

/* Compile the actual adapter with only these boundary substitutions. Rename
 * public entry points to coexist with the separately linked modular library. */
#define xrtWsConnect setup_connect
#define xrtWsConnectRequest setup_connect_request
#define xrtHttpCallCancel setup_call_cancel
#define xrtHttpCallDestroy setup_call_destroy
#define xrtFutureBridgeWatch setup_bridge_watch
#define xrtFutureBridgeFail setup_bridge_fail
#define xrtWsConnectAsync setup_connect_async
#define xrtWsConnectRequestAsync setup_connect_request_async
#define xrtWsConnectSync setup_connect_sync
#define xrtWsConnectRequestSync setup_connect_request_sync
#include "../../src/websocket/client_future.c"
#undef xrtWsConnect
#undef xrtWsConnectRequest
#undef xrtHttpCallCancel
#undef xrtHttpCallDestroy
#undef xrtFutureBridgeWatch
#undef xrtFutureBridgeFail
#undef xrtWsConnectAsync
#undef xrtWsConnectRequestAsync
#undef xrtWsConnectSync
#undef xrtWsConnectRequestSync

static bool setup_case(bool request, unsigned mode)
{
    xfuture* future;
    bool valid;

    Mode = mode;
    Completions = 0;
    WatchError = NULL;
    xrtClearError();
    future = request ? setup_connect_request_async((xhttpclient*)&Client,
        (const xhttprequest*)&Request, NULL, NULL, NULL) :
        setup_connect_async((xhttpclient*)&Client, XRT_STR_LITERAL("ws://setup.test/"),
            NULL, NULL, NULL);
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
    printf("[%s] WebSocket %s setup mode=%u invalid=%u cancels=%u refs=%d\n",
        valid ? "PASS" : "FAIL", request ? "request" : "URL", mode,
        Call.Invalid, Call.Cancels, Call.References);
    xrtErrorFree(WatchError);
    WatchError = NULL;
    xrtClearError();
    testMemoryDebugDrain("WebSocket setup completion leaked Core resources");
    return valid;
}

static bool setup_decline(xnetlistener* listener, xnetstream* stream, ptr data)
{
    (void)listener; (void)stream; (void)data;
    return false;
}

/* Supplement the deterministic producer interleavings with actual HTTP Call
 * ownership, TCP cancellation and asynchronous retirement on real Workers. */
static bool setup_real_case(bool request)
{
    xnetengineconfig engine_config;
    xnetlistenconfig listen_config;
    xnetlistenerevents events = {0};
    xnetengine* engine;
    xnetlistener* listener;
    xhttpclient* client;
    xhttprequest* prepared = NULL;
    xnetaddr address;
    xfuture* future;
    char url[128];
    int length;
    bool valid;
    double deadline;

    /* No worker observes a change to this test routing flag: both transitions
     * occur outside the entire engine lifetime. */
    RealCall = true;
    Mode = 1;
    WatchError = NULL;
    xrtNetEngineConfigInit(&engine_config);
    engine_config.Backend = XNET_PORT_SELECT;
    engine_config.Workers = 2;
    engine = xrtNetEngineCreate(&engine_config);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "real setup engine failed");
    xrtNetListenConfigInit(&listen_config);
    testRequire(xrtNetAddrLoopback(&listen_config.Address, XNET_FAMILY_IPV4, 0),
        "real setup listener address failed");
    events.Accept = setup_decline;
    listener = xrtNetListen(engine, &listen_config, &events, NULL, NULL);
    testRequire(listener != NULL && xrtNetListenerLocal(listener, &address),
        "real setup listener failed");
    client = xrtHttpClientCreate(engine, NULL);
    testRequire(client != NULL, "real setup HTTP client failed");
    length = snprintf(url, sizeof(url), "ws://127.0.0.1:%u/setup", (unsigned)address.Port);
    testRequire(length > 0 && (size_t)length < sizeof(url), "real setup URL overflowed");
    if (request) {
        prepared = xrtWsRequestCreate((xstrview){url, (size_t)length});
        testRequire(prepared != NULL, "real setup request failed");
    }
    future = request ? setup_connect_request_async(client, prepared, NULL, NULL, NULL) :
        setup_connect_async(client, (xstrview){url, (size_t)length}, NULL, NULL, NULL);
    valid = future == NULL && WatchError != NULL && xrtGetError() == WatchError &&
        xrtErrorKind(WatchError) == XERR_MEMORY;
    xrtFutureDestroy(future);
    xrtHttpRequestDestroy(prepared);
    testRequire(xrtHttpClientDrain(client), "real setup client drain failed");
    deadline = __xrtWaitAfter(10000);
    while (xrtHttpClientState(client) != XHTTP_CLIENT_CLOSED) {
        testRequire(!__xrtWaitExpired(deadline), "real setup client did not retire");
        xrtThreadYield();
    }
    xrtHttpClientDestroy(client);
    testRequire(xrtNetListenerClose(listener), "real setup listener close failed");
    xrtNetListenerDestroy(listener);
    testRequire(testNetEngineDestroy(engine), "real setup engine did not retire");
    RealCall = false;
    xrtErrorFree(WatchError);
    WatchError = NULL;
    xrtClearError();
    testMemoryDebugDrain("real WebSocket setup failure leaked Core resources");
    printf("[%s] WebSocket %s real Call setup OOM retired\n",
        valid ? "PASS" : "FAIL", request ? "request" : "URL");
    return valid;
}

int main(void)
{
    bool valid = true;
    for (unsigned request = 0; request != 2; request++) {
        for (unsigned mode = 0; mode != 3; mode++) {
            if (!setup_case(request != 0, mode)) valid = false;
        }
    }
    if (!setup_real_case(false)) valid = false;
    if (!setup_real_case(true)) valid = false;
    return valid ? 0 : 1;
}
