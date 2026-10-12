/* 使用同一组连续正文唤醒契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "IOCP"
#include "test_http_client_stream_async_reentrant.c"
