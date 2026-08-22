/* 使用同一真实 TLS/HTTP 生命周期验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_TLS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_TLS_BACKEND_NAME "IOCP"
#include "test_http_client_tls.c"
