/* 在 IOCP 后端复用同一套 WebSocket 代理组合契约。 */
#define TEST_WS_HTTP_PROXY_BACKEND XNET_PORT_IOCP
#define TEST_WS_HTTP_PROXY_BACKEND_NAME "iocp"
#include "test_http_proxy.c"
