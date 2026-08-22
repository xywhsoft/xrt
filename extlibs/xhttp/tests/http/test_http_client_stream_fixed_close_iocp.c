/* 使用 IOCP 验证固定长度响应完成与 FIN 的同批竞态。 */
#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "IOCP"
#include "test_http_client_stream_fixed_close.c"
