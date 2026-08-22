/* 使用同一份真实 TLS 连接池契约验证 Windows IOCP。 */
#define TEST_HTTP_POOL_HTTPS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_POOL_HTTPS_BACKEND_NAME "IOCP"
#include "test_http_client_pool_https.c"


