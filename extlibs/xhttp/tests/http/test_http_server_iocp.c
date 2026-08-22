/* 使用同一组 HTTP Server 契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_SERVER_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_SERVER_BACKEND_NAME "IOCP"
#include "test_http_server.c"
