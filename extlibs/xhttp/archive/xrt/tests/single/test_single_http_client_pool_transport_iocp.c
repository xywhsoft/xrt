/* 复用完整单头 HTTP Client Pool 契约验证 Windows IOCP。 */
#define TEST_SINGLE_HTTP_POOL_BACKEND XNET_PORT_IOCP
#define TEST_SINGLE_HTTP_POOL_BACKEND_NAME "single IOCP"
#include "test_single_http_client_pool_transport.c"
