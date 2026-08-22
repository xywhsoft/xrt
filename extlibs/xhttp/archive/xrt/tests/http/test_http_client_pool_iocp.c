/* 使用同一份连接池契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_POOL_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_POOL_BACKEND_NAME "IOCP"
#include "test_http_client_pool.c"
