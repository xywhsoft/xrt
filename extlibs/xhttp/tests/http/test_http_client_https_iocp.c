/* 使用同一真实 TLS 高层客户端契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_HTTPS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_HTTPS_BACKEND_NAME "IOCP"
#include "test_http_client_https.c"
