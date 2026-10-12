/* 使用同一份高层 HTTP 客户端契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_BACKEND_NAME "IOCP"
#include "test_http_client.c"
