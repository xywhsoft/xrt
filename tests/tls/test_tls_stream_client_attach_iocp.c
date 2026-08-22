/* 使用同一组事件接管契约验证 Windows IOCP 后端。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_TLS_STREAM_BACKEND_NAME "IOCP client attach"
#define TEST_TLS_STREAM_CLIENT_ATTACH
#include "test_tls_stream.c"
