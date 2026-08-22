/* 使用共享 HTTP CONNECT 托管代理契约验证 Windows IOCP。 */
#define TEST_PROXY_DIAL_BACKEND XNET_PORT_IOCP
#define TEST_PROXY_DIAL_HTTP_CONNECT 1
#include "test_net_proxy_dial.c"
