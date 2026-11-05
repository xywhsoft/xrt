#include "../test.h"
#include <xrt/future_bridge.h>
#include "../../../../tests/network/engine_test.h"

static xerror* WatchError;
static xatomic32 RequestDone, Shutdown;

/* Keep the real Upgrade, bridge, Future, error and allocator. Inject only the
 * current Worker's Watch allocation, after the real producer has accepted.
 * Upgrade completion is queued on this same Worker, as the API requires. */
static bool setup_bridge_watch(xfuturebridge* bridge, xcancelproc proc, ptr data)
{
    bool result;
    testRequire(xrtMemDebugFailAfter(0), "real server watch OOM injection failed");
    result = xrtFutureBridgeWatch(bridge, proc, data);
    testRequire(!result && xrtMemDebugFailTriggered(), "real server Watch did not hit OOM");
    WatchError = xrtErrorRef(xrtGetError());
    xrtMemDebugFailClear();
    return result;
}

#define xrtWsUpgradeAsync setup_upgrade_async
#define xrtFutureBridgeWatch setup_bridge_watch
#include "../../src/websocket/server_future.c"
#undef xrtWsUpgradeAsync
#undef xrtFutureBridgeWatch

static void setup_request(xhttpserver* server, xhttpconn* http,
    const xhttpserverrequest* request, ptr data)
{
    xfuture* future;
    (void)server; (void)request; (void)data;
    future = setup_upgrade_async(http, NULL, NULL, NULL);
    testRequire(future == NULL && WatchError != NULL && xrtGetError() == WatchError &&
        xrtErrorKind(WatchError) == XERR_MEMORY, "real server setup OOM error mismatch");
    xrtFutureDestroy(future);
    xrtErrorFree(WatchError);
    WatchError = NULL;
    xrtClearError();
    xrtAtomic32Store(&RequestDone, 1, XMEMORY_RELEASE);
}

static void setup_shutdown(xhttpserver* server, ptr data)
{
    (void)server; (void)data;
    xrtAtomic32Store(&Shutdown, 1, XMEMORY_RELEASE);
}

static void setup_wait(const xatomic32* value, cstr message)
{
    double deadline = __xrtWaitAfter(10000);
    while (xrtAtomic32Load(value, XMEMORY_ACQUIRE) == 0) {
        testRequire(!__xrtWaitExpired(deadline), message);
        xrtThreadYield();
    }
}

/* Exercise the actual Upgrade, HTTP connection and queued completion on a
 * real server, respecting the connection's Worker requirement. */
static void setup_real_case(void)
{
    static const char request[] =
        "GET /setup HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\n"
        "Upgrade: websocket\r\nSec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
    xnetengineconfig engine_config;
    xhttpserverconfig config;
    xhttpserverevents events;
    xnetengine* engine;
    xhttpserver* server;
    xnetaddr address;
    xnetsocket socket;
    size_t offset = 0;

    WatchError = NULL;
    xrtAtomic32Init(&RequestDone, 0);
    xrtAtomic32Init(&Shutdown, 0);
    xrtNetEngineConfigInit(&engine_config);
    engine_config.Backend = XNET_PORT_SELECT;
    engine_config.Workers = 2;
    engine = xrtNetEngineCreate(&engine_config);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "real server setup engine failed");
    xrtHttpServerConfigInit(&config);
    testRequire(xrtNetAddrLoopback(&config.Network.Listen.Address, XNET_FAMILY_IPV4, 0),
        "real server setup listener address failed");
    xrtHttpServerEventsInit(&events);
    events.Request = setup_request;
    events.Shutdown = setup_shutdown;
    server = xrtHttpServerStart(engine, &config, &events);
    testRequire(server != NULL && xrtHttpServerLocal(server, 0, &address),
        "real server setup start failed");
    socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
    testRequire(socket != NULL && xrtNetSocketConnect(socket, &address) == XNET_RESULT_OK,
        "real server setup client connect failed");
    while (offset != sizeof(request) - 1) {
        size_t sent = 0;
        testRequire(xrtNetSocketSend(socket, (cbytes)request + offset,
            sizeof(request) - 1 - offset, &sent) == XNET_RESULT_OK && sent != 0,
            "real server setup request send failed");
        offset += sent;
    }
    setup_wait(&RequestDone, "real server setup request did not finish");
    testRequire(xrtNetSocketClose(socket), "real server setup socket close failed");
    testRequire(xrtHttpServerDrain(server), "real server setup drain failed");
    setup_wait(&Shutdown, "real server setup did not drain");
    xrtHttpServerDestroy(server);
    testRequire(testNetEngineDestroy(engine), "real server setup engine did not retire");
    xrtClearError();
    testMemoryDebugDrain("real server setup failure leaked Core resources");
    printf("[PASS] WebSocket server real Upgrade setup OOM retired\n");
}

int main(void)
{
    setup_real_case();
    return 0;
}
