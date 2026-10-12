/* 使用同一真实 CONNECT/TLS 契约验证 Windows IOCP。 */
#define TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND_NAME "IOCP"
#include "test_http_client_proxy_https.c"


