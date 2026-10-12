/* 使用同一真实 CONNECT 与池隔离契约验证 Windows IOCP。 */
#define TEST_HTTP_CLIENT_PROXY_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_PROXY_BACKEND_NAME "IOCP"
#include "test_http_client_proxy.c"


