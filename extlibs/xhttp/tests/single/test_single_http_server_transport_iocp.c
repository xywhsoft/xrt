/* 复用完整单头 HTTP Server 契约验证 Windows IOCP。 */
#define TEST_SINGLE_HTTP_SERVER_BACKEND XNET_PORT_IOCP
#define TEST_SINGLE_HTTP_SERVER_BACKEND_NAME "single IOCP"
#include "test_single_http_server_transport.c"
