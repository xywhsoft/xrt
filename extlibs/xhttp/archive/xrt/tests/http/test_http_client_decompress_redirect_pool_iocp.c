/* 在 IOCP 后端复用自动解压、重定向与连接池组合测试。 */
#define TEST_HTTP_POOL_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_POOL_BACKEND_NAME "iocp"
#include "test_http_client_decompress_redirect_pool.c"
