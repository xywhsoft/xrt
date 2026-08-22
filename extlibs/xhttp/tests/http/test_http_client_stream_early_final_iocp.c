/* 使用同一份提前最终响应契约验证 Windows IOCP。 */
#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "IOCP"
#include "test_http_client_stream_early_final.c"
