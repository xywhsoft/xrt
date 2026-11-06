/* Keep the feature-test macros in bench_common.h ahead of system headers. */
#define main testWebSocketLoopbackMain
#include "../../bench/bench_websocket_loopback.c"
#undef main

int main(void)
{
    char* arguments[] = {"websocket_loopback_smoke", "50", NULL};
    int result = testWebSocketLoopbackMain(2, arguments);
    if (result == 0) printf("[PASS] WebSocket loopback benchmark smoke\n");
    return result;
}
