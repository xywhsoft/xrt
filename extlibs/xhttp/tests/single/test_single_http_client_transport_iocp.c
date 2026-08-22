/* 复用完整单头 HTTP Client 契约验证 Windows IOCP。 */
#define TEST_SINGLE_HTTP_CLIENT_BACKEND XNET_PORT_IOCP
#define TEST_SINGLE_HTTP_CLIENT_BACKEND_NAME "single IOCP"
#include "test_single_http_client_transport.c"
